#pragma once
#include "enemy/enemy.h"
#include "player/player.h"

enum class ActionType { Attack, Skill, Guard, Potion };
struct BattleAction {
  ActionType type;
  int index = 0;
};

enum class EncounterObjective { Defeat, Survive, Disrupt, Escape };
struct EncounterRules {
  EncounterObjective objective = EncounterObjective::Defeat;
  int targetTurns = 4;
  int difficulty = 0;
  bool bossSabotaged = false;
  bool elite = false;
};

struct BattleReport {
  int turns = 0;
  int damageTaken = 0;
  int poisonDamage = 0;
  int suppressedHealing = 0;
  int readPunishments = 0;
  int potionsUsed = 0;
  int guards = 0;
  int interrupts = 0;
  bool timeout = false;
};

// Shared by the terminal and simulations: failed choices never advance time.
class BattleSession {
public:
  BattleSession(player &hero, enemy &monster, EncounterRules rules = {});
  ~BattleSession();
  BattleSession(const BattleSession &) = delete;
  BattleSession &operator=(const BattleSession &) = delete;
  bool beginTurn();
  bool act(BattleAction action);
  int turns() const { return battleReport.turns; }
  bool finished() const;
  bool won() const;
  bool claimSettlement();
  const BattleReport &report() const { return battleReport; }
  int turnLimit() const { return maxTurns; }
  std::string objectiveDescription() const;
  EncounterObjective objective() const { return rules.objective; }

private:
  player &hero;
  enemy &monster;
  EnemyIntent intent;
  EncounterRules rules;
  BattleReport battleReport;
  bool validEncounter = false;
  bool awaitingAction = false;
  bool objectiveComplete = false;
  bool settlementClaimed = false;
  int maxTurns = 80;
  enum class DamageSource { Attack, Poison, Blast };
  void dealEnemyDamage(int damage, DamageSource source = DamageSource::Attack);
  void resolveObjective(BattleAction action);
};
