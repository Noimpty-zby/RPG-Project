// Port of src/enemy/enemy.{h,cpp}: affixes, conditional intents, boss
// profiles, themed enemy names and bestiary ids.
import { Character } from './character.js';
import { chapterOf, clamp, floorOf, IntMax } from './config.js';
import { below } from './random.js';
import { emit } from './log.js';

export const Affix = { STRONG: 0, TOUGH: 1, QUICK: 2, EXPLOSIVE: 3, POISONOUS: 4, HEALING: 5, SHIELDING: 6 };
export const AffixCount = 7;

export const Intent = {
  Attack: 'Attack', Charge: 'Charge', HeavyStrike: 'HeavyStrike', Wound: 'Wound',
  Shatter: 'Shatter', ReadPattern: 'ReadPattern', Recover: 'Recover',
};

// How hard each telegraphed intent hits, in percent of attack.
export const IntentTuning = {
  HeavyStrikePercent: 220,
  BossHeavyStrikePercent: 250,
  SabotagedHeavyStrikePercent: 170,
  SabotagedBossHeavyStrikePercent: 190,
  WoundPercent: 100,
  WoundHealingPercent: 50,
  ShatterPercent: 120,
  ReadMissPercent: 75,
  ReadHitPercent: 175,
  HealingAffixPercent: 4,
};

const AffixNames = ['强力', '坚韧', '迅捷', '爆炸', '剧毒', '治疗', '护盾'];
const AffixDescriptions = [
  '攻击提高20%。',
  '生命提高20%。',
  '闪避率提高（最高25%），攻击可能落空。',
  '被击败时爆炸：造成其攻击一半的伤害（最多基础生命10%），无法格挡或闪避。',
  '每回合开始造成其攻击1/5的伤害（最多基础生命5%），无法格挡或闪避。',
  '蓄力完成时回复4%最大生命；普攻打断蓄力即可阻止。',
  '吸收普攻和多数技能的部分伤害（最多一半）；穿盾技能不受影响。',
];
export const affixName = (affix) => AffixNames[affix] ?? '未知';
export const affixDescription = (affix) => AffixDescriptions[affix] ?? '';

export const BossProfiles = [
  { title: '荒野前哨·警钟巨兵', primary: Intent.Charge, secondary: Intent.Charge,
    hint: '目标：普攻打断巨兵的敲钟蓄力；错过窗口时格挡重击。', attackPercent: 100 },
  { title: '幽林回廊·树心祭司', primary: Intent.Wound, secondary: Intent.Charge,
    hint: '目标：避开树心抑疗集中恢复，普攻打断仪式蓄力。', attackPercent: 100 },
  { title: '沉没书库·禁书守卫', primary: Intent.Shatter, secondary: Intent.Charge,
    hint: '目标：守卫破防时主动出击，普攻打断蓄力，为学者争取时间。', attackPercent: 100 },
  { title: '赤砂矿井·引爆监工', primary: Intent.ReadPattern, secondary: Intent.Charge,
    hint: '目标：改变监工预测的行动，用普攻打断引爆蓄力。', attackPercent: 100 },
  { title: '寒霜关隘·双首冰兽', primary: Intent.Wound, secondary: Intent.Shatter,
    hint: '目标：避开冰霜抑疗安排恢复；连续格挡会招来破防。', attackPercent: 90 },
  { title: '熔炉要塞·供能核心', primary: Intent.Charge, secondary: Intent.Shatter,
    hint: '目标：摧毁供能炉削弱核心，普攻打断蓄力，破防时主动出击。', attackPercent: 100 },
  { title: '镜影剧场·镜阵导演', primary: Intent.ReadPattern, secondary: Intent.Wound,
    hint: '目标：打乱镜阵记录的连招，避免连续依赖回血。', attackPercent: 100 },
  { title: '星蚀观测站·蚀星观测者', primary: Intent.Shatter, secondary: Intent.ReadPattern,
    hint: '目标：星蚀破防时主动出招，观测者预判时切换行动种类。', attackPercent: 90 },
  { title: '王庭废墟·王座祭司', primary: Intent.Wound, secondary: Intent.ReadPattern,
    hint: '目标：避开王座抑疗安排恢复，防止回血习惯被祭司读招。', attackPercent: 80 },
  { title: '天顶之门·远征裁决者', primary: Intent.ReadPattern, secondary: Intent.Shatter,
    hint: '目标：应对裁决者的读招、破防与低血蓄力，改变行动争取突围窗口。', attackPercent: 100 },
];

