#pragma once
#include "character.h"
#include <array>
#include <vector>
enum class Affix {
  STRONG,
  TOUGH,
  QUICK,
  EXPLOSIVE,
  POSIONOUS,
  HEALING,
  SHIELDING
};

enum class EnemyIntentKind {
  Attack,
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
};

class enemy : public Character {
private:
  int exp_reward;
  int coin_reward;
  int level;
  int dogeRate = 10;
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

  int predictAction() const;
  void updateWeakenedAttack();

public:
  enemy(std::string name, int hp, int attack, int mp, int exp_reward,
        int coin_reward, int level);
  int getexp_reward() const;
  int getcoin_reward() const;
  int getlevel() const;
  int getdogeRate() const;
  void setdogeRate(int rate);
  Affix getRandomAffix() const;
  void setAffix(int floor);
  void resethp(int amount);
  void resetexp_reward(int amount);
  void resetcoin_reward(int amount);
  // Sets the encounter's attack baseline and clears any temporary weakening.
  void resetattack(int amount);
  void resetall(int hp, int attack, int mp, int exp_reward, int coin_reward);
  virtual void takedamage(int damage) override;
  bool hasAffix(Affix affix) const;
  void displayAffixes() const;
  void heal(int amount);

  void configureEncounter(int floor, bool boss);
  bool isBoss() const;
  EnemyIntent getIntent() const;
  void displayIntent() const;
  // Call once after resolving a successful player action and the enemy turn.
  // 0=attack, 1..5=skill slots, -1=guard, -2=potion. Invalid IDs do nothing.
  void finishTurn(int actionId);
  // Reapplications refresh duration/strength without compounding attack loss.
  void weakenAttack(int percent, int turns);
};
