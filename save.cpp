#include "save.h"
#include "game_config.h"
#include "jobs.h"
#include "progression.h"
#include "skills.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

namespace {
constexpr int kMaxStat = 1000000;
constexpr int kMaxItems = 10000;
constexpr int kMaxLegacySkillRank = 1000;
const vector<string> runKeys{
    "RUN_SEED", "CHALLENGE", "LEGACY_RULES", "LOOT_TARGET", "PITY", "MATERIALS",
    "ROUTES", "EVENTS", "CAMP_RESTED", "ORDERS_CLAIMED", "SHOP_REFRESHES",
    "SHOP_PURCHASED", "CLEARED", "ATTEMPTS", "BATTLES", "TURNS", "POTIONS_USED",
    "DEATHS", "DAMAGE_TAKEN", "USEFUL_DROPS", "GEAR_DROUGHT", "LONGEST_GEAR_DROUGHT"};

vector<string> split(const string &s, char delim) {
  vector<string> tokens;
  stringstream ss(s);
  string tok;
  while (getline(ss, tok, delim)) tokens.push_back(tok);
  if (!s.empty() && s.back() == delim) tokens.emplace_back();
  return tokens;
}

long long parseInteger(const string &text, long long minimum, long long maximum,
                       const string &field) {
  istringstream input(text);
  long long value;
  if (!(input >> value) || value < minimum || value > maximum)
    throw runtime_error("无效数值: " + field);
  input >> ws;
  if (!input.eof()) throw runtime_error("数值包含多余字符: " + field);
  return value;
}
int parseNumber(const string &text, int minimum, int maximum, const string &field) {
  return static_cast<int>(parseInteger(text, minimum, maximum, field));
}

template<size_t N>
array<int, N> parseArray(const string &text, int minimum, int maximum, const string &field) {
  const auto values = split(text, '|');
  if (values.size() != N) throw runtime_error("数组长度错误: " + field);
  array<int, N> result{};
  for (size_t i = 0; i < N; ++i)
    result[i] = parseNumber(values[i], minimum, maximum, field);
  return result;
}
template<size_t N>
void writeArray(ostream &output, const string &key, const array<int, N> &values) {
  output << key << ' ';
  for (size_t i = 0; i < N; ++i) output << (i ? "|" : "") << values[i];
  output << '\n';
}

void requireRange(int value, int minimum, int maximum, const string &field) {
  if (value < minimum || value > maximum) throw runtime_error("范围错误: " + field);
}
template<size_t N>
void validateArray(const array<int, N> &values, int maximum, const string &field) {
  for (int value : values) requireRange(value, 0, maximum, field);
}
void validateRun(const RunState &run, int highestFloor) {
  requireRange(run.challenge, 0, 2, "挑战");
  requireRange(run.lootTarget, Weapon, Armor, "掉落目标");
  requireRange(run.pity, 0, 8, "保底");
  requireRange(run.materials, 0, GameConfig::MaxMaterials, "材料");
  validateArray(run.routes, 3, "路线");
  validateArray(run.events, 2, "事件");
  validateArray(run.campRested, 1, "营地休息");
  validateArray(run.ordersClaimed, 1, "订单");
  validateArray(run.shopRefreshes, 20, "商店刷新");
  validateArray(run.shopPurchased, 31, "商店购买");
  validateArray(run.cleared, 1, "通关记录");
  validateArray(run.attempts, GameConfig::MaxCounter, "尝试次数");
  for (int value : {run.battles, run.turns, run.potionsUsed, run.deaths, run.damageTaken,
                    run.usefulDrops, run.gearDrought, run.longestGearDrought})
    requireRange(value, 0, GameConfig::MaxCounter, "统计");
  if (run.cleared[0] != 0 || run.attempts[0] != 0)
    throw runtime_error("楼层数组第0项必须为空");
  for (int floor = 1; floor <= 100; ++floor) {
    if (run.cleared[floor] && run.attempts[floor] == 0)
      throw runtime_error("通关楼层缺少尝试记录");
    if (floor > highestFloor && (run.cleared[floor] || run.attempts[floor]))
      throw runtime_error("楼层记录超出最高楼层");
  }
  if (run.longestGearDrought < run.gearDrought)
    throw runtime_error("装备空窗统计矛盾");
}

bool canEquip(const Item &item, const string &job) {
  const auto required = item.getJobRequirement();
  return required == AnyJob || (required == WarriorOnly && job == "Warrior") ||
      (required == MageOnly && job == "Mage") || (required == AssassinOnly && job == "Assassin");
}
void validateItem(const Item &item) {
  if (item.getName().empty() || item.getName().size() > 256 ||
      item.getName().find_first_of("|\r\n") != string::npos)
    throw runtime_error("物品名称无效");
  requireRange(item.getLevelRequirement(), 1, 10000, "物品等级");
  requireRange(static_cast<int>(item.getType()), Weapon, Potion, "物品类型");
  requireRange(item.getValue(), 0, kMaxStat, "物品数值");
  requireRange(item.getDurability(), 0, kMaxStat, "耐久");
  requireRange(item.getPrice(), 0, 1000000000, "物品价格");
  requireRange(item.getQuantity(), 1, 1000000000, "物品数量");
  requireRange(static_cast<int>(item.getJobRequirement()), AnyJob, AssassinOnly, "物品职业");
  requireRange(item.getTier(), 0, 3, "物品品质");
  requireRange(static_cast<int>(item.getEffect()), 0, 4, "物品效果");
  if (!item.supportsEffect(item.getEffect())) throw runtime_error("物品效果不适用");
  if (item.getType() == Potion && item.getJobRequirement() != AnyJob)
    throw runtime_error("药水职业限制无效");
  if (item.getId() != Item::templateId(item.getName(), item.getType(),
                                      item.getJobRequirement(), item.getTier()))
    throw runtime_error("物品模板ID不匹配");
}

unique_ptr<player> newHero(const string &job, const string &name) {
  if (job == "Warrior") return make_unique<Warrior>(name);
  if (job == "Mage") return make_unique<Mage>(name);
  if (job == "Assassin") return make_unique<Assassin>(name);
  throw runtime_error("未知职业: " + job);
}

void writeRun(ostream &output, const RunState &run) {
  output << "RUN_SEED " << run.seed << "\nCHALLENGE " << run.challenge
         << "\nLEGACY_RULES " << (run.legacyRules ? 1 : 0)
         << "\nLOOT_TARGET " << run.lootTarget << "\nPITY " << run.pity
         << "\nMATERIALS " << run.materials << '\n';
  writeArray(output, "ROUTES", run.routes);
  writeArray(output, "EVENTS", run.events);
  writeArray(output, "CAMP_RESTED", run.campRested);
  writeArray(output, "ORDERS_CLAIMED", run.ordersClaimed);
  writeArray(output, "SHOP_REFRESHES", run.shopRefreshes);
  writeArray(output, "SHOP_PURCHASED", run.shopPurchased);
  writeArray(output, "CLEARED", run.cleared);
  writeArray(output, "ATTEMPTS", run.attempts);
  output << "BATTLES " << run.battles << "\nTURNS " << run.turns
         << "\nPOTIONS_USED " << run.potionsUsed << "\nDEATHS " << run.deaths
         << "\nDAMAGE_TAKEN " << run.damageTaken << "\nUSEFUL_DROPS " << run.usefulDrops
         << "\nGEAR_DROUGHT " << run.gearDrought
         << "\nLONGEST_GEAR_DROUGHT " << run.longestGearDrought << '\n';
}

// A temporary file is written beside the target. POSIX rename replaces it
// atomically. Windows rename cannot replace an existing file: keep a unique
// backup until the replacement succeeds, and roll back on any failure.
bool replaceSave(const filesystem::path &target, const string &contents) {
  static atomic<unsigned long long> counter{0};
  const string suffix = "." + to_string(chrono::steady_clock::now().time_since_epoch().count()) +
                        "." + to_string(counter++);
  const filesystem::path temporary = target.string() + ".tmp" + suffix;
  const filesystem::path backup = target.string() + ".bak" + suffix;
  error_code error;
  const bool existed = filesystem::exists(target, error);
  if (error || (existed && !filesystem::is_regular_file(target, error)) || error) return false;
  {
    ofstream output(temporary, ios::binary | ios::trunc);
    if (!output) return false;
    output.write(contents.data(), static_cast<streamsize>(contents.size()));
    output.flush();
    if (!output) {
      output.close();
      filesystem::remove(temporary, error);
      return false;
    }
    output.close();
    if (!output) {
      filesystem::remove(temporary, error);
      return false;
    }
  }
  filesystem::rename(temporary, target, error);
  if (!error) return true;
  if (!existed) {
    filesystem::remove(temporary, error);
    return false;
  }
  error.clear();
  filesystem::rename(target, backup, error);
  if (error) {
    filesystem::remove(temporary, error);
    return false;
  }
  filesystem::rename(temporary, target, error);
  if (error) {
    error_code restoreError;
    filesystem::rename(backup, target, restoreError);
    if (restoreError)
      cerr << "[存档] 无法恢复原路径，旧存档保留在 " << backup.string() << endl;
    filesystem::remove(temporary, error);
    return false;
  }
  filesystem::remove(backup, error);
  return true;
}
} // namespace

