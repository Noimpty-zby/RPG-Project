// Port of src/game/campaign.{h,cpp}: ten chapters, routes, events, camp,
// shop and encounter settlement.
import {
  addCounter, chapterOf, clamp, floorOf, idiv, IntMax, MaxMaterials, refreshCost, repeatRewardPercent, restCost,
  MaxRestsPerChapter, challengeName,
} from './config.js';
import { Affix } from './enemy.js';
import { ItemType, JobReq } from './item.js';
import { equipmentClass } from './jobs.js';
import { seed as seedRandom } from './random.js';
import { emit } from './log.js';
import { newRunState } from './run_state.js';
import { Objective, defaultRules } from './combat.js';
import { EnemyTuning, generateEliteReward, generateEnemy, generateLoot, generateTargetedItem } from './world.js';

export const Chapters = [
  { name: '荒野前哨', mission: '切断警钟，护送迷路斥候', ally: '斥候' },
  { name: '幽林回廊', mission: '打断树心仪式，救回采药师', ally: '采药师' },
  { name: '沉没书库', mission: '夺回禁书，掩护学者撤离', ally: '学者' },
  { name: '赤砂矿井', mission: '拆除引爆装置，救出矿工', ally: '矿工' },
  { name: '寒霜关隘', mission: '摧毁冰晶，运送御寒物资', ally: '守关人' },
  { name: '熔炉要塞', mission: '切断供能炉，救出锻造师', ally: '锻造师' },
  { name: '镜影剧场', mission: '破坏镜阵，保护失忆旅人', ally: '旅人' },
  { name: '星蚀观测站', mission: '打断观星仪式，抢救星图', ally: '观测者' },
  { name: '王庭废墟', mission: '拆除王座封印，护送继承人', ally: '继承人' },
  { name: '天顶之门', mission: '切断核心回路，带领同伴突围', ally: '远征队' },
];

const validFloor = (floor) => floor >= 1 && floor <= 100;
export const localFloor = (floor) => ((floor - 1) % 10) + 1;
export const eventType = (floor) => chapterOf(floor) % 3;

function objectiveSlot(chapter, local) {
  const positions = [3, 6, 8];
  for (let slot = 0; slot < 3; ++slot)
    if (local === ((positions[slot] - 1 + chapter * 2) % 9) + 1) return slot;
  return -1;
}
function loseHealthSafely(hero, percent) {
  const loss = Math.max(1, idiv(hero.gethp() * percent, 100));
  hero.sethp(Math.max(1, hero.gethp() - loss));
}
export function objectiveName(objective) {
  switch (objective) {
    case Objective.Survive: return '生存';
    case Objective.Disrupt: return '打断';
    case Objective.Escape: return '撤离';
    default: return '击败';
  }
}
export function eventOutcome(type, choice) {
  if (choice === 0) return '事件尚未处理';
  if (type === 0) return choice === 1 ? '援助已兑现：本章商品九折，Boss攻击-10%' : '物资已取回，材料领取完成';
  if (type === 1) return choice === 1 ? '净化已生效：本章敌人移除剧毒' : '协助已完成，材料领取完成';
  return choice === 1 ? '情报已生效：本章敌人闪避-10个百分点，最低0' : '悬赏已领取：本章Boss攻击+10%';
}
const isUsable = (hero, item) => item.getLevelRequirement() <= hero.getlevel() && hero.canWear(item);
export function isUpgrade(hero, item) {
  if (item.getType() === ItemType.Potion || !isUsable(hero, item)) return false;
  for (const equipped of hero.getSaveEquipments())
    if (equipped.getType() === item.getType()) return item.getValue() > equipped.getValue();
  return true;
}
function giveItem(hero, item) {
  const run = hero.runState();
  const upgrade = isUpgrade(hero, item);
  if (upgrade) {
    run.usefulDrops = addCounter(run.usefulDrops, 1);
    run.gearDrought = 0;
  }
  hero.addItem(item);
  emit('loot', `获得：${item.getDisplayName()}（数值 ${item.getValue()}）`, { item: item.clone(), upgrade });
}
function addMaterials(hero, amount) {
  const run = hero.runState();
  const before = run.materials;
  run.materials = Math.min(MaxMaterials, run.materials + amount);
  if (run.materials > before) emit('materials', `获得材料 ${run.materials - before}。`, { amount: run.materials - before });
}