// Ordinary enemies per chapter, in signature-tactic order: Charge, Wound, Shatter, ReadPattern.
export const OrdinaryNames = [
  ['警钟哨兵', '荆棘野狼', '碎盾山贼', '鹰眼游猎者'],
  ['树心侍祭', '毒藤潜伏者', '朽木巨蛛', '林影窥伺者'],
  ['禁咒书灵', '墨潮水妖', '石化书卫', '记忆抄写者'],
  ['引爆工头', '酸蚀蠕虫', '碎岩傀儡', '矿灯窥探者'],
  ['冰晶术士', '霜咬雪狼', '冻甲巨熊', '白雾追踪者'],
  ['熔炉卫兵', '灼烧监工', '锻锤机兵', '齿轮测算者'],
  ['镜阵舞者', '伤痕小丑', '碎镜演员', '镜中模仿者'],
  ['观星祭司', '星尘噬魂者', '陨铁守卫', '星图推演者'],
  ['封印骑士', '王座刽子手', '断剑卫士', '宫廷谋士'],
  ['回路守门人', '裁决之刃', '破界巨像', '天顶预言者'],
];
export const TacticRoles = ['蓄力守卫', '抑疗猎手', '破阵斥候', '读招追猎者'];
const TacticHints = [
  '会蓄力并在下回合重击；普攻即可打断（被闪避也算），换来一回合休整。',
  '在你治疗后压制治疗效果；先攻击或格挡，等离开该意图再回血。蓄力时普攻可打断。',
  '在你格挡后发动无视格挡的破防攻击，此时主动出手更划算。蓄力时普攻可打断。',
  '记录你的出招习惯并预判重复行动，改变行动可让预判落空。蓄力时普攻可打断。',
];
// The Charge hint for pressured enemies, which do not rest after an interrupt.
const PressuredChargeHint = '会蓄力并在下回合重击；普攻即可打断（被闪避也算），但它被打断后不休整，下回合仍会行动。';

export const bossAttackPercent = (floor) => BossProfiles[chapterOf(floor)].attackPercent;

export const Bestiary = {
  EntriesPerChapter: 5,
  EntryCount: 50,
  entryForFloor(floor) {
    const f = floorOf(floor);
    const slot = f % 10 === 0 ? 4 : f % 4;
    return chapterOf(f) * 5 + slot;
  },
  entryChapter(entry) { return Math.trunc(clamp(entry, 0, 49) / 5); },
  entryIsBoss(entry) { return clamp(entry, 0, 49) % 5 === 4; },
  entryName(entry) {
    entry = clamp(entry, 0, 49);
    if (Bestiary.entryIsBoss(entry)) return BossProfiles[Bestiary.entryChapter(entry)].title;
    return OrdinaryNames[Bestiary.entryChapter(entry)][entry % 5];
  },
  entryDescription(entry) {
    entry = clamp(entry, 0, 49);
    if (Bestiary.entryIsBoss(entry)) return BossProfiles[Bestiary.entryChapter(entry)].hint;
    const slot = entry % 5;
    return `${TacticRoles[slot]}：${TacticHints[slot]}`;
  },
};
export const enemyNameForFloor = (floor) => Bestiary.entryName(Bestiary.entryForFloor(floor));

export function actionDescription(actionId) {
  if (actionId === 0) return '普通攻击';
  if (actionId === -1) return '格挡';
  if (actionId === -2) return '使用药水';
  if (actionId >= 1 && actionId <= 5) return `技能槽 ${actionId}`;
  return '尚未识别的行动';
}

export class Enemy extends Character {
  constructor(name, hp, attack, mp, expReward, coinReward, level) {
    super(name, hp, attack, mp);
    this.exp_reward = expReward;
    this.coin_reward = coinReward;
    this.level = level;
    this.dodgeRate = 10;
    this.affixes = [];
    this.encounterFloor = 0;
    this.bossEncounter = false;
    this.baseAttack = attack;
    this.resetTactics();
  }
  get side() { return 'enemy'; }

  resetTactics() {
    this.completedTurns = 0;
    this.lastAction = -99;
    this.actionTransitions = Array.from({ length: 8 }, () => new Array(8).fill(0));
    this.attackWeakenPercent = 0;
    this.attackWeakenTurns = 0;
    this.tacticalPressure = 0;
    this.specialCooldown = 0;
    this.turnsSinceSpecial = 0;
    this.repeatedActions = 0;
    this.recoveryTurns = 0;
    this.chargePending = false;
    this.interruptedThisTurn = false;
    this.lastPlayerHealed = false;
    this.desperationUsed = false;
    this.objectivePressure = false;
    this.sabotaged = false;
  }

