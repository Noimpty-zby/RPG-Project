// Port of src/game/world.{h,cpp}: enemy stats, equipment generation and loot.
import { bossAttackPercent, enemyNameForFloor, Enemy, Affix } from './enemy.js';
import { equipmentTier, floorOf, idiv, MaxFloor, repeatRewardPercent } from './config.js';
import { Item, ItemEffect, ItemType, JobReq } from './item.js';
import { equipmentClass } from './jobs.js';
import { below } from './random.js';

export const EnemyTuning = {
  // The base curve every enemy scales from. Raised when floor exploration joined
  // the game, so a player who explores meets about the earlier difficulty.
  BaseHpPercent: 110,
  BaseAttackPercent: 115,
  OrdinaryHpPercent: 160,
  OrdinaryAttackPercent: 85,
  EliteHpPercent: 170,
  EliteAttackPercent: 115,
  SurviveHpPercent: 300,
  DisruptHpPercent: 400,
  ObjectiveAttackPercent: 80,
};

// Equipment names by class and quality (tier 0..3).
const W_WPN = [
  ['破旧铁剑', '精锻铁剑', '精钢巨剑', '神圣圣剑'],
  ['铁制战锤', '精钢战锤', '符文战锤', '暗金战锤'],
  ['缺口战斧', '精锻战斧', '暗铁战斧', '符文巨斧'],
  ['朽木长枪', '精钢战枪', '龙魂长枪', '神圣战枪'],
  ['生锈弯刀', '精钢弯刀', '血月弯刀', '神器弯刀'],
];
const W_ARM = [
  ['破旧链甲', '铁链甲', '精钢板甲', '秘银重甲'],
  ['木制圆盾', '铁皮盾牌', '精钢塔盾', '神盾'],
  ['皮革护腿', '铁制护腿', '精钢护腿', '龙鳞护腿'],
  ['旧铁头盔', '铁制头盔', '精钢战盔', '龙骨战盔'],
];
const M_WPN = [
  ['朽木法杖', '水晶法杖', '龙纹法杖', '混沌魔杖'],
  ['学徒魔杖', '元素魔杖', '奥术魔杖', '虚空魔杖'],
  ['破旧法典', '元素法典', '古代法典', '禁忌法典'],
  ['玻璃念珠', '水晶念珠', '符文念珠', '星辰念珠'],
];
const M_ARM = [
  ['粗布法袍', '学者长袍', '魔纹法衣', '星辰法衣'],
  ['布制帽子', '学者帽', '魔法高帽', '星辰法冠'],
  ['皮质腰带', '魔力腰带', '奥术腰带', '虚空腰带'],
];
const A_WPN = [
  ['锈铁短刀', '寒铁匕首', '幽影刺刀', '影刃'],
  ['木弓', '角弓', '精灵长弓', '龙骨弓'],
  ['毒涂匕首', '蛇毒短刃', '幽毒利刃', '死亡之刃'],
  ['铁制飞镖', '毒针飞镖', '幻影飞镖', '神器飞镖'],
];
const A_ARM = [
  ['破旧皮甲', '精皮甲', '影袭皮甲', '暗影战甲'],
  ['布制披风', '皮质披风', '暗影披风', '虚空披风'],
  ['皮手套', '利爪手套', '暗影手套', '死神手套'],
];
const C_ARM = [
  ['破旧护符', '精制护符', '圣光护符', '神器护符'],
  ['兽皮腰带', '皮腰带', '守护腰带', '圣战腰带'],
];
const POTIONS = ['小型治疗药水', '治疗药水', '优质治疗药水', '神圣圣水'];

