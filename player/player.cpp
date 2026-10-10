#include "player.h"
#include "skills.h"
#include "progression.h"
#include "game_config.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace std;

namespace {
constexpr int MaxCoins = 1000000000; // Matches save format validation, including legacy balances.
bool validItem(const Item &item) {
  return item.getType() >= Weapon && item.getType() <= Potion &&
      item.getJobRequirement() >= AnyJob && item.getJobRequirement() <= AssassinOnly &&
      item.getLevelRequirement() >= 1 && item.getValue() >= 0 &&
      item.getValue() <= 1000000 && item.getDurability() >= 0 &&
      item.getPrice() >= 0 && item.getQuantity() > 0 &&
      item.getTier() >= 0 && item.getTier() <= 3 && item.supportsEffect(item.getEffect());
}
bool permitted(const Item &item, const player &hero) {
  const auto job = item.getJobRequirement();
  return job == AnyJob || (job == WarriorOnly && hero.getjob() == "Warrior") ||
         (job == MageOnly && hero.getjob() == "Mage") ||
         (job == AssassinOnly && hero.getjob() == "Assassin");
}
bool samePotion(const Item &a, const Item &b) {
  return a.getType() == Potion && b.getType() == Potion &&
      a.getId() == b.getId() && a.getName() == b.getName() &&
      a.getValue() == b.getValue() && a.getPrice() == b.getPrice() &&
      a.getLevelRequirement() == b.getLevelRequirement() &&
      a.getDurability() == b.getDurability() &&
      a.getJobRequirement() == b.getJobRequirement() &&
      a.getTier() == b.getTier() && a.getEffect() == b.getEffect() &&
      a.isLocked() == b.isLocked();
}
const char *effectName(ItemEffect effect) {
  switch (effect) {
  case ItemEffect::ManaOnAttack: return "普攻命中回蓝+3";
  case ItemEffect::GuardRecovery: return "格挡回蓝+3";
  case ItemEffect::SkillEfficiency: return "固定技能耗蓝-10%";
  case ItemEffect::ControlFocus: return "减攻控制延长1回合";
  default: return "无";
  }
}
}

player::player(string name, int hp, int attack, int mp, int level, int exp,
               int coin, int critRate)
    : Character(name, hp, attack, mp), level(level), exp(exp), coin(coin),
      free_revives(3), critRate(critRate) {}
player::~player() = default;
int player::getlevel() const { return level; }
int player::getexp() const { return exp; }
int player::getcoin() const { return coin; }
int player::getfreerevives() const { return free_revives; }
int player::getcritRate() const { return critRate; }
int player::getHighestFloor() const { return highestFloor; }
void player::setHighestFloor(int floor) { highestFloor = max(highestFloor, clamp(floor, 1, 100)); }
void player::decreaserevives() { if (free_revives > 0) --free_revives; }
void player::resetFreeRevive() { free_revives = 3; }

