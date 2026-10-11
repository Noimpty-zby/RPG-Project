// Port of src/player/skills.{h,cpp}: 15 skills, linear rank growth and the
// mutually exclusive specializations of the first two skills.
import { clamp, idiv, MaxCounter, skillRankCap, skillRankFloor, skillUnlockFloor, IntMax } from './config.js';
import { Affix } from './enemy.js';
import { ItemEffect } from './item.js';
import { Job } from './jobs.js';
import { below } from './random.js';
import { emit } from './log.js';

const percent = (value, pct) => clamp(idiv(value * pct, 100), 0, MaxCounter);

function shielded(raw, target) {
  if (!target.hasAffix(Affix.SHIELDING)) return raw;
  const absorbed = Math.min(8 + target.getlevel() * 6, idiv(raw, 2));
  return Math.max(0, raw - absorbed);
}
const controlDuration = (hero, turns) => turns + Math.min(1, hero.equipmentEffect(ItemEffect.ControlFocus));

function beginCast(ability, hero, target) {
  if (!target.isalive() || !ability.canUse(hero)) return false;
  const cost = ability.getActualCost(hero);
  hero.setmp(hero.getmp() - cost);
  ability.resetcooldown();
  emit('cast', `施放【${ability.getskillname()}】，消耗 ${cost} MP。`,
    { side: 'hero', job: hero.job(), slot: ability.getskillid(), name: ability.getskillname(),
      branch: ability.getSpecialization(), cost });
  return true;
}

function directDamage(ability, hero) {
  const base = ability.getScaledDamage(hero);
  const branch = ability.getSpecialization();
  const slot = ability.getskillid();
  const resource = hero.getCombatResource();
  if (hero.job() === Job.Warrior) {
    if (slot === 1) return percent(base, branch === 1 ? 80 + 35 * resource : branch === 2 ? 140 : 100 + 15 * resource);
    if (slot === 2) return percent(base, branch === 1 ? 65 : branch === 2 ? 130 : 100);
    if (slot === 3) return Math.max(5, base + hero.getlevel() * 3);
    if (slot === 4) {
      const hpPercent = idiv(hero.gethp() * 100, Math.max(1, hero.getmaxhp()));
      return percent(base, hpPercent > 60 ? 100 : hpPercent >= 30 ? 180 : 300);
    }
  } else if (hero.job() === Job.Mage) {
    if (slot === 1) return percent(base, branch === 1 ? 120 + 30 * resource : branch === 2 ? 95 : 135 + 15 * resource);
    if (slot === 2) return percent(base, branch === 1 ? 70 : branch === 2 ? 155 : 115);
    if (slot === 3) return base + hero.getlevel() * 2;
    if (slot === 4) return percent(ability.getActualCost(hero), 300 + 75 * (ability.getEffectiveRank() - 1));
    if (slot === 5) return percent(base, 150);
  } else if (hero.job() === Job.Assassin) {
    if (slot === 1) return percent(base, branch === 1 ? 80 : branch === 2 ? 125 : 100);
    if (slot === 2) return percent(base, branch === 1 ? 120 + 40 * resource : branch === 2 ? 120 : 160 + 15 * resource);
  }
  return base;
}

function strike(ability, target, damage, pierce = false, hit = 0) {
  const amount = pierce ? damage : shielded(damage, target);
  emit('strike', `【${ability.getskillname()}】预计命中伤害 ${amount}。`,
    { side: 'enemy', name: ability.getskillname(), amount, pierce, hit });
  target.takedamage(amount);
}

// Rank-dependent effects, shared by the skills and their previews.
const finisherHeal = (ability) => 10 * ability.getEffectiveRank();
const surgeMana = (ability) => 20 + (ability.getEffectiveRank() - 1) * 3;
const lifestealPercent = (ability) => 40 + (ability.getEffectiveRank() - 1) * 3;
const lifestealCap = (hero) => percent(hero.getRecoveryBaseHp(), 10);
// The number a preview leads with: per-hit damage for the combo, the flat
// bonus for the execute, otherwise the expected direct damage.
function headlineDamage(ability, hero) {
  if (hero.job() === Job.Assassin && ability.getskillid() === 3) return percent(ability.getScaledDamage(hero), 55);
  if (hero.job() === Job.Assassin && ability.getskillid() === 5) return ability.getdamage();
  return directDamage(ability, hero);
}

