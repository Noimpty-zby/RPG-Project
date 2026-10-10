#pragma once
#include "character.h"
#include "enemy.h"
#include "item.h"
#include <memory>
#include <vector>

class skill;

class player : public Character {
protected:
  int level;
  int exp;
  int coin;
  int free_revives;
  int critRate;
  int highestFloor = 1;
  int healingPercent = 100;
  bool guarding = false;
  bool guardPierced = false;
  std::vector<Item> backpack;
  std::vector<Item> equipments;
  std::vector<std::unique_ptr<skill>> skills;

public:
  player(std::string name, int hp, int attack, int mp, int level, int exp,
         int coin, int critRate);
  virtual ~player();

  // ── 基本属性 ──
  int getlevel() const;
  int getexp() const;
  int getcoin() const;
  int getfreerevives() const;
  int getcritRate() const;
  int getHighestFloor() const;
  void setHighestFloor(int floor);

  // ── 经验 / 金币 / 升级 ──
  void gainexp(int exp);
  void gaincoin(int coin);
  virtual void levelup();
  void costcoin(int amount);

  // ── 复活 ──
  void revive();
  void decreaserevives();
  void resetFreeRevive();

  // ── 战斗 ──
  virtual void attacktarget(Character &target) override;
  virtual void takedamage(int damage) override;
  void attacktargetDealt(Character &target, int damage);
  void heal(int amount);
  void setHealingPercent(int percent);
  void guard(bool pierced);
  void clearCombatModifiers();

  // ── 职业（纯虚） ──
  virtual std::string getjob() const = 0;

  // ── 显示 ──
  void printstats() const;

  // ── 背包 / 装备 ──
  void addItem(const Item &item);
  void printBackpack() const;
  bool useItem(int index);
  void equipItem(int index);
  void unequipItem(int index);
  void showEquippedItems() const;
  void equipItem_T(const Item &item, bool restoring = false);
  void dropItem(int index);           // 丢弃背包物品

  // ── 技能 ──
  void showSkills(player &hero) const;
  bool useSkill(int index, enemy &target);
  void learnSkill(std::unique_ptr<skill> newSkill);
  skill *getSkill(int index) const;
  int getSkillCount() const;
  void reduceallCooldowns();
  void levelUpSkill(int index);

  // ════════════════════════════════════════════════════
  //  存档 / 读档专用接口
  // ════════════════════════════════════════════════════

  /**
   * loadStats — 读档时一次性恢复所有核心数值。
   * 参数顺序：level, exp, coin,
   *           hp, max_hp,
   *           attack, mp, max_mp,
   *           critRate, free_revives
   */
  void loadStats(int level, int exp, int coin, int hp, int max_hp, int attack,
                 int mp, int max_mp, int critRate, int free_revives);

  /** 只读引用：暴露背包给 SaveSystem 序列化 */
  const std::vector<Item> &getSaveBackpack() const { return backpack; }

  /** 只读引用：暴露已装备列表给 SaveSystem 序列化 */
  const std::vector<Item> &getSaveEquipments() const { return equipments; }
};