string SaveSystem::serializeItem(const Item &item) {
  validateItem(item);
  ostringstream output;
  output << item.getName() << '|' << item.getLevelRequirement() << '|'
         << static_cast<int>(item.getType()) << '|' << item.getValue() << '|'
         << item.getDurability() << '|' << item.getPrice() << '|' << item.getQuantity()
         << '|' << static_cast<int>(item.getJobRequirement()) << '|' << item.getTier()
         << '|' << static_cast<int>(item.getEffect()) << '|' << (item.isLocked() ? 1 : 0)
         << '|' << item.getId();
  return output.str();
}

Item SaveSystem::deserializeItem(const string &line) {
  const auto tok = split(line, '|');
  if (tok.size() != 7 && tok.size() != 8 && tok.size() != 12)
    throw runtime_error("存档物品格式错误");
  const int level = parseNumber(tok[1], 1, 10000, "物品等级");
  const auto type = static_cast<ItemType>(parseNumber(tok[2], Weapon, Potion, "物品类型"));
  const int value = parseNumber(tok[3], 0, kMaxStat, "物品数值");
  const int durability = parseNumber(tok[4], 0, kMaxStat, "耐久");
  const int price = parseNumber(tok[5], 0, 1000000000, "价格");
  const int quantity = parseNumber(tok[6], 1, 1000000000, "数量");
  const auto job = tok.size() >= 8
      ? static_cast<JobReq>(parseNumber(tok[7], AnyJob, AssassinOnly, "职业限制")) : AnyJob;
  const int tier = tok.size() == 12 ? parseNumber(tok[8], 0, 3, "品质") : 0;
  const auto effect = tok.size() == 12
      ? static_cast<ItemEffect>(parseNumber(tok[9], 0, 4, "效果")) : ItemEffect::None;
  const bool locked = tok.size() == 12 && parseNumber(tok[10], 0, 1, "锁定") != 0;
  if (tok.size() == 12 && tok[11].empty()) throw runtime_error("缺少物品ID");
  Item result(tok[0], level, type, value, durability, price, quantity, job, tier,
               effect, locked, tok.size() == 12 ? tok[11] : "");
  validateItem(result);
  return result;
}