export const chapter = (floor) => Chapters[chapterOf(floor)];
export const jobOf = (hero) => equipmentClass(hero.job());

export function seedStream(hero, floor, stream) {
  const f = floorOf(floor) >>> 0;
  let value = (hero.runState().seed ^ Math.imul(f, 0x9e3779b9) ^ Math.imul(stream >>> 0, 0x85ebca6b)) >>> 0;
  value = (value ^ (value >>> 16)) >>> 0;
  value = Math.imul(value, 0x7feb352d) >>> 0;
  value = (value ^ (value >>> 15)) >>> 0;
  seedRandom(value);
}

export function initialize(hero, seedValue, challenge) {
  const run = hero.runState();
  if (hero.getlevel() !== 1 || hero.getcoin() !== 0 || hero.getHighestFloor() !== 1 ||
    hero.getSaveBackpack().length || hero.getSaveEquipments().length || run.battles !== 0 || run.legacyRules ||
    run.cleared.some((n) => n !== 0))
    throw new Error('campaign initialization requires a fresh character');
  hero.run = newRunState();
  hero.run.seed = (seedValue >>> 0) === 0 ? 1 : seedValue >>> 0;
  hero.run.challenge = clamp(challenge, 0, 2);
  hero.gaincoin(120);
  seedStream(hero, 1, 1);
  giveItem(hero, generateTargetedItem(1, jobOf(hero), ItemType.Weapon));
  hero.equipItem(hero.getSaveBackpack().length);
  giveItem(hero, generateTargetedItem(1, jobOf(hero), ItemType.Armor));
  hero.equipItem(hero.getSaveBackpack().length);
  hero.sethp(hero.getmaxhp());
  const supplies = generateTargetedItem(1, JobReq.AnyJob, ItemType.Potion);
  supplies.setQuantity(challenge === 1 ? 2 : 4);
  hero.addItem(supplies);
}

export const RouteNames = ['未选择', '破坏线', '救援线', '补给线'];
export function routeDescription(route) {
  if (route === 1) return '破坏线：完成本章第2、7层，Boss失去护盾并降低核心压力；订单需完成5层；路线遭遇以打断为目标';
  if (route === 2) return '救援线：订单提前至完成3层；路线遭遇以生存为目标';
  if (route === 3) return '补给线：首通第3、7层额外药水；Boss保留全部机制';
  return '尚未选择路线';
}
export function chooseRoute(hero, floor, route) {
  if (!validFloor(floor) || route < 1 || route > 3) return false;
  const routes = hero.runState().routes;
  const c = chapterOf(floor);
  if (routes[c] !== 0) return false;
  routes[c] = route;
  return true;
}
export function objectivesCompleted(hero, floor) {
  const start = chapterOf(floor) * 10 + 1;
  let count = 0;
  for (let i = start; i < start + 9; ++i) count += hero.runState().cleared[i] !== 0 ? 1 : 0;
  return count;
}
export function eliteFloor(chapterIndex) {
  chapterIndex = clamp(chapterIndex, 0, 9);
  for (const local of [7, 6, 8, 5, 9, 4])
    if (objectiveSlot(chapterIndex, local) < 0) return chapterIndex * 10 + local;
  return chapterIndex * 10 + 4;
}
export const isEliteFloor = (floor) => validFloor(floor) && eliteFloor(chapterOf(floor)) === floor;

export function rulesFor(hero, floor) {
  const rules = defaultRules();
  if (!validFloor(floor)) return rules;
  const run = hero.runState();
  const c = chapterOf(floor);
  const local = localFloor(floor);
  rules.elite = isEliteFloor(floor);
  rules.difficulty = (run.challenge === 1 ? 1 : 0) + (rules.elite ? 1 : 0);
  rules.bossSabotaged = local === 10 && run.routes[c] === 1 &&
    Boolean(run.cleared[c * 10 + 2]) && Boolean(run.cleared[c * 10 + 7]);
  const goals = [Objective.Survive, Objective.Disrupt,
    run.routes[c] === 1 ? Objective.Disrupt : run.routes[c] === 2 ? Objective.Survive : Objective.Escape];
  const slot = objectiveSlot(c, local);
  if (slot >= 0) rules.objective = goals[(slot + c) % 3];
  rules.targetTurns = 3 + idiv(c, 4);
  return rules;
}

