#include "storage/profile.h"
#include "core/game_config.h"
#include "game/exploration.h"
#include "player/player.h"
#include "player/skills.h"
#include "storage/save.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {
// Raised for a profile written by a newer build: never move or overwrite it.
struct NewerProfileVersion : runtime_error {
  NewerProfileVersion() : runtime_error("档案版本较新") {}
};
const char *jobDisplayName(const string &saveKey) {
  for (Job job : {Job::Warrior, Job::Mage, Job::Assassin})
    if (saveKey == jobProfile(job).saveKey) return jobProfile(job).displayName;
  return "未知";
}
long long parseInteger(const string &text, long long minimum, long long maximum, const char *field) {
  istringstream input(text);
  long long value;
  if (!(input >> value) || value < minimum || value > maximum)
    throw runtime_error(string("档案数值无效: ") + field);
  input >> ws;
  if (!input.eof()) throw runtime_error(string("档案数值包含多余字符: ") + field);
  return value;
}
int parseNumber(const string &text, int minimum, int maximum, const char *field) {
  return static_cast<int>(parseInteger(text, minimum, maximum, field));
}
vector<string> splitFields(const string &text, char delimiter, size_t maxFields) {
  vector<string> fields;
  size_t start = 0;
  while (fields.size() + 1 < maxFields) {
    const size_t end = text.find(delimiter, start);
    if (end == string::npos) break;
    fields.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  fields.push_back(text.substr(start));
  return fields;
}
bool validDate(const string &date) {
  if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
  for (size_t i = 0; i < date.size(); ++i)
    if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) return false;
  return true;
}
bool knownJob(const string &saveKey) {
  for (Job job : {Job::Warrior, Job::Mage, Job::Assassin})
    if (saveKey == jobProfile(job).saveKey) return true;
  return false;
}
} // namespace

string todayDate() {
  const time_t now = time(nullptr);
  tm local{};
#ifdef _WIN32
  const bool ok = localtime_s(&local, &now) == 0;
#else
  const bool ok = localtime_r(&now, &local) != nullptr;
#endif
  char buffer[16] = "1970-01-01";
  if (ok) strftime(buffer, sizeof buffer, "%Y-%m-%d", &local);
  return buffer;
}

const vector<AchievementInfo> &Profile::achievements() {
  static const vector<AchievementInfo> list = {
      {"first_chapter", "初出茅庐", "击败第10层Boss"},
      {"halfway", "半程远征", "击败第50层Boss"},
      {"summit", "百层登顶", "击败第100层Boss，完成远征"},
      {"scarce_summit", "精打细算", "在补给匮乏模式下完成远征"},
      {"iron_trial", "铁人试炼", "在无复苏模式下击败第30层Boss"},
      {"iron_summit", "不灭传说", "在无复苏模式下完成远征"},
      {"flawless_boss", "毫发无伤", "不受任何伤害击败一名Boss"},
      {"no_potion_boss", "以身为盾", "不使用药水击败第50层或更高的Boss"},
      {"last_stand", "绝境逆转", "以低于10%的生命击败一名Boss"},
      {"interrupt_master", "打断专家", "在一场战斗中打断3次蓄力"},
      {"specialist", "术业专攻", "为前两个技能都选择专精"},
      {"epic_gear", "史诗之选", "装备一件史诗品质装备"},
      {"all_routes", "条条大路", "在同一次远征中用三种路线各完成一章"},
      {"rich", "腰缠万贯", "同时持有20000金币"},
      {"affix_scholar", "词缀学者", "遭遇全部7种敌人词缀"},
      {"collector", "博物学家", "在图鉴中发现全部50种敌人"},
      {"all_jobs", "三职业远征", "用战士、法师、刺客分别击败第10层Boss"},
      {"explorer", "足迹遍布", "单次远征探索300处地点"},
      {"vault_hunter", "宝库猎人", "单次远征打开10座宝库"},
      {"puzzle_master", "解谜大师", "单次远征解开全部10个章节谜题"},
      {"gourmet", "远征厨师", "单次远征做20次料理"},
      {"lore_keeper", "见闻初录", "收集10页见闻录"},
      {"lore_complete", "百层见闻", "收集全部100页见闻录"},
  };
  return list;
}

