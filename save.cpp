#include "save.h"
#include "jobs.h"
#include "skills.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
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

vector<string> split(const string &s, char delim) {
  vector<string> tokens;
  stringstream ss(s);
  string tok;
  while (getline(ss, tok, delim))
    tokens.push_back(tok);
  if (!s.empty() && s.back() == delim)
    tokens.emplace_back();
  return tokens;
}

int parseNumber(const string &text, int minimum, int maximum,
                const string &field) {
  istringstream input(text);
  long long value;
  if (!(input >> value) || value < minimum || value > maximum)
    throw runtime_error("无效数值: " + field);
  input >> ws;
  if (!input.eof())
    throw runtime_error("数值包含多余字符: " + field);
  return static_cast<int>(value);
}

bool canEquip(const Item &item, const string &job) {
  const JobReq requirement = item.getJobRequirement();
  return requirement == AnyJob ||
         (requirement == WarriorOnly && job == "Warrior") ||
         (requirement == MageOnly && job == "Mage") ||
         (requirement == AssassinOnly && job == "Assassin");
}
} // namespace

string SaveSystem::serializeItem(const Item &item) {
  ostringstream oss;
  oss << item.getName() << "|" << item.getLevelRequirement() << "|"
      << static_cast<int>(item.getType()) << "|" << item.getValue() << "|"
      << item.getDurability() << "|" << item.getPrice() << "|"
      << item.getQuantity() << "|"
      << static_cast<int>(item.getJobRequirement());
  return oss.str();
}

Item SaveSystem::deserializeItem(const string &line) {
  const auto tok = split(line, '|');
  if ((tok.size() != 7 && tok.size() != 8) || tok[0].empty())
    throw runtime_error("存档物品格式错误");
  const int levelReq = parseNumber(tok[1], 1, 10000, "物品等级");
  const auto type = static_cast<ItemType>(parseNumber(tok[2], Weapon, Potion,
                                                     "物品类型"));
  const int value = parseNumber(tok[3], 0, kMaxStat, "物品数值");
  const int durability = parseNumber(tok[4], 0, kMaxStat, "耐久");
  const int price = parseNumber(tok[5], 0, 1000000000, "价格");
  const int quantity = parseNumber(tok[6], 1, 1000000000, "数量");
  // Older saves omit the class restriction column.
  const auto jobReq = tok.size() == 8
      ? static_cast<JobReq>(parseNumber(tok[7], AnyJob, AssassinOnly, "职业限制"))
      : AnyJob;
  return Item(tok[0], levelReq, type, value, durability, price, quantity, jobReq);
}

bool SaveSystem::saveGame(const player &hero, int floor,
                          const string &filename) {
  if (floor < 1 || floor > 101) {
    cerr << "[存档] 楼层无效。" << endl;
    return false;
  }
  ofstream ofs(filename);
  if (!ofs.is_open()) {
    cerr << "[存档] 无法打开文件: " << filename << endl;
    return false;
  }
  ofs << "VERSION 2\n";
  ofs << "NAME " << hero.getname() << "\n";
  ofs << "JOB " << hero.getjob() << "\n";
  ofs << "LEVEL " << hero.getlevel() << "\n";
  ofs << "EXP " << hero.getexp() << "\n";
  ofs << "COIN " << hero.getcoin() << "\n";
  ofs << "HP " << hero.gethp() << "\n";
  ofs << "MAX_HP " << hero.getmaxhp() << "\n";
  ofs << "ATTACK " << hero.getattack() << "\n";
  ofs << "MP " << hero.getmp() << "\n";
  ofs << "MAX_MP " << hero.getmaxmp() << "\n";
  ofs << "CRIT_RATE " << hero.getcritRate() << "\n";
  ofs << "FREE_REVIVES " << hero.getfreerevives() << "\n";
  ofs << "FLOOR " << floor << "\n";
  ofs << "HIGHEST_FLOOR " << min(100, max(floor, hero.getHighestFloor())) << "\n";
  const auto &bp = hero.getSaveBackpack();
  ofs << "BACKPACK_COUNT " << bp.size() << "\n";
  for (const auto &item : bp)
    ofs << "ITEM " << serializeItem(item) << "\n";
  const auto &eq = hero.getSaveEquipments();
  ofs << "EQUIP_COUNT " << eq.size() << "\n";
  for (const auto &item : eq)
    ofs << "EQUIP " << serializeItem(item) << "\n";
  ofs << "SKILL_COUNT " << hero.getSkillCount() << "\n";
  for (int i = 1; i <= hero.getSkillCount(); ++i) {
    const skill *s = hero.getSkill(i);
    if (s)
      ofs << "SKILL " << s->getskillid() << "|" << s->getskilllevel() << "|"
          << s->getcurrentcooldown() << "\n";
  }
  ofs.flush();
  if (!ofs) {
    cerr << "[存档] 写入失败: " << filename << endl;
    return false;
  }
  cout << "[存档] 游戏已保存到 " << filename << endl;
  return true;
}