void player::gainexp(int amount) {
  if (amount <= 0) return;
  exp = GameConfig::addCounter(exp, amount);
  // The campaign supports 100 character levels; repeated encounters cannot grow forever.
  while (level < 100 && exp >= level * 100) {
    exp -= level * 100;
    levelup();
  }
  if (level >= 100) exp = min(exp, 9999);
}
void player::gaincoin(int amount) {
  if (amount > 0)
    coin = static_cast<int>(min(static_cast<long long>(coin) + amount, static_cast<long long>(MaxCoins)));
}
void player::costcoin(int amount) {
  if (amount > 0 && coin >= amount) coin -= amount;
}
void player::levelup() {
  if (level >= 100) return;
  ++level;
  attack = GameConfig::addCounter(attack, 2);
  max_hp = GameConfig::addCounter(max_hp, 10);
  sethp(hp + max(1, getRecoveryBaseHp() * 12 / 100));
  setmp(mp + max(1, max_mp / 5));
}
void player::revive() {
  clearCombatModifiers();
  combatResource = 0;
  hp = max_hp;
  cout << getname() << "复苏，HP " << hp << "。" << endl;
}
int player::getRecoveryBaseHp() const {
  long long base = max_hp;
  for (const auto &item : equipments)
    if (item.getType() == Armor) base -= item.getValue();
  return static_cast<int>(clamp(base, 1LL, static_cast<long long>(GameConfig::MaxCounter)));
}
void player::heal(int amount) {
  if (amount <= 0 || !isalive()) return;
  const int available = max(0, max_hp - hp);
  const int unsuppressed = min(available, amount);
  const int effective = static_cast<int>(static_cast<long long>(amount) * healingPercent / 100);
  const int restored = min(available, effective);
  suppressedHealingTotal = GameConfig::addCounter(suppressedHealingTotal, unsuppressed - restored);
  hp += restored;
  cout << "HP 恢复 " << restored;
  if (healingPercent < 100) cout << "（抑疗后效果 " << healingPercent << "%）";
  cout << "。" << endl;
}
void player::setHealingPercent(int percent) { healingPercent = clamp(percent, 0, 100); }
void player::guard(bool pierced) {
  if (!isalive()) return;
  guarding = true;
  guardPierced = pierced;
  int recovered = (getjob() == "Warrior" ? 6 : 4) +
      3 * equipmentEffect(static_cast<int>(ItemEffect::GuardRecovery));
  setmp(mp + recovered);
  if (getjob() == "Warrior") addCombatResource(1);
  cout << "格挡本回合伤害，恢复 " << recovered << " MP。" << endl;
}
void player::clearCombatModifiers() {
  healingPercent = 100;
  guarding = false;
  guardPierced = false;
}
void player::addCombatResource(int amount) {
  combatResource = static_cast<int>(clamp(static_cast<long long>(combatResource) + amount, 0LL, 2LL));
}
int player::consumeCombatResource() { int amount = combatResource; combatResource = 0; return amount; }
void player::resetEncounterResources() {
  combatResource = 0;
  encounterWearPending = true;
  clearCombatModifiers();
}
void player::restoreTurnResources() { setmp(mp + (getjob() == "Mage" ? 3 : 2)); }

void player::attacktarget(Character &target) { attacktargetDealt(target, attack); }
void player::attacktargetDealt(Character &target, int baseDamage) {
  if (!isalive() || !target.isalive() || baseDamage < 0) return;
  long long damage = baseDamage;
  if (rand() % 100 < clamp(critRate, 0, 100)) damage *= 2;
  const int before = target.gethp();
  target.takedamage(static_cast<int>(min(damage, static_cast<long long>(GameConfig::MaxCounter))));
  if (target.gethp() < before) {
    const int mana = (getjob() == "Mage" ? 8 : (getjob() == "Assassin" ? 5 : 3)) +
        3 * equipmentEffect(static_cast<int>(ItemEffect::ManaOnAttack));
    setmp(mp + mana);
    addCombatResource(1);
    cout << "普攻命中：恢复 " << mana << " MP，职业资源 " << combatResource << "/2。" << endl;
  }
}
void player::takedamage(int damage) {
  damage = max(0, damage);
  if (guarding && !guardPierced) damage = damage / 2 + damage % 2;
  hp = max(0, hp - damage);
  cout << getname() << "受到 " << damage << " 伤害（HP " << hp << "/" << max_hp << "）。" << endl;
}
void player::finishEncounterWear() {
  if (!encounterWearPending) return;
  encounterWearPending = false;
  for (int i = static_cast<int>(equipments.size()) - 1; i >= 0; --i) {
    Item &item = equipments[i];
    item.setDurability(max(0, item.getDurability() - 1));
    if (item.getDurability() > 0) continue;
    cout << "【装备破损】" << item.getName() << endl;
    if (item.getType() == Weapon) attack = max(0, attack - item.getValue());
    if (item.getType() == Armor) {
      max_hp = max(1, max_hp - item.getValue());
      hp = min(hp, max_hp);
    }
    equipments.erase(equipments.begin() + i);
  }
  combatResource = 0;
  clearCombatModifiers();
}
int player::equipmentEffect(int effectId) const {
  if (effectId < 1 || effectId > 4) return 0;
  int count = 0;
  for (const Item &item : equipments)
    if (item.getDurability() > 0 && static_cast<int>(item.getEffect()) == effectId) ++count;
  return min(2, count);
}

