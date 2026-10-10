#include "item.h"
using namespace std;

Item::Item(string name, int levelRequirement, ItemType type, int value,
           int durability, int price, int quantity, JobReq jobRequirement)
    : name(name), levelRequirement(levelRequirement), type(type), value(value),
      durability(durability), price(price), quantity(quantity),
      jobRequirement(jobRequirement) {}

string Item::getName() const { return name; }
int Item::getLevelRequirement() const { return levelRequirement; }
ItemType Item::getType() const { return type; }
int Item::getValue() const { return value; }
int Item::getDurability() const { return durability; }
int Item::getPrice() const { return price; }
int Item::getQuantity() const { return quantity; }
JobReq Item::getJobRequirement() const { return jobRequirement; }

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