// Icon family for an item name, used by the UI to pick a sprite.
export function itemIconKind(item) {
  if (item.getType() === ItemType.Potion) return 'potion';
  const name = item.getName();
  const families = [
    ['sword', /剑|弯刀/], ['hammer', /锤/], ['axe', /斧/], ['spear', /枪/], ['staff', /法杖|魔杖/],
    ['book', /法典/], ['beads', /念珠/], ['dagger', /短刀|匕首|刺刀|影刃|短刃|利刃|之刃/], ['bow', /弓/],
    ['dart', /飞镖/], ['shield', /盾/], ['helmet', /头盔|战盔/], ['legs', /护腿/], ['hat', /帽|法冠/],
    ['belt', /腰带/], ['robe', /法袍|长袍|法衣/], ['cloak', /披风/], ['glove', /手套/], ['amulet', /护符/],
  ];
  for (const [kind, pattern] of families) if (pattern.test(name)) return kind;
  return item.getType() === ItemType.Weapon ? 'sword' : 'armor';
}

function makeItem(floor, job, type, tier, maxLevel = 10000) {
  floor = floorOf(floor);
  if (type === ItemType.Potion)
    return new Item(POTIONS[tier], 1, ItemType.Potion, 30 + (tier + 1) * 15 + floor * 3,
      0, (tier + 1) * 20 + floor * 5, 1, JobReq.AnyJob, tier);
  if ((type !== ItemType.Weapon && type !== ItemType.Armor) || job < JobReq.AnyJob || job > JobReq.AssassinOnly ||
    (type === ItemType.Weapon && job === JobReq.AnyJob))
    throw new Error('目标装备类型或职业无效');
  let name;
  if (type === ItemType.Weapon) {
    if (job === JobReq.WarriorOnly) name = W_WPN[below(5)][tier];
    else if (job === JobReq.MageOnly) name = M_WPN[below(4)][tier];
    else name = A_WPN[below(4)][tier];
  } else if (job === JobReq.WarriorOnly) name = W_ARM[below(4)][tier];
  else if (job === JobReq.MageOnly) name = M_ARM[below(3)][tier];
  else if (job === JobReq.AssassinOnly) name = A_ARM[below(3)][tier];
  else name = C_ARM[below(2)][tier];
  const base = type === ItemType.Weapon ? 8 + idiv(floor * 11, 10) : 20 + floor * 3;
  const center = idiv(base * (100 + tier * 15), 100);
  const variation = idiv(center * 3, 100);
  const value = center + below(variation * 2 + 1) - variation;
  const durability = 10 + idiv(floor, 2) + tier * 10 + below(6);
  const price = (tier + 1) * 35 + floor * 10;
  const required = Math.min(Math.max(1, maxLevel), Math.max(1, idiv(floor, 2) - 2 + tier));
  const item = new Item(name, required, type, value, durability, price, 1, job, tier);
  if (tier >= 2) {
    const choices = [ItemEffect.SkillEfficiency];
    choices.push(type === ItemType.Weapon ? ItemEffect.ManaOnAttack : ItemEffect.GuardRecovery);
    if (job === JobReq.WarriorOnly || job === JobReq.MageOnly) choices.push(ItemEffect.ControlFocus);
    item.setEffect(choices[below(choices.length)]);
  }
  return item;
}

const usableTarget = (item, hero, target) =>
  item.getType() === target && item.getLevelRequirement() <= hero.getlevel() && hero.canWear(item);

export function generateEliteReward(hero, floor) {
  floor = floorOf(floor);
  const type = hero.runState().lootTarget === ItemType.Armor ? ItemType.Armor : ItemType.Weapon;
  return makeItem(floor, equipmentClass(hero.job()), type, equipmentTier(floor, 99), hero.getlevel());
}

export function generateTargetedItem(floor, job, type) {
  floor = floorOf(floor);
  const tier = equipmentTier(floor, below(100));
  return makeItem(floor, job, type, tier);
}

export function generateRandomItem(floor) {
  floor = floorOf(floor);
  const tier = equipmentTier(floor, below(100));
  const roll = below(100);
  const type = roll < 30 ? ItemType.Weapon : roll < 65 ? ItemType.Armor : ItemType.Potion;
  const jr = below(100);
  const job = type === ItemType.Weapon
    ? (jr < 35 ? JobReq.WarriorOnly : jr < 65 ? JobReq.MageOnly : JobReq.AssassinOnly)
    : (jr < 35 ? JobReq.WarriorOnly : jr < 60 ? JobReq.MageOnly : jr < 85 ? JobReq.AssassinOnly : JobReq.AnyJob);
  return makeItem(floor, job, type, tier);
}

