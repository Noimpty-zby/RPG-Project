#include "player.h"
#include "character.h"
#include "item.h"
#include "skills.h"
#include "progression.h"
#include <algorithm>
#include <ctime>
#include <iostream>

using namespace std;

player::player(string name, int hp, int attack, int mp, int level, int exp,
               int coin, int critRate)
    : Character(name, hp, attack, mp), level(level), exp(exp), coin(coin),
      free_revives(3), critRate(critRate) {}

int player::getlevel() const { return level; }
int player::getexp() const { return exp; }
int player::getcoin() const { return coin; }
int player::getfreerevives() const { return free_revives; }
int player::getcritRate() const { return critRate; }
int player::getHighestFloor() const { return highestFloor; }
void player::setHighestFloor(int floor) {
  highestFloor = max(highestFloor, clamp(floor, 1, 100));
}

void player::decreaserevives() {
  if (free_revives > 0)
    free_revives--;
}
void player::resetFreeRevive() { free_revives = 3; }

void player::gainexp(int amount) {
  this->exp += amount;
  cout << getname() << " gain " << amount << " points of exp!" << endl;
  while (this->exp >= level * 100) {
    int threshold = level * 100; // 先用当前 level 计算阈值
    exp -= threshold;            // 再扣除，避免 levelup() 改变 level 后错误扣减
    levelup();
  }
}

void player::gaincoin(int coin) {
  this->coin += coin;
  cout << getname() << "gain" << coin << "coins!" << endl;
}

void player::levelup() {
  level++;
  attack += 2;
  max_hp += 10;
  hp = max_hp;
  cout << getname() << "level up to" << level << endl;
}

void player::costcoin(int amount) {
  if (coin >= amount) {
    coin -= amount;
    cout << getname() << "cost" << amount << "coins" << endl;
  } else {
    cout << getname() << "doesn't have enough coins!" << endl;
  }
}

void player::revive() {
  clearCombatModifiers();
  hp = max_hp;
  cout << getname() << "revived with " << hp << " HP!" << endl;
}

void player::printstats() const {
  cout << "=== " << getname() << "'s Stats ===" << endl;
  cout << "Level: " << level << endl;
  cout << "HP: " << hp << "/" << max_hp << endl;
  cout << "Attack: " << attack << endl;
  cout << "MP: " << mp << endl;
  cout << "EXP: " << exp << endl;
  cout << "Coins: " << coin << endl;
  cout << "Free Revives Left: " << getfreerevives() << endl;
  cout << "Crit Rate: " << critRate << "%" << endl;
  cout << "最高到达楼层: " << highestFloor << " | 技能等级上限: "
       << Progression::skillRankCap(highestFloor) << endl;
}

void player::heal(int amount) {
  int restored = min(max_hp - hp, max(0, amount) * healingPercent / 100);
  hp += restored;
  cout << "HP 恢复了 " << restored << " 点";
  if (healingPercent < 100)
    cout << "（本回合抑疗：恢复效果 " << healingPercent << "%）";
  cout << "。" << endl;
}

void player::setHealingPercent(int percent) {
  healingPercent = clamp(percent, 0, 100);
}

void player::guard(bool pierced) {
  guarding = true;
  guardPierced = pierced;
  setmp(getmp() + 6);
  cout << "你放弃攻击，格挡本回合伤害并恢复 6 MP。" << endl;
}

void player::clearCombatModifiers() {
  healingPercent = 100;
  guarding = false;
  guardPierced = false;
}

void player::attacktarget(Character &target) {
  int damage = attack;
  bool isCrit = (rand() % 100 < critRate);
  if (isCrit) {
    damage *= 2;
    cout << "【暴击】" << endl;
  }

  cout << getname() << " attacks " << target.getname() << " for " << damage
       << " damage!" << endl;
  target.takedamage(damage);

  // 武器耐久（从后向前）
  for (int i = (int)equipments.size() - 1; i >= 0; --i) {
    auto &item = equipments[i];
    if (item.getType() == Weapon) {
      item.setDurability(item.getDurability() - 1);
      if (item.getDurability() <= 0) {
        cout << "【装备破损】" << item.getName() << "已破损，攻击力减少"
             << item.getValue() << "点！" << endl;
        attack -= item.getValue();
        equipments.erase(equipments.begin() + i);
      }
    }
  }
}