export function makeEnemy(hero, floor) {
  if (!validFloor(floor)) throw new RangeError('floor must be 1..100');
  seedStream(hero, floor, 10 + hero.runState().attempts[floor]);
  const rules = rulesFor(hero, floor);
  const monster = generateEnemy(floor, rules.elite);
  if (rules.objective !== Objective.Defeat) {
    const hpPercent = rules.objective === Objective.Disrupt ? EnemyTuning.DisruptHpPercent : EnemyTuning.SurviveHpPercent;
    monster.resethp(idiv(monster.getmaxhp() * hpPercent, EnemyTuning.OrdinaryHpPercent));
    monster.resetattack(Math.max(1, idiv(monster.getattack() * EnemyTuning.ObjectiveAttackPercent,
      EnemyTuning.OrdinaryAttackPercent)));
  }
  const event = hero.runState().events[chapterOf(floor)];
  const type = eventType(floor);
  if (event === 1 && type === 1) monster.removeAffix(Affix.POISONOUS);
  if (event === 1 && type === 2) monster.setdodgeRate(Math.max(0, monster.getdodgeRate() - 10));
  if (floor % 10 === 0) {
    if (event === 1 && type === 0) monster.resetattack(Math.max(1, idiv(monster.getattack() * 90, 100)));
    if (event === 2 && type === 2) monster.resetattack(Math.max(1, idiv(monster.getattack() * 110, 100)));
  }
  return monster;
}

export const eventPending = (hero, floor) =>
  validFloor(floor) && localFloor(floor) >= 5 && hero.runState().events[chapterOf(floor)] === 0;

// Structured event choices for the dialog; mirrors eventDescription().
export function eventOptions(floor) {
  const cost = restCost(floor);
  const ally = chapter(floor).ally;
  switch (eventType(floor)) {
    case 0:
      return { ally, title: '援助', pay: { cost, text: `支付 ${cost} 金币援助：本章商品九折，Boss攻击降低10%` },
        risk: { text: '冒险取回物资：损失当前HP的15%（不会致死），获得3材料' } };
    case 1:
      return { ally, title: '净化', pay: { cost, text: `支付 ${cost} 金币净化：本章所有敌人移除剧毒词缀` },
        risk: { text: '协助净化：消耗最大MP的25%（至少1），恢复基础HP的10%并获得1材料；MP不足时改为损失当前HP的5%（不会致死），仅获得1材料' } };
    default:
      return { ally, title: '情报', pay: { cost, text: `支付 ${cost} 金币购买情报：本章所有敌人闪避降低10个百分点` },
        risk: { text: `接受悬赏：立即获得 ${cost * 2} 金币，但本章Boss攻击提高10%` } };
  }
}
export function eventDescription(floor) {
  const options = eventOptions(floor);
  return `遇到${options.ally}：[1] ${options.pay.text}  [2] ${options.risk.text}`;
}
export function resolveEvent(hero, floor, choice) {
  if (!eventPending(hero, floor) || (choice !== 1 && choice !== 2) || !hero.isalive()) return false;
  const cost = restCost(floor);
  if (choice === 1 && hero.getcoin() < cost) return false;
  const type = eventType(floor);
  if (choice === 1) hero.costcoin(cost);
  else if (type === 0) {
    loseHealthSafely(hero, 15);
    addMaterials(hero, 3);
  } else if (type === 1) {
    const manaCost = Math.max(1, idiv(hero.getmaxmp(), 4));
    if (hero.getmp() >= manaCost) {
      hero.setmp(hero.getmp() - manaCost);
      hero.heal(Math.max(1, idiv(hero.getRecoveryBaseHp(), 10)));
      emit('notice', `协助净化消耗 ${manaCost} MP。`);
    } else {
      loseHealthSafely(hero, 5);
      emit('notice', 'MP不足，改为搬运物资：损失当前HP的5%（不会致死），不回血。');
    }
    addMaterials(hero, 1);
  } else {
    hero.gaincoin(cost * 2);
  }
  hero.runState().events[chapterOf(floor)] = choice;
  emit('notice', `${eventOutcome(type, choice)}。`);
  return true;
}

