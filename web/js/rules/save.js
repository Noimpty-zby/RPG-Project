// Browser save format (JSON). Mirrors the content of the C++ VERSION 6 save
// and its validation: malformed or contradictory data is rejected whole.
// Version 2 adds exploration; version 1 saves load with none.
import { chapterOf, MaxCoins, MaxCounter, MaxMaterials, MaxRestsPerChapter, skillRankCap } from './config.js';
import * as Exploration from './exploration.js';
import { Item, ItemType } from './item.js';
import { JobProfiles, jobFromKey } from './jobs.js';
import { Player, validItem } from './player.js';
import { newRunState } from './run_state.js';

export const SaveVersion = 2;
const ExplorationFields = ['explored', 'keys', 'ingredients', 'boon', 'bane', 'questItems', 'spotsExplored',
  'vaultsOpened', 'puzzlesSolved', 'mealsCooked'];
const MaxExploredMask = (1 << (Exploration.MerchantBit + Exploration.MerchantGoods)) - 1;

function itemData(item) {
  return {
    name: item.name, levelRequirement: item.levelRequirement, type: item.type, value: item.value,
    durability: item.durability, maxDurability: item.maxDurability, price: item.price, quantity: item.quantity,
    jobRequirement: item.jobRequirement, tier: item.tier, effect: item.effect, locked: item.locked, id: item.id,
    enhanceLevel: item.enhanceLevel,
  };
}
function require(condition, message) {
  if (!condition) throw new Error(message);
}
const isInt = (value, low, high) => Number.isInteger(value) && value >= low && value <= high;
function readItem(row) {
  require(row && typeof row.name === 'string' && row.name.length > 0 && row.name.length <= 256, '物品名称无效');
  const item = new Item(row.name, row.levelRequirement, row.type, row.value, row.durability, row.price,
    row.quantity, row.jobRequirement, row.tier, 0, Boolean(row.locked), row.id);
  item.maxDurability = row.maxDurability;
  item.enhanceLevel = row.enhanceLevel;
  require(isInt(row.effect, 0, 4) && item.setEffect(row.effect), '物品效果无效');
  for (const field of ['levelRequirement', 'type', 'value', 'durability', 'maxDurability', 'price', 'quantity',
    'jobRequirement', 'tier', 'enhanceLevel'])
    require(Number.isInteger(row[field]), `物品字段无效: ${field}`);
  require(validItem(item), '物品数值无效');
  require(item.id === Item.templateId(item.name, item.type, item.jobRequirement, item.tier), '物品模板ID不匹配');
  return item;
}
function validateRun(run, highestFloor) {
  require(isInt(run.challenge, 0, 2), '挑战');
  require(isInt(run.lootTarget, 1, 2), '掉落目标');
  require(isInt(run.pity, 0, 8), '保底');
  require(isInt(run.materials, 0, MaxMaterials), '材料');
  require(isInt(run.seed, 0, 4294967295), '种子');
  const arrays = [['routes', 10, 3], ['events', 10, 2], ['campRested', 10, MaxRestsPerChapter],
    ['ordersClaimed', 10, 1], ['shopRefreshes', 10, 20], ['shopPurchased', 10, 31],
    ['cleared', 101, 1], ['attempts', 101, MaxCounter], ['bounties', 20, 99]];
  for (const [field, length, maximum] of arrays) {
    require(Array.isArray(run[field]) && run[field].length === length, `${field} 长度无效`);
    require(run[field].every((value) => isInt(value, 0, maximum)), `${field} 数值无效`);
  }
  for (const field of ['battles', 'turns', 'potionsUsed', 'deaths', 'damageTaken', 'usefulDrops', 'gearDrought',
    'longestGearDrought'])
    require(isInt(run[field], 0, MaxCounter), '统计');
  require(run.cleared[0] === 0 && run.attempts[0] === 0, '楼层数组第0项必须为空');
  for (let floor = 1; floor <= 100; ++floor) {
    require(!(run.cleared[floor] && run.attempts[floor] === 0), '通关楼层缺少尝试记录');
    require(!(floor > highestFloor && (run.cleared[floor] || run.attempts[floor])), '楼层记录超出最高楼层');
  }
  require(run.longestGearDrought >= run.gearDrought, '装备空窗统计矛盾');
  validateExploration(run, highestFloor);
}
// Exploration records must match the spots each floor actually holds, and the
// counters must agree with them.
function validateExploration(run, highestFloor) {
  require(isInt(run.keys, 0, Exploration.MaxKeys), '钥匙');
  require(isInt(run.boon, 0, Exploration.BoonCount), '祝福');
  require(isInt(run.bane, 0, Exploration.BaneCount), '诅咒');
  for (const [field, length, maximum] of [['ingredients', 5, Exploration.MaxIngredients],
    ['questItems', 10, Exploration.QuestItemsPerChapter], ['explored', 101, MaxExploredMask]]) {
    require(Array.isArray(run[field]) && run[field].length === length, `${field} 长度无效`);
    require(run[field].every((value) => isInt(value, 0, maximum)), `${field} 数值无效`);
  }
  for (const field of ['spotsExplored', 'vaultsOpened', 'puzzlesSolved', 'mealsCooked'])
    require(isInt(run[field], 0, MaxCounter), '探索统计');
  require(run.explored[0] === 0, '楼层数组第0项必须为空');
  let found = 0;
  let vaults = 0;
  let puzzles = 0;
  const quests = new Array(10).fill(0);
  for (let floor = 1; floor <= 100; ++floor) {
    const mask = run.explored[floor];
    if (!mask) continue;
    require(floor <= highestFloor, '探索记录超出最高楼层');
    const spots = Exploration.generate(run.seed, floor);
    const spotBits = mask & ((1 << Exploration.MerchantBit) - 1);
    const goodsBits = mask >> Exploration.MerchantBit;
    require((spotBits >> spots.length) === 0, '探索记录与楼层内容不符');
    const merchant = spots.map((s) => s.kind).lastIndexOf(Exploration.SpotKind.Merchant);
    require(!(goodsBits && merchant < 0), '该楼层没有商人');
    require(merchant < 0 || (((spotBits >> merchant) & 1) !== 0) === (goodsBits !== 0), '商人记录矛盾');
    spots.forEach((spot, i) => {
      if (!((spotBits >> i) & 1)) return;
      ++found;
      if (spot.kind === Exploration.SpotKind.Vault) ++vaults;
      if (spot.kind === Exploration.SpotKind.Puzzle) ++puzzles;
      if (spot.kind === Exploration.SpotKind.QuestItem) ++quests[chapterOf(floor)];
    });
  }
  require(found === run.spotsExplored && vaults === run.vaultsOpened && puzzles === run.puzzlesSolved, '探索统计与记录矛盾');
  require(quests.every((count, c) => count === run.questItems[c]), '同伴遗物记录矛盾');
}