const SpecializationNames = [
  [['反击', '重斩'], ['坚守', '振奋']],
  [['蓄焰', '火花'], ['深冻', '碎冰']],
  [['开隙', '穿甲'], ['伏击', '扰乱']],
];
const SpecializationOptions = [
  ['1反击：伤害80%+每层战意35%；2重斩：伤害140%，多耗3MP，不消耗战意',
    '1坚守：伤害65%，恢复基础HP25%，多耗4MP；2振奋：伤害130%，不回血，命中回6MP'],
  ['1蓄焰：伤害120%+每层专注30%，多耗3MP；2火花：伤害95%，少耗4MP，积累专注',
    '1深冻：伤害70%，减攻40%持续3行动；2碎冰：伤害155%，无减攻，命中回6MP'],
  ['1开隙：伤害80%，命中增加2层破绽；2穿甲：伤害125%，多耗3MP，穿盾且不积破绽',
    '1伏击：伤害120%+每层破绽40%；2扰乱：伤害120%，命中回8MP，减攻15%一次行动'],
];
// Per-branch summaries for the specialization picker.
export const SpecializationBranches = SpecializationOptions.map((job) =>
  job.map((line) => line.split('；').map((part) => part.replace(/^\d/, ''))));

export class Skill {
  constructor(job, skillid, levelrequired, costmp, cooldown, damage, level, skillname, use) {
    this.jobType = job;
    this.skillid = skillid;
    this.levelrequired = levelrequired;
    this.level = Math.max(1, level);
    this.costmp = costmp;
    this.cooldown = cooldown;
    this.currentcooldown = 0;
    this.damage = damage;
    this.basecostmp = costmp;
    this.basedamage = damage;
    this.specialization = 0;
    this.skillname = skillname;
    this.useImpl = use;
  }
  getskillid() { return this.skillid; }
  getlevelrequired() { return this.levelrequired; }
  getunlockfloor() { return skillUnlockFloor(this.skillid); }
  getcostmp() { return this.costmp; }
  getcooldown() { return this.cooldown; }
  getcurrentcooldown() { return this.currentcooldown; }
  getdamage() { return this.damage; }
  getEffectiveRank() { return clamp(this.level, 1, 5); }
  getScaledDamage(hero) { return clamp(this.damage + idiv(hero.getattack() * 3, 4), 0, MaxCounter); }
  getActualCost(hero) {
    if (hero.job() === Job.Mage && this.skillid === 4) return Math.max(10, idiv(hero.getmp(), 2));
    let cost = this.costmp;
    if (cost === 0) return 0;
    if (this.specialization === 1 && this.skillid === 2 && hero.job() === Job.Warrior) cost += 4;
    if (this.specialization === 2 && this.skillid === 1) cost += hero.job() === Job.Mage ? -4 : 3;
    if (this.specialization === 1 && this.skillid === 1 && hero.job() === Job.Mage) cost += 3;
    const efficiency = 100 - 10 * hero.equipmentEffect(ItemEffect.SkillEfficiency);
    return Math.max(1, idiv(Math.max(1, cost) * efficiency + 99, 100));
  }
  getskilllevel() { return this.level; }
  getskillname() { return this.skillname; }
  getSpecialization() { return this.specialization; }
  setSpecialization(value) {
    if (value < 0 || value > 2 || (value !== 0 && (this.skillid < 1 || this.skillid > 2))) return false;
    this.specialization = value;
    return true;
  }
  static specializationFloor(slot) { return slot === 1 ? 10 : 30; }
  getlevelUpCost() {
    const rank = clamp(this.level, 1, 3784);
    return Math.min(150 * rank * rank, IntMax);
  }
  reducecooldown() { if (this.currentcooldown > 0) --this.currentcooldown; }
  resetcooldown() { this.currentcooldown = this.getcooldown(); }
  resetcurrentcooltime() { this.currentcooldown = 0; }
  restoreProgress(rank, remainingCooldown) {
    this.level = Math.max(1, rank);
    const effective = this.getEffectiveRank();
    this.damage = this.basedamage + (effective - 1) * 15;
    this.costmp = this.basecostmp === 0 ? 0 : this.basecostmp + (effective - 1) * 3;
    this.currentcooldown = clamp(remainingCooldown, 0, this.cooldown);
  }
  canUse(hero) {
    return hero.isalive() && hero.getlevel() >= this.levelrequired && hero.getHighestFloor() >= this.getunlockfloor() &&
      hero.getmp() >= this.getActualCost(hero) && this.currentcooldown === 0;
  }
  // Why canUse is false, for the battle menu.
  blockedReason(hero) {
    if (hero.getlevel() < this.levelrequired || hero.getHighestFloor() < this.getunlockfloor()) return '未解锁';
    if (this.currentcooldown > 0) return `冷却 ${this.currentcooldown}`;
    if (hero.getmp() < this.getActualCost(hero)) return 'MP不足';
    return '';
  }
  isUnlocked(hero) {
    return hero.getlevel() >= this.levelrequired && hero.getHighestFloor() >= this.getunlockfloor();
  }
  canUpgrade(playerLevel, highestFloor) {
    return highestFloor >= this.getunlockfloor() && this.level < skillRankCap(highestFloor) &&
      playerLevel >= Math.max(this.levelrequired, this.level + 1);
  }
  levelUpSkill(playerLevel, coins, highestFloor) {
    if (!this.canUpgrade(playerLevel, highestFloor) || coins < this.getlevelUpCost()) return false;
    this.restoreProgress(this.level + 1, this.currentcooldown);
    emit('skillRank', `【技能进阶】${this.skillname} → ${this.level}阶。`, { name: this.skillname, level: this.level });
    return true;
  }
  getSpecializationName(hero) {
    if (this.specialization === 0) return '基础';
    return SpecializationNames[hero.job()][clamp(this.skillid, 1, 2) - 1][this.specialization - 1];
  }
  getSpecializationOptions(hero) {
    if (this.skillid < 1 || this.skillid > 2) return '';
    return SpecializationOptions[hero.job()][this.skillid - 1];
  }
  getPreview(hero) {
    let out = '';
    const amount = directDamage(this, hero);
    const id = this.skillid;
    const branch = this.specialization;
    if (hero.job() === Job.Assassin && id === 3)
      out += `2—4次连击，每击 ${headlineDamage(this, hero)}；后续击穿盾`;
    else if (hero.job() === Job.Assassin && id === 5)
      out += `无视护盾：当前敌HP的普通35%/Boss15% + ${this.getdamage()}`;
    else out += `当前预计伤害 ${amount}（未计敌方减免/闪避）`;
    if (hero.job() === Job.Warrior) {
      if (id === 1 && branch !== 2) out += '；消耗战意，最多2层';
      if (id === 2) {
        if (branch === 2) out += '；不回血，命中恢复6MP';
        else out += `；基础生命恢复 ${percent(hero.getRecoveryBaseHp(), branch === 1 ? 25 : 15)}`;
      }
      if (id === 3) out += `；无视护盾；敌攻击-35%，持续 ${controlDuration(hero, 2)} 次行动`;
      if (id === 4) out += '；生命比例>60%/30—60%/<30%对应1/1.8/3倍';
      if (id === 5) out += `；恢复 ${finisherHeal(this)} HP`;
    } else if (hero.job() === Job.Mage) {
      if (id === 1 && branch !== 2) out += '；消耗专注，最多2层';
      if (id === 1 && branch === 2) out += '；保留并增加1层专注';
      if (id === 2 && branch !== 2)
        out += `；敌攻击-${branch === 1 ? '40%' : '30%（强力敌45%）'}，持续 ${controlDuration(hero, branch === 1 ? 3 : 2)} 次行动；命中回4MP`;
      if (id === 2 && branch === 2) out += '；放弃减攻，命中回6MP';
      if (id === 3) out += `；无视护盾，恢复 ${surgeMana(this)} MP`;
      if (id === 4) out += '；实际消耗当前MP一半，至少10MP';
      if (id === 5) out += '；命中后敌生命<30%时追加其当前生命30%伤害';
    } else {
      if (id === 1) out += branch === 2 ? '；无视护盾' : branch === 1 ? '；命中增加2层破绽' : '；命中增加1层破绽';
      if (id === 2 && branch !== 2) out += '；消耗破绽，最多2层';
      if (id === 2 && branch === 2) out += '；命中回8MP并减攻15%一次行动';
      if (id === 4) out += `；吸取实际伤害的 ${lifestealPercent(this)}%（最多基础生命10%，即 ${lifestealCap(hero)} HP）`;
    }
    return out;
  }
  // Current versus next rank at the hero's current stats, or why it cannot rise.
  getUpgradePreview(hero) {
    if (this.level >= 5) return '已满阶（5阶），无法继续升阶。';
    const next = Object.assign(Object.create(Object.getPrototypeOf(this)), this);
    next.restoreProgress(this.level + 1, this.currentcooldown);
    const assassin = hero.job() === Job.Assassin;
    const id = this.skillid;
    let out = `升至${this.level + 1}阶（${this.getlevelUpCost()}金币）：` +
      `${assassin && id === 3 ? '每击伤害 ' : assassin && id === 5 ? '固定加成 ' : '伤害 '}` +
      `${headlineDamage(this, hero)}→${headlineDamage(next, hero)}，MP ${this.getActualCost(hero)}→${next.getActualCost(hero)}，` +
      `冷却 ${this.cooldown} 不变`;
    if (hero.job() === Job.Warrior && id === 5) out += `，恢复 ${finisherHeal(this)}→${finisherHeal(next)} HP`;
    if (hero.job() === Job.Mage && id === 3) out += `，回复 ${surgeMana(this)}→${surgeMana(next)} MP`;
    if (assassin && id === 4) {
      const drain = (rank) => Math.min(percent(headlineDamage(rank, hero), lifestealPercent(rank)), lifestealCap(hero));
      out += `，吸血比例 ${lifestealPercent(this)}%→${lifestealPercent(next)}%；按当前属性预计回复 ` +
        `${drain(this)}→${drain(next)} HP（上限 ${lifestealCap(hero)}）`;
      if (drain(next) === drain(this)) out += '，已触及上限，升阶不增加实际治疗';
    }
    if (hero.getlevel() < this.levelrequired || hero.getHighestFloor() < this.getunlockfloor()) out += '。技能尚未解锁';
    else if (this.level >= skillRankCap(hero.getHighestFloor()))
      out += `。当前阶数上限 ${this.level}，到达第${skillRankFloor(this.level + 1)}层后可升`;
    else if (hero.getlevel() < this.level + 1) out += `。需要角色 Lv.${this.level + 1}`;
    return `${out}。`;
  }
  useSkill(hero, target) { return this.useImpl(this, hero, target); }
}