export function restsRemaining(hero, floor) {
  if (!validFloor(floor)) return 0;
  const limit = hero.runState().challenge === 1 ? 1 : MaxRestsPerChapter;
  return Math.max(0, limit - hero.runState().campRested[chapterOf(floor)]);
}
export const restPrice = (hero, floor) => restCost(floor) * (1 + hero.runState().campRested[chapterOf(floor)]);
// What the hero carries into the chapter's next hard fight: the first elite or
// boss floor from here that is not yet cleared (0 if none), a summary line and,
// when that fight is on this floor and the hero lacks half MP or a ready
// skill, a warning. Cooldowns only tick during battle turns.
export function readiness(hero, floor) {
  const result = { majorFloor: 0, summary: '', warning: '' };
  if (!validFloor(floor)) return result;
  const bossFloor = (chapterOf(floor) + 1) * 10;
  for (let f = floor; f <= bossFloor && !result.majorFloor; ++f)
    if (!hero.runState().cleared[f] && (f === bossFloor || isEliteFloor(f))) result.majorFloor = f;
  const cooling = hero.skills.filter((ability) => ability.getcurrentcooldown() > 0)
    .map((ability) => `${ability.getskillname()} ${ability.getcurrentcooldown()}`);
  const label = result.majorFloor === bossFloor ? '首领' : '精英';
  const rests = restsRemaining(hero, floor);
  let summary = '出战准备：';
  if (!result.majorFloor) summary += '本章的精英与首领都已通过';
  else if (result.majorFloor === floor) summary += `本层就是${label}`;
  else summary += `下一场硬仗是第${result.majorFloor}层${label}（还有 ${result.majorFloor - floor} 层）`;
  result.summary = `${summary}；MP ${hero.getmp()}/${hero.getmaxmp()}；本章休息剩余 ${rests} 次；` +
    `${cooling.length ? `冷却中：${cooling.join('、')}（只在战斗回合中恢复）` : '技能全部就绪'}。`;
  const lowMp = hero.getmp() * 2 < hero.getmaxmp();
  if (result.majorFloor === floor && (lowMp || cooling.length)) {
    result.warning = `注意：本层就是${label}，但 ${lowMp ? 'MP 不到一半' : ''}${lowMp && cooling.length ? '、' : ''}` +
      `${cooling.length ? `${cooling.length} 个技能仍在冷却` : ''}。${rests ? '营火休息可恢复一半 MP。' : '本章休息次数已用完。'}`;
  }
  return result;
}
export function rest(hero, floor) {
  if (!validFloor(floor) || !hero.isalive() || restsRemaining(hero, floor) === 0) return false;
  const run = hero.runState();
  const cost = restPrice(hero, floor);
  if (hero.getcoin() < cost || (hero.gethp() === hero.getmaxhp() && hero.getmp() === hero.getmaxmp())) return false;
  hero.costcoin(cost);
  hero.heal(idiv(hero.getRecoveryBaseHp() * (run.challenge === 1 ? 25 : 40), 100));
  hero.setmp(hero.getmp() + idiv(hero.getmaxmp(), 2));
  ++run.campRested[chapterOf(floor)];
  return true;
}
export function orderRequirement(hero, floor) {
  return hero.runState().routes[chapterOf(floor)] === 2 ? 3 : 5;
}
export function claimOrder(hero, floor) {
  if (!validFloor(floor)) return false;
  const run = hero.runState();
  const c = chapterOf(floor);
  if (run.routes[c] === 0 || run.ordersClaimed[c] ||
    objectivesCompleted(hero, floor) < (run.routes[c] === 2 ? 3 : 5)) return false;
  seedStream(hero, floor, 300 + run.lootTarget);
  const type = run.lootTarget === 2 ? ItemType.Armor : ItemType.Weapon;
  giveItem(hero, generateTargetedItem(floor, jobOf(hero), type));
  run.ordersClaimed[c] = 1;
  return true;
}
export function shopStock(hero, floor) {
  if (!validFloor(floor)) return [];
  const c = chapterOf(floor);
  seedStream(hero, c * 10 + 1, 500 + hero.runState().shopRefreshes[c]);
  const stockFloor = c * 10 + 5;
  const stock = [];
  stock.push(generateTargetedItem(stockFloor, jobOf(hero), ItemType.Weapon));
  stock.push(generateTargetedItem(stockFloor, jobOf(hero), ItemType.Armor));
  stock.push(generateTargetedItem(stockFloor, JobReq.AnyJob, ItemType.Potion));
  stock.push(generateTargetedItem(stockFloor, JobReq.AnyJob, ItemType.Potion));
  stock.push(generateTargetedItem(stockFloor, jobOf(hero),
    ((hero.runState().seed + c) >>> 0) % 2 ? ItemType.Armor : ItemType.Weapon));
  return stock;
}
export function purchasePrice(hero, floor, item) {
  if (!validFloor(floor)) return 0;
  let price = Math.max(0, item.getPrice());
  if (eventType(floor) === 0 && hero.runState().events[chapterOf(floor)] === 1) price = Math.max(1, idiv(price * 90, 100));
  if (hero.runState().challenge === 1) price = idiv(price * 125, 100);
  return Math.min(price, IntMax);
}
export function purchase(hero, floor, slot) {
  if (!validFloor(floor) || slot < 1 || slot > 5) return false;
  const run = hero.runState();
  const c = chapterOf(floor);
  const bit = 1 << (slot - 1);
  if (run.shopPurchased[c] & bit) return false;
  const stock = shopStock(hero, floor);
  const item = stock[slot - 1];
  const price = purchasePrice(hero, floor, item);
  if (hero.getcoin() < price) return false;
  hero.costcoin(price);
  giveItem(hero, item);
  run.shopPurchased[c] |= bit;
  return true;
}
export function refreshShop(hero, floor) {
  if (!validFloor(floor)) return false;
  const run = hero.runState();
  const c = chapterOf(floor);
  const cost = refreshCost(floor, run.shopRefreshes[c]);
  if (run.shopRefreshes[c] >= 20 || hero.getcoin() < cost) return false;
  hero.costcoin(cost);
  ++run.shopRefreshes[c];
  run.shopPurchased[c] = 0;
  return true;
}