  getexp_reward() { return this.exp_reward; }
  getcoin_reward() { return this.coin_reward; }
  getlevel() { return this.level; }
  getdodgeRate() { return this.dodgeRate; }
  setdodgeRate(rate) { this.dodgeRate = clamp(rate, 0, 100); }

  takedamage(damage) {
    const dodgeChance = below(100);
    if (dodgeChance < this.dodgeRate) {
      emit('dodge', `${this.getname()}闪避了攻击！`, { side: 'enemy' });
      return;
    }
    super.takedamage(damage);
  }

  resethp(amount) { this.hp = Math.max(0, amount); this.max_hp = this.hp; }
  resetexp_reward(amount) { this.exp_reward = amount; }
  resetcoin_reward(amount) { this.coin_reward = amount; }
  resetattack(amount) {
    this.baseAttack = Math.max(0, amount);
    this.attackWeakenPercent = 0;
    this.attackWeakenTurns = 0;
    this.attack = this.baseAttack;
  }

  getRandomAffix() {
    const r = below(100);
    if (r < 25) return Affix.STRONG;
    if (r < 45) return Affix.TOUGH;
    if (r < 60) return Affix.QUICK;
    if (r < 73) return Affix.POISONOUS;
    if (r < 85) return Affix.HEALING;
    if (r < 93) return Affix.EXPLOSIVE;
    return Affix.SHIELDING;
  }
  setAffix(floor) {
    let chances = 0;
    if (floor >= 1 && floor < 10) chances = 1;
    if (floor >= 10 && floor < 20) chances = 2;
    if (floor >= 20 && floor < 40) chances = 3;
    if (floor >= 40 && floor < 60) chances = 4;
    if (floor >= 60 && floor < 80) chances = 5;
    if (floor >= 80 && floor <= 100) chances = 6;
    for (let i = 0; i < chances; i++) {
      const threshold = Math.min(40 + Math.trunc(floor / 5), 70);
      if (below(100) < threshold) this.addAffix(this.getRandomAffix());
    }
  }
  addAffix(affix) {
    if (this.hasAffix(affix)) return false;
    this.affixes.push(affix);
    const bonusLevel = Math.max(0, this.getlevel());
    this.exp_reward = clamp(this.exp_reward + bonusLevel * 3, 0, IntMax);
    this.coin_reward = clamp(this.coin_reward + bonusLevel * 2, 0, IntMax);
    return true;
  }
  ensureAffixes(count) {
    count = clamp(count, 0, AffixCount);
    for (let roll = 0; roll < 1000 && this.affixes.length < count; ++roll)
      this.addAffix(this.getRandomAffix());
  }
  hasAffix(affix) { return this.affixes.includes(affix); }
  getAffixes() { return this.affixes; }
  removeAffix(affix) { this.affixes = this.affixes.filter((a) => a !== affix); }
  heal(amount) {
    if (amount <= 0 || !this.isalive()) return;
    this.hp = Math.min(this.hp + amount, this.max_hp);
  }

  configureEncounter(floor, boss) {
    this.encounterFloor = Math.max(0, floor);
    this.bossEncounter = boss;
    this.resetTactics();
    this.attack = this.baseAttack;
  }
  isBoss() { return this.bossEncounter; }
  // 0..49 bestiary entry, or -1 for an unconfigured enemy.
  bestiaryEntry() { return this.encounterFloor > 0 ? Bestiary.entryForFloor(this.encounterFloor) : -1; }

  getEncounterTitle() {
    if (this.bossEncounter) return BossProfiles[chapterOf(this.encounterFloor)].title;
    return TacticRoles[Math.max(0, this.encounterFloor) % 4];
  }
  getTacticalHint() {
    if (this.bossEncounter) return BossProfiles[chapterOf(this.encounterFloor)].hint;
    if (this.encounterFloor > 0) {
      const slot = this.encounterFloor % 4;
      if (slot === 0 && !this.interruptGrantsRecovery()) return PressuredChargeHint;
      return TacticHints[slot];
    }
    return '观察早期意图：普攻可打断蓄力；格挡能抗重击，换招能避开预判。';
  }
  primaryTactic() {
    if (this.bossEncounter) return BossProfiles[chapterOf(this.encounterFloor)].primary;
    return [Intent.Charge, Intent.Wound, Intent.Shatter, Intent.ReadPattern][Math.max(0, this.encounterFloor) % 4];
  }
  secondaryTactic() {
    return this.bossEncounter ? BossProfiles[chapterOf(this.encounterFloor)].secondary : Intent.Charge;
  }

