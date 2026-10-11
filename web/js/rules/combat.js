// Port of src/game/combat.{h,cpp}: the shared turn engine. Failed choices
// never advance time.
import { clamp, idiv } from './config.js';
import { Affix, Intent, IntentTuning } from './enemy.js';
import { emit } from './log.js';

export const ActionType = { Attack: 'Attack', Skill: 'Skill', Guard: 'Guard', Potion: 'Potion' };
export const Objective = { Defeat: 'Defeat', Survive: 'Survive', Disrupt: 'Disrupt', Escape: 'Escape' };

export const defaultRules = () => ({
  objective: Objective.Defeat, targetTurns: 4, difficulty: 0, bossSabotaged: false, elite: false,
});

export class BattleSession {
  constructor(hero, monster, rules = defaultRules()) {
    this.hero = hero;
    this.monster = monster;
    this.rules = { ...rules };
    this.intent = null;
    this.report = {
      turns: 0, damageTaken: 0, poisonDamage: 0, suppressedHealing: 0, readPunishments: 0,
      potionsUsed: 0, guards: 0, waitingGuards: 0, heavyGuards: 0, skillsUsed: 0, interrupts: 0, timeout: false,
    };
    this.awaitingAction = false;
    this.objectiveComplete = false;
    this.settlementClaimed = false;
    this.maxTurns = 80;
    this.validEncounter = hero.isalive() && monster.isalive();
    if (!this.validEncounter) return;
    this.rules.targetTurns = clamp(rules.targetTurns, 1, 30);
    this.rules.difficulty = clamp(rules.difficulty, 0, 2);
    if (rules.objective === Objective.Survive) this.maxTurns = this.rules.targetTurns + 5;
    else if (rules.objective === Objective.Disrupt) this.maxTurns = 20;
    else if (rules.objective === Objective.Escape) this.maxTurns = 15;
    monster.setTacticalPressure(this.rules.difficulty);
    monster.setObjectivePressure(rules.objective === Objective.Disrupt);
    if (rules.bossSabotaged) monster.applySabotage();
    hero.resetEncounterResources();
    hero.clearCombatModifiers();
  }

  // The C++ destructor clears combat modifiers; call this when a fight ends.
  close() { if (this.validEncounter) this.hero.clearCombatModifiers(); }
  turns() { return this.report.turns; }
  turnLimit() { return this.maxTurns; }
  objective() { return this.rules.objective; }
  finished() {
    return !this.validEncounter || !this.hero.isalive() || !this.monster.isalive() ||
      this.objectiveComplete || this.report.timeout;
  }
  won() {
    return this.validEncounter && this.hero.isalive() && (!this.monster.isalive() || this.objectiveComplete);
  }
  claimSettlement() {
    if (!this.validEncounter || !this.finished() || this.settlementClaimed) return false;
    this.settlementClaimed = true;
    return true;
  }
  objectiveDescription() {
    switch (this.rules.objective) {
      case Objective.Survive:
        return `生存 ${this.rules.targetTurns} 个有效回合，或击败敌人（${this.turns()}/${this.rules.targetTurns}）`;
      case Objective.Disrupt:
        return `普攻蓄力中的敌人，累计打断 2 次，或击败敌人（${this.report.interrupts}/2；限20回合）`;
      case Objective.Escape:
        return `撑过至少 3 个有效回合，并以格挡撤离，或击败敌人（当前${this.turns()}回合；限15回合）`;
      default:
        return '击败敌人（限80个有效回合）';
    }
  }

  dealEnemyDamage(damage, source = 'Attack') {
    if (damage <= 0) return;
    const before = this.hero.gethp();
    if (source === 'Attack') this.hero.takedamage(damage);
    else this.hero.takeUnavoidableDamage(damage);
    const taken = Math.max(0, before - this.hero.gethp());
    this.report.damageTaken += taken;
    if (source === 'Poison') this.report.poisonDamage += taken;
  }

  beginTurn() {
    if (this.finished()) return false;
    if (this.awaitingAction) return true;
    this.hero.clearCombatModifiers();
    this.hero.restoreTurnResources();
    this.hero.reduceallCooldowns();
    if (this.monster.hasAffix(Affix.POISONOUS)) {
      emit('poison', '【剧毒】', { side: 'hero' });
      this.dealEnemyDamage(Math.max(1, Math.min(idiv(this.monster.getattack(), 5),
        idiv(this.hero.getRecoveryBaseHp(), 20))), 'Poison');
      if (!this.hero.isalive()) return false;
    }
    this.intent = this.monster.getIntent();
    this.hero.setHealingPercent(this.intent.healingPercent);
    this.awaitingAction = true;
    emit('intent', this.monster.describeIntent(this.intent), { intent: this.intent });
    return true;
  }

  resolveObjective(action) {
    if (!this.hero.isalive()) return;
    switch (this.rules.objective) {
      case Objective.Survive:
        this.objectiveComplete = this.turns() >= this.rules.targetTurns;
        break;
      case Objective.Disrupt:
        this.objectiveComplete = this.report.interrupts >= 2;
        break;
      case Objective.Escape:
        this.objectiveComplete = this.turns() >= 3 && action.type === ActionType.Guard;
        break;
      default:
        break;
    }
    if (this.objectiveComplete && this.monster.isalive())
      emit('objective', '【目标完成】', { objective: this.rules.objective });
    if (this.hero.isalive() && this.monster.isalive() && !this.objectiveComplete && this.turns() >= this.maxTurns) {
      this.report.timeout = true;
      emit('timeout', '【目标超时】未完成遭遇目标，本次挑战失败，可重新准备。');
    }
  }