Profile Profile::load(const string &path, string *warning) {
  Profile profile;
  profile.path = path;
  error_code error;
  if (!filesystem::exists(path, error)) return profile;
  ostringstream text;
  {
    // Close the file before any rename below: Windows cannot rename a file
    // that is still open.
    ifstream input(path, ios::binary);
    if (!input) {
      profile.canWrite = false;
      if (warning) *warning = "档案无法读取，本次不会写入：" + path;
      return profile;
    }
    text << input.rdbuf();
  }
  try {
    Profile parsed = parse(text.str());
    parsed.path = path;
    return parsed;
  } catch (const NewerProfileVersion &) {
    profile.canWrite = false;
    if (warning) *warning = "档案来自更新的游戏版本，本次不会写入：" + path;
    return profile;
  } catch (const exception &failure) {
    const string message = failure.what();
    const string aside = path + ".broken";
    filesystem::remove(aside, error);
    filesystem::rename(path, aside, error);
    if (error) {
      profile.canWrite = false;
      if (warning) *warning = "档案损坏且无法移走，本次不会写入：" + message;
    } else if (warning) {
      *warning = "档案损坏（" + message + "），已移至 " + aside + "，将使用新档案。";
    }
    return profile;
  }
}

Profile Profile::parse(const string &text) {
  Profile profile;
  istringstream lines(text);
  string line;
  bool header = false;
  set<int> seenEntries, seenAffixes;
  while (getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    if (line.size() > 1024) throw runtime_error("档案行过长");
    istringstream row(line);
    string key, value;
    row >> key;
    getline(row >> ws, value);
    if (!header) {
      if (key != "PROFILE") throw runtime_error("缺少档案头");
      const int version = parseNumber(value, 1, 1000000, "版本");
      if (version > Version) throw NewerProfileVersion();
      header = true;
      continue;
    }
    if (key == "JOBS_CH1") {
      profile.chapterOneJobs = parseNumber(value, 0, 7, "职业记录");
    } else if (key == "SEEN") {
      istringstream fields(value);
      string id, seen, won, first;
      fields >> id >> seen >> won >> first;
      const int entry = parseNumber(id, 0, Bestiary::EntryCount - 1, "图鉴编号");
      if (!seenEntries.insert(entry).second) throw runtime_error("图鉴重复");
      auto &record = profile.bestiary[entry];
      record.seen = parseNumber(seen, 1, GameConfig::MaxCounter, "遭遇次数");
      record.won = parseNumber(won, 0, record.seen, "战胜次数");
      record.firstFloor = parseNumber(first, 1, GameConfig::MaxFloor, "首遇楼层");
      if (GameConfig::chapter(record.firstFloor) != Bestiary::entryChapter(entry))
        throw runtime_error("图鉴楼层与章节不符");
    } else if (key == "AFFIX") {
      istringstream fields(value);
      string id, count;
      fields >> id >> count;
      const int affix = parseNumber(id, 0, AffixCount - 1, "词缀编号");
      if (!seenAffixes.insert(affix).second) throw runtime_error("词缀重复");
      profile.affixes[affix] = parseNumber(count, 1, GameConfig::MaxCounter, "词缀次数");
    } else if (key == "ACHIEVEMENT") {
      if (value.empty() || value.size() > 64 || value.find_first_of(" |") != string::npos)
        throw runtime_error("成就键无效");
      profile.unlockedKeys.insert(value); // Unknown keys survive for newer builds.
    } else if (key == "LORE") {
      if (value.size() != profile.lorePages.size() || value.find_first_not_of("01") != string::npos)
        throw runtime_error("见闻录记录无效");
      for (size_t i = 0; i < value.size(); ++i) profile.lorePages[i] = value[i] == '1';
    } else if (key == "RECORD") {
      const auto fields = splitFields(value, '|', 12);
      if (fields.size() != 12) throw runtime_error("记录字段数量错误");
      if (profile.runRecords.size() >= MaxRecords) throw runtime_error("记录过多");
      RunRecord record;
      record.date = fields[0];
      if (!validDate(record.date)) throw runtime_error("记录日期无效");
      record.job = fields[1];
      if (!knownJob(record.job)) throw runtime_error("记录职业无效");
      record.challenge = parseNumber(fields[2], 0, 2, "挑战");
      record.cleared = parseNumber(fields[3], 0, 1, "通关") != 0;
      record.floor = parseNumber(fields[4], 1, GameConfig::MaxFloor, "楼层");
      record.level = parseNumber(fields[5], 1, 10000, "等级");
      record.battles = parseNumber(fields[6], 0, GameConfig::MaxCounter, "战斗");
      record.turns = parseNumber(fields[7], 0, GameConfig::MaxCounter, "回合");
      record.deaths = parseNumber(fields[8], 0, GameConfig::MaxCounter, "阵亡");
      record.potions = parseNumber(fields[9], 0, GameConfig::MaxCounter, "药水");
      record.seed = static_cast<unsigned>(parseInteger(fields[10], 0, 4294967295LL, "种子"));
      record.name = fields[11];
      if (record.name.empty() || record.name.size() > 256) throw runtime_error("记录名字无效");
      profile.runRecords.push_back(record);
    } else {
      throw runtime_error("未知档案字段: " + key);
    }
  }
  if (!header) throw runtime_error("缺少档案头");
  return profile;
}