unique_ptr<player> SaveSystem::loadGame(int &floor, const string &filename) {
  ifstream ifs(filename);
  if (!ifs.is_open()) {
    cerr << "[读档] 无法打开文件: " << filename << endl;
    return nullptr;
  }
  // Parse and validate the complete file before changing caller state.
  try {
    string name, job;
    int version = 0, level = 1, exp = 0, coin = 0;
    int hp = 0, maxHp = 0, attack = 0, mp = 0, maxMp = 0;
    int critRate = 0, freeRevives = 0, savedFloor = 1, highestFloor = 0;
    int backpackCount = 0, equipCount = 0, skillCount = 0;
    vector<Item> backpackItems, equippedItems;
    vector<tuple<int, int, int>> skillData;
    set<string> seen;
    set<int> skillIds;
    long long totalQuantity = 0;
    string line;
    while (getline(ifs, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.size() > 4096)
        throw runtime_error("存档行过长");
      istringstream iss(line);
      string key, rest;
      if (!(iss >> key) || key[0] == '#')
        continue;
      getline(iss >> ws, rest);
      if (key != "ITEM" && key != "EQUIP" && key != "SKILL" &&
          !seen.insert(key).second)
        throw runtime_error("重复字段: " + key);
      if (key == "VERSION")
        version = parseNumber(rest, 1, 2, key);
      else if (key == "NAME") name = rest;
      else if (key == "JOB") job = rest;
      else if (key == "LEVEL") level = parseNumber(rest, 1, 10000, key);
      else if (key == "EXP") exp = parseNumber(rest, 0, 1000000000, key);
      else if (key == "COIN") coin = parseNumber(rest, 0, 1000000000, key);
      else if (key == "HP") hp = parseNumber(rest, 0, kMaxStat, key);
      else if (key == "MAX_HP") maxHp = parseNumber(rest, 1, kMaxStat, key);
      else if (key == "ATTACK") attack = parseNumber(rest, 1, kMaxStat, key);
      else if (key == "MP") mp = parseNumber(rest, 0, kMaxStat, key);
      else if (key == "MAX_MP") maxMp = parseNumber(rest, 0, kMaxStat, key);
      else if (key == "CRIT_RATE") critRate = parseNumber(rest, 0, 100, key);
      else if (key == "FREE_REVIVES") freeRevives = parseNumber(rest, 0, 1000, key);
      else if (key == "FLOOR") savedFloor = parseNumber(rest, 1, 101, key);
      else if (key == "HIGHEST_FLOOR") highestFloor = parseNumber(rest, 1, 100, key);
      else if (key == "BACKPACK_COUNT") backpackCount = parseNumber(rest, 0, kMaxItems, key);
      else if (key == "EQUIP_COUNT") equipCount = parseNumber(rest, 0, 2, key);
      else if (key == "SKILL_COUNT") skillCount = parseNumber(rest, 0, 15, key);
      else if (key == "ITEM" || key == "EQUIP") {
        auto &items = key == "ITEM" ? backpackItems : equippedItems;
        if (items.size() >= static_cast<size_t>(key == "ITEM" ? kMaxItems : 2))
          throw runtime_error("物品行数过多");
        items.push_back(deserializeItem(rest));
        totalQuantity += items.back().getQuantity();
        if (totalQuantity > 1000000000)
          throw runtime_error("物品总数量过多");
      } else if (key == "SKILL") {
        const auto tok = split(rest, '|');
        if (tok.size() != 3 || skillData.size() >= 15)
          throw runtime_error("技能格式错误");
        const int id = parseNumber(tok[0], 1, 1000, "技能ID");
        const int rank = parseNumber(tok[1], 1, kMaxLegacySkillRank, "技能等级");
        const int cooldown = parseNumber(tok[2], 0, 1000, "技能冷却");
        if (!skillIds.insert(id).second)
          throw runtime_error("重复技能ID");
        skillData.emplace_back(id, rank, cooldown);
      } else {
        throw runtime_error("未知字段: " + key);
      }
    }
    if (ifs.bad()) throw runtime_error("读取文件失败");
    for (const char *key : {"VERSION", "NAME", "JOB", "LEVEL", "EXP", "COIN",
                            "HP", "MAX_HP", "ATTACK", "MP", "MAX_MP",
                            "CRIT_RATE", "FREE_REVIVES", "FLOOR",
                            "BACKPACK_COUNT", "EQUIP_COUNT", "SKILL_COUNT"}) {
      if (!seen.count(key)) throw runtime_error(string("缺少字段: ") + key);
    }
    if (name.empty() || name.size() > 256 || hp > maxHp || mp > maxMp)
      throw runtime_error("角色属性无效");
    if (version == 2 && !seen.count("HIGHEST_FLOOR"))
      throw runtime_error("缺少最高楼层");
    if (!seen.count("HIGHEST_FLOOR"))
      highestFloor = min(savedFloor, 100); // v1, including a cleared tower.
    if (highestFloor < min(savedFloor, 100))
      throw runtime_error("最高楼层小于当前楼层");
    if (backpackCount != static_cast<int>(backpackItems.size()) ||
        equipCount != static_cast<int>(equippedItems.size()) ||
        skillCount != static_cast<int>(skillData.size()))
      throw runtime_error("物品或技能数量不匹配");
    unique_ptr<player> hero;
    if (job == "Warrior") hero = make_unique<Warrior>(name);
    else if (job == "Mage") hero = make_unique<Mage>(name);
    else if (job == "Assassin") hero = make_unique<Assassin>(name);
    else throw runtime_error("未知职业: " + job);

    int attackBonus = 0, hpBonus = 0;
    set<ItemType> equipmentSlots;
    for (const auto &item : equippedItems) {
      if (item.getType() == Potion || item.getQuantity() != 1 ||
          !equipmentSlots.insert(item.getType()).second || !canEquip(item, job))
        throw runtime_error("装备槽位、数量或职业限制无效");
      if (item.getType() == Weapon) attackBonus += item.getValue();
      else hpBonus += item.getValue();
    }
    if (attack <= attackBonus || maxHp <= hpBonus)
      throw runtime_error("装备加成超出角色属性");
    if (skillCount != hero->getSkillCount())
      throw runtime_error("职业技能数量不匹配");
    // Match stable skill IDs, never the order of SKILL lines in the save.
    for (const auto &[id, rank, cooldown] : skillData) {
      skill *matched = nullptr;
      for (int slot = 1; slot <= hero->getSkillCount(); ++slot) {
        skill *candidate = hero->getSkill(slot);
        if (candidate && candidate->getskillid() == id) {
          matched = candidate;
          break;
        }
      }
      if (!matched || cooldown > matched->getcooldown())
        throw runtime_error("未知职业技能或冷却超出上限");
      matched->restoreProgress(rank, cooldown);
    }
    // Stored totals include gear; equipItem_T applies its bonuses again.
    hero->loadStats(level, exp, coin, min(hp, maxHp - hpBonus), maxHp - hpBonus,
                    attack - attackBonus, mp, maxMp, critRate, freeRevives);
    hero->setHighestFloor(highestFloor);
    for (const auto &item : backpackItems) hero->addItem(item);
    for (const auto &item : equippedItems) hero->equipItem_T(item, true);
    hero->sethp(hp); // Restore saved HP after armor has restored maximum HP.
    floor = savedFloor;
    cout << "[读档] 欢迎回来，" << name << "！当前楼层：" << floor << endl;
    return hero;
  } catch (const exception &error) {
    cerr << "[读档] 存档文件损坏或格式错误: " << error.what() << endl;
    return nullptr;
  }
}

bool SaveSystem::hasSaveFile(const string &filename) {
  ifstream file(filename);
  return file.good();
}

void SaveSystem::deleteSave(const string &filename) {
  if (remove(filename.c_str()) == 0)
    cout << "[存档] 存档文件已删除。" << endl;
  else
    cerr << "[存档] 删除存档失败：文件不存在或无权限。" << endl;
}