void player::addItem(const Item &item) {
  if (!validItem(item)) return;
  for (Item &existing : backpack) {
    if (samePotion(existing, item)) {
      const long long combined = static_cast<long long>(existing.getQuantity()) + item.getQuantity();
      if (combined <= numeric_limits<int>::max()) {
        existing.setQuantity(static_cast<int>(combined));
        return;
      }
    }
  }
  // Equipment remains separate, so effects and locks never merge implicitly.
  backpack.push_back(item);
}
void player::printstats() const {
  cout << "=== " << getname() << " | " << getjob() << " ===\n"
       << "等级 " << level << " | HP " << hp << "/" << max_hp
       << " | MP " << mp << "/" << max_mp << " | 攻击 " << attack
       << " | 暴击 " << critRate << "%\n"
       << "EXP " << exp << " | 金币 " << coin << " | 改造材料 " << run.materials
       << " | 免费复苏 " << free_revives << "\n最高层 " << highestFloor
       << " | 技能阶数上限 " << Progression::skillRankCap(highestFloor)
       << " | " << (getjob() == "Warrior" ? "战意" : (getjob() == "Mage" ? "专注" : "破绽"))
       << " " << combatResource << "/2" << endl;
}
void player::printBackpack() const {
  if (backpack.empty()) { cout << "背包空空如也！" << endl; return; }
  for (int i = 0; i < static_cast<int>(backpack.size()); ++i) {
    const Item &item = backpack[i];
    cout << i + 1 << ". " << (item.isLocked() ? "[锁定] " : "") << item.getName()
         << " ×" << item.getQuantity() << " | 数值 " << item.getValue()
         << " | 单价 " << item.getPrice() << " | 等级 " << item.getLevelRequirement()
         << " | " << item.getJobRequirementName();
    if (item.getType() != Potion) {
      int equipped = 0;
      for (const Item &current : equipments)
        if (current.getType() == item.getType()) equipped = current.getValue();
      const int difference = item.getValue() - equipped;
      cout << " | " << (item.getType() == Weapon ? "攻击" : "生命") << "变化 "
           << (difference >= 0 ? "+" : "") << difference
           << " | 耐久 " << item.getDurability() << " | 效果 " << effectName(item.getEffect());
      if (!permitted(item, *this) || item.getLevelRequirement() > level || item.getDurability() <= 0)
        cout << " [当前不可装备]";
    }
    cout << endl;
  }
}
bool player::useItem(int index) {
  if (!isalive() || index < 1 || index > static_cast<int>(backpack.size())) return false;
  Item &item = backpack[index - 1];
  if (item.getType() != Potion || hp >= max_hp || item.getValue() <= 0 ||
      static_cast<long long>(item.getValue()) * healingPercent / 100 <= 0) return false;
  heal(item.getValue());
  run.potionsUsed = GameConfig::addCounter(run.potionsUsed, 1);
  item.setQuantity(item.getQuantity() - 1);
  if (item.getQuantity() <= 0) backpack.erase(backpack.begin() + index - 1);
  return true;
}
void player::equipItem(int index) {
  if (index < 1 || index > static_cast<int>(backpack.size())) return;
  Item selected = backpack[index - 1];
  if (selected.getType() == Potion || selected.getLevelRequirement() > level ||
      selected.getDurability() <= 0 || !permitted(selected, *this)) {
    cout << "该物品当前不可装备。" << endl;
    return;
  }
  // Remove the selected unit before returning old equipment to the bag.
  if (selected.getQuantity() == 1) backpack.erase(backpack.begin() + index - 1);
  else backpack[index - 1].setQuantity(selected.getQuantity() - 1);
  selected.setQuantity(1);
  equipItem_T(selected);
}
void player::unequipItem(int index) {
  if (index < 1 || index > static_cast<int>(equipments.size())) return;
  Item removed = equipments[index - 1];
  equipments.erase(equipments.begin() + index - 1);
  if (removed.getType() == Weapon) attack = max(0, attack - removed.getValue());
  if (removed.getType() == Armor) { max_hp = max(1, max_hp - removed.getValue()); hp = min(hp, max_hp); }
  addItem(removed);
}
void player::equipItem_T(const Item &item, bool restoring) {
  Item selected = item; // The caller may pass a reference into a vector below.
  if (!validItem(selected) || selected.getType() == Potion || selected.getDurability() <= 0 ||
      !permitted(selected, *this) || (!restoring && selected.getLevelRequirement() > level)) {
    if (!restoring) addItem(selected);
    return;
  }
  long long resultingStat = selected.getType() == Weapon ? attack : max_hp;
  for (const Item &current : equipments)
    if (current.getType() == selected.getType()) resultingStat -= current.getValue();
  resultingStat += selected.getValue();
  if (resultingStat > GameConfig::MaxCounter) {
    if (!restoring) addItem(selected);
    return;
  }
  const int hpBeforeSwap = hp;
  for (int i = static_cast<int>(equipments.size()) - 1; i >= 0; --i)
    if (equipments[i].getType() == selected.getType()) unequipItem(i + 1);
  selected.setQuantity(1);
  if (selected.getType() == Weapon) attack = GameConfig::addCounter(attack, selected.getValue());
  else max_hp = GameConfig::addCounter(max_hp, selected.getValue());
  hp = min(hpBeforeSwap, max_hp);
  equipments.push_back(selected);
}
void player::showEquippedItems() const {
  if (equipments.empty()) { cout << "没有装备任何物品！" << endl; return; }
  for (int i = 0; i < static_cast<int>(equipments.size()); ++i) {
    const auto &item = equipments[i];
    cout << i + 1 << ". " << item.getName() << " | "
         << (item.getType() == Weapon ? "攻击+" : "生命+") << item.getValue()
         << " | 耐久 " << item.getDurability() << " | " << effectName(item.getEffect()) << endl;
  }
}
void player::dropItem(int index) {
  if (index < 1 || index > static_cast<int>(backpack.size()) || backpack[index - 1].isLocked()) return;
  const Item &item = backpack[index - 1];
  const long long proceeds = static_cast<long long>(item.getPrice() / 3) * item.getQuantity();
  gaincoin(static_cast<int>(min(proceeds, static_cast<long long>(MaxCoins))));
  backpack.erase(backpack.begin() + index - 1);
}
bool player::toggleItemLock(int index) {
  if (index < 1 || index > static_cast<int>(backpack.size())) return false;
  Item &item = backpack[index - 1];
  item.setLocked(!item.isLocked());
  return true;
}
int player::salvageUnlocked() {
  int gained = 0;
  for (int i = static_cast<int>(backpack.size()) - 1; i >= 0; --i) {
    const Item &item = backpack[i];
    if (item.isLocked() || item.getType() == Potion) continue;
    const long long quantity = static_cast<long long>(item.getQuantity()) * (item.getTier() + 1);
    // Do not delete gear once material capacity is exhausted.
    const int space = GameConfig::MaxMaterials - run.materials;
    if (quantity > space) continue;
    const int value = static_cast<int>(quantity);
    run.materials += value;
    gained += value;
    backpack.erase(backpack.begin() + i);
  }
  return gained;
}
bool player::modifyEquipment(int index, int effectId) {
  if (index < 1 || index > static_cast<int>(equipments.size()) || effectId < 1 || effectId > 4) return false;
  Item &item = equipments[index - 1];
  if (static_cast<int>(item.getEffect()) == effectId || item.isLocked()) return false;
  const int cost = 3 + item.getTier() * 2;
  if (run.materials < cost || !item.setEffect(static_cast<ItemEffect>(effectId))) return false;
  run.materials -= cost;
  return true;
}