  predictAction() {
    if (this.lastAction < -2 || this.lastAction > 5) return -99;
    const transitions = this.actionTransitions[this.lastAction + 2];
    let bestCount = 0;
    let prediction = -99;
    for (let i = 0; i < transitions.length; ++i) {
      if (transitions[i] > bestCount) {
        bestCount = transitions[i];
        prediction = i - 2;
      }
    }
    return prediction;
  }

  makeIntent(kind) {
    const intent = { kind, damagePercent: 100, healingPercent: 100, piercesGuard: false,
      predictedAction: -99, interruptible: false };
    switch (kind) {
      case Intent.Charge:
        intent.damagePercent = 0;
        intent.interruptible = true;
        break;
      case Intent.HeavyStrike:
        intent.damagePercent = this.bossEncounter
          ? (this.sabotaged ? IntentTuning.SabotagedBossHeavyStrikePercent : IntentTuning.BossHeavyStrikePercent)
          : (this.sabotaged ? IntentTuning.SabotagedHeavyStrikePercent : IntentTuning.HeavyStrikePercent);
        break;
      case Intent.Wound:
        intent.damagePercent = IntentTuning.WoundPercent;
        intent.healingPercent = IntentTuning.WoundHealingPercent;
        break;
      case Intent.Shatter:
        intent.damagePercent = IntentTuning.ShatterPercent;
        intent.piercesGuard = true;
        break;
      case Intent.ReadPattern:
        intent.damagePercent = IntentTuning.ReadMissPercent;
        intent.predictedAction = this.predictAction();
        break;
      case Intent.Recover:
        intent.damagePercent = 0;
        break;
      default:
        break;
    }
    return intent;
  }

  getIntent() {
    if (this.encounterFloor === 0) return this.makeIntent(Intent.Attack);
    if (this.recoveryTurns > 0) return this.makeIntent(Intent.Recover);
    if (this.chargePending) return this.makeIntent(Intent.HeavyStrike);
    if (this.objectivePressure && this.specialCooldown === 0) return this.makeIntent(Intent.Charge);
    const primary = this.primaryTactic();
    const secondary = this.secondaryTactic();
    const supports = (kind) => primary === kind || secondary === kind ||
      (this.bossEncounter && chapterOf(this.encounterFloor) === 9);
    if (this.specialCooldown === 0) {
      if (this.lastPlayerHealed && supports(Intent.Wound)) return this.makeIntent(Intent.Wound);
      if (this.lastAction === -1 && supports(Intent.Shatter)) return this.makeIntent(Intent.Shatter);
      if (supports(Intent.ReadPattern) && this.predictAction() !== -99 &&
        (this.repeatedActions >= 2 || this.completedTurns >= 4))
        return this.makeIntent(Intent.ReadPattern);
      if (!this.desperationUsed && this.hp * 100 <= this.max_hp * 35) return this.makeIntent(Intent.Charge);
      if ((this.bossEncounter && this.completedTurns === 0) ||
        (!this.bossEncounter && this.completedTurns === 1) ||
        this.turnsSinceSpecial >= Math.max(2, 4 - this.tacticalPressure)) {
        let tactic = primary;
        if (tactic === Intent.ReadPattern && this.predictAction() === -99) tactic = Intent.Charge;
        return this.makeIntent(tactic);
      }
    }
    return this.makeIntent(Intent.Attack);
  }

  // The terminal's intent line, reused by the battle UI.
  describeIntent(intent = this.getIntent()) {
    switch (intent.kind) {
      case Intent.Attack:
        return '普通攻击：造成 100% 攻击伤害，可格挡减半。';
      case Intent.Charge: {
        let text = (this.interruptGrantsRecovery()
          ? '蓄力：本回合不攻击；普通攻击即可打断（被闪避也算），敌人随后休整一回合。'
          : '蓄力：本回合不攻击；普通攻击即可打断（被闪避也算），但该敌人被打断后不休整。') + '未打断则下回合重击（' +
          `${this.makeIntent(Intent.HeavyStrike).damagePercent}%），可格挡减半。`;
        if (this.hasAffix(Affix.HEALING))
          text += `【治疗】蓄力完成会回复 ${IntentTuning.HealingAffixPercent}% 最大生命。`;
        return text;
      }
      case Intent.HeavyStrike:
        return `蓄力重击：造成 ${intent.damagePercent}% 攻击伤害，可格挡减半。`;
      case Intent.Wound:
        return `压制治疗：本次行动的治疗只生效 ${intent.healingPercent}%，随后造成 ${intent.damagePercent}% 攻击伤害；这回合先别回血。`;
      case Intent.Shatter:
        return `破防攻击：造成 ${intent.damagePercent}% 攻击伤害，无视格挡减伤；减攻技能仍然有效，主动进攻比格挡划算。`;
      case Intent.ReadPattern:
        if (intent.predictedAction === -99)
          return `观察连招：经验不足，尚无法预测；本回合仅造成 ${intent.damagePercent}% 攻击伤害。`;
        return `预判连招：预计你使用【${actionDescription(intent.predictedAction)}】；命中预判造成 ${IntentTuning.ReadHitPercent}% 攻击伤害，换一种行动只受 ${intent.damagePercent}%。`;
      case Intent.Recover:
        return '休整：蓄力被打断，本回合不攻击，适合回血或爆发。';
      default:
        return '';
    }
  }