string Profile::serialize() const {
  ostringstream output;
  output << "PROFILE " << Version << "\nJOBS_CH1 " << chapterOneJobs << '\n';
  for (int id = 0; id < Bestiary::EntryCount; ++id) {
    const auto &record = bestiary[id];
    if (record.seen > 0)
      output << "SEEN " << id << ' ' << record.seen << ' ' << record.won << ' '
             << record.firstFloor << '\n';
  }
  for (int id = 0; id < AffixCount; ++id)
    if (affixes[id] > 0) output << "AFFIX " << id << ' ' << affixes[id] << '\n';
  for (const auto &key : unlockedKeys) output << "ACHIEVEMENT " << key << '\n';
  if (loreCount()) {
    output << "LORE ";
    for (bool found : lorePages) output << (found ? '1' : '0');
    output << '\n';
  }
  for (const auto &record : runRecords)
    output << "RECORD " << record.date << '|' << record.job << '|' << record.challenge << '|'
           << (record.cleared ? 1 : 0) << '|' << record.floor << '|' << record.level << '|'
           << record.battles << '|' << record.turns << '|' << record.deaths << '|'
           << record.potions << '|' << record.seed << '|' << record.name << '\n';
  return output.str();
}

bool Profile::save() const {
  if (!canWrite || path.empty()) return false;
  if (!SaveSystem::writeFileAtomically(path, serialize())) {
    cerr << "[档案] 写入失败，原档案保留。" << endl;
    return false;
  }
  return true;
}

void Profile::recordEncounter(int floor, const enemy &monster) {
  auto &record = bestiary[Bestiary::entryForFloor(floor)];
  if (record.seen == 0) record.firstFloor = GameConfig::floor(floor);
  record.seen = GameConfig::addCounter(record.seen, 1);
  for (Affix affix : monster.getAffixes()) {
    int &count = affixes[static_cast<int>(affix)];
    count = GameConfig::addCounter(count, 1);
  }
}

void Profile::recordVictory(int floor) {
  auto &record = bestiary[Bestiary::entryForFloor(floor)];
  if (record.seen > record.won) ++record.won;
}

void Profile::addRecord(RunRecord record) {
  for (char &c : record.name)
    if (c == '\n' || c == '\r') c = ' ';
  if (record.name.empty()) record.name = "旅人";
  if (record.name.size() > 256) record.name.resize(256);
  if (!validDate(record.date)) record.date = todayDate();
  record.floor = GameConfig::floor(record.floor);
  runRecords.push_back(std::move(record));
  if (runRecords.size() > MaxRecords)
    runRecords.erase(runRecords.begin(), runRecords.end() - MaxRecords);
}