void player::showSkills(player &hero) const {
  for (int i = 0; i < static_cast<int>(skills.size()); ++i) {
    const skill &ability = *skills[i];
    cout << i + 1 << ". " << ability.getskillname() << " Lv." << ability.getskilllevel()
         << " [" << ability.getSpecializationName(hero) << "]\n   "
         << ability.getPreview(hero) << "\n   MP " << ability.getActualCost(hero)
         << " | 冷却 " << ability.getcurrentcooldown() << "/" << ability.getcooldown()
         << " | 等级需求 " << ability.getlevelrequired() << " | 楼层需求 " << ability.getunlockfloor()
         << " | 升级费用 " << ability.getlevelUpCost() << " | " << (ability.canUse(hero) ? "可用" : "不可用") << endl;
    if (i < 2) {
      const char *choices[3][2] = {
        {"1反击：伤害80%+每层战意35%；2重斩：伤害140%，多耗3MP，不消耗战意",
         "1坚守：伤害65%，恢复基础HP25%，多耗4MP；2振奋：伤害130%，不回血，命中回6MP"},
        {"1蓄焰：伤害120%+每层专注30%，多耗3MP；2火花：伤害95%，少耗4MP，积累专注",
         "1深冻：伤害70%，减攻40%持续3行动；2碎冰：伤害155%，无减攻，命中回6MP"},
        {"1开隙：伤害80%，命中增加2层破绽；2穿甲：伤害125%，多耗3MP，穿盾且不积破绽",
         "1伏击：伤害120%+每层破绽40%；2扰乱：伤害120%，命中回8MP，减攻15%一次行动"}
      };
      const int job = getjob() == "Warrior" ? 0 : (getjob() == "Mage" ? 1 : 2);
      cout << "   专精第" << (i == 0 ? 10 : 30) << "层开放：0基础；" << choices[job][i]
           << "。初选免费，重选费用 " << 100 + highestFloor * 5 << " 金币。" << endl;
    }
  }
}
bool player::useSkill(int index, enemy &target) {
  skill *ability = getSkill(index);
  return ability && ability->useSkill(*this, target);
}
void player::learnSkill(unique_ptr<skill> newSkill) { if (newSkill) skills.push_back(std::move(newSkill)); }
skill *player::getSkill(int index) const {
  return index >= 1 && index <= static_cast<int>(skills.size()) ? skills[index - 1].get() : nullptr;
}
int player::getSkillCount() const { return static_cast<int>(skills.size()); }
void player::reduceallCooldowns() { for (auto &ability : skills) ability->reducecooldown(); }
void player::levelUpSkill(int index) {
  skill *ability = getSkill(index);
  if (!ability) return;
  const int cost = ability->getlevelUpCost();
  if (ability->levelUpSkill(level, coin, highestFloor)) costcoin(cost);
}
bool player::chooseSpecialization(int slot, int branch) {
  skill *ability = getSkill(slot);
  if (!ability || slot > 2 || branch < 0 || branch > 2 ||
      highestFloor < (slot == 1 ? 10 : 30) || level < ability->getlevelrequired() ||
      ability->getSpecialization() == branch || encounterWearPending) return false;
  // Selecting or resetting any branch has no immediate resource or cooldown reward.
  // First choice is free; returning to branch zero costs the same as a respec.
  const int cost = ability->getSpecialization() == 0 ? 0 : 100 + highestFloor * 5;
  if (coin < cost || !ability->setSpecialization(branch)) return false;
  costcoin(cost);
  return true;
}
void player::loadStats(int newLevel, int newExp, int newCoin, int newHp, int newMaxHp,
                       int newAttack, int newMp, int newMaxMp, int newCritRate,
                       int newFreeRevives) {
  level = clamp(newLevel, 1, 10000); // Legacy saves retain up to 10000; new progression stops at 100.
  exp = max(0, newExp);
  coin = clamp(newCoin, 0, MaxCoins);
  max_hp = clamp(newMaxHp, 1, GameConfig::MaxCounter);
  hp = clamp(newHp, 0, max_hp);
  attack = clamp(newAttack, 0, GameConfig::MaxCounter);
  max_mp = clamp(newMaxMp, 0, GameConfig::MaxCounter);
  mp = clamp(newMp, 0, max_mp);
  critRate = clamp(newCritRate, 0, 100);
  free_revives = clamp(newFreeRevives, 0, 1000); // Preserve supported legacy saves; migration resets the allowance.
  clearCombatModifiers();
  combatResource = 0;
  encounterWearPending = false;
}