void player::attacktargetDealt(Character &target, int damage) {
  bool isCrit = (rand() % 100 < critRate);
  if (isCrit) {
    damage *= 2;
    cout << "【暴击】" << endl;
  }

  cout << getname() << " attacks " << target.getname() << " for " << damage
       << " damage!" << endl;
  target.takedamage(damage);

  for (int i = (int)equipments.size() - 1; i >= 0; --i) {
    auto &item = equipments[i];
    if (item.getType() == Weapon) {
      item.setDurability(item.getDurability() - 1);
      if (item.getDurability() <= 0) {
        cout << "【装备破损】" << item.getName() << "已破损，攻击力减少"
             << item.getValue() << "点！" << endl;
        attack -= item.getValue();
        equipments.erase(equipments.begin() + i);
      }
    }
  }
}

void player::addItem(const Item &item) {
  for (auto &existing : backpack) {
    if (existing.getName() == item.getName() &&
        existing.getLevelRequirement() == item.getLevelRequirement() &&
        existing.getType() == item.getType() &&
        existing.getValue() == item.getValue() &&
        existing.getDurability() == item.getDurability() &&
        existing.getPrice() == item.getPrice() &&
        existing.getJobRequirement() == item.getJobRequirement()) {
      existing.setQuantity(existing.getQuantity() + item.getQuantity());
      return;
    }
  }
  backpack.push_back(item);
}

void player::printBackpack() const {
  if (backpack.empty()) {
    cout << "背包空空如也！" << endl;
    return;
  }

  cout << "=== " << getname() << "的背包 ===" << endl;
  for (int i = 0; i < (int)backpack.size(); i++) {
    const Item &item = backpack[i];
    cout << i + 1 << ". " << item.getName();
    cout << " x" << item.getQuantity();
    if (item.getType() == Weapon || item.getType() == Armor)
      cout << " 耐久度: " << item.getDurability();
    cout << "\n   类型: "
         << (item.getType() == Weapon  ? "武器"
             : item.getType() == Armor ? "防具"
                                       : "药水")
         << "\n   价值: " << item.getValue() << "\n   价格: " << item.getPrice()
         << "\n   等级需求: " << item.getLevelRequirement()
         << "\n   职业限制: " << item.getJobRequirementName() << endl;
    cout << "-----------------------------\n";
  }
}

bool player::useItem(int index) {
  if (index < 1 || index > (int)backpack.size()) {
    cout << "无效的物品索引！" << endl;
    return false;
  }
  Item &item = backpack[index - 1];
  if (item.getType() == Potion) {
    if (hp >= max_hp) {
      cout << "生命已满，未消耗药水。" << endl;
      return false;
    }
    cout << getname() << "使用了" << item.getName() << "。" << endl;
    heal(item.getValue());
    item.setQuantity(item.getQuantity() - 1);
    if (item.getQuantity() <= 0)
      backpack.erase(backpack.begin() + index - 1);
    return true;
  } else {
    cout << "只能使用药水类物品！" << endl;
    return false;
  }
}

void player::equipItem(int index) {
  if (index < 1 || index > (int)backpack.size()) {
    cout << "无效的物品索引！" << endl;
    return;
  }
  Item itemToEquip = backpack[index - 1];
  if (itemToEquip.getLevelRequirement() > level) {
    cout << "等级不足，无法装备【" << itemToEquip.getName() << "】！" << endl;
    return;
  }
  // ── 职业限制检查 ──
  JobReq jr = itemToEquip.getJobRequirement();
  if (jr != AnyJob) {
    string job = getjob();
    bool ok = (jr == WarriorOnly && job == "Warrior") ||
              (jr == MageOnly && job == "Mage") ||
              (jr == AssassinOnly && job == "Assassin");
    if (!ok) {
      cout << "【" << itemToEquip.getName() << "】是"
           << itemToEquip.getJobRequirementName() << "，你无法装备！" << endl;
      return;
    }
  }
  if (itemToEquip.getType() == Weapon || itemToEquip.getType() == Armor) {
    for (int i = (int)equipments.size() - 1; i >= 0; --i) {
      if (equipments[i].getType() == itemToEquip.getType()) {
        cout << "自动卸下 " << equipments[i].getName() << endl;
        if (equipments[i].getType() == Weapon)
          attack -= equipments[i].getValue();
        else {
          max_hp -= equipments[i].getValue();
          if (hp > max_hp)
            hp = max_hp;
        }
        bool merged = false;
        for (auto &existing : backpack) {
          if (existing.getName() == equipments[i].getName() &&
              existing.getLevelRequirement() ==
                  equipments[i].getLevelRequirement() &&
              existing.getType() == equipments[i].getType() &&
              existing.getValue() == equipments[i].getValue() &&
              existing.getDurability() == equipments[i].getDurability() &&
              existing.getPrice() == equipments[i].getPrice() &&
              existing.getJobRequirement() == equipments[i].getJobRequirement()) {
            existing.setQuantity(existing.getQuantity() + 1);
            merged = true;
            break;
          }
        }
        if (!merged)
          backpack.push_back(equipments[i]);
        equipments.erase(equipments.begin() + i);
        break;
      }
    }
    cout << getname() << "装备了" << itemToEquip.getName() << "！" << endl;
    if (itemToEquip.getType() == Weapon)
      attack += itemToEquip.getValue();
    else {
      max_hp += itemToEquip.getValue();
    }
    itemToEquip.setQuantity(1);
    equipments.push_back(itemToEquip);

    Item &bp = backpack[index - 1];
    bp.setQuantity(bp.getQuantity() - 1);
    if (bp.getQuantity() <= 0)
      backpack.erase(backpack.begin() + index - 1);
  } else {
    cout << "只能装备武器或防具！" << endl;
  }
}