export function settleEncounter(hero, monster, battle, floor) {
  if (!validFloor(floor) || !battle.claimSettlement()) return false;
  const run = hero.runState();
  const report = battle.report;
  run.battles = addCounter(run.battles, 1);
  run.turns = addCounter(run.turns, battle.turns());
  run.damageTaken = addCounter(run.damageTaken, report.damageTaken);
  run.attempts[floor] = addCounter(run.attempts[floor], 1);
  hero.finishEncounterWear();
  if (!battle.won()) {
    if (!hero.isalive()) run.deaths = addCounter(run.deaths, 1);
    return false;
  }
  const first = run.cleared[floor] === 0;
  const percent = first ? 100 : repeatRewardPercent(run.attempts[floor] - 1);
  const exp = idiv(monster.getexp_reward() * percent, 100);
  const coins = idiv(monster.getcoin_reward() * percent, 100);
  emit('reward', `获得 ${exp} 经验、${coins} 金币。`, { exp, coins, first, percent });
  hero.gainexp(exp);
  hero.gaincoin(coins);
  if (first) {
    run.gearDrought = addCounter(run.gearDrought, 1);
    hero.heal(Math.max(1, idiv(hero.getRecoveryBaseHp() * (run.challenge === 1 ? 2 : 4), 100)));
  }
  seedStream(hero, floor, 900 + run.attempts[floor]);
  const dropped = generateLoot(hero, floor, first);
  if (dropped) giveItem(hero, dropped);
  const c = chapterOf(floor);
  const local = localFloor(floor);
  if (first && (local === 3 || local === 7)) {
    addMaterials(hero, 2);
    if (run.routes[c] === 3) giveItem(hero, generateTargetedItem(floor, JobReq.AnyJob, ItemType.Potion));
  }
  if (first && isEliteFloor(floor)) {
    addMaterials(hero, 2);
    emit('notice', '【精英奖励】高品质装备与 2 材料：');
    giveItem(hero, generateEliteReward(hero, floor));
    run.pity = 0;
  }
  if (first && local === 10) {
    addMaterials(hero, 3);
    emit('chapterClear', `章节完成：${chapter(floor).name}；专精与改造可在营地重新准备。`, { chapter: c });
  }
  run.cleared[floor] = 1;
  run.longestGearDrought = Math.max(run.longestGearDrought, run.gearDrought);
  return true;
}

