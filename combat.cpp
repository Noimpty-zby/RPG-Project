#include "combat.h"
#include <algorithm>
#include <iostream>

BattleSession::BattleSession(player &hero, enemy &monster)
    : hero(hero), monster(monster) {
  hero.clearCombatModifiers();
}
BattleSession::~BattleSession() { hero.clearCombatModifiers(); }

bool BattleSession::finished() const {
  return !hero.isalive() || !monster.isalive();
}

bool BattleSession::beginTurn() {
  if (finished()) return false;
  if (awaitingAction) return true;
  hero.clearCombatModifiers();
  hero.setmp(hero.getmp() + 8);
  hero.reduceallCooldowns();
  if (monster.hasAffix(Affix::POSIONOUS)) {
    std::cout << "【剧毒】" << std::endl;
    hero.takedamage(std::max(1, std::min(monster.getattack() / 5,
                                       hero.getmaxhp() / 20)));
    if (!hero.isalive()) return false;
  }
  intent = monster.getIntent();
  hero.setHealingPercent(intent.healingPercent);
  awaitingAction = true;
  return true;
}

bool BattleSession::act(BattleAction action) {
  if (!awaitingAction || finished()) return false;
  int actionId = 0;
  switch (action.type) {
  case ActionType::Attack:
    if (monster.hasAffix(Affix::SHIELDING)) {
      // A shield absorbs at most half the attack; it cannot lock a build out.
      int shield = std::min(hero.getattack() / 2, 8 + monster.getlevel() * 6);
      hero.attacktargetDealt(monster, hero.getattack() - shield);
    } else {
      hero.attacktarget(monster);
    }
    break;
  case ActionType::Skill:
    if (!hero.useSkill(action.index, monster)) return false;
    actionId = action.index;
    break;
  case ActionType::Guard:
    hero.guard(intent.piercesGuard);
    actionId = -1;
    break;
  case ActionType::Potion:
    if (!hero.useItem(action.index)) return false;
    actionId = -2;
    break;
  default:
    return false;
  }

  if (!monster.isalive()) {
    if (monster.hasAffix(Affix::EXPLOSIVE)) {
      std::cout << "【死亡爆炸】" << std::endl;
      hero.takedamage(std::max(1, std::min(monster.getattack() / 2,
                                         hero.getmaxhp() / 10)));
    }
  } else {
    int damagePercent = intent.damagePercent;
    if (intent.kind == EnemyIntentKind::ReadPattern && intent.predictedAction != -99) {
      bool predicted = actionId == intent.predictedAction;
      damagePercent = predicted ? 150 : 75;
      std::cout << (predicted ? "连招被识破，敌人追击！" : "你改变了行动，敌人判断落空！")
                << std::endl;
    }
    if (damagePercent > 0 && monster.getattack() > 0)
      hero.takedamage(std::max(1, monster.getattack() * damagePercent / 100));
    // Regeneration has a window to interrupt with burst damage, not an endless race.
    if (hero.isalive() && monster.hasAffix(Affix::HEALING) &&
        (monster.getlevel() < 40 || intent.kind == EnemyIntentKind::Recover)) {
      monster.heal(std::max(1, monster.getmaxhp() * 3 / 100));
    }
  }
  monster.finishTurn(actionId);
  ++completedTurns;
  awaitingAction = false;
  hero.clearCombatModifiers();
  return true;
}