bool SaveSystem::saveGame(const player &hero, int floor, const string &filename) {
  try {
    requireRange(floor, 1, 101, "楼层");
    const int highest = min(100, max(floor, hero.getHighestFloor()));
    validateRun(hero.runState(), highest);
    if (hero.getname().empty() || hero.getname().size() > 256 ||
        hero.getname().find_first_of("\r\n") != string::npos)
      throw runtime_error("角色名称无效");
    requireRange(hero.getlevel(), 1, hero.runState().legacyRules ? 10000 : 100, "等级");
    requireRange(hero.getexp(), 0, 1000000000, "经验");
    requireRange(hero.getcoin(), 0, 1000000000, "金币");
    requireRange(hero.getmaxhp(), 1, kMaxStat, "生命上限");
    requireRange(hero.gethp(), 0, hero.getmaxhp(), "生命");
    requireRange(hero.getattack(), 1, kMaxStat, "攻击");
    requireRange(hero.getmaxmp(), 0, kMaxStat, "魔力上限");
    requireRange(hero.getmp(), 0, hero.getmaxmp(), "魔力");
    requireRange(hero.getcritRate(), 0, 100, "暴击");
    requireRange(hero.getfreerevives(), 0, 1000, "复活");
    ostringstream output;
    output << "VERSION 3\nNAME " << hero.getname() << "\nJOB " << hero.getjob()
           << "\nLEVEL " << hero.getlevel() << "\nEXP " << hero.getexp()
           << "\nCOIN " << hero.getcoin() << "\nHP " << hero.gethp()
           << "\nMAX_HP " << hero.getmaxhp() << "\nATTACK " << hero.getattack()
           << "\nMP " << hero.getmp() << "\nMAX_MP " << hero.getmaxmp()
           << "\nCRIT_RATE " << hero.getcritRate() << "\nFREE_REVIVES " << hero.getfreerevives()
           << "\nFLOOR " << floor << "\nHIGHEST_FLOOR " << highest << '\n';
    writeRun(output, hero.runState());
    const auto &backpack = hero.getSaveBackpack(), &equipped = hero.getSaveEquipments();
    if (backpack.size() > kMaxItems || equipped.size() > 2) throw runtime_error("物品过多");
    output << "BACKPACK_COUNT " << backpack.size() << '\n';
    long long totalQuantity = 0;
    for (const auto &item : backpack) {
      output << "ITEM " << serializeItem(item) << '\n';
      totalQuantity += item.getQuantity();
    }
    output << "EQUIP_COUNT " << equipped.size() << '\n';
    set<ItemType> slots;
    int attackBonus = 0, hpBonus = 0;
    for (const auto &item : equipped) {
      if (item.getType() == Potion || item.getQuantity() != 1 || item.getDurability() <= 0 ||
          !slots.insert(item.getType()).second || !canEquip(item, hero.getjob()))
        throw runtime_error("装备槽位或职业无效");
      output << "EQUIP " << serializeItem(item) << '\n';
      totalQuantity += item.getQuantity();
      if (item.getType() == Weapon) attackBonus += item.getValue();
      else hpBonus += item.getValue();
    }
    if (hero.getattack() <= attackBonus || hero.getmaxhp() <= hpBonus)
      throw runtime_error("装备加成超出角色属性");
    if (totalQuantity > 1000000000) throw runtime_error("物品总数量过多");
    output << "SKILL_COUNT " << hero.getSkillCount() << '\n';
    set<int> skillIds;
    for (int slot = 1; slot <= hero.getSkillCount(); ++slot) {
      const skill *s = hero.getSkill(slot);
      if (!s || !skillIds.insert(s->getskillid()).second) throw runtime_error("技能ID无效");
      requireRange(s->getskilllevel(), 1, hero.runState().legacyRules ? kMaxLegacySkillRank :
                     Progression::skillRankCap(highest), "技能等级");
      requireRange(s->getcurrentcooldown(), 0, s->getcooldown(), "技能冷却");
      requireRange(s->getSpecialization(), 0, slot <= 2 ? 2 : 0, "专精");
      if (s->getSpecialization() && (highest < (slot == 1 ? 10 : 30) ||
                                     hero.getlevel() < s->getlevelrequired()))
        throw runtime_error("专精未解锁");
      output << "SKILL " << s->getskillid() << '|' << s->getskilllevel() << '|'
             << s->getcurrentcooldown() << '|' << s->getSpecialization() << '\n';
    }
    if (!replaceSave(filesystem::path(filename), output.str()))
      throw runtime_error("写入或替换失败，原存档保留");
    cout << "[存档] 游戏已保存到 " << filename << endl;
    return true;
  } catch (const exception &error) {
    cerr << "[存档] " << error.what() << endl;
    return false;
  }
}