export function revive(hero, useFree) {
  const run = hero.runState();
  if (run.challenge === 2 || hero.isalive()) return false;
  if (useFree) {
    if (hero.getfreerevives() <= 0) return false;
    hero.decreaserevives();
  } else {
    if (hero.getcoin() <= 0) return false;
    hero.costcoin(Math.max(1, idiv(hero.getcoin(), 3)));
  }
  hero.revive();
  hero.setmp(hero.getmaxmp());
  emit('notice', '回到本层营地。路线与首通记录保留，可调整打法后重试。');
  return true;
}

// Battle review lines and advice, from printBattleReport().
export function battleReview(battle) {
  const r = battle.report;
  const lines = [`战斗复盘：${battle.turns()}回合，承伤 ${r.damageTaken}，其中剧毒 ${r.poisonDamage}；抑疗损失 ${r.suppressedHealing}；被识破 ${r.readPunishments} 次；打断 ${r.interrupts} 次；药水 ${r.potionsUsed}。`];
  if (r.readPunishments) lines.push('调整建议：留意预测行动，改变已被识破的出招。');
  if (r.suppressedHealing) lines.push('调整建议：优先在无重伤的回合恢复，危险时仍可应急治疗。');
  // Only Defeat fights let the player set the pace; the other objectives ask
  // them to wait, so poison alone is no sign of stalling.
  if (r.poisonDamage && battle.objective() === Objective.Defeat && r.waitingGuards >= 2)
    lines.push(`调整建议：剧毒每回合持续消耗，本场有 ${r.waitingGuards} 次在敌人普攻、蓄力或休整时格挡等待；用普攻或技能推进可以缩短战斗。`);
  return lines;
}

// Chapter journal, from printJournal(), as structured data for the UI.
export function journal(hero, floor) {
  const run = hero.runState();
  const c = chapterOf(floor);
  const goals = [];
  for (let local = 1; local <= 9; ++local) {
    const targetFloor = c * 10 + local;
    const rules = rulesFor(hero, targetFloor);
    if (rules.objective !== Objective.Defeat) goals.push({ floor: targetFloor, label: objectiveName(rules.objective) });
    if (rules.elite) goals.push({ floor: targetFloor, label: '精英' });
  }
  goals.push({ floor: (c + 1) * 10, label: 'Boss' });
  return {
    chapter: c,
    name: chapter(floor).name,
    mission: chapter(floor).mission,
    route: run.routes[c],
    routeText: routeDescription(run.routes[c]),
    progress: objectivesCompleted(hero, floor),
    orderClaimed: Boolean(run.ordersClaimed[c]),
    orderNeed: run.routes[c] === 2 ? 3 : 5,
    materials: run.materials,
    lootTarget: run.lootTarget,
    pity: run.pity,
    pityNeed: run.lootTarget === 1 ? 6 : 8,
    seed: run.seed,
    challenge: challengeName(run.challenge),
    battles: run.battles,
    turns: run.turns,
    deaths: run.deaths,
    potions: run.potionsUsed,
    usefulDrops: run.usefulDrops,
    longestGearDrought: run.longestGearDrought,
    goals,
    eventText: eventOutcome(eventType(floor), run.events[c]),
    letter: c > 0 && run.events[c - 1] === 1
      ? `来自${Chapters[c - 1].ally}的信：上一章的合作已经记入远征日志，效果限于当地。` : '',
  };
}
