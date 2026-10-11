#include "game/combat.h"
#include "player/skills.h"
#include <algorithm>
#include <iostream>

BattleSession::BattleSession(player &hero, enemy &monster, EncounterRules rules)
    : hero(hero), monster(monster), rules(rules) {
  validEncounter = hero.isalive() && monster.isalive();
  if (!validEncounter) return;
  this->rules.targetTurns = std::clamp(rules.targetTurns, 1, 30);
  this->rules.difficulty = std::clamp(rules.difficulty, 0, 2);
  if (rules.objective == EncounterObjective::Survive)
    maxTurns = this->rules.targetTurns + 5;
  else if (rules.objective == EncounterObjective::Disrupt)
    maxTurns = 20;
  else if (rules.objective == EncounterObjective::Escape)
    maxTurns = 15;
  monster.setTacticalPressure(this->rules.difficulty);
  monster.setObjectivePressure(rules.objective == EncounterObjective::Disrupt);
  if (rules.bossSabotaged) monster.applySabotage();
  hero.resetEncounterResources();
  hero.clearCombatModifiers();
}
BattleSession::~BattleSession() {
  if (validEncounter) hero.clearCombatModifiers();
}

bool BattleSession::finished() const {
  return !validEncounter || !hero.isalive() || !monster.isalive() || objectiveComplete ||
         battleReport.timeout;
}

bool BattleSession::won() const {
  // Death takes priority over a simultaneous kill, interruption or escape.
  return validEncounter && hero.isalive() && (!monster.isalive() || objectiveComplete);
}

bool BattleSession::claimSettlement() {
  if (!validEncounter || !finished() || settlementClaimed) return false;
  settlementClaimed = true;
  return true;
}

std::string BattleSession::objectiveDescription() const {
  switch (rules.objective) {
  case EncounterObjective::Survive:
    return "生存 " + std::to_string(rules.targetTurns) +
           " 个有效回合，或击败敌人（" + std::to_string(turns()) + "/" +
           std::to_string(rules.targetTurns) + "）";
  case EncounterObjective::Disrupt:
    return "普攻蓄力中的敌人，累计打断 2 次，或击败敌人（" +
           std::to_string(battleReport.interrupts) + "/2；限20回合）";
  case EncounterObjective::Escape:
    return "撑过至少 3 个有效回合，并以格挡撤离，或击败敌人（当前" +
           std::to_string(turns()) + "回合；限15回合）";
  case EncounterObjective::Defeat:
    return "击败敌人（限80个有效回合）";
  }
  return "击败敌人";
}

void BattleSession::dealEnemyDamage(int damage, DamageSource source) {
  if (damage <= 0) return;
  const int before = hero.gethp();
  // Attacks can be guarded or dodged; poison ticks and death blasts cannot.
  if (source == DamageSource::Attack) hero.takedamage(damage);
  else hero.takeUnavoidableDamage(damage);
  const int taken = std::max(0, before - hero.gethp());
  battleReport.damageTaken += taken;
  if (source == DamageSource::Poison) battleReport.poisonDamage += taken;
}

bool BattleSession::beginTurn() {
  if (finished()) return false;
  if (awaitingAction) return true;
  hero.clearCombatModifiers();
  hero.restoreTurnResources();
  hero.reduceallCooldowns();
  if (monster.hasAffix(Affix::POISONOUS)) {
    std::cout << "【剧毒】" << std::endl;
    dealEnemyDamage(std::max(1, std::min(monster.getattack() / 5,
                                        hero.getRecoveryBaseHp() / 20)), DamageSource::Poison);
    if (!hero.isalive()) return false;
  }
  intent = monster.getIntent();
  hero.setHealingPercent(intent.healingPercent);
  awaitingAction = true;
  return true;
}

void BattleSession::resolveObjective(BattleAction action) {
  if (!hero.isalive()) return;
  switch (rules.objective) {
  case EncounterObjective::Survive:
    objectiveComplete = turns() >= rules.targetTurns;
    break;
  case EncounterObjective::Disrupt:
    objectiveComplete = battleReport.interrupts >= 2;
    break;
  case EncounterObjective::Escape:
    objectiveComplete = turns() >= 3 && action.type == ActionType::Guard;
    break;
  case EncounterObjective::Defeat:
    break;
  }
  if (hero.isalive() && monster.isalive() && !objectiveComplete &&
      turns() >= maxTurns) {
    battleReport.timeout = true;
    std::cout << "【目标超时】未完成遭遇目标，本次挑战失败，可重新准备。" << std::endl;
  }
}