export function generateLoot(hero, floor, firstClear) {
  if (floor < 1 || floor > MaxFloor) return null;
  const run = hero.runState();
  const target = run.lootTarget === ItemType.Armor ? ItemType.Armor : ItemType.Weapon;
  const threshold = target === ItemType.Weapon ? 6 : 8;
  let result = null;
  if (firstClear) run.pity = Math.min(threshold, run.pity + 1);
  if (firstClear && run.pity >= threshold) {
    const tier = equipmentTier(floor, below(100));
    result = makeItem(floor, equipmentClass(hero.job()), target, tier, hero.getlevel());
  } else {
    const chance = firstClear ? 60 : idiv(60 * repeatRewardPercent(Math.max(1, run.attempts[floor] - 1)), 100);
    if (below(100) < chance) {
      result = generateRandomItem(floor);
      if (result.getType() !== ItemType.Potion && below(100) < 75)
        result = makeItem(floor, equipmentClass(hero.job()), result.getType(), result.getTier(), hero.getlevel());
    }
  }
  if (result && firstClear && usableTarget(result, hero, target)) run.pity = 0;
  return result;
}

export function enemyStatsForFloor(value) {
  const floor = floorOf(value);
  const baseHp = idiv((40 + floor * 8) * EnemyTuning.BaseHpPercent, 100);
  const baseAttack = idiv((4 + idiv(floor * 3, 2)) * EnemyTuning.BaseAttackPercent, 100);
  const exp = 30 + floor * 25;
  const coins = 8 + floor * 10;
  if (floor % 10 === 0)
    return { hp: idiv(baseHp * 5, 2), attack: idiv(baseAttack * 17, 10), expReward: exp * 4,
      coinReward: coins * 5, level: floor + 3, boss: true };
  return { hp: idiv(baseHp * EnemyTuning.OrdinaryHpPercent, 100),
    attack: Math.max(1, idiv(baseAttack * EnemyTuning.OrdinaryAttackPercent, 100)),
    expReward: exp, coinReward: coins, level: floor, boss: false };
}

export function generateEnemy(value, elite = false) {
  const floor = floorOf(value);
  const stats = enemyStatsForFloor(floor);
  elite = elite && !stats.boss;
  const name = stats.boss ? `★ ${enemyNameForFloor(floor)} ★` : (elite ? '精英·' : '') + enemyNameForFloor(floor);
  const monster = new Enemy(name, stats.hp, stats.attack, 0, stats.expReward, stats.coinReward, stats.level);
  monster.setAffix(floor);
  if (elite) {
    monster.ensureAffixes(2);
    monster.resethp(idiv(monster.gethp() * EnemyTuning.EliteHpPercent, 100));
    monster.resetattack(idiv(monster.getattack() * EnemyTuning.EliteAttackPercent, 100));
    monster.resetexp_reward(monster.getexp_reward() * 2);
    monster.resetcoin_reward(idiv(monster.getcoin_reward() * 3, 2));
  }
  if (stats.boss) monster.resetattack(idiv(monster.getattack() * bossAttackPercent(floor), 100));
  if (monster.hasAffix(Affix.STRONG)) monster.resetattack(idiv(monster.getattack() * 120, 100));
  if (monster.hasAffix(Affix.TOUGH)) monster.resethp(idiv(monster.gethp() * 120, 100));
  if (monster.hasAffix(Affix.QUICK))
    monster.setdodgeRate(Math.min(monster.getdodgeRate() + idiv(monster.getlevel(), 2), 25));
  monster.configureEncounter(floor, stats.boss);
  monster.elite = elite;
  return monster;
}