export function serializeGame(hero, floor, extra = {}) {
  return {
    version: SaveVersion,
    savedAt: new Date().toISOString(),
    floor,
    hero: {
      name: hero.name, job: hero.getjob(), level: hero.level, exp: hero.exp, coin: hero.coin, hp: hero.hp,
      maxHp: hero.max_hp, attack: hero.attack, mp: hero.mp, maxMp: hero.max_mp, critRate: hero.critRate,
      freeRevives: hero.free_revives, highestFloor: hero.highestFloor,
    },
    backpack: hero.backpack.map(itemData),
    equipment: hero.equipments.map(itemData),
    skills: hero.skills.map((s) => ({ id: s.skillid, rank: s.level, cooldown: s.currentcooldown, branch: s.specialization })),
    run: JSON.parse(JSON.stringify(hero.run)),
    ...extra,
  };
}

// Returns { hero, floor, data } or throws with a Chinese reason.
export function deserializeGame(data) {
  require(data && typeof data === 'object', '存档为空');
  require(isInt(data.version, 1, SaveVersion), data.version > SaveVersion ? '存档来自更新的游戏版本' : '存档版本无效');
  const h = data.hero;
  require(h && typeof h.name === 'string' && h.name.length > 0 && h.name.length <= 120, '名字无效');
  const job = jobFromKey(h.job);
  require(job >= 0, '职业无效');
  require(isInt(h.level, 1, 100) && isInt(h.exp, 0, MaxCounter) && isInt(h.coin, 0, MaxCoins), '等级、经验或金币无效');
  require(isInt(h.maxHp, 1, MaxCounter) && isInt(h.hp, 0, h.maxHp), '生命无效');
  require(isInt(h.attack, 0, MaxCounter) && isInt(h.maxMp, 0, MaxCounter) && isInt(h.mp, 0, h.maxMp), '攻击或魔力无效');
  require(isInt(h.critRate, 0, 100) && isInt(h.freeRevives, 0, 3), '暴击或复苏次数无效');
  require(isInt(h.highestFloor, 1, 100) && isInt(data.floor, 1, 100) && data.floor <= h.highestFloor, '楼层无效');
  require(data.run && typeof data.run === 'object', '缺少章节数据');
  for (const field of ExplorationFields)
    require((field in data.run) === (data.version >= 2), data.version >= 2 ? `缺少探索字段: ${field}` : `旧版本包含探索字段: ${field}`);
  const run = { ...newRunState(), ...data.run };
  validateRun(run, h.highestFloor);
  const hero = new Player(h.name, job);
  require(Array.isArray(data.backpack) && data.backpack.length <= 500, '背包无效');
  require(Array.isArray(data.equipment) && data.equipment.length <= 2, '装备无效');
  const backpack = data.backpack.map(readItem);
  const equipment = data.equipment.map(readItem);
  const slots = new Set();
  let attackBonus = 0;
  let hpBonus = 0;
  for (const item of equipment) {
    require(item.type !== ItemType.Potion && item.quantity === 1 && item.durability > 0 && !slots.has(item.type) &&
      hero.canWear(item), '装备槽位、数量或职业限制无效');
    slots.add(item.type);
    if (item.type === ItemType.Weapon) attackBonus += item.value;
    else hpBonus += item.value;
  }
  require(h.attack > attackBonus && h.maxHp > hpBonus, '装备加成超出角色属性');
  require(Array.isArray(data.skills) && data.skills.length === hero.getSkillCount(), '职业技能数量不匹配');
  for (const row of data.skills) {
    const skill = hero.skills.find((s) => s.skillid === row.id);
    require(skill && isInt(row.rank, 1, skillRankCap(h.highestFloor)) && isInt(row.cooldown, 0, skill.cooldown) &&
      isInt(row.branch, 0, 2) && skill.setSpecialization(row.branch), '技能数据无效');
    require(!(row.branch && (h.highestFloor < (row.id === 1 ? 10 : 30) || h.level < skill.levelrequired)), '专精未解锁');
    skill.restoreProgress(row.rank, row.cooldown);
  }
  hero.loadStats(h.level, h.exp, h.coin, h.hp, h.maxHp, h.attack, h.mp, h.maxMp, h.critRate, h.freeRevives);
  hero.highestFloor = h.highestFloor;
  hero.run = run;
  hero.backpack = backpack;
  hero.equipments = equipment;
  return { hero, floor: data.floor, data };
}

export function saveSummary(data) {
  try {
    const h = data.hero;
    const profile = JobProfiles[jobFromKey(h.job)];
    return { name: h.name, job: profile?.displayName ?? '?', level: h.level, floor: data.floor,
      savedAt: data.savedAt, challenge: data.run?.challenge ?? 0 };
  } catch {
    return null;
  }
}