bool BattleSession::act(BattleAction action) {
  if (!awaitingAction || finished()) return false;
  const int hpBeforeAction = hero.gethp();
  const int enemyHpBefore = monster.gethp();
  const int suppressedBefore = hero.getSuppressedHealingTotal();
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
  case ActionType::Skill: {
    const skill *ability = hero.getSkill(action.index);
    const int cost = ability ? ability->getActualCost(hero) : 0;
    if (!hero.useSkill(action.index, monster)) return false;
    // A skill the enemy dodged entirely refunds half its MP; the cooldown still runs.
    if (cost >= 2 && monster.gethp() == enemyHpBefore) {
      const int mpBefore = hero.getmp();
      hero.setmp(hero.getmp() + cost / 2);
      std::cout << "【落空返还】技能被完全闪避，返还 " << hero.getmp() - mpBefore << " MP。" << std::endl;
    }
    actionId = action.index;
    break;
  }
  case ActionType::Guard:
    hero.guard(intent.piercesGuard);
    actionId = -1;
    ++battleReport.guards;
    if (intent.kind == EnemyIntentKind::Attack || intent.kind == EnemyIntentKind::Charge ||
        intent.kind == EnemyIntentKind::Recover)
      ++battleReport.waitingGuards;
    break;
  case ActionType::Potion:
    if (!hero.useItem(action.index)) return false;
    actionId = -2;
    ++battleReport.potionsUsed;
    break;
  default:
    return false;
  }

  const bool healed = hero.gethp() > hpBeforeAction;
  battleReport.suppressedHealing +=
      std::max(0, hero.getSuppressedHealingTotal() - suppressedBefore);
  bool interrupted = false;
  // A plain attack answers a charge even when dodged: the dodge costs the
  // hit's damage and MP, never the correct read.
  if (action.type == ActionType::Attack && intent.interruptible && monster.interruptCharge()) {
    interrupted = true;
    ++battleReport.interrupts;
    std::cout << (monster.gethp() < enemyHpBefore ? "【打断成功】普通攻击打断蓄力"
                                                  : "【打断成功】普攻虽被闪避，仍打断了蓄力")
              << (monster.interruptGrantsRecovery() ? "，下回合敌人休整。"
                                                    : "，重击取消；该敌人不休整，下回合仍会行动。")
              << std::endl;
  }

  if (!monster.isalive()) {
    if (monster.hasAffix(Affix::EXPLOSIVE)) {
      std::cout << "【死亡爆炸】" << std::endl;
      dealEnemyDamage(std::max(1, std::min(monster.getattack() / 2,
                                          hero.getRecoveryBaseHp() / 10)), DamageSource::Blast);
    }
  } else {
    int damagePercent = intent.damagePercent;
    if (intent.kind == EnemyIntentKind::ReadPattern && intent.predictedAction != -99) {
      bool predicted = actionId == intent.predictedAction;
      damagePercent = predicted ? IntentTuning::ReadHitPercent : IntentTuning::ReadMissPercent;
      if (predicted) ++battleReport.readPunishments;
      std::cout << (predicted ? "连招被识破，敌人追击！" : "你改变了行动，敌人判断落空！")
                << std::endl;
    }
    if (damagePercent > 0 && monster.getattack() > 0)
      dealEnemyDamage(std::max(1, monster.getattack() * damagePercent / 100));
    // Healers mend while charging; landing a basic attack interrupts it.
    if (hero.isalive() && monster.hasAffix(Affix::HEALING) &&
        intent.kind == EnemyIntentKind::Charge && !interrupted) {
      monster.heal(std::max(1, monster.getmaxhp() * IntentTuning::HealingAffixPercent / 100));
      std::cout << "【治疗】蓄力中的敌人回复了生命。" << std::endl;
    }
  }
  monster.finishTurn(actionId, healed, intent.kind);
  ++battleReport.turns;
  awaitingAction = false;
  hero.clearCombatModifiers();
  resolveObjective(action);
  return true;
}