vector<const AchievementInfo *> Profile::checkAchievements(const player &hero,
                                                           const BattleOutcome *battle) {
  const RunState &run = hero.runState();
  if (run.cleared[10]) chapterOneJobs |= 1 << static_cast<int>(hero.job());
  const bool bossWin = battle && battle->won && battle->floor % 10 == 0;
  bool specialist = hero.getSkillCount() >= 2;
  for (int slot = 1; slot <= 2 && specialist; ++slot)
    specialist = hero.getSkill(slot)->getSpecialization() != 0;
  bool epic = false;
  for (const Item &item : hero.getSaveEquipments()) epic |= item.getTier() == 3;
  set<int> routesCompleted;
  for (int chapter = 0; chapter < 10; ++chapter)
    if (run.cleared[chapter * 10 + 10] && run.routes[chapter] != 0)
      routesCompleted.insert(run.routes[chapter]);

  auto holds = [&](const string &key) {
    if (key == "first_chapter") return run.cleared[10] != 0;
    if (key == "halfway") return run.cleared[50] != 0;
    if (key == "summit") return run.cleared[100] != 0;
    if (key == "scarce_summit") return run.challenge == 1 && run.cleared[100] != 0;
    if (key == "iron_trial") return run.challenge == 2 && run.cleared[30] != 0;
    if (key == "iron_summit") return run.challenge == 2 && run.cleared[100] != 0;
    if (key == "flawless_boss") return bossWin && battle->damageTaken == 0;
    if (key == "no_potion_boss") return bossWin && battle->floor >= 50 && battle->potionsUsed == 0;
    if (key == "last_stand")
      return bossWin && static_cast<long long>(battle->hpAfter) * 10 < battle->maxHp;
    if (key == "interrupt_master") return battle && battle->interrupts >= 3;
    if (key == "specialist") return specialist;
    if (key == "epic_gear") return epic;
    if (key == "all_routes") return routesCompleted.size() == 3;
    if (key == "rich") return hero.getcoin() >= 20000;
    if (key == "affix_scholar") return affixesDiscovered() == AffixCount;
    if (key == "collector") return discovered() == Bestiary::EntryCount;
    if (key == "all_jobs") return chapterOneJobs == 7;
    if (key == "explorer") return run.spotsExplored >= 300;
    if (key == "vault_hunter") return run.vaultsOpened >= 10;
    if (key == "puzzle_master") return run.puzzlesSolved >= 10;
    if (key == "gourmet") return run.mealsCooked >= 20;
    if (key == "lore_keeper") return loreCount() >= 10;
    if (key == "lore_complete") return loreCount() == static_cast<int>(lorePages.size());
    return false;
  };
  vector<const AchievementInfo *> unlockedNow;
  for (const auto &achievement : achievements())
    if (!unlocked(achievement.key) && holds(achievement.key)) {
      unlockedKeys.insert(achievement.key);
      unlockedNow.push_back(&achievement);
    }
  return unlockedNow;
}

bool Profile::recordLore(int page) {
  if (page < 1 || page > static_cast<int>(lorePages.size()) || lorePages[page - 1]) return false;
  lorePages[page - 1] = true;
  return true;
}
bool Profile::loreFound(int page) const {
  return page >= 1 && page <= static_cast<int>(lorePages.size()) && lorePages[page - 1];
}
int Profile::loreCount() const {
  return static_cast<int>(count(lorePages.begin(), lorePages.end(), true));
}

const Profile::BestiaryEntry &Profile::entry(int id) const {
  return bestiary[clamp(id, 0, Bestiary::EntryCount - 1)];
}
int Profile::discovered() const {
  return static_cast<int>(count_if(bestiary.begin(), bestiary.end(),
                                   [](const BestiaryEntry &e) { return e.seen > 0; }));
}
int Profile::affixSeen(Affix affix) const { return affixes[static_cast<int>(affix)]; }
int Profile::affixesDiscovered() const {
  return static_cast<int>(count_if(affixes.begin(), affixes.end(), [](int n) { return n > 0; }));
}
int Profile::unlockedCount() const {
  int count = 0;
  for (const auto &achievement : achievements()) count += unlocked(achievement.key);
  return count;
}