unique_ptr<player> SaveSystem::loadGame(int &floor, const string &filename) {
  ifstream input(filename, ios::binary);
  if (!input) {
    cerr << "[读档] 无法打开文件: " << filename << endl;
    return nullptr;
  }
  try {
    string name, job;
    int version = 0, level = 1, exp = 0, coin = 0, hp = 0, maxHp = 0, attack = 0;
    int mp = 0, maxMp = 0, crit = 0, revives = 0, savedFloor = 1, highest = 0;
    int backpackCount = 0, equipCount = 0, skillCount = 0, modernItems = 0, modernSkills = 0;
    RunState run;
    vector<Item> backpack, equipped;
    vector<tuple<int, int, int, int>> skillData;
    set<string> seen;
    set<int> skillIds;
    long long totalQuantity = 0;
    string line;
    while (getline(input, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.size() > 4096) throw runtime_error("存档行过长");
      istringstream row(line);
      string key, value;
      if (!(row >> key) || key[0] == '#') continue;
      getline(row >> ws, value);
      if (key != "ITEM" && key != "EQUIP" && key != "SKILL" && !seen.insert(key).second)
        throw runtime_error("重复字段: " + key);
      if (key == "VERSION") version = parseNumber(value, 1, 3, key);
      else if (key == "NAME") name = value;
      else if (key == "JOB") job = value;
      else if (key == "LEVEL") level = parseNumber(value, 1, 10000, key);
      else if (key == "EXP") exp = parseNumber(value, 0, 1000000000, key);
      else if (key == "COIN") coin = parseNumber(value, 0, 1000000000, key);
      else if (key == "HP") hp = parseNumber(value, 0, kMaxStat, key);
      else if (key == "MAX_HP") maxHp = parseNumber(value, 1, kMaxStat, key);
      else if (key == "ATTACK") attack = parseNumber(value, 1, kMaxStat, key);
      else if (key == "MP") mp = parseNumber(value, 0, kMaxStat, key);
      else if (key == "MAX_MP") maxMp = parseNumber(value, 0, kMaxStat, key);
      else if (key == "CRIT_RATE") crit = parseNumber(value, 0, 100, key);
      else if (key == "FREE_REVIVES") revives = parseNumber(value, 0, 1000, key);
      else if (key == "FLOOR") savedFloor = parseNumber(value, 1, 101, key);
      else if (key == "HIGHEST_FLOOR") highest = parseNumber(value, 1, 100, key);
      else if (key == "RUN_SEED")
        run.seed = static_cast<uint32_t>(parseInteger(value, 0, numeric_limits<uint32_t>::max(), key));
      else if (key == "CHALLENGE") run.challenge = parseNumber(value, 0, 2, key);
      else if (key == "LEGACY_RULES") run.legacyRules = parseNumber(value, 0, 1, key) != 0;
      else if (key == "LOOT_TARGET") run.lootTarget = parseNumber(value, Weapon, Armor, key);
      else if (key == "PITY") run.pity = parseNumber(value, 0, 8, key);
      else if (key == "MATERIALS") run.materials = parseNumber(value, 0, GameConfig::MaxMaterials, key);
      else if (key == "ROUTES") run.routes = parseArray<10>(value, 0, 3, key);
      else if (key == "EVENTS") run.events = parseArray<10>(value, 0, 2, key);
      else if (key == "CAMP_RESTED") run.campRested = parseArray<10>(value, 0, 1, key);
      else if (key == "ORDERS_CLAIMED") run.ordersClaimed = parseArray<10>(value, 0, 1, key);
      else if (key == "SHOP_REFRESHES") run.shopRefreshes = parseArray<10>(value, 0, 20, key);
      else if (key == "SHOP_PURCHASED") run.shopPurchased = parseArray<10>(value, 0, 31, key);
      else if (key == "CLEARED") run.cleared = parseArray<101>(value, 0, 1, key);
      else if (key == "ATTEMPTS") run.attempts = parseArray<101>(value, 0, GameConfig::MaxCounter, key);
      else if (key == "BATTLES") run.battles = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "TURNS") run.turns = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "POTIONS_USED") run.potionsUsed = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "DEATHS") run.deaths = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "DAMAGE_TAKEN") run.damageTaken = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "USEFUL_DROPS") run.usefulDrops = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "GEAR_DROUGHT") run.gearDrought = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "LONGEST_GEAR_DROUGHT") run.longestGearDrought = parseNumber(value, 0, GameConfig::MaxCounter, key);
      else if (key == "BACKPACK_COUNT") backpackCount = parseNumber(value, 0, kMaxItems, key);
      else if (key == "EQUIP_COUNT") equipCount = parseNumber(value, 0, 2, key);
      else if (key == "SKILL_COUNT") skillCount = parseNumber(value, 0, 15, key);
      else if (key == "ITEM" || key == "EQUIP") {
        auto &items = key == "ITEM" ? backpack : equipped;
        if (items.size() >= static_cast<size_t>(key == "ITEM" ? kMaxItems : 2))
          throw runtime_error("物品行数过多");
        if (split(value, '|').size() == 12) ++modernItems;
        items.push_back(deserializeItem(value));
        totalQuantity += items.back().getQuantity();
        if (totalQuantity > 1000000000) throw runtime_error("物品总数量过多");
      } else if (key == "SKILL") {
        const auto fields = split(value, '|');
        if ((fields.size() != 3 && fields.size() != 4) || skillData.size() >= 15)
          throw runtime_error("技能格式错误");
        int id = parseNumber(fields[0], 1, 1000, "技能ID");
        int rank = parseNumber(fields[1], 1, kMaxLegacySkillRank, "技能等级");
        int cooldown = parseNumber(fields[2], 0, 1000, "技能冷却");
        int specialization = fields.size() == 4 ? parseNumber(fields[3], 0, 2, "专精") : 0;
        if (fields.size() == 4) ++modernSkills;
        if (!skillIds.insert(id).second) throw runtime_error("重复技能ID");
        skillData.emplace_back(id, rank, cooldown, specialization);
      } else throw runtime_error("未知字段: " + key);
    }
    if (input.bad()) throw runtime_error("读取文件失败");
    for (const char *key : {"VERSION", "NAME", "JOB", "LEVEL", "EXP", "COIN", "HP", "MAX_HP",
                            "ATTACK", "MP", "MAX_MP", "CRIT_RATE", "FREE_REVIVES", "FLOOR",
                            "BACKPACK_COUNT", "EQUIP_COUNT", "SKILL_COUNT"})
      if (!seen.count(key)) throw runtime_error(string("缺少字段: ") + key);
    if (name.empty() || name.size() > 256 || hp > maxHp || mp > maxMp)
      throw runtime_error("角色属性无效");
    if (version >= 2 && !seen.count("HIGHEST_FLOOR")) throw runtime_error("缺少最高楼层");
    if (!seen.count("HIGHEST_FLOOR")) highest = min(savedFloor, 100);
    if (highest < min(savedFloor, 100)) throw runtime_error("最高楼层小于当前楼层");
    if (backpackCount != static_cast<int>(backpack.size()) || equipCount != static_cast<int>(equipped.size()) ||
        skillCount != static_cast<int>(skillData.size())) throw runtime_error("物品或技能数量不匹配");
    if (version == 3) {
      for (const auto &key : runKeys)
        if (!seen.count(key)) throw runtime_error("缺少章节字段: " + key);
      if (modernItems != backpackCount + equipCount || modernSkills != skillCount)
        throw runtime_error("V3缺少物品或专精字段");
    } else {
      for (const auto &key : runKeys)
        if (seen.count(key)) throw runtime_error("旧版本包含未知章节字段: " + key);
      if (modernItems || modernSkills) throw runtime_error("旧版本包含V3记录");
      run.legacyRules = true;
      // V2 retains the highest reached floor even after retreat. Floors below
      // it were already passed; floor 101 additionally proves floor 100 cleared.
      const int reachedFloor = max(savedFloor, highest);
      for (int i = 1; i < reachedFloor && i <= 100; ++i) {
        run.cleared[i] = 1;
        run.attempts[i] = 1;
      }
    }
    validateRun(run, highest);
    if (!run.legacyRules && level > 100) throw runtime_error("角色等级超出新规则上限");
    auto hero = newHero(job, name);
    int attackBonus = 0, hpBonus = 0;
    set<ItemType> slots;
    for (const auto &item : equipped) {
      if (item.getType() == Potion || item.getQuantity() != 1 || item.getDurability() <= 0 ||
          !slots.insert(item.getType()).second || !canEquip(item, job))
        throw runtime_error("装备槽位、数量或职业限制无效");
      if (item.getType() == Weapon) attackBonus += item.getValue();
      else hpBonus += item.getValue();
    }
    if (attack <= attackBonus || maxHp <= hpBonus) throw runtime_error("装备加成超出角色属性");
    if (skillCount != hero->getSkillCount()) throw runtime_error("职业技能数量不匹配");
    for (const auto &[id, rank, cooldown, specialization] : skillData) {
      skill *matched = nullptr;
      for (int slot = 1; slot <= hero->getSkillCount(); ++slot)
        if (hero->getSkill(slot)->getskillid() == id) matched = hero->getSkill(slot);
      if (!matched || !matched->setSpecialization(specialization) || cooldown > matched->getcooldown())
        throw runtime_error("未知技能、无效专精或冷却超出上限");
      if (!run.legacyRules && rank > Progression::skillRankCap(highest))
        throw runtime_error("技能等级超出阶段上限");
      if (specialization && (highest < (id == 1 ? 10 : 30) || level < matched->getlevelrequired()))
        throw runtime_error("专精未解锁");
      matched->restoreProgress(rank, cooldown);
    }
    hero->loadStats(level, exp, coin, min(hp, maxHp - hpBonus), maxHp - hpBonus,
                    attack - attackBonus, mp, maxMp, crit, revives);
    hero->setHighestFloor(highest);
    hero->runState() = run;
    for (const auto &item : backpack) hero->addItem(item);
    for (const auto &item : equipped) hero->equipItem_T(item, true);
    hero->sethp(hp);
    floor = savedFloor;
    cout << "[读档] 欢迎回来，" << name << "！当前楼层：" << floor << endl;
    return hero;
  } catch (const exception &error) {
    cerr << "[读档] 存档文件损坏或格式错误: " << error.what() << endl;
    return nullptr;
  }
}

