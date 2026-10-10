#include "player/player.h"
#include "core/game_config.h"
#include "core/progression.h"
#include "core/random.h"
#include "player/skills.h"
#include <algorithm>
#include <iostream>
#include <limits>

using namespace std;

namespace {
bool validItem(const Item &item) {
  return item.getType() >= Weapon && item.getType() <= Potion &&
      item.getJobRequirement() >= AnyJob && item.getJobRequirement() <= AssassinOnly &&
      item.getLevelRequirement() >= 1 && item.getValue() >= 0 &&
      item.getValue() <= 1000000 && item.getDurability() >= 0 &&
      item.getMaxDurability() >= item.getDurability() &&
      item.getPrice() >= 0 && item.getQuantity() > 0 &&
      item.getTier() >= 0 && item.getTier() <= 3 && item.supportsEffect(item.getEffect()) &&
      item.getEnhanceLevel() >= 0 && item.getEnhanceLevel() <= GameConfig::MaxEnhanceLevel;
}
bool samePotion(const Item &a, const Item &b) {
  return a.getType() == Potion && b.getType() == Potion &&
      a.getId() == b.getId() && a.getName() == b.getName() &&
      a.getValue() == b.getValue() && a.getPrice() == b.getPrice() &&
      a.getLevelRequirement() == b.getLevelRequirement() &&
      a.getDurability() == b.getDurability() &&
      a.getJobRequirement() == b.getJobRequirement() &&
      a.getTier() == b.getTier() && a.getEffect() == b.getEffect() &&
      a.isLocked() == b.isLocked() && a.getEnhanceLevel() == b.getEnhanceLevel() &&
      a.getMaxDurability() == b.getMaxDurability();
}
constexpr JobProfile profiles[] = {
  {"Warrior", "战士", "战意", 200, 18, 24, 8, 20, 3, 3},
  {"Mage", "法师", "专注", 150, 12, 40, 12, 17, 2, 8},
  {"Assassin", "刺客", "破绽", 150, 15, 25, 20, 15, 3, 4},
};
}

const JobProfile &jobProfile(Job job) { return profiles[static_cast<int>(job)]; }
JobReq equipmentClass(Job job) {
  switch (job) {
  case Job::Warrior: return WarriorOnly;
  case Job::Mage: return MageOnly;
  case Job::Assassin: return AssassinOnly;
  }
  return AnyJob;
}

player::player(string name, Job job)
    : Character(name, jobProfile(job).hp, jobProfile(job).attack, jobProfile(job).mp),
      jobType(job), level(1), exp(0), coin(0), free_revives(3),
      critRate(jobProfile(job).critRate) {}
player::~player() = default;
int player::getlevel() const { return level; }
int player::getexp() const { return exp; }
int player::getcoin() const { return coin; }
int player::getfreerevives() const { return free_revives; }
int player::getcritRate() const { return critRate; }
int player::getHighestFloor() const { return highestFloor; }
void player::setHighestFloor(int floor) { highestFloor = max(highestFloor, GameConfig::floor(floor)); }
void player::decreaserevives() { if (free_revives > 0) --free_revives; }
void player::resetFreeRevive() { free_revives = 3; }
bool player::canWear(const Item &item) const {
  return item.getJobRequirement() == AnyJob || item.getJobRequirement() == equipmentClass(jobType);
}

