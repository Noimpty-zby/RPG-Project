#pragma once
#include "enemy.h"
#include "player.h"

enum class ActionType { Attack, Skill, Guard, Potion };
struct BattleAction {
  ActionType type;
  int index = 0;
};

// Shared by the terminal and simulations: failed choices never advance time.
class BattleSession {
public:
  BattleSession(player &hero, enemy &monster);
  ~BattleSession();
  BattleSession(const BattleSession &) = delete;
  BattleSession &operator=(const BattleSession &) = delete;
  bool beginTurn();
  bool act(BattleAction action);
  int turns() const { return completedTurns; }
  bool finished() const;

private:
  player &hero;
  enemy &monster;
  EnemyIntent intent;
  bool awaitingAction = false;
  int completedTurns = 0;
};
