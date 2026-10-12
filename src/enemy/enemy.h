#pragma once
#include "character/character.h"
#include <array>
#include <string>
#include <vector>

enum class Affix {
  STRONG,
  TOUGH,
  QUICK,
  EXPLOSIVE,
  POISONOUS,
  HEALING,
  SHIELDING
};

enum class EnemyIntentKind {
  Attack,
  Charge,
  HeavyStrike,
  Wound,
  Shatter,
  ReadPattern,
  Recover
};

struct EnemyIntent {
  EnemyIntentKind kind = EnemyIntentKind::Attack;
  int damagePercent = 100;
  int healingPercent = 100;
  bool piercesGuard = false;
  int predictedAction = -99;
  bool interruptible = false;
};

// How hard each telegraphed intent hits, in percent of attack. An unanswered
// special must hurt more than plain attacks; answering it opens a window.
namespace IntentTuning {
constexpr int HeavyStrikePercent = 220;
constexpr int BossHeavyStrikePercent = 250;
constexpr int SabotagedHeavyStrikePercent = 170;
constexpr int SabotagedBossHeavyStrikePercent = 190;
constexpr int WoundPercent = 100;
constexpr int WoundHealingPercent = 50; // Player healing that still lands.
constexpr int ShatterPercent = 120;
constexpr int ReadMissPercent = 75;  // The player changed the predicted action.
constexpr int ReadHitPercent = 175;  // The player repeated it.
constexpr int HealingAffixPercent = 4; // Max HP restored per uninterrupted charge.
} // namespace IntentTuning

constexpr int AffixCount = 7;
const char *affixName(Affix affix);
const char *affixDescription(Affix affix);

// Chapter boss attack scaling, applied once when the boss is generated.
int bossAttackPercent(int floor);
// Chapter-themed display name for the enemy met on a floor.
std::string enemyNameForFloor(int floor);

// Each chapter has four ordinary enemies (one per signature tactic) and one
// boss: 50 stable entries. IDs are persisted in the player profile.
namespace Bestiary {
constexpr int EntriesPerChapter = 5;
constexpr int EntryCount = 50;
int entryForFloor(int floor);
std::string entryName(int entry);
std::string entryDescription(int entry);
int entryChapter(int entry);
bool entryIsBoss(int entry);
} // namespace Bestiary

class enemy : public Character {
private:
  int exp_reward;
  int coin_reward;
  int level;
  int dodgeRate = 10;
  std::vector<Affix> affixes;
  int encounterFloor = 0;
  bool bossEncounter = false;
  unsigned int completedTurns = 0;
  int lastAction = -99;
  // Action IDs -2..5 map to indices 0..7. Each row predicts the next action.
  std::array<std::array<unsigned int, 8>, 8> actionTransitions{};
  int baseAttack;
  int attackWeakenPercent = 0;
  int attackWeakenTurns = 0;
  int tacticalPressure = 0;
  int specialCooldown = 0;
  int turnsSinceSpecial = 0;
  int repeatedActions = 0;
  int recoveryTurns = 0;
  bool chargePending = false;
  bool interruptedThisTurn = false;
  bool lastPlayerHealed = false;
  bool desperationUsed = false;
  bool objectivePressure = false;
  bool sabotaged = false;

  int predictAction() const;
  void updateWeakenedAttack();
  EnemyIntentKind primaryTactic() const;
  EnemyIntentKind secondaryTactic() const;
  EnemyIntent makeIntent(EnemyIntentKind kind) const;

public:
  enemy(std::string name, int hp, int attack, int mp, int exp_reward,
        int coin_reward, int level);
  int getexp_reward() const;
  int getcoin_reward() const;
  int getlevel() const;
  int getdodgeRate() const;
  void setdodgeRate(int rate);
  Affix getRandomAffix() const;
  void setAffix(int floor);
  // Adds a distinct affix and its reward bonus; false if already present.
  bool addAffix(Affix affix);
  // Rolls weighted affixes until at least count distinct ones are present.
  void ensureAffixes(int count);
  void resethp(int amount);
  void resetexp_reward(int amount);
  void resetcoin_reward(int amount);
  // Sets the encounter's attack baseline and clears any temporary weakening.
  void resetattack(int amount);
  void resetall(int hp, int attack, int mp, int exp_reward, int coin_reward);
  virtual void takedamage(int damage) override;
  bool hasAffix(Affix affix) const;
  const std::vector<Affix> &getAffixes() const { return affixes; }
  void removeAffix(Affix affix);
  void displayAffixes() const;
  void heal(int amount);

  void configureEncounter(int floor, bool boss);
  bool isBoss() const;
  std::string getEncounterTitle() const;
  std::string getTacticalHint() const;
  EnemyIntent getIntent() const;
  void displayIntent() const;
  bool interruptCharge();
  // Whether a successful interrupt is followed by a Recover turn. Every text
  // that promises the rest reads this so it never contradicts the rule.
  bool interruptGrantsRecovery() const;
  void setObjectivePressure(bool enabled);
  void setTacticalPressure(int difficulty);
  void applySabotage();
  // Call once after resolving a successful player action and the enemy turn.
  // 0=attack, 1..5=skill slots, -1=guard, -2=potion. Invalid IDs do nothing.
  void finishTurn(int actionId, bool healed = false);
  // Pass the intent cached before acting to avoid retroactive HP-trigger changes.
  void finishTurn(int actionId, bool healed, EnemyIntentKind resolvedIntent);
  // Reapplications refresh duration/strength without compounding attack loss.
  void weakenAttack(int percent, int turns);
};
