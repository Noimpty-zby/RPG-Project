#pragma once
#include "character/character.h"
#include "core/run_state.h"
#include "enemy/enemy.h"
#include "item/item.h"
#include <memory>
#include <vector>

class skill;

enum class Job { Warrior, Mage, Assassin };

// Base stats and per-level growth live in one table so new characters,
// level-ups and legacy-save migration cannot drift apart.
struct JobProfile {
  const char *saveKey;     // Stable save-file identifier.
  const char *displayName; // Shown to players.
  const char *resourceName;
  int hp, attack, mp, critRate;
  int hpGrowth, attackGrowth, mpGrowth;
};
const JobProfile &jobProfile(Job job);
JobReq equipmentClass(Job job);

class player : public Character {
protected:
  Job jobType;
  int level;
  int exp;
  int coin;
  int free_revives;
  int critRate;
  int highestFloor = 1;
  int healingPercent = 100;
  bool guarding = false;
  bool guardPierced = false;
  int combatResource = 0;
  int suppressedHealingTotal = 0;
  bool encounterWearPending = false;
  RunState run;
  std::vector<Item> backpack;
  std::vector<Item> equipments;
  std::vector<std::unique_ptr<skill>> skills;

  player(std::string name, Job job);
  // Potion choice shared by planPotions and drinkPotionsTo; see below.
  int potionHeal(const Item &item) const;
  int nextPotion(const std::vector<int> &left, int missing) const;

public:
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
  void levelup();
  void costcoin(int amount);

  // ── 复活 ──
  void revive();
  void decreaserevives();
  void resetFreeRevive();

  // ── 战斗 ──
  void attacktarget(Character &target);
  virtual void takedamage(int damage) override;
  // Poison and death blasts: no guard reduction and no dodge.
  void takeUnavoidableDamage(int damage);
  void attacktargetDealt(Character &target, int damage);
  void heal(int amount);
  void setHealingPercent(int percent);
  void guard(bool pierced);
  void clearCombatModifiers();
  int getRecoveryBaseHp() const;
  int getSuppressedHealingTotal() const { return suppressedHealingTotal; }
  int getCombatResource() const { return combatResource; }
  void addCombatResource(int amount);
  int consumeCombatResource();
  void resetEncounterResources();
  void restoreTurnResources();
  void finishEncounterWear();
  RunState &runState() { return run; }
  const RunState &runState() const { return run; }

  // ── 职业 ──
  Job job() const { return jobType; }
  std::string getjob() const { return jobProfile(jobType).saveKey; }
  std::string getJobName() const { return jobProfile(jobType).displayName; }
  bool canWear(const Item &item) const;

  // ── 显示 ──
  void printstats() const;

  // ── 背包 / 装备 ──
  void addItem(const Item &item);
  void printBackpack() const;
  bool useItem(int index);
  // Out-of-battle potions up to targetHp: each drink takes the largest potion
  // that does not overshoot the HP still missing, or the smallest one when
  // every potion would. planPotions consumes nothing; drinkPotionsTo makes the
  // same choices and returns the HP restored. Overflow is healing past max HP.
  struct PotionPlan {
    std::vector<std::pair<int, int>> uses; // backpack slot (1-based), count
    int restored = 0;
    int overflow = 0;
  };
  PotionPlan planPotions(int targetHp) const;
  int drinkPotionsTo(int targetHp);
  bool equipItem(int index);
  void unequipItem(int index);
  void showEquippedItems() const;
  void equipItem_T(const Item &item, bool restoring = false);
  // Sells part of a stack for a third of its unit price; locked items are kept.
  // Returns the coins earned, or 0 if nothing was sold.
  int sellItem(int index, int quantity);
  int equipmentEffect(int effectId) const;
  bool toggleItemLock(int index);
  int salvageUnlocked();
  bool modifyEquipment(int index, int effectId);
  // Coins to raise an equipped item one enhancement level; 0 when maxed or invalid.
  int enhanceCost(int index) const;
  bool enhanceEquipment(int index);
  // Coins to restore an equipped item to full durability; 0 when nothing to repair.
  int repairCost(int index) const;
  bool repairEquipment(int index);

  // ── 技能 ──
  void showSkills() const;
  void showSkillsCompact() const;
  bool useSkill(int index, enemy &target);
  void learnSkill(std::unique_ptr<skill> newSkill);
  skill *getSkill(int index) const;
  int getSkillCount() const;
  void reduceallCooldowns();
  bool levelUpSkill(int index);
  int respecCost() const;
  bool chooseSpecialization(int slot, int branch);

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