  act(action) {
    if (!this.awaitingAction || this.finished()) return false;
    const hero = this.hero;
    const monster = this.monster;
    const intent = this.intent;
    const hpBeforeAction = hero.gethp();
    const enemyHpBefore = monster.gethp();
    const suppressedBefore = hero.getSuppressedHealingTotal();
    let actionId = 0;
    switch (action.type) {
      case ActionType.Attack:
        if (monster.hasAffix(Affix.SHIELDING)) {
          const shield = Math.min(idiv(hero.getattack(), 2), 8 + monster.getlevel() * 6);
          hero.attacktargetDealt(monster, hero.getattack() - shield);
        } else {
          hero.attacktarget(monster);
        }
        break;
      case ActionType.Skill: {
        const ability = hero.getSkill(action.index);
        const cost = ability ? ability.getActualCost(hero) : 0;
        if (!hero.useSkill(action.index, monster)) return false;
        // A skill the enemy dodged entirely refunds half its MP; the cooldown still runs.
        if (cost >= 2 && monster.gethp() === enemyHpBefore) {
          const mpBefore = hero.getmp();
          hero.setmp(hero.getmp() + idiv(cost, 2));
          const amount = hero.getmp() - mpBefore;
          emit('mana', `【落空返还】技能被完全闪避，返还 ${amount} MP。`, { side: 'hero', amount });
        }
        actionId = action.index;
        ++this.report.skillsUsed;
        break;
      }
      case ActionType.Guard:
        hero.guard(intent.piercesGuard);
        actionId = -1;
        ++this.report.guards;
        // Guarding a plain attack, a charge or a rest is waiting, not answering a threat.
        if (intent.kind === Intent.Attack || intent.kind === Intent.Charge || intent.kind === Intent.Recover)
          ++this.report.waitingGuards;
        if (intent.kind === Intent.HeavyStrike) ++this.report.heavyGuards;
        break;
      case ActionType.Potion:
        if (!hero.useItem(action.index)) return false;
        actionId = -2;
        ++this.report.potionsUsed;
        break;
      default:
        return false;
    }

    const healed = hero.gethp() > hpBeforeAction;
    this.report.suppressedHealing += Math.max(0, hero.getSuppressedHealingTotal() - suppressedBefore);
    let interrupted = false;
    // A plain attack answers a charge even when dodged: the dodge costs the
    // hit's damage and MP, never the correct read.
    if (action.type === ActionType.Attack && intent.interruptible && monster.interruptCharge()) {
      interrupted = true;
      ++this.report.interrupts;
      emit('interrupt', (monster.gethp() < enemyHpBefore ? '【打断成功】普通攻击打断蓄力' : '【打断成功】普攻虽被闪避，仍打断了蓄力') +
        (monster.interruptGrantsRecovery() ? '，下回合敌人休整。' : '，重击取消；该敌人不休整，下回合仍会行动。'), { side: 'enemy' });
    }

    if (!monster.isalive()) {
      emit('defeated', `${monster.getname()}倒下了。`, { side: 'enemy' });
      if (monster.hasAffix(Affix.EXPLOSIVE)) {
        emit('blast', '【死亡爆炸】', { side: 'hero' });
        this.dealEnemyDamage(Math.max(1, Math.min(idiv(monster.getattack(), 2),
          idiv(hero.getRecoveryBaseHp(), 10))), 'Blast');
      }
    } else {
      let damagePercent = intent.damagePercent;
      let predicted = null;
      if (intent.kind === Intent.ReadPattern && intent.predictedAction !== -99) {
        predicted = actionId === intent.predictedAction;
        damagePercent = predicted ? IntentTuning.ReadHitPercent : IntentTuning.ReadMissPercent;
        if (predicted) ++this.report.readPunishments;
      }
      emit('enemyAction', predicted === null ? '' : predicted ? '连招被识破，敌人追击！' : '你改变了行动，敌人判断落空！',
        { side: 'enemy', kind: interrupted ? 'Interrupted' : intent.kind, damagePercent, predicted,
          pierces: intent.piercesGuard });
      if (damagePercent > 0 && monster.getattack() > 0)
        this.dealEnemyDamage(Math.max(1, idiv(monster.getattack() * damagePercent, 100)));
      if (hero.isalive() && monster.hasAffix(Affix.HEALING) && intent.kind === Intent.Charge && !interrupted) {
        const before = monster.gethp();
        monster.heal(Math.max(1, idiv(monster.getmaxhp() * IntentTuning.HealingAffixPercent, 100)));
        emit('enemyHeal', '【治疗】蓄力中的敌人回复了生命。',
          { side: 'enemy', amount: monster.gethp() - before, hp: monster.gethp(), maxHp: monster.getmaxhp() });
      }
    }
    monster.finishTurn(actionId, healed, intent.kind);
    ++this.report.turns;
    this.awaitingAction = false;
    hero.clearCombatModifiers();
    this.resolveObjective(action);
    return true;
  }
}