void player::unequipItem(int index) {
  if (index < 1 || index > (int)equipments.size()) {
    cout << "无效的装备索引！" << endl;
    return;
  }
  Item &item = equipments[index - 1];
  cout << getname() << "卸下了" << item.getName() << "！" << endl;
  if (item.getType() == Weapon)
    attack -= item.getValue();
  else if (item.getType() == Armor) {
    max_hp -= item.getValue();
    if (hp > max_hp)
      hp = max_hp;
  }

  for (auto &existing : backpack) {
    if (existing.getName() == item.getName() &&
        existing.getLevelRequirement() == item.getLevelRequirement() &&
        existing.getType() == item.getType() &&
        existing.getValue() == item.getValue() &&
        existing.getDurability() == item.getDurability() &&
        existing.getPrice() == item.getPrice() &&
        existing.getJobRequirement() == item.getJobRequirement()) {
      existing.setQuantity(existing.getQuantity() + 1);
      equipments.erase(equipments.begin() + index - 1);
      return;
    }
  }
  backpack.push_back(item);
  equipments.erase(equipments.begin() + index - 1);
}

void player::showEquippedItems() const {
  if (equipments.empty()) {
    cout << "没有装备任何物品！" << endl;
    return;
  }
  cout << "=== " << getname() << "的装备 ===" << endl;
  for (const auto &item : equipments) {
    cout << "- " << item.getName() << " (";
    if (item.getType() == Weapon)
      cout << "武器, 攻击+" << item.getValue();
    else if (item.getType() == Armor)
      cout << "防具, HP+" << item.getValue();
    cout << ")" << endl;
  }
}

void player::takedamage(int damage) {
  damage = max(0, damage);
  if (guarding && !guardPierced)
    damage = (damage + 1) / 2;
  hp -= damage;
  if (hp < 0)
    hp = 0;

  for (int i = (int)equipments.size() - 1; i >= 0; --i) {
    auto &item = equipments[i];
    if (item.getType() == Armor) {
      item.setDurability(item.getDurability() - 1);
      if (item.getDurability() <= 0) {
        cout << "【装备破损】" << item.getName() << "已破损，HP减少"
             << item.getValue() << "点！" << endl;
        max_hp -= item.getValue();
        if (hp > max_hp)
          hp = max_hp;
        equipments.erase(equipments.begin() + i);
      }
    }
  }
  cout << getname() << " takes " << damage << " damage! (HP: " << hp << "/"
       << max_hp << ")" << endl;
}

void player::showSkills(player &hero) const {
  if (skills.empty()) {
    cout << "没有任何技能！" << endl;
    return;
  }
  cout << "=== " << getname() << "的技能 ===" << endl;
  for (int i = 0; i < (int)skills.size(); i++) {
    const skill *s = skills[i].get();
    cout << i + 1 << ". " << s->getskillname()
         << " (等级: " << s->getskilllevel() << ", MP消耗: " << s->getcostmp()
         << ", 冷却: " << s->getcooldown()
         << "回合, 当前冷却: " << s->getcurrentcooldown()
         << "回合, 基础伤害: " << s->getdamage() << ")" << endl
         << "等级需求: " << s->getlevelrequired() << endl
         << "楼层需求: " << s->getunlockfloor()
         << " | 当前技能等级上限: " << Progression::skillRankCap(highestFloor)
         << " | 升级费用: " << s->getlevelUpCost() << endl
         << "当前技能状态: " << (s->canUse(hero) ? "可用" : "不可用") << endl
         << "===============================" << endl;
  }
}