void Profile::printBestiaryOverview() const {
  cout << "敌人图鉴：已发现 " << discovered() << "/" << Bestiary::EntryCount << '\n';
  for (int chapter = 0; chapter < 10; ++chapter) {
    int found = 0;
    for (int slot = 0; slot < Bestiary::EntriesPerChapter; ++slot)
      found += bestiary[chapter * Bestiary::EntriesPerChapter + slot].seen > 0;
    cout << "  [" << chapter + 1 << "] 第" << chapter + 1 << "章 " << found << "/"
         << Bestiary::EntriesPerChapter << '\n';
  }
}
void Profile::printBestiaryChapter(int chapter) const {
  chapter = clamp(chapter, 0, 9);
  cout << "—— 第" << chapter + 1 << "章 ——\n";
  for (int slot = 0; slot < Bestiary::EntriesPerChapter; ++slot) {
    const int id = chapter * Bestiary::EntriesPerChapter + slot;
    const auto &record = bestiary[id];
    if (record.seen == 0) {
      cout << (Bestiary::entryIsBoss(id) ? "★ " : "· ") << "？？？（尚未遭遇）\n";
      continue;
    }
    cout << (Bestiary::entryIsBoss(id) ? "★ " : "· ") << Bestiary::entryName(id)
         << " | 首遇第" << record.firstFloor << "层 | 遭遇 " << record.seen << " | 战胜 "
         << record.won << "\n    " << Bestiary::entryDescription(id) << '\n';
  }
}
void Profile::printAffixes() const {
  cout << "词缀图鉴：已发现 " << affixesDiscovered() << "/" << AffixCount << '\n';
  for (int id = 0; id < AffixCount; ++id) {
    const Affix affix = static_cast<Affix>(id);
    if (affixes[id] == 0) cout << "· ？？？（尚未遭遇）\n";
    else cout << "· [" << affixName(affix) << "] 遭遇 " << affixes[id] << " 次："
              << affixDescription(affix) << '\n';
  }
}
void Profile::printAchievements() const {
  cout << "成就：" << unlockedCount() << "/" << achievements().size() << '\n';
  for (const auto &achievement : achievements())
    cout << (unlocked(achievement.key) ? "【已解锁】" : "【未解锁】") << achievement.title
         << "：" << achievement.description << '\n';
}
void Profile::printLoreOverview() const {
  cout << "见闻录：已收集 " << loreCount() << "/" << lorePages.size()
       << "（每层一页，藏在地图的隐秘处）\n";
  for (int chapter = 0; chapter < 10; ++chapter) {
    int found = 0;
    for (int page = chapter * 10 + 1; page <= chapter * 10 + 10; ++page) found += loreFound(page);
    cout << "  [" << chapter + 1 << "] 第" << chapter + 1 << "章 " << found << "/10\n";
  }
}
void Profile::printLoreChapter(int chapter) const {
  chapter = clamp(chapter, 0, 9);
  cout << "—— 第" << chapter + 1 << "章见闻 ——\n";
  for (int page = chapter * 10 + 1; page <= chapter * 10 + 10; ++page) {
    if (!loreFound(page)) {
      cout << "· 第" << page << "页：？？？（第" << page << "层）\n";
      continue;
    }
    cout << "· 第" << page << "页 " << Exploration::loreTitle(page) << "\n    "
         << Exploration::loreText(page) << '\n';
  }
}
void Profile::printRecords() const {
  if (runRecords.empty()) {
    cout << "还没有完成的远征。通关或阵亡结束远征后会记录在这里。\n";
    return;
  }
  cout << "最近的远征（最多保留" << MaxRecords << "条，新的在前）：\n";
  for (auto it = runRecords.rbegin(); it != runRecords.rend(); ++it)
    cout << it->date << " | " << it->name << " | " << jobDisplayName(it->job) << " | "
         << GameConfig::challengeName(it->challenge) << " | " << (it->cleared ? "通关" : "止步第" +
         to_string(it->floor) + "层") << " | Lv." << it->level << " | 战斗 " << it->battles
         << " | 阵亡 " << it->deaths << " | 药水 " << it->potions << " | 种子 " << it->seed << '\n';
  cout << "各职业最佳：";
  for (Job job : {Job::Warrior, Job::Mage, Job::Assassin}) {
    int best = 0;
    bool cleared = false;
    for (const auto &record : runRecords)
      if (record.job == jobProfile(job).saveKey) {
        best = max(best, record.floor);
        cleared |= record.cleared;
      }
    cout << ' ' << jobProfile(job).displayName << " "
         << (cleared ? "通关" : (best ? "第" + to_string(best) + "层" : "—"));
  }
  cout << '\n';
}