  interruptCharge() {
    if (this.getIntent().kind !== Intent.Charge) return false;
    this.chargePending = false;
    this.recoveryTurns = this.interruptGrantsRecovery() ? 1 : 0;
    this.interruptedThisTurn = true;
    this.specialCooldown = Math.max(1, 3 - this.tacticalPressure) + (this.sabotaged ? 1 : 0);
    return true;
  }
  setObjectivePressure(enabled) { this.objectivePressure = enabled; }
  // Pressured enemies (challenges, elites) shake off an interrupt at once.
  // Every text that promises the rest reads this so it never contradicts the rule.
  interruptGrantsRecovery() { return this.tacticalPressure === 0; }
  setTacticalPressure(difficulty) { this.tacticalPressure = clamp(difficulty, 0, 2); }
  applySabotage() {
    this.removeAffix(Affix.SHIELDING);
    this.sabotaged = true;
  }

  // 0=attack, 1..5=skill slots, -1=guard, -2=potion. Invalid ids do nothing.
  finishTurn(actionId, healed = false, resolved = this.getIntent().kind) {
    if (actionId < -2 || actionId > 5) return;
    if (this.interruptedThisTurn) {
      this.interruptedThisTurn = false;
      this.turnsSinceSpecial = 0;
    } else if (resolved === Intent.Charge) {
      this.chargePending = true;
      this.turnsSinceSpecial = 0;
    } else if (resolved === Intent.HeavyStrike) {
      this.chargePending = false;
      this.specialCooldown = Math.max(1, 3 - this.tacticalPressure) + (this.sabotaged ? 1 : 0);
      this.turnsSinceSpecial = 0;
    } else if (resolved === Intent.Wound || resolved === Intent.Shatter || resolved === Intent.ReadPattern) {
      this.specialCooldown = Math.max(1, 3 - this.tacticalPressure) + (this.sabotaged ? 1 : 0);
      this.turnsSinceSpecial = 0;
    } else {
      if (resolved === Intent.Recover && this.recoveryTurns > 0) --this.recoveryTurns;
      if (this.specialCooldown > 0) --this.specialCooldown;
      ++this.turnsSinceSpecial;
    }
    if (resolved !== Intent.Attack && this.hp * 100 <= this.max_hp * 35) this.desperationUsed = true;
    if (this.lastAction >= -2 && this.lastAction <= 5)
      ++this.actionTransitions[this.lastAction + 2][actionId + 2];
    this.repeatedActions = actionId === this.lastAction ? this.repeatedActions + 1 : 1;
    this.lastAction = actionId;
    this.lastPlayerHealed = healed;
    ++this.completedTurns;
    if (this.attackWeakenTurns > 0) {
      --this.attackWeakenTurns;
      if (this.attackWeakenTurns === 0) this.attackWeakenPercent = 0;
      this.updateWeakenedAttack();
    }
  }

  updateWeakenedAttack() {
    this.attack = this.baseAttack === 0 ? 0
      : Math.max(1, Math.trunc(this.baseAttack * (100 - this.attackWeakenPercent) / 100));
  }
  weakenAttack(percent, turns) {
    if (percent <= 0 || turns <= 0) return;
    this.attackWeakenPercent = Math.max(this.attackWeakenPercent, Math.min(percent, 60));
    this.attackWeakenTurns = Math.max(this.attackWeakenTurns, turns);
    this.updateWeakenedAttack();
    emit('weaken', `${this.getname()}攻击降低 ${this.attackWeakenPercent}%，持续 ${this.attackWeakenTurns} 次行动。`,
      { side: 'enemy', percent: this.attackWeakenPercent, turns: this.attackWeakenTurns });
  }
}