bool player::useSkill(int index, enemy &target) {
  if (index < 1 || index > (int)skills.size()) {
    cout << "无效的技能索引！" << endl;
    return false;
  }
  return skills[index - 1]->useSkill(*this, target);
}

void player::learnSkill(unique_ptr<skill> newSkill) {
  skills.push_back(std::move(newSkill));
}

skill *player::getSkill(int index) const {
  if (index < 1 || index > (int)skills.size())
    return nullptr;
  return skills[index - 1].get();
}

int player::getSkillCount() const { return (int)skills.size(); }

void player::reduceallCooldowns() {
  for (auto &s : skills)
    s->reducecooldown();
}

player::~player() = default;

void player::levelUpSkill(int index) {
  if (index < 1 || index > (int)skills.size()) {
    cout << "无效的技能索引！" << endl;
    return;
  }
  skill *s = skills[index - 1].get();
  int costcoins = s->getlevelUpCost();
  if (s->levelUpSkill(level, coin, highestFloor))
    costcoin(costcoins);
}

void player::equipItem_T(const Item &item, bool restoring) {
  if (!restoring && (item.getLevelRequirement() > level || item.getType() == Potion)) {
    addItem(item);
    cout << "当前不能直接装备，物品已放入背包。" << endl;
    return;
  }
  // 职业限制（宝箱/事件直接装备也要检查）
  JobReq jr = item.getJobRequirement();
  if (jr != AnyJob) {
    string job = getjob();
    bool ok = (jr == WarriorOnly && job == "Warrior") ||
              (jr == MageOnly && job == "Mage") ||
              (jr == AssassinOnly && job == "Assassin");
    if (!ok) {
      cout << "【" << item.getName() << "】是" << item.getJobRequirementName()
           << "，你无法装备！" << endl;
      if (!restoring) {
        addItem(item);
        cout << "物品已放入背包。" << endl;
      }
      return;
    }
  }
  if (item.getType() == Weapon || item.getType() == Armor) {
    for (int i = (int)equipments.size() - 1; i >= 0; --i) {
      if (equipments[i].getType() == item.getType()) {
        cout << "自动卸下 " << equipments[i].getName() << endl;
        if (equipments[i].getType() == Weapon)
          attack -= equipments[i].getValue();
        else {
          max_hp -= equipments[i].getValue();
          if (hp > max_hp)
            hp = max_hp;
        }
        bool merged = false;
        for (auto &existing : backpack) {
          if (existing.getName() == equipments[i].getName() &&
              existing.getLevelRequirement() ==
                  equipments[i].getLevelRequirement() &&
              existing.getType() == equipments[i].getType() &&
              existing.getValue() == equipments[i].getValue() &&
              existing.getDurability() == equipments[i].getDurability() &&
              existing.getPrice() == equipments[i].getPrice() &&
              existing.getJobRequirement() == equipments[i].getJobRequirement()) {
            existing.setQuantity(existing.getQuantity() + 1);
            merged = true;
            break;
          }
        }
        if (!merged)
          backpack.push_back(equipments[i]);
        equipments.erase(equipments.begin() + i);
        break;
      }
    }
    cout << getname() << "装备了" << item.getName() << "！" << endl;
    if (item.getType() == Weapon)
      attack += item.getValue();
    else {
      max_hp += item.getValue();
    }
    Item newItem = item;
    newItem.setQuantity(1);
    equipments.push_back(newItem);
  } else {
    cout << "只能装备武器或防具！" << endl;
  }
}

// ── 丢弃背包物品 ──
void player::dropItem(int index) {
  if (index < 1 || index > (int)backpack.size()) {
    cout << "无效的物品索引！" << endl;
    return;
  }
  const Item &item = backpack[index - 1];
  cout << "丢弃了【" << item.getName() << "】×" << item.getQuantity()
       << "。（卖出价约 " << item.getPrice() / 3 << " 金币）" << endl;
  // 丢弃时给予三分之一售价作为补偿
  coin += item.getPrice() / 3;
  backpack.erase(backpack.begin() + index - 1);
}

// ════════════════════════════════════════════════════
//  存档 / 读档专用
// ════════════════════════════════════════════════════
void player::loadStats(int _level, int _exp, int _coin, int _hp, int _max_hp,
                       int _attack, int _mp, int _max_mp, int _critRate,
                       int _free_revives) {
  level = _level;
  exp = _exp;
  coin = _coin;
  hp = _hp;
  max_hp = _max_hp;
  attack = _attack;
  mp = _mp;
  max_mp = _max_mp;
  critRate = _critRate;
  free_revives = _free_revives;
}
