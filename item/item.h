#pragma once
#include <string>

enum ItemType { Weapon = 1, Armor = 2, Potion = 3 };
// 职业限制：0=通用，1=战士，2=法师，3=刺客
enum JobReq { AnyJob = 0, WarriorOnly = 1, MageOnly = 2, AssassinOnly = 3 };

class Item {
private:
  std::string name;
  int levelRequirement;
  ItemType type;
  int value;
  int durability;
  int price;
  int quantity;
  JobReq jobRequirement; // ★ 新增：职业限制

public:
  // jobRequirement 默认 AnyJob，向后兼容
  Item(std::string name, int levelRequirement, ItemType type, int value,
       int durability, int price, int quantity, JobReq jobRequirement = AnyJob);

  std::string getName() const;
  int getLevelRequirement() const;
  ItemType getType() const;
  int getValue() const;
  int getDurability() const;
  int getPrice() const;
  int getQuantity() const;
  JobReq getJobRequirement() const;

  // 职业名字符串（用于显示）
  std::string getJobRequirementName() const;

  void setQuantity(int quantity);
  void setDurability(int durability);
};