#include "item.h"
#include <cstdint>
#include <iomanip>
#include <sstream>
using namespace std;

Item::Item(string name, int levelRequirement, ItemType type, int value,
           int durability, int price, int quantity, JobReq jobRequirement,
           int tier, ItemEffect effect, bool locked, string id)
    : name(name), levelRequirement(levelRequirement), type(type), value(value),
      durability(durability), price(price), quantity(quantity),
      jobRequirement(jobRequirement), tier(tier), effect(effect), locked(locked),
      id(id.empty() ? templateId(name, type, jobRequirement, tier) : id) {}

string Item::getName() const { return name; }
int Item::getLevelRequirement() const { return levelRequirement; }
ItemType Item::getType() const { return type; }
int Item::getValue() const { return value; }
int Item::getDurability() const { return durability; }
int Item::getPrice() const { return price; }
int Item::getQuantity() const { return quantity; }
JobReq Item::getJobRequirement() const { return jobRequirement; }
int Item::getTier() const { return tier; }
ItemEffect Item::getEffect() const { return effect; }
bool Item::isLocked() const { return locked; }
const string &Item::getId() const { return id; }

string Item::templateId(const string &name, ItemType type, JobReq job, int tier) {
  // Fixed FNV-1a over explicit template fields: independent of std::hash,
  // process RNG, roll values, lock state and the order files are loaded.
  const string key = to_string(static_cast<int>(job)) + ":" +
      to_string(static_cast<int>(type)) + ":" + to_string(tier) + ":" + name;
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char byte : key) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  ostringstream result;
  result << "item-" << hex << setw(16) << setfill('0') << hash;
  return result.str();
}

string Item::getTierName() const {
  switch (tier) {
  case 1: return "精良";
  case 2: return "稀有";
  case 3: return "史诗";
  default: return "普通";
  }
}

const char *Item::effectName(ItemEffect effect) {
  switch (effect) {
  case ItemEffect::ManaOnAttack: return "普攻回蓝";
  case ItemEffect::GuardRecovery: return "格挡回蓝";
  case ItemEffect::SkillEfficiency: return "技能节能";
  case ItemEffect::ControlFocus: return "控制强化";
  default: return "无";
  }
}
const char *Item::effectDescription(ItemEffect effect) {
  switch (effect) {
  case ItemEffect::ManaOnAttack: return "普攻回蓝：普攻命中额外回复3MP（仅武器）";
  case ItemEffect::GuardRecovery: return "格挡回蓝：格挡额外回复3MP（仅防具）";
  case ItemEffect::SkillEfficiency: return "技能节能：固定耗蓝技能消耗-10%";
  case ItemEffect::ControlFocus: return "控制强化：减攻控制延长1次行动（战士/法师装备）";
  default: return "无";
  }
}
string Item::getEffectName() const { return effectName(effect); }
string Item::getEffectDescription() const { return effectDescription(effect); }

bool Item::supportsEffect(ItemEffect candidate) const {
  if (candidate == ItemEffect::None) return true;
  if (type != Weapon && type != Armor) return false;
  if (candidate == ItemEffect::ManaOnAttack) return type == Weapon;
  if (candidate == ItemEffect::GuardRecovery) return type == Armor;
  if (candidate == ItemEffect::SkillEfficiency) return true;
  if (candidate == ItemEffect::ControlFocus)
    return jobRequirement == WarriorOnly || jobRequirement == MageOnly;
  return false;
}

bool Item::setEffect(ItemEffect candidate) {
  if (!supportsEffect(candidate)) return false;
  effect = candidate;
  return true;
}
void Item::setLocked(bool value) { locked = value; }

string Item::getJobRequirementName() const {
  switch (jobRequirement) {
  case WarriorOnly:
    return "战士专用";
  case MageOnly:
    return "法师专用";
  case AssassinOnly:
    return "刺客专用";
  default:
    return "通用";
  }
}

void Item::setQuantity(int q) { quantity = q; }
void Item::setDurability(int d) { durability = d; }