void player::gainexp(int amount) {
  if (amount <= 0) return;
  exp = GameConfig::addCounter(exp, amount);
  // The campaign supports 100 character levels; repeated encounters cannot grow forever.
  while (level < GameConfig::MaxLevel && exp >= level * 100) {
    exp -= level * 100;
    levelup();
  }
  if (level >= GameConfig::MaxLevel) exp = min(exp, 9999);
}
void player::gaincoin(int amount) {
  if (amount > 0)
    coin = static_cast<int>(min(static_cast<long long>(coin) + amount,
                                static_cast<long long>(GameConfig::MaxCoins)));
}
void player::costcoin(int amount) {
  if (amount > 0 && coin >= amount) coin -= amount;
}
void player::levelup() {
  if (level >= GameConfig::MaxLevel) return;
  const JobProfile &growth = jobProfile(jobType);
  ++level;
  attack = GameConfig::addCounter(attack, growth.attackGrowth);
  max_hp = GameConfig::addCounter(max_hp, growth.hpGrowth);
  max_mp = GameConfig::addCounter(max_mp, growth.mpGrowth);
  sethp(hp + max(1, getRecoveryBaseHp() * 12 / 100));
  setmp(mp + max(1, max_mp / 5));
  cout << getname() << " 升到 " << level << " 级，恢复基础生命12%及最大魔力20%。" << endl;
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
  if (available == 0) return;
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
  const int recovered = (jobType == Job::Warrior ? 6 : 4) +
      3 * equipmentEffect(static_cast<int>(ItemEffect::GuardRecovery));
  setmp(mp + recovered);
  if (jobType == Job::Warrior) addCombatResource(1);
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
void player::restoreTurnResources() { setmp(mp + (jobType == Job::Mage ? 3 : 2)); }

void player::attacktarget(Character &target) { attacktargetDealt(target, attack); }
void player::attacktargetDealt(Character &target, int baseDamage) {
  if (!isalive() || !target.isalive() || baseDamage < 0) return;
  long long damage = baseDamage;
  if (Random::below(100) < clamp(critRate, 0, 100)) damage *= 2;
  const int before = target.gethp();
  target.takedamage(static_cast<int>(min(damage, static_cast<long long>(GameConfig::MaxCounter))));
  if (target.gethp() < before) {
    const int mana = (jobType == Job::Mage ? 8 : (jobType == Job::Assassin ? 5 : 3)) +
        3 * equipmentEffect(static_cast<int>(ItemEffect::ManaOnAttack));
    setmp(mp + mana);
    addCombatResource(1);
    cout << "普攻命中：恢复 " << mana << " MP，" << jobProfile(jobType).resourceName << " "
         << combatResource << "/2。" << endl;
  }
}
void player::takedamage(int damage) {
  damage = max(0, damage);
  if (guarding && !guardPierced) damage = damage / 2 + damage % 2;
  takeUnavoidableDamage(damage);
}
void player::takeUnavoidableDamage(int damage) {
  damage = max(0, damage);
  hp = max(0, hp - damage);
  cout << getname() << " 受到 " << damage << " 伤害（HP " << hp << "/" << max_hp << "）。" << endl;
}
void player::finishEncounterWear() {
  if (!encounterWearPending) return;
  encounterWearPending = false;
  for (int i = static_cast<int>(equipments.size()) - 1; i >= 0; --i) {
    Item &item = equipments[i];
    item.setDurability(max(0, item.getDurability() - 1));
    if (item.getDurability() > 0) continue;
    cout << "【装备破损】" << item.getDisplayName() << endl;
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
  cout << "=== " << getname() << " | " << getJobName() << " ===\n"
       << "等级 " << level << " | HP " << hp << "/" << max_hp
       << " | MP " << mp << "/" << max_mp << " | 攻击 " << attack
       << " | 暴击 " << critRate << "%\n"
       << "EXP " << exp << " | 金币 " << coin << " | 改造材料 " << run.materials
       << " | 免费复苏 " << free_revives << "\n最高层 " << highestFloor
       << " | 技能阶数上限 " << Progression::skillRankCap(highestFloor)
       << " | " << jobProfile(jobType).resourceName << " " << combatResource << "/2" << endl;
}
void player::printBackpack() const {
  if (backpack.empty()) { cout << "背包空空如也！" << endl; return; }
  for (int i = 0; i < static_cast<int>(backpack.size()); ++i) {
    const Item &item = backpack[i];
    cout << i + 1 << ". " << (item.isLocked() ? "[锁定] " : "") << item.getDisplayName()
         << " ×" << item.getQuantity() << " | 数值 " << item.getValue()
         << " | 单价 " << item.getPrice() << " | 等级 " << item.getLevelRequirement()
         << " | " << item.getJobRequirementName();
    if (item.getType() != Potion) {
      int equipped = 0;
      for (const Item &current : equipments)
        if (current.getType() == item.getType()) equipped = current.getValue();
      const int difference = item.getValue() - equipped;
      cout << " | " << item.getTierName() << " | " << (item.getType() == Weapon ? "攻击" : "生命") << "变化 "
           << (difference >= 0 ? "+" : "") << difference
           << " | 耐久 " << item.getDurability() << "/" << item.getMaxDurability()
           << " | 效果 " << item.getEffectDescription();
      if (!canWear(item) || item.getLevelRequirement() > level || item.getDurability() <= 0)
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
bool player::equipItem(int index) {
  if (index < 1 || index > static_cast<int>(backpack.size())) return false;
  Item selected = backpack[index - 1];
  if (selected.getType() == Potion || selected.getLevelRequirement() > level ||
      selected.getDurability() <= 0 || !canWear(selected)) {
    cout << "该物品当前不可装备。" << endl;
    return false;
  }
  // Remove the selected unit before returning old equipment to the bag.
  if (selected.getQuantity() == 1) backpack.erase(backpack.begin() + index - 1);
  else backpack[index - 1].setQuantity(selected.getQuantity() - 1);
  selected.setQuantity(1);
  equipItem_T(selected);
  return true;
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
      !canWear(selected) || (!restoring && selected.getLevelRequirement() > level)) {
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
    cout << i + 1 << ". " << item.getDisplayName() << " | " << item.getTierName() << " | "
         << (item.getType() == Weapon ? "攻击+" : "生命+") << item.getValue()
         << " | 耐久 " << item.getDurability() << "/" << item.getMaxDurability()
         << " | " << item.getEffectDescription() << endl;
  }
}
int player::sellItem(int index, int quantity) {
  if (index < 1 || index > static_cast<int>(backpack.size())) return 0;
  Item &item = backpack[index - 1];
  if (item.isLocked() || quantity < 1 || quantity > item.getQuantity()) return 0;
  const long long proceeds = static_cast<long long>(item.getPrice() / 3) * quantity;
  const int earned = static_cast<int>(min(proceeds, static_cast<long long>(GameConfig::MaxCoins)));
  gaincoin(earned);
  if (quantity == item.getQuantity()) backpack.erase(backpack.begin() + index - 1);
  else item.setQuantity(item.getQuantity() - quantity);
  return earned;
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
  // Locks protect against selling and salvage; they never block upgrading worn gear.
  if (static_cast<int>(item.getEffect()) == effectId) return false;
  const int cost = 3 + item.getTier() * 2;
  if (run.materials < cost || !item.setEffect(static_cast<ItemEffect>(effectId))) return false;
  run.materials -= cost;
  return true;
}

int player::enhanceCost(int index) const {
  if (index < 1 || index > static_cast<int>(equipments.size())) return 0;
  const Item &item = equipments[index - 1];
  if (item.getEnhanceLevel() >= GameConfig::MaxEnhanceLevel) return 0;
  return GameConfig::enhanceCost(item.getPrice(), item.getEnhanceLevel() + 1);
}
bool player::enhanceEquipment(int index) {
  const int cost = enhanceCost(index);
  if (cost <= 0 || coin < cost) return false;
  Item &item = equipments[index - 1];
  const int gain = max(1, item.getValue() * GameConfig::EnhancePercent / 100);
  if (item.getValue() + gain > 1000000) return false;
  costcoin(cost);
  item.setValue(item.getValue() + gain);
  item.setEnhanceLevel(item.getEnhanceLevel() + 1);
  // The worn item already counts toward stats, so only the gain is added.
  if (item.getType() == Weapon) attack = GameConfig::addCounter(attack, gain);
  else max_hp = GameConfig::addCounter(max_hp, gain);
  return true;
}
int player::repairCost(int index) const {
  if (index < 1 || index > static_cast<int>(equipments.size())) return 0;
  const Item &item = equipments[index - 1];
  const long long missing = max(0, item.getMaxDurability() - item.getDurability());
  const long long cost = missing * GameConfig::repairCostPerPoint(item.getPrice(), item.getTier());
  return static_cast<int>(min(cost, static_cast<long long>(GameConfig::MaxCoins)));
}
bool player::repairEquipment(int index) {
  const int cost = repairCost(index);
  if (cost <= 0 || coin < cost) return false;
  costcoin(cost);
  Item &item = equipments[index - 1];
  item.setDurability(item.getMaxDurability());
  return true;
}

void player::showSkills() const {
  for (int i = 0; i < static_cast<int>(skills.size()); ++i) {
    const skill &ability = *skills[i];
    cout << i + 1 << ". " << ability.getskillname() << " Lv." << ability.getskilllevel()
         << " [" << ability.getSpecializationName(*this) << "]\n   "
         << ability.getPreview(*this) << "\n   MP " << ability.getActualCost(*this)
         << " | 冷却 " << ability.getcurrentcooldown() << "/" << ability.getcooldown()
         << " | 等级需求 " << ability.getlevelrequired() << " | 楼层需求 " << ability.getunlockfloor()
         << " | 升级费用 " << ability.getlevelUpCost() << " | " << (ability.canUse(*this) ? "可用" : "不可用") << endl;
    if (i < 2)
      cout << "   专精第" << skill::specializationFloor(i + 1) << "层开放：0基础；"
           << ability.getSpecializationOptions(*this) << "。初选免费，重选费用 "
           << respecCost() << " 金币。" << endl;
  }
}
void player::showSkillsCompact() const {
  for (int i = 0; i < static_cast<int>(skills.size()); ++i) {
    const skill &ability = *skills[i];
    cout << i + 1 << ". " << ability.getskillname() << " [" << ability.getSpecializationName(*this)
         << "] MP " << ability.getActualCost(*this) << " | ";
    if (ability.canUse(*this)) cout << "可用";
    else if (ability.getcurrentcooldown() > 0) cout << "冷却 " << ability.getcurrentcooldown();
    else if (level < ability.getlevelrequired() || highestFloor < ability.getunlockfloor()) cout << "未解锁";
    else cout << "MP不足";
    cout << " | " << ability.getPreview(*this) << endl;
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
bool player::levelUpSkill(int index) {
  skill *ability = getSkill(index);
  if (!ability) return false;
  const int cost = ability->getlevelUpCost();
  if (!ability->levelUpSkill(level, coin, highestFloor)) return false;
  costcoin(cost);
  return true;
}
int player::respecCost() const { return 100 + highestFloor * 5; }
bool player::chooseSpecialization(int slot, int branch) {
  skill *ability = getSkill(slot);
  if (!ability || slot > 2 || branch < 0 || branch > 2 ||
      highestFloor < skill::specializationFloor(slot) || level < ability->getlevelrequired() ||
      ability->getSpecialization() == branch || encounterWearPending) return false;
  // Selecting or resetting any branch has no immediate resource or cooldown reward.
  // First choice is free; returning to branch zero costs the same as a respec.
  const int cost = ability->getSpecialization() == 0 ? 0 : respecCost();
  if (coin < cost || !ability->setSpecialization(branch)) return false;
  costcoin(cost);
  return true;
}
void player::loadStats(int newLevel, int newExp, int newCoin, int newHp, int newMaxHp,
                       int newAttack, int newMp, int newMaxMp, int newCritRate,
                       int newFreeRevives) {
  level = clamp(newLevel, 1, 10000); // Legacy saves retain up to 10000; new progression stops at 100.
  exp = max(0, newExp);
  coin = clamp(newCoin, 0, GameConfig::MaxCoins);
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