bool SaveSystem::migrateLegacy(player &hero) {
  if (!hero.runState().legacyRules) return false;
  const int highest = hero.getHighestFloor();
  const int newLevel = min(hero.getlevel(), min(100, max(1, highest * 7 / 10 + 5)));
  const int rankCap = Progression::skillRankCap(highest);
  auto base = newHero(hero.getjob(), hero.getname());
  const int hpGrowth = hero.getjob() == "Warrior" ? 20 : (hero.getjob() == "Mage" ? 12 : 15);
  const int attackGrowth = hero.getjob() == "Mage" ? 2 : 3;
  const int mpGrowth = hero.getjob() == "Warrior" ? 3 : (hero.getjob() == "Mage" ? 8 : 4);
  int newMaxHp = base->getmaxhp() + (newLevel - 1) * hpGrowth;
  int newAttack = base->getattack() + (newLevel - 1) * attackGrowth;
  const int newMaxMp = base->getmaxmp() + (newLevel - 1) * mpGrowth;
  for (const auto &item : hero.getSaveEquipments()) {
    if (item.getType() == Armor) newMaxHp += item.getValue();
    if (item.getType() == Weapon) newAttack += item.getValue();
  }
  if (newMaxHp > kMaxStat || newAttack > kMaxStat) {
    cerr << "[迁移] 旧装备数值超出新规则可保存范围，已保留原状态。" << endl;
    return false;
  }
  const int newHp = hero.isalive()
      ? max(1, static_cast<int>(static_cast<long long>(hero.gethp()) * newMaxHp / max(1, hero.getmaxhp()))) : 0;
  const int newMp = static_cast<int>(static_cast<long long>(hero.getmp()) * newMaxMp / max(1, hero.getmaxmp()));
  int compensation = 5 + max(0, hero.getlevel() - newLevel);
  for (int slot = 1; slot <= hero.getSkillCount(); ++slot) {
    skill *ability = hero.getSkill(slot);
    const int rank = min(ability->getskilllevel(), rankCap);
    compensation += max(0, ability->getskilllevel() - rank);
    ability->restoreProgress(rank, ability->getcurrentcooldown());
  }
  hero.loadStats(newLevel, min(hero.getexp(), newLevel * 100 - 1), hero.getcoin(),
                  newHp, newMaxHp, newAttack, min(newMp, newMaxMp), newMaxMp,
                  base->getcritRate(), min(hero.getfreerevives(), 3));
  auto &run = hero.runState();
  const int awarded = min(30, min(compensation, GameConfig::MaxMaterials - run.materials));
  run.materials += max(0, awarded);
  run.legacyRules = false;
  cout << "[迁移] 已保留楼层、装备和章节进度；等级 " << newLevel
       << "，技能上限 " << rankCap << "，补偿材料 " << max(0, awarded) << "。" << endl;
  return true;
}

bool SaveSystem::hasSaveFile(const string &filename) {
  ifstream input(filename);
  return input.good();
}
void SaveSystem::deleteSave(const string &filename) {
  if (remove(filename.c_str()) == 0) cout << "[存档] 存档文件已删除。" << endl;
  else cerr << "[存档] 删除存档失败：文件不存在或无权限。" << endl;
}
