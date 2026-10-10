#pragma once
#include <string>

enum ItemType { Weapon = 1, Armor = 2, Potion = 3 };
// 职业限制：0=通用，1=战士，2=法师，3=刺客
enum JobReq { AnyJob = 0, WarriorOnly = 1, MageOnly = 2, AssassinOnly = 3 };
enum class ItemEffect {
  None = 0, ManaOnAttack = 1, GuardRecovery = 2,
  SkillEfficiency = 3, ControlFocus = 4
};

class Item {
private:
  std::string name;
  int levelRequirement;
  ItemType type;
  int value;
  int durability;
  int maxDurability; // Repairs restore up to this; starts at the generated durability.
  int price;
  int quantity;
  JobReq jobRequirement;
  int tier;
  ItemEffect effect;
  bool locked;
  std::string id;
  int enhanceLevel = 0;

public:
  Item(std::string name, int levelRequirement, ItemType type, int value,
       int durability, int price, int quantity, JobReq jobRequirement = AnyJob,
       int tier = 0, ItemEffect effect = ItemEffect::None, bool locked = false,
       std::string id = "");

  std::string getName() const;
  // Name with the enhancement suffix, e.g. "精钢巨剑 +2"; getName() stays the identity.
  std::string getDisplayName() const;
  int getEnhanceLevel() const { return enhanceLevel; }
  void setEnhanceLevel(int level) { enhanceLevel = level; }
  int getLevelRequirement() const;
  ItemType getType() const;
  int getValue() const;
  int getDurability() const;
  int getMaxDurability() const;
  int getPrice() const;
  int getQuantity() const;
  JobReq getJobRequirement() const;
  int getTier() const;
  ItemEffect getEffect() const;
  bool isLocked() const;
  const std::string &getId() const;
  std::string getTierName() const;
  std::string getEffectName() const;
  std::string getEffectDescription() const;
  static const char *effectName(ItemEffect effect);
  static const char *effectDescription(ItemEffect effect);
  bool supportsEffect(ItemEffect effect) const;
  static std::string templateId(const std::string &name, ItemType type,
                                JobReq job, int tier);

  // 职业名字符串（用于显示）
  std::string getJobRequirementName() const;

  void setQuantity(int quantity);
  void setValue(int value);
  void setDurability(int durability);
  void setMaxDurability(int maxDurability);
  bool setEffect(ItemEffect effect);
  void setLocked(bool locked);
};