const Definitions = {
  [Job.Warrior]: [
    [1, 1, 6, 2, 30, '猛击', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const damage = directDamage(s, hero);
      if (s.getSpecialization() !== 2) hero.consumeCombatResource();
      strike(s, target, damage);
      return true;
    }],
    [2, 2, 14, 3, 55, '战吼斩', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const before = target.gethp();
      strike(s, target, directDamage(s, hero));
      if (s.getSpecialization() === 2) {
        if (target.gethp() < before) hero.gainMana(6);
      } else hero.heal(percent(hero.getRecoveryBaseHp(), s.getSpecialization() === 1 ? 25 : 15));
      return true;
    }],
    [3, 4, 18, 4, 0, '盾反', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      target.weakenAttack(35, controlDuration(hero, 2));
      strike(s, target, directDamage(s, hero), true);
      return true;
    }],
    [4, 6, 22, 4, 80, '血怒', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      strike(s, target, directDamage(s, hero));
      return true;
    }],
    [5, 9, 35, 6, 180, '无双斩', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      strike(s, target, directDamage(s, hero));
      hero.heal(finisherHeal(s));
      return true;
    }],
  ],
  [Job.Mage]: [
    [1, 1, 10, 2, 40, '火球术', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const damage = directDamage(s, hero);
      if (s.getSpecialization() === 2) hero.addCombatResource(1);
      else hero.consumeCombatResource();
      strike(s, target, damage);
      return true;
    }],
    [2, 2, 18, 3, 70, '冰霜新星', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const before = target.gethp();
      strike(s, target, directDamage(s, hero));
      if (s.getSpecialization() !== 2) {
        const freeze = s.getSpecialization() === 1 ? 40 : (target.hasAffix(Affix.STRONG) ? 45 : 30);
        target.weakenAttack(freeze, controlDuration(hero, s.getSpecialization() === 1 ? 3 : 2));
      }
      if (target.gethp() < before) hero.gainMana(s.getSpecialization() === 2 ? 6 : 4);
      return true;
    }],
    [3, 4, 0, 3, 20, '毒云术', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      strike(s, target, directDamage(s, hero), true);
      hero.gainMana(surgeMana(s));
      return true;
    }],
    [4, 6, 0, 5, 0, '奥术爆发', (s, hero, target) => {
      const damage = directDamage(s, hero); // Dynamic cost must be captured before payment.
      if (!beginCast(s, hero, target)) return false;
      strike(s, target, damage);
      return true;
    }],
    [5, 9, 50, 6, 200, '流星陨落', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const hpBefore = target.gethp();
      strike(s, target, directDamage(s, hero));
      if (target.isalive() && target.gethp() < hpBefore && target.gethp() * 100 < target.getmaxhp() * 30) {
        emit('followUp', '流星余烬追击！', { side: 'enemy', name: s.getskillname() });
        target.takedamage(percent(target.gethp(), 30));
      }
      return true;
    }],
  ],
  [Job.Assassin]: [
    [1, 1, 8, 2, 35, '穿刺', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const before = target.gethp();
      strike(s, target, directDamage(s, hero), s.getSpecialization() === 2);
      if (s.getSpecialization() !== 2 && target.gethp() < before)
        hero.addCombatResource(s.getSpecialization() === 1 ? 2 : 1);
      return true;
    }],
    [2, 2, 16, 3, 60, '影袭', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const damage = directDamage(s, hero);
      if (s.getSpecialization() !== 2) hero.consumeCombatResource();
      const before = target.gethp();
      const absorbed = target.hasAffix(Affix.SHIELDING) ? Math.min(4 + target.getlevel() * 3, idiv(damage, 4)) : 0;
      strike(s, target, damage - absorbed, true);
      if (s.getSpecialization() === 2 && target.gethp() < before) {
        hero.gainMana(8);
        target.weakenAttack(15, 1);
      }
      return true;
    }],
    [3, 4, 24, 4, 45, '连环刺', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const hits = 2 + below(3);
      const each = percent(s.getScaledDamage(hero), 55);
      for (let hit = 0; hit < hits && target.isalive(); ++hit) strike(s, target, each, hit !== 0, hit);
      return true;
    }],
    [4, 6, 28, 4, 90, '吸血刃', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const before = target.gethp();
      strike(s, target, directDamage(s, hero));
      const actual = Math.max(0, before - target.gethp());
      const drain = Math.min(percent(actual, lifestealPercent(s)), lifestealCap(hero));
      hero.heal(drain);
      return true;
    }],
    [5, 9, 40, 6, 50, '瞬杀', (s, hero, target) => {
      if (!beginCast(s, hero, target)) return false;
      const amount = percent(target.gethp(), target.isBoss() ? 15 : 35) + s.getdamage();
      strike(s, target, amount, true);
      return true;
    }],
  ],
};

export function createSkills(job) {
  return Definitions[job].map(([id, levelRequired, cost, cooldown, damage, name, use]) =>
    new Skill(job, id, levelRequired, cost, cooldown, damage, 1, name, use));
}
