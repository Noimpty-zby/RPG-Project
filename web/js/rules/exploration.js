// Port of src/game/exploration.{h,cpp}: things to find on a floor besides its
// guardian. The rules decide what a floor holds and what each choice does; the
// map decides where to draw it. Spots come from the run seed and the floor
// alone, and every choice is resolved from its own seed stream, so results
// match the C++ build and survive reloads.
import { addCounter, chapterOf, clamp, floorOf, idiv, MaxFloor, MaxMaterials, restCost } from './config.js';
import { Affix, affixName } from './enemy.js';
import { ItemType, JobReq } from './item.js';
import { below, seed as seedRandom } from './random.js';
import { emit } from './log.js';
import { chapterText, forkText, lore } from './exploration_text.js';
import { EnemyTuning, generateEliteReward, generateTargetedItem } from './world.js';
import { Chapters, eliteFloor, grantItem, makePatrol, PatrolMaterials } from './campaign.js';

export const SpotKind = {
  Cache: 1,       // Supply cache: free, small random reward.
  Curio: 2,       // Chapter curio: gamble by hand or pay for the good result.
  Gather: 3,      // Fish, herbs, ore...: ingredients for camp cooking.
  AffixSource: 4, // Strips one dangerous affix from this floor's guardian.
  Key: 5,         // Vault key; keys carry over between chapters.
  Vault: 6,       // Locked room: variant 1 needs one key, variant 2 needs two.
  Merchant: 7,    // Wandering merchant with three goods.
  QuestItem: 8,   // One of the chapter ally's three lost items.
  Puzzle: 9,      // The chapter's ordering puzzle; see puzzle()/solvePuzzle().
  Lore: 10,       // A hidden page of the expedition journal (cross-run collection).
  FogSource: 11,  // Fork floors: shutting it makes the fog branch safe.
  Vantage: 12,    // Fork floors: high ground that shows the patrol and the stash.
  Shortcut: 13,   // Fork floors, past the fork: a crack (variant 0) or a bridge lever
                  // (variant 1) that opens the fog branch's supplies from whichever
                  // branch was taken.
};
// Where a map should put a spot. Hidden spots sit in alcoves or behind cracked
// walls; vaults are locked side rooms. On fork floors the guardian's room lies
// past the fork, and Branch spots lie along one branch (spot.branch).
export const Zone = { Middle: 0, Guardian: 1, Hidden: 2, Vault: 3, Branch: 4 };

// The ways past a fork floor's fork; only one can be taken. The fog branch is
// always offered and can always be walked, so a fork never blocks the guardian.
export const Branch = {
  None: 0,
  Patrol: 1, // Near: win a fight against an elite patrol (Campaign.makePatrol).
  Fog: 2,    // Far: lose some health in the fog unless its source is shut; supplies on the way.
  Locked: 3, // Key: spend one key; a stash behind the gate.
};

export const MaxSpots = 16;     // run.explored bits 0..15, one per spot.
export const MerchantBit = 16;  // Bits 16..18: merchant goods sold on this floor.
export const MerchantGoods = 3;
export const BranchBit = 19;    // Bits 19..20: the Branch taken on a fork floor.
export const ForkFloorsPerChapter = 3;
export const IngredientCount = 5;
export const MaxKeys = 99;
export const MaxIngredients = 99;
export const LorePageCount = 100;
export const QuestItemsPerChapter = 3;
export const BoonCount = 4;
export const BaneCount = 2;
export const RecipeCount = 6;

// Seed streams; spot outcomes use SpotStream + index.
const FloorStream = 700;
const PlanStream = 701;
const SpotStream = 720;
const VaultStream = 740;
const VaultGearStream = 741;
const MerchantStream = 750;
const BlindBoxStream = 760;
const PuzzleStream = 780;
const ForkPlanStream = 790;
const ForkStream = 791;
const StashStream = 792;

// Outcome tables as cumulative percent thresholds on one roll of 0..99, and
// the other tuning knobs. Option texts are built from these.
const CacheGold = 60, CachePotion = 66;          // Else 1 material and half the gold.
const ContainerGold = 55, ContainerPotion = 70;  // Else a trap.
const TrapHealthPercent = 8;
const ShrineBoon = 40, ShrineGold = 75;          // Else a curse.
const RelicMaterials = 40, RelicExperience = 75; // Else a backlash.
const RelicManaPercent = 40, BacklashManaPercent = 35;
const GatherPercent = 50, SecondGatherPercent = 10, GatherBonusPercent = 20;
const AffixSourcePercent = 40, SourceHealthPercent = 10, SourceMaterials = 2;
const VaultMaterialsRoll = 40, VaultPotionRoll = 70; // Else gold.
const BoxRefund = 35, BoxPotion = 60, BoxMaterials = 78, BoxIngredients = 92; // Else a key.
const WeakenPercent = 90; // Attack left when a source or boon finds no affix.
const BoonAttackPercent = 92, BoonHealthPercent = 92, BoonDodgePoints = 8;
const BaneAttackPercent = 112, BaneHealthPercent = 110;
const AmbushHealthPercent = 92;
const PuzzleHealthPercent = 5;
// Forks: which branches beside the fog one, what the fog costs, and how often
// the fog source hides and a shortcut exists. The stash pays like a one-key vault.
const PatrolForkPercent = 35, KeyForkPercent = 25; // Else all three branches.
const FogHealthPercent = 20;
const HiddenSourcePercent = 50, ShortcutPercent = 50;

export const IngredientNames = ['鲜鱼', '草药', '矿砂', '霜晶', '星尘'];
// Dangerous affixes in the order an affix source or the boon removes them.
const Strippable = [Affix.SHIELDING, Affix.POISONOUS, Affix.HEALING, Affix.EXPLOSIVE];

const validFloor = (floor) => Number.isInteger(floor) && floor >= 1 && floor <= MaxFloor;
const localOf = (floor) => ((floor - 1) % 10) + 1;
// Campaign.seedStream's mixing, from the seed alone.
function seedFor(seedValue, floor, stream) {
  const f = floorOf(floor) >>> 0;
  let value = ((seedValue >>> 0) ^ Math.imul(f, 0x9e3779b9) ^ Math.imul(stream >>> 0, 0x85ebca6b)) >>> 0;
  value = (value ^ (value >>> 16)) >>> 0;
  value = Math.imul(value, 0x7feb352d) >>> 0;
  value = (value ^ (value >>> 15)) >>> 0;
  seedRandom(value);
}
function pick(pool) {
  const index = below(pool.length);
  const [value] = pool.splice(index, 1);
  return value;
}
// Small rewards scale with the floor: about a twentieth of a fight's coins and a third of its experience.
const coinUnit = (floor) => 4 + idiv(floor, 2);
const expUnit = (floor) => 10 + floor * 8;
const relicManaCost = (hero) => Math.max(1, idiv(hero.getmaxmp() * RelicManaPercent, 100));
const shrinePrice = (floor) => restCost(floor);
const puzzleSize = (chapter) => 3 + (chapter >= 3 ? 1 : 0) + (chapter >= 6 ? 1 : 0);
const puzzleMaterials = (chapter) => 2 + idiv(chapter, 3);
const vaultMaterials = (chapter) => 3 + idiv(chapter, 3);

const reachable = (hero, floor) => validFloor(floor) && floor <= hero.getHighestFloor() && hero.isalive();
const spotDone = (run, floor, index) => ((run.explored[floor] >> index) & 1) !== 0;
function markExplored(run, floor, index) {
  if (spotDone(run, floor, index)) return;
  run.explored[floor] |= 1 << index;
  run.spotsExplored = addCounter(run.spotsExplored, 1);
}
const hasKind = (spots, kind) => spots.some((s) => s.kind === kind);
const kindDone = (run, spots, floor, kind) => spots.some((s, i) => s.kind === kind && spotDone(run, floor, i));
// Every fork floor has a fog source, so it marks the fork.
const forked = (spots) => hasKind(spots, SpotKind.FogSource);
const takenOf = (run, floor) => (run.explored[floor] >> BranchBit) & 3;
// The guardian's side of a fork floor opens once a branch is taken. Floors
// cleared before forks existed stay open.
const guardianOpen = (run, floor, spots) => !forked(spots) || takenOf(run, floor) !== Branch.None || run.cleared[floor] !== 0;
const farName = (floor) => `${forkText(chapterOf(floor)).fog}远路`;
function branchName(branch, floor) {
  switch (branch) {
    case Branch.Patrol: return '巡逻近路';
    case Branch.Fog: return farName(floor);
    case Branch.Locked: return '钥匙小径';
    default: return '';
  }
}
// Why a spot cannot be reached now, or ''. A spot on a branch that was not
// taken opens through a shortcut.
function blockedReason(run, spots, floor, index) {
  const spot = spots[index];
  if (spot.zone === Zone.Branch) {
    const taken = takenOf(run, floor);
    if (taken === spot.branch || (taken !== Branch.None && kindDone(run, spots, floor, SpotKind.Shortcut))) return '';
    if (taken === Branch.None) return `在${branchName(spot.branch, floor)}上：先在岔路口选这条路`;
    return `${branchName(spot.branch, floor)}已经关闭` +
      (hasKind(spots, SpotKind.Shortcut) ? '，打通守卫房间附近的捷径才能过去' : '');
  }
  if (spot.zone === Zone.Guardian && !guardianOpen(run, floor, spots)) return '在岔路之后：先在岔路口选一条路';
  return '';
}
// A spot the branch taken settled: on a closed branch that no shortcut can
// open, or the shortcut itself once the fog branch was walked.
function settledByBranch(run, spots, floor, index) {
  const spot = spots[index];
  const taken = takenOf(run, floor);
  if (spot.kind === SpotKind.Shortcut) return taken === Branch.Fog;
  return spot.zone === Zone.Branch && taken !== Branch.None && taken !== spot.branch &&
    !hasKind(spots, SpotKind.Shortcut);
}
// The fork's branches from the current random state: one roll.
function drawBranches() {
  const roll = below(100);
  if (roll < PatrolForkPercent) return [Branch.Patrol, Branch.Fog];
  if (roll < PatrolForkPercent + KeyForkPercent) return [Branch.Fog, Branch.Locked];
  return [Branch.Patrol, Branch.Fog, Branch.Locked];
}
// The key branch's stash: one roll on the one-key vault table.
function stashRoll(seedValue, floor) {
  seedFor(seedValue, floor, StashStream);
  return below(100);
}
function loseHealthSafely(hero, percent) {
  const loss = Math.max(1, idiv(hero.gethp() * percent, 100));
  hero.sethp(Math.max(1, hero.gethp() - loss));
}
function addMaterials(hero, amount) {
  const run = hero.runState();
  const before = run.materials;
  run.materials = Math.min(MaxMaterials, run.materials + amount);
  return run.materials - before;
}
function addIngredient(run, ingredient, amount) {
  const i = clamp(ingredient, 0, IngredientCount - 1);
  const before = run.ingredients[i];
  run.ingredients[i] = Math.min(MaxIngredients, before + amount);
  return run.ingredients[i] - before;
}
function addKeys(run, amount) {
  const before = run.keys;
  run.keys = Math.min(MaxKeys, run.keys + amount);
  return run.keys - before;
}
function givePotions(hero, floor, quantity) {
  const potion = generateTargetedItem(floor, JobReq.AnyJob, ItemType.Potion);
  potion.setQuantity(quantity);
  grantItem(hero, potion);
}
const potionText = (quantity) => `${quantity} 瓶药水`;
function merchantPrice(hero, floor, kind) {
  const base = restCost(floor);
  let price = kind === 1 ? base * 3 : kind === 3 ? Math.max(1, idiv(base, 2)) : base;
  if (hero.runState().challenge === 1) price = idiv(price * 125, 100);
  return price;
}

// ---- Puzzle generation --------------------------------------------------------
// Clue types: 0 a is at position b, 1 a is not at position b, 2 a before b,
// 3 a next to b, 4 a not next to b.
const positionOf = (order, element) => order.indexOf(element);
function holds(clue, order) {
  const pa = positionOf(order, clue.a);
  switch (clue.type) {
    case 0: return pa === clue.b;
    case 1: return pa !== clue.b;
    case 2: return pa < positionOf(order, clue.b);
    case 3: return Math.abs(pa - positionOf(order, clue.b)) === 1;
    default: return Math.abs(pa - positionOf(order, clue.b)) !== 1;
  }
}
// Every order of 0..n-1, in std::next_permutation order.
function permutations(n) {
  const order = Array.from({ length: n }, (_, i) => i);
  const all = [];
  for (;;) {
    all.push(order.slice());
    let i = n - 2;
    while (i >= 0 && order[i] >= order[i + 1]) --i;
    if (i < 0) return all;
    let j = n - 1;
    while (order[j] <= order[i]) --j;
    [order[i], order[j]] = [order[j], order[i]];
    order.splice(i + 1, n - i - 1, ...order.slice(i + 1).reverse());
  }
}
const countMatching = (clues, all) => all.filter((order) => clues.every((c) => holds(c, order))).length;
// The solution is a random order; clues are true facts added until only that
// order remains, relations before fixed positions, then pruned of redundancy.
function generatePuzzle(seedValue, floor) {
  const n = puzzleSize(chapterOf(floor));
  seedFor(seedValue, floor, PuzzleStream);
  const solution = Array.from({ length: n }, (_, i) => i);
  for (let i = n - 1; i > 0; --i) {
    const j = below(i + 1);
    [solution[i], solution[j]] = [solution[j], solution[i]];
  }
  const pool = [];
  for (let a = 0; a < n; ++a)
    for (let k = 0; k < n; ++k) pool.push({ type: positionOf(solution, a) === k ? 0 : 1, a, b: k });
  for (let a = 0; a < n; ++a)
    for (let b = a + 1; b < n; ++b) {
      const pa = positionOf(solution, a);
      const pb = positionOf(solution, b);
      pool.push(pa < pb ? { type: 2, a, b } : { type: 2, a: b, b: a });
      pool.push({ type: Math.abs(pa - pb) === 1 ? 3 : 4, a, b });
    }
  for (let i = pool.length - 1; i > 0; --i) {
    const j = below(i + 1);
    [pool[i], pool[j]] = [pool[j], pool[i]];
  }
  const ordered = [...pool.filter((c) => c.type !== 0), ...pool.filter((c) => c.type === 0)];
  const all = permutations(n);
  let candidates = all;
  let clues = [];
  for (const clue of ordered) {
    if (candidates.length === 1) break;
    const kept = candidates.filter((candidate) => holds(clue, candidate));
    if (kept.length < candidates.length) {
      clues.push(clue);
      candidates = kept;
    }
  }
  for (let i = 0; i < clues.length;) {
    const without = clues.filter((_, k) => k !== i);
    if (countMatching(without, all) === 1) clues = without;
    else ++i;
  }
  return { solution, clues };
}
function clueText(clue, text) {
  const a = text.puzzleElements[clue.a];
  switch (clue.type) {
    case 0: return `${a}是第${clue.b + 1}个`;
    case 1: return `${a}不是第${clue.b + 1}个`;
    case 2: return `${a}在${text.puzzleElements[clue.b]}之前`;
    case 3: return `${a}和${text.puzzleElements[clue.b]}相邻`;
    default: return `${a}和${text.puzzleElements[clue.b]}不相邻`;
  }
}

// ---- Spot texts and options ---------------------------------------------------------
function spotName(spot, floor) {
  const text = chapterText(chapterOf(floor));
  switch (spot.kind) {
    case SpotKind.Cache: return spot.variant === 1 ? '遗落的药箱' : spot.zone === Zone.Hidden ? text.hiddenCache : text.cache;
    case SpotKind.Curio: return text.curios[spot.variant];
    case SpotKind.Gather: return text.gather[spot.variant];
    case SpotKind.AffixSource: return text.affixSource;
    case SpotKind.Key: return '宝库钥匙';
    case SpotKind.Vault: return text.vaults[spot.variant - 1];
    case SpotKind.Merchant: return '流浪商人';
    case SpotKind.QuestItem: return text.questItems[spot.variant];
    case SpotKind.Puzzle: return text.puzzleTitle;
    case SpotKind.Lore: return '见闻录残页';
    case SpotKind.FogSource: return forkText(chapterOf(floor)).fogSource;
    case SpotKind.Vantage: return forkText(chapterOf(floor)).vantage;
    case SpotKind.Shortcut: return forkText(chapterOf(floor)).shortcuts[spot.variant];
    default: return '';
  }
}
function spotDescription(hero, spot, floor) {
  const c = chapterOf(floor);
  const text = chapterText(c);
  switch (spot.kind) {
    case SpotKind.Cache: return spot.variant === 1 ? '前一支队伍留下的药箱，里面应该还有药水。' : '可能有金币、药水或材料。';
    case SpotKind.Curio: return text.curioDescriptions[spot.variant];
    case SpotKind.Gather:
      return `${text.gatherDescriptions[spot.variant]}（可采集${IngredientNames[text.gatherIngredient[spot.variant]]}）`;
    case SpotKind.AffixSource: return text.affixSourceDescription;
    case SpotKind.Key: return '能打开任意章节的宝库，钥匙跨章节保留。';
    case SpotKind.Vault: return `上锁的储藏室，需要 ${spot.variant} 把钥匙。`;
    case SpotKind.Merchant: return '背着大包的商人，只在这一层停留。';
    case SpotKind.QuestItem:
      return `${Chapters[c].ally}遗落的物品（本章已找到 ${hero.runState().questItems[c]}/3）。`;
    case SpotKind.Puzzle: return text.puzzlePrompt;
    case SpotKind.Lore: return '墙缝里夹着一页泛黄的纸。';
    case SpotKind.FogSource: return forkText(c).fogSourceDescription;
    case SpotKind.Vantage: return forkText(c).vantageDescription;
    case SpotKind.Shortcut:
      return spot.variant === 0
        ? `墙后就是${farName(floor)}的补给点。砸开它，不穿过${forkText(c).fog}也能过去。`
        : `拉下机关，桥会落到${farName(floor)}的补给点旁边。`;
    default: return '';
  }
}
const makeOption = (id, label, detail, available = true, reason = '') =>
  ({ id, label, detail, available, reason: available ? '' : reason });
const stripText = `本层守卫失去最危险的一个词缀（护盾 > 剧毒 > 治疗 > 爆炸）；没有这些词缀时改为攻击 -${100 - WeakenPercent}%`;
function vaultContents(hero, floor, keys) {
  const c = chapterOf(floor);
  if (keys === 2)
    return `本职业${hero.runState().lootTarget === 2 ? '防具' : '武器'}（保底目标部位，本层最高品质档）与 2 份材料`;
  seedFor(hero.runState().seed, floor, VaultStream);
  const roll = below(100);
  if (roll < VaultMaterialsRoll) return `${vaultMaterials(c)} 份材料`;
  if (roll < VaultPotionRoll) return `${potionText(1)}与 ${coinUnit(floor) * 2} 金币`;
  return `${coinUnit(floor) * 5} 金币`;
}

function stashContents(seedValue, floor) {
  const roll = stashRoll(seedValue, floor);
  if (roll < VaultMaterialsRoll) return `${vaultMaterials(chapterOf(floor))} 份材料`;
  if (roll < VaultPotionRoll) return `${potionText(1)}与 ${coinUnit(floor) * 2} 金币`;
  return `${coinUnit(floor) * 5} 金币`;
}
function openStash(hero, floor) {
  const roll = stashRoll(hero.runState().seed, floor);
  if (roll < VaultMaterialsRoll) return `获得 ${addMaterials(hero, vaultMaterials(chapterOf(floor)))} 份材料`;
  if (roll < VaultPotionRoll) {
    givePotions(hero, floor, 1);
    hero.gaincoin(coinUnit(floor) * 2);
    return `获得 1 瓶药水与 ${coinUnit(floor) * 2} 金币`;
  }
  hero.gaincoin(coinUnit(floor) * 5);
  return `获得 ${coinUnit(floor) * 5} 金币`;
}
function patrolReport(hero, floor) {
  const monster = makePatrol(hero, floor);
  const affixes = monster.getAffixes().map((affix) => affixName(affix)).join('、');
  return `${monster.getname()}（生命 ${monster.getmaxhp()}，攻击 ${monster.getattack()}，词缀 ${affixes}）`;
}
// What the high ground shows: the patrol and the stash, for the branches the fork has.
function scoutReport(hero, floor, list) {
  let text = '';
  for (const branch of list) {
    if (branch === Branch.Patrol) text += `巡逻近路是${patrolReport(hero, floor)}`;
    else if (branch === Branch.Locked)
      text += `${text ? '；' : ''}${forkText(chapterOf(floor)).stash}里是 ${stashContents(hero.runState().seed, floor)}`;
  }
  return text;
}

function finish(result, data = {}) {
  if (result.ok && result.text) emit('explore', result.text, data);
  return result;
}
function stripAffix(monster, source, notes) {
  for (const affix of Strippable)
    if (monster.hasAffix(affix)) {
      monster.removeAffix(affix);
      notes?.push(`${source}：移除【${affixName(affix)}】`);
      return;
    }
  monster.resetattack(Math.max(1, idiv(monster.getattack() * WeakenPercent, 100)));
  notes?.push(`${source}：攻击 -${100 - WeakenPercent}%`);
}

// ---- Generation ------------------------------------------------------------------------
// One chapter's fixed layout, as floor numbers (local floors 1..9).
export function chapterPlan(seedValue, chapter) {
  chapter = clamp(chapter, 0, 9);
  seedFor(seedValue, chapter * 10 + 1, PlanStream);
  const base = chapter * 10;
  const draw = (pool, count) => Array.from({ length: count }, () => base + pick(pool)).sort((x, y) => x - y);
  // Draw one floor at a time, in this order; each list is then sorted.
  const vaultFloors = draw([3, 4, 5, 6, 7, 8, 9], 2); // [0] needs one key, [1] needs two.
  const keyFloors = draw([1, 2, 3, 4, 5, 6, 7, 8], 2);
  const merchantFloors = draw([2, 3, 4, 5, 6, 7, 8, 9], 2);
  const questFloors = draw([1, 2, 3, 4, 5, 6, 7, 8, 9], 3);
  const puzzleFloor = base + pick([3, 4, 5, 6, 7, 8, 9]);
  // Forks draw from their own stream so the floors above never move. Never the
  // first floor, the elite floor, a vault floor or the puzzle floor.
  seedFor(seedValue, chapter * 10 + 1, ForkPlanStream);
  const forkPool = [];
  for (let floor = base + 2; floor <= base + 9; ++floor)
    if (floor !== eliteFloor(chapter) && floor !== puzzleFloor && !vaultFloors.includes(floor)) forkPool.push(floor);
  const forkFloors = Array.from({ length: ForkFloorsPerChapter }, () => pick(forkPool)).sort((x, y) => x - y);
  return { keyFloors, vaultFloors, merchantFloors, questFloors, puzzleFloor, forkFloors };
}

// Pure generation from the run seed; usable without a hero (save validation).
// Spots keep their indices when forks change: a fork floor lists the same
// spots as before, moves its open caches and gathering spots onto the fog
// branch, and appends its own spots after them. Each spot is
// { kind, variant, zone, branch }.
export function generate(seedValue, floor) {
  if (!validFloor(floor)) return [];
  const local = localOf(floor);
  const boss = local === 10;
  const plan = chapterPlan(seedValue, chapterOf(floor));
  seedFor(seedValue, floor, FloorStream);
  const spots = [];
  const caches = boss ? 1 : 1 + below(2);
  for (let i = 0; i < caches; ++i)
    spots.push({ kind: SpotKind.Cache, variant: 0, zone: below(100) < 25 ? Zone.Hidden : Zone.Middle });
  const curios = boss ? 1 : 1 + below(2);
  for (let i = 0; i < curios; ++i) spots.push({ kind: SpotKind.Curio, variant: below(3), zone: Zone.Middle });
  if (!boss) {
    const first = below(100) < GatherPercent ? 1 : 0;
    const gathers = first + (below(100) < SecondGatherPercent ? 1 : 0);
    for (let i = 0; i < gathers; ++i) spots.push({ kind: SpotKind.Gather, variant: below(2), zone: Zone.Middle });
    const roll = below(100);
    if (local >= 2 && roll < AffixSourcePercent) spots.push({ kind: SpotKind.AffixSource, variant: 0, zone: Zone.Guardian });
  }
  for (const keyFloor of plan.keyFloors)
    if (keyFloor === floor) spots.push({ kind: SpotKind.Key, variant: 0, zone: below(100) < 50 ? Zone.Hidden : Zone.Middle });
  for (let i = 0; i < 2; ++i)
    if (plan.vaultFloors[i] === floor) spots.push({ kind: SpotKind.Vault, variant: i + 1, zone: Zone.Vault });
  for (const merchantFloor of plan.merchantFloors)
    if (merchantFloor === floor) spots.push({ kind: SpotKind.Merchant, variant: 0, zone: Zone.Middle });
  for (let i = 0; i < QuestItemsPerChapter; ++i)
    if (plan.questFloors[i] === floor) spots.push({ kind: SpotKind.QuestItem, variant: i, zone: Zone.Middle });
  if (plan.puzzleFloor === floor) spots.push({ kind: SpotKind.Puzzle, variant: 0, zone: Zone.Middle });
  spots.push({ kind: SpotKind.Lore, variant: floor, zone: Zone.Hidden });
  for (const spot of spots) spot.branch = Branch.None;
  if (!plan.forkFloors.includes(floor)) return spots;
  // The far branch carries the floor's supplies: open caches, gathering spots
  // and, past the fog, a medicine chest.
  seedFor(seedValue, floor, ForkStream);
  drawBranches();
  for (const spot of spots)
    if ((spot.kind === SpotKind.Cache && spot.zone === Zone.Middle) || spot.kind === SpotKind.Gather) {
      spot.zone = Zone.Branch;
      spot.branch = Branch.Fog;
    }
  spots.push({ kind: SpotKind.FogSource, variant: 0, zone: below(100) < HiddenSourcePercent ? Zone.Hidden : Zone.Middle,
    branch: Branch.None });
  spots.push({ kind: SpotKind.Vantage, variant: 0, zone: Zone.Middle, branch: Branch.None });
  spots.push({ kind: SpotKind.Cache, variant: 1, zone: Zone.Branch, branch: Branch.Fog });
  if (below(100) < ShortcutPercent)
    spots.push({ kind: SpotKind.Shortcut, variant: below(2), zone: Zone.Guardian, branch: Branch.None });
  return spots;
}

export function isForkFloor(seedValue, floor) {
  return validFloor(floor) && chapterPlan(seedValue, chapterOf(floor)).forkFloors.includes(floor);
}

// The fork's branches in the order near, far, key; empty without a fork.
export function branches(seedValue, floor) {
  if (!isForkFloor(seedValue, floor)) return [];
  seedFor(seedValue, floor, ForkStream);
  return drawBranches();
}

// Whether finding everything on a floor includes this spot (merchants do not).
export const countsTowardCompletion = (kind) => kind !== SpotKind.Merchant;

// Each spot with its index (pass to options()/interact()), state and texts.
// blocked says why the spot cannot be reached yet on a fork floor (past the
// fork, or on a branch that was not taken); '' when it can.
export function survey(hero, floor) {
  const run = hero.runState();
  const spots = generate(run.seed, floor);
  return spots.map((spot, index) => ({
    index, spot, done: spotDone(run, floor, index), name: spotName(spot, floor),
    description: spotDescription(hero, spot, floor), blocked: blockedReason(run, spots, floor, index),
  }));
}

// Spots that count toward completion, are not explored yet and still matter:
// a branch that was not taken stays open only through a shortcut, and the
// shortcut is moot once the fog branch was walked.
export function remaining(hero, floor) {
  const run = hero.runState();
  const spots = generate(run.seed, floor);
  return spots.filter((spot, index) => !spotDone(run, floor, index) && countsTowardCompletion(spot.kind) &&
    !settledByBranch(run, spots, floor, index)).length;
}

// Choices for a spot; empty for an invalid, unreachable or finished spot. A spot
// that cannot be reached yet (survey's blocked) lists its choices as unavailable.
export function options(hero, floor, index) {
  if (!reachable(hero, floor)) return [];
  const spots = generate(hero.runState().seed, floor);
  if (!Number.isInteger(index) || index < 0 || index >= spots.length) return [];
  const spot = spots[index];
  const run = hero.runState();
  if (spot.kind !== SpotKind.Merchant && spotDone(run, floor, index)) return [];
  const c = chapterOf(floor);
  const text = chapterText(c);
  const unit = coinUnit(floor);
  const unit2 = coinUnit(floor) * 2;
  const expText = expUnit(floor);
  const materials = `（现有 ${run.materials}）`;
  const list = [];
  switch (spot.kind) {
    case SpotKind.Cache:
      if (spot.variant === 1) {
        list.push(makeOption(1, '打开', '必定获得 1 瓶药水'));
        break;
      }
      list.push(makeOption(1, '打开', `${CacheGold}%：${unit} 金币；${CachePotion - CacheGold}%：1 瓶药水；` +
        `${100 - CachePotion}%：1 份材料与 ${idiv(coinUnit(floor), 2)} 金币`));
      break;
    case SpotKind.Curio:
      if (spot.variant === 0) {
        list.push(makeOption(1, '徒手打开', `${ContainerGold}%：${unit2} 金币；${ContainerPotion - ContainerGold}%：1 瓶药水；` +
          `${100 - ContainerPotion}%：陷阱，损失当前生命 ${TrapHealthPercent}%（不会致死）`));
        list.push(makeOption(2, '用 2 份材料撬开', `必定获得 ${unit2} 金币和 1 瓶药水${materials}`,
          run.materials >= 2, `材料不足（需要 2，现有 ${run.materials}）`));
      } else if (spot.variant === 1) {
        list.push(makeOption(1, '祈祷', `${ShrineBoon}%：祝福（下一场战斗${boonDescription(1)}）；` +
          `${ShrineGold - ShrineBoon}%：${unit} 金币；${100 - ShrineGold}%：诅咒（下一场战斗${baneDescription(1)}）`));
        const price = shrinePrice(floor);
        list.push(makeOption(2, `供奉 ${price} 金币`, `必定获得祝福（下一场战斗${boonDescription(1)}），并解除已有的诅咒`,
          hero.getcoin() >= price, '金币不足'));
      } else {
        list.push(makeOption(1, '触碰', `${RelicMaterials}%：2 份材料；${RelicExperience - RelicMaterials}%：${expText} 经验；` +
          `${100 - RelicExperience}%：反噬，失去当前魔力的 ${BacklashManaPercent}%，下一场战斗${baneDescription(2)}`));
        const mana = relicManaCost(hero);
        list.push(makeOption(2, `注入 ${mana} 魔力（最大魔力的 ${RelicManaPercent}%）`, `必定获得 2 份材料和 ${expText} 经验`,
          hero.getmp() >= mana, '魔力不足'));
      }
      break;
    case SpotKind.Gather: {
      const ingredient = text.gatherIngredient[spot.variant];
      list.push(makeOption(1, '采集', `获得 1 份${IngredientNames[ingredient]}，${GatherBonusPercent}% 再多 1 份` +
        `（现有 ${run.ingredients[ingredient]}，上限 99）`, run.ingredients[ingredient] < MaxIngredients, '这种食材已经带满了'));
      break;
    }
    case SpotKind.AffixSource:
      list.push(makeOption(1, '强行破坏', `损失当前生命 ${SourceHealthPercent}%（不会致死）；${stripText}`));
      list.push(makeOption(2, `小心拆除（${SourceMaterials} 份材料）`, stripText + materials,
        run.materials >= SourceMaterials, `材料不足（需要 ${SourceMaterials}，现有 ${run.materials}）`));
      break;
    case SpotKind.Key:
      list.push(makeOption(1, '拾取', `钥匙 +1（现有 ${run.keys}，上限 99）`, run.keys < MaxKeys, '钥匙已经带满了'));
      break;
    case SpotKind.Vault:
      list.push(makeOption(1, `用 ${spot.variant} 把钥匙打开`,
        `里面是：${vaultContents(hero, floor, spot.variant)}（现有钥匙 ${run.keys}）`, run.keys >= spot.variant,
        `钥匙不足（需要 ${spot.variant}，现有 ${run.keys}）`));
      break;
    case SpotKind.Merchant:
      merchantGoods(hero, floor).forEach((good, i) => {
        const full = good.kind === 1 && run.keys >= MaxKeys;
        list.push(makeOption(i + 1, `购买${good.name}（${good.price} 金币）`, good.description,
          !good.sold && !full && hero.getcoin() >= good.price, good.sold ? '已售出' : full ? '钥匙已经带满了' : '金币不足'));
      });
      break;
    case SpotKind.QuestItem:
      list.push(makeOption(1, '收好', `${Chapters[c].ally}的遗物（本章 ${run.questItems[c]}/3）；集齐 3 件获得 2 份材料，` +
        `本章 Boss 战开局生命 -${100 - AmbushHealthPercent}%`));
      break;
    case SpotKind.Puzzle:
      list.push(makeOption(1, '解谜', `按线索排出唯一正确的顺序；答错损失当前生命 ${PuzzleHealthPercent}%（不会致死），可以重试。` +
        `奖励：1 把钥匙、${puzzleMaterials(c)} 份材料、${expText} 经验`));
      break;
    case SpotKind.Lore:
      list.push(makeOption(1, '阅读', '收入见闻录（远征档案中查看，跨远征保留）'));
      break;
    case SpotKind.FogSource:
      list.push(makeOption(1, '封住源头', `${farName(floor)}不再损失生命`));
      break;
    case SpotKind.Vantage: {
      const ways = branches(run.seed, floor);
      const patrol = ways.includes(Branch.Patrol);
      const locked = ways.includes(Branch.Locked);
      list.push(makeOption(1, '登高远望', `看清${patrol ? '巡逻精英的生命、攻击和词缀' : ''}${patrol && locked ? '，以及' : ''}` +
        `${locked ? `${forkText(c).stash}里的东西` : ''}`));
      break;
    }
    case SpotKind.Shortcut:
      list.push(makeOption(1, spot.variant === 0 ? '砸开' : '拉下机关',
        `打通到${farName(floor)}补给点的捷径：走哪条路都能拿到沿途的补给`));
      break;
    default:
      break;
  }
  const blocked = blockedReason(run, spots, floor, index);
  if (blocked) for (const option of list) Object.assign(option, { available: false, reason: blocked });
  return list;
}

// Applies a choice and returns { ok, text, lorePage }. A floor above the
// highest reached, a dead hero, a finished spot or an unaffordable choice
// changes nothing. lorePage (1..100) asks the caller to record the page in
// the profile. Puzzles are answered with solvePuzzle().
export function interact(hero, floor, index, option) {
  const result = { ok: false, text: '', lorePage: 0 };
  if (!reachable(hero, floor)) return result;
  const run = hero.runState();
  const seedValue = run.seed;
  const spots = generate(seedValue, floor);
  if (!Number.isInteger(index) || index < 0 || index >= spots.length) return result;
  const spot = spots[index];
  if (spot.kind !== SpotKind.Merchant && spotDone(run, floor, index)) return result;
  const chosen = options(hero, floor, index).find((o) => o.id === option);
  if (!chosen || !chosen.available || spot.kind === SpotKind.Puzzle) return result;
  const c = chapterOf(floor);
  const text = chapterText(c);
  const name = spotName(spot, floor);
  seedFor(seedValue, floor, SpotStream + index);
  let outcome = '';
  let experience = 0; // Granted after the text, so level-up messages follow it.
  switch (spot.kind) {
    case SpotKind.Cache: {
      if (spot.variant === 1) {
        givePotions(hero, floor, 1);
        outcome = '获得 1 瓶药水';
        break;
      }
      const roll = below(100);
      if (roll < CacheGold) {
        hero.gaincoin(coinUnit(floor));
        outcome = `获得 ${coinUnit(floor)} 金币`;
      } else if (roll < CachePotion) {
        givePotions(hero, floor, 1);
        outcome = '获得 1 瓶药水';
      } else {
        const gained = addMaterials(hero, 1);
        hero.gaincoin(idiv(coinUnit(floor), 2));
        outcome = `获得 ${gained} 份材料与 ${idiv(coinUnit(floor), 2)} 金币`;
      }
      break;
    }
    case SpotKind.Curio:
      if (spot.variant === 0) {
        if (option === 2) {
          run.materials -= 2;
          hero.gaincoin(coinUnit(floor) * 2);
          givePotions(hero, floor, 1);
          outcome = `用材料撬开：获得 ${coinUnit(floor) * 2} 金币和 1 瓶药水`;
        } else {
          const roll = below(100);
          if (roll < ContainerGold) {
            hero.gaincoin(coinUnit(floor) * 2);
            outcome = `获得 ${coinUnit(floor) * 2} 金币`;
          } else if (roll < ContainerPotion) {
            givePotions(hero, floor, 1);
            outcome = '获得 1 瓶药水';
          } else {
            loseHealthSafely(hero, TrapHealthPercent);
            outcome = `触发了陷阱，损失当前生命的 ${TrapHealthPercent}%`;
          }
        }
      } else if (spot.variant === 1) {
        if (option === 2) {
          hero.costcoin(shrinePrice(floor));
          run.boon = 1;
          run.bane = 0;
          outcome = `供奉后获得祝福：${boonDescription(1)}（下一场战斗）`;
        } else {
          const roll = below(100);
          if (roll < ShrineBoon) {
            run.boon = 1;
            outcome = `获得祝福：${boonDescription(1)}（下一场战斗）`;
          } else if (roll < ShrineGold) {
            hero.gaincoin(coinUnit(floor));
            outcome = `神龛前散落着供品：获得 ${coinUnit(floor)} 金币`;
          } else {
            run.bane = 1;
            outcome = `受到诅咒：${baneDescription(1)}（下一场战斗）`;
          }
        }
      } else {
        const exp = expUnit(floor);
        if (option === 2) {
          hero.setmp(hero.getmp() - relicManaCost(hero));
          const gained = addMaterials(hero, 2);
          outcome = `注入魔力后遗物平静下来：获得 ${gained} 份材料和 ${exp} 经验`;
          experience = exp;
        } else {
          const roll = below(100);
          if (roll < RelicMaterials) {
            outcome = `获得 ${addMaterials(hero, 2)} 份材料`;
          } else if (roll < RelicExperience) {
            outcome = `遗物里封着前人的记忆：获得 ${exp} 经验`;
            experience = exp;
          } else {
            hero.setmp(hero.getmp() - idiv(hero.getmp() * BacklashManaPercent, 100));
            run.bane = 2;
            outcome = `遗物反噬：失去当前魔力的 ${BacklashManaPercent}%，${baneDescription(2)}（下一场战斗）`;
          }
        }
      }
      break;
    case SpotKind.Gather: {
      const ingredient = text.gatherIngredient[spot.variant];
      const amount = 1 + (below(100) < GatherBonusPercent ? 1 : 0);
      const gained = addIngredient(run, ingredient, amount);
      outcome = `采集到 ${gained} 份${IngredientNames[ingredient]}`;
      break;
    }
    case SpotKind.AffixSource:
      if (option === 2) run.materials -= SourceMaterials;
      else loseHealthSafely(hero, SourceHealthPercent);
      outcome = `${option === 2 ? '小心拆除了' : '强行破坏了'}${name}：本层守卫会失去最危险的词缀（没有时攻击 -${100 - WeakenPercent}%）`;
      break;
    case SpotKind.Key:
      addKeys(run, 1);
      outcome = `拾得宝库钥匙（现有 ${run.keys}）`;
      break;
    case SpotKind.Vault:
      run.keys -= spot.variant;
      run.vaultsOpened = addCounter(run.vaultsOpened, 1);
      if (spot.variant === 2) {
        seedFor(seedValue, floor, VaultGearStream);
        emit('notice', `打开${name}：`);
        grantItem(hero, generateEliteReward(hero, floor));
        outcome = `另获得 ${addMaterials(hero, 2)} 份材料`;
      } else {
        seedFor(seedValue, floor, VaultStream);
        const roll = below(100);
        if (roll < VaultMaterialsRoll) {
          outcome = `打开${name}：获得 ${addMaterials(hero, vaultMaterials(c))} 份材料`;
        } else if (roll < VaultPotionRoll) {
          givePotions(hero, floor, 1);
          hero.gaincoin(coinUnit(floor) * 2);
          outcome = `打开${name}：获得 1 瓶药水与 ${coinUnit(floor) * 2} 金币`;
        } else {
          hero.gaincoin(coinUnit(floor) * 5);
          outcome = `打开${name}：获得 ${coinUnit(floor) * 5} 金币`;
        }
      }
      break;
    case SpotKind.Merchant: {
      const good = merchantGoods(hero, floor)[option - 1];
      hero.costcoin(good.price);
      run.explored[floor] |= 1 << (MerchantBit + option - 1);
      markExplored(run, floor, index);
      if (good.kind === 0) {
        seedFor(seedValue, floor, BlindBoxStream + option);
        const roll = below(100);
        if (roll < BoxRefund) {
          hero.gaincoin(good.price);
          outcome = `盲盒里是一袋金币：${good.price}`;
        } else if (roll < BoxPotion) {
          givePotions(hero, floor, 1);
          outcome = '盲盒里是 1 瓶药水';
        } else if (roll < BoxMaterials) {
          outcome = `盲盒里是 ${addMaterials(hero, 2)} 份材料`;
        } else if (roll < BoxIngredients) {
          const a = below(IngredientCount);
          const b = below(IngredientCount);
          addIngredient(run, a, 1);
          addIngredient(run, b, 1);
          outcome = `盲盒里是${IngredientNames[a]}和${IngredientNames[b]}`;
        } else if (addKeys(run, 1)) {
          outcome = '盲盒里是一把宝库钥匙！';
        } else {
          hero.gaincoin(good.price);
          outcome = `盲盒里是一把钥匙，但钥匙已经带满，商人退回了 ${good.price} 金币`;
        }
      } else if (good.kind === 1) {
        addKeys(run, 1);
        outcome = `买到宝库钥匙（现有 ${run.keys}）`;
      } else if (good.kind === 2) {
        run.boon = 2;
        outcome = `戴上护符：${boonDescription(2)}（下一场战斗）`;
      } else {
        const a = text.gatherIngredient[0];
        const b = text.gatherIngredient[1];
        addIngredient(run, a, 1);
        addIngredient(run, b, 1);
        outcome = `买到${IngredientNames[a]}和${IngredientNames[b]}各 1 份`;
      }
      result.ok = true;
      result.text = outcome;
      return finish(result, { floor, index, kind: spot.kind });
    }
    case SpotKind.QuestItem: {
      run.questItems[c] = Math.min(QuestItemsPerChapter, run.questItems[c] + 1);
      const ally = Chapters[c].ally;
      outcome = `收好${name}（${ally}的遗物 ${run.questItems[c]}/3）`;
      if (run.questItems[c] === QuestItemsPerChapter)
        outcome += `。集齐了！获得 ${addMaterials(hero, 2)} 份材料；第${(c + 1) * 10}层 Boss 战开局，${ally}会先手偷袭` +
          `（Boss 生命 -${100 - AmbushHealthPercent}%）`;
      break;
    }
    case SpotKind.Lore: {
      const page = lore(floor);
      result.lorePage = floor;
      outcome = `【见闻录 ${floor}/100 · ${page.title}】${page.text}`;
      break;
    }
    case SpotKind.FogSource:
      outcome = `封住了${name}：${forkText(c).fog}散去，${farName(floor)}可以安全通过`;
      break;
    case SpotKind.Vantage:
      outcome = `站在${name}上远望：${scoutReport(hero, floor, branches(seedValue, floor))}`;
      break;
    case SpotKind.Shortcut:
      outcome = `${spot.variant === 0 ? '砸开了' : '拉下了'}${name}：现在不管走哪条路，都能去${farName(floor)}的补给点`;
      break;
    default:
      return result;
  }
  markExplored(run, floor, index);
  result.ok = true;
  result.text = outcome;
  finish(result, { floor, index, kind: spot.kind, lorePage: result.lorePage });
  hero.gainexp(experience);
  return result;
}

// ---- Forks ------------------------------------------------------------------------------
export const branchTaken = (hero, floor) => (validFloor(floor) ? takenOf(hero.runState(), floor) : Branch.None);

// False on a fork floor until a branch is taken (floors cleared before forks
// existed stay open). Frontends check it before starting the guardian's fight.
export const guardianReachable = (hero, floor) =>
  validFloor(floor) && guardianOpen(hero.runState(), floor, generate(hero.runState().seed, floor));

// { branches: [{ kind, name, cost, reward, available, reason }], taken, hint };
// branches is empty when the floor has no fork.
export function fork(hero, floor) {
  const result = { branches: [], taken: Branch.None, hint: '' };
  const list = branches(hero.runState().seed, floor);
  if (!list.length) return result;
  const run = hero.runState();
  const spots = generate(run.seed, floor);
  const text = forkText(chapterOf(floor));
  const scouted = kindDone(run, spots, floor, SpotKind.Vantage);
  const safe = kindDone(run, spots, floor, SpotKind.FogSource);
  result.taken = takenOf(run, floor);
  for (const kind of list) {
    const view = { kind, name: branchName(kind, floor), cost: '', reward: '', available: false, reason: '' };
    if (kind === Branch.Patrol) {
      const monster = makePatrol(hero, floor);
      view.cost = scouted ? `打赢${patrolReport(hero, floor)}`
        : `打赢巡逻精英（至少两个词缀，比普通敌人多 ${EnemyTuning.PatrolHpPercent - 100}% 生命）；登上${text.vantage}可以看清它`;
      view.reward = `${monster.getexp_reward()} 经验、${monster.getcoin_reward()} 金币、${PatrolMaterials} 份材料；输了可以改走别的路`;
    } else if (kind === Branch.Fog) {
      view.cost = safe ? '源头已封住，安全通过'
        : `穿过${text.fog}：损失当前生命 ${FogHealthPercent}%（不会致死）；封住${text.fogSource}就不会`;
      const supplies = spots.filter((spot) => spot.zone === Zone.Branch && spot.branch === Branch.Fog)
        .map((spot) => spotName(spot, floor));
      view.reward = `沿途 ${supplies.length} 处补给：${supplies.join('、')}`;
    } else {
      view.cost = `1 把钥匙（现有 ${run.keys}）`;
      view.reward = text.stash + (scouted ? `：${stashContents(run.seed, floor)}` : `（内容不明，登上${text.vantage}可以看清）`);
    }
    if (result.taken === kind) view.reason = '已经走了这条路';
    else if (result.taken !== Branch.None) view.reason = `已经走了${branchName(result.taken, floor)}，这条路关闭了`;
    else if (!reachable(hero, floor)) view.reason = '现在无法通行';
    else if (kind === Branch.Locked && run.keys < 1) view.reason = '钥匙不足（现有 0）';
    view.available = !view.reason;
    result.branches.push(view);
  }
  if (result.taken === Branch.None) {
    result.hint = '岔路只能选一条，选定后其他路关闭。';
    if (!scouted) result.hint += `登上${text.vantage}能先看清路上有什么。`;
    if (!safe) result.hint += `找到${text.fogSource}并封住，远路就不再伤人。`;
  } else {
    result.hint = `已走：${branchName(result.taken, floor)}`;
    if (result.taken !== Branch.Fog && hasKind(spots, SpotKind.Shortcut))
      result.hint += kindDone(run, spots, floor, SpotKind.Shortcut) ? '；捷径已打通，远路的补给也能拿'
        : '；守卫房间附近有通往远路补给的捷径';
  }
  return result;
}

// Takes the fog or key branch; returns { ok, text, lorePage }. The patrol
// branch is taken by winning its fight (Campaign.settlePatrol). Taking a
// branch closes the others for the run.
export function takeBranch(hero, floor, branch) {
  const result = { ok: false, text: '', lorePage: 0 };
  if (!reachable(hero, floor) || branch === Branch.Patrol) return result;
  const run = hero.runState();
  if (!branches(run.seed, floor).includes(branch) || takenOf(run, floor) !== Branch.None) return result;
  const text = forkText(chapterOf(floor));
  if (branch === Branch.Fog) {
    if (kindDone(run, generate(run.seed, floor), floor, SpotKind.FogSource)) {
      result.text = `源头已经封住，安全穿过了${text.fog}。沿途的补给可以拿了`;
    } else {
      loseHealthSafely(hero, FogHealthPercent);
      result.text = `硬闯${text.fog}：损失当前生命的 ${FogHealthPercent}%。沿途的补给可以拿了`;
    }
  } else {
    if (run.keys < 1) return result;
    run.keys -= 1;
    result.text = `用 1 把钥匙打开小径，${text.stash}：${openStash(hero, floor)}`;
  }
  run.explored[floor] |= branch << BranchBit;
  result.ok = true;
  return finish(result, { floor, branch });
}

// For Campaign.settlePatrol: records the patrol branch as taken.
export function recordPatrolWin(hero, floor) {
  const run = hero.runState();
  if (branches(run.seed, floor).includes(Branch.Patrol) && takenOf(run, floor) === Branch.None)
    run.explored[floor] |= Branch.Patrol << BranchBit;
}

// The merchant's goods on this floor ({ kind, name, description, price, sold });
// empty when the floor has no merchant. Kinds: 0 blind box, 1 key, 2 charm,
// 3 ingredient pack.
export function merchantGoods(hero, floor) {
  const run = hero.runState();
  if (!generate(run.seed, floor).some((s) => s.kind === SpotKind.Merchant)) return [];
  seedFor(run.seed, floor, MerchantStream);
  const kinds = [0, 1, 2, 3];
  kinds.splice(below(4), 1);
  const text = chapterText(chapterOf(floor));
  return kinds.map((kind, i) => {
    const good = { kind, name: '', description: '', price: merchantPrice(hero, floor, kind),
      sold: ((run.explored[floor] >> (MerchantBit + i)) & 1) !== 0 };
    switch (kind) {
      case 0:
        good.name = '神秘盲盒';
        good.description = `${BoxRefund}%：退回等额金币；${BoxPotion - BoxRefund}%：1 瓶药水；${BoxMaterials - BoxPotion}%：2 份材料；` +
          `${BoxIngredients - BoxMaterials}%：两份随机食材；${100 - BoxIngredients}%：1 把宝库钥匙`;
        break;
      case 1:
        good.name = '宝库钥匙';
        good.description = '钥匙 +1，跨章节保留';
        break;
      case 2:
        good.name = '守护护符';
        good.description = `祝福：${boonDescription(2)}（下一场战斗；替换已有的祝福）`;
        break;
      default:
        good.name = '特产食材';
        good.description = `${IngredientNames[text.gatherIngredient[0]]}和${IngredientNames[text.gatherIngredient[1]]}各 1 份`;
        break;
    }
    return good;
  });
}

export const hasPuzzle = (hero, floor) => generate(hero.runState().seed, floor).some((s) => s.kind === SpotKind.Puzzle);

// The chapter puzzle on this floor: { title, prompt, elements, clues }.
// Elements are numbered from 0; an answer lists every element once, first to
// last. Exactly one order fits the clues.
export function puzzle(hero, floor) {
  if (!hasPuzzle(hero, floor)) return { title: '', prompt: '', elements: [], clues: [] };
  const text = chapterText(chapterOf(floor));
  const generated = generatePuzzle(hero.runState().seed, floor);
  return {
    title: text.puzzleTitle,
    prompt: text.puzzlePrompt,
    elements: generated.solution.map((_, i) => text.puzzleElements[i]),
    clues: generated.clues.map((clue) => clueText(clue, text)),
  };
}

// The unique answer; for tests and simulations.
export const puzzleSolution = (seedValue, floor) => (validFloor(floor) ? generatePuzzle(seedValue, floor).solution : []);

// A wrong order costs 5% of current HP (never lethal) and may be retried.
export function solvePuzzle(hero, floor, order) {
  const result = { ok: false, text: '', lorePage: 0 };
  if (!reachable(hero, floor)) return result;
  const spots = generate(hero.runState().seed, floor);
  const index = spots.map((s) => s.kind).lastIndexOf(SpotKind.Puzzle);
  const run = hero.runState();
  if (index < 0 || spotDone(run, floor, index)) return result;
  const solution = puzzleSolution(run.seed, floor);
  if (!Array.isArray(order) || order.length !== solution.length ||
    [...order].sort((a, b) => a - b).some((value, i) => value !== i)) return result; // Not an order of every element.
  result.ok = true;
  if (order.some((value, i) => value !== solution[i])) {
    loseHealthSafely(hero, PuzzleHealthPercent);
    result.text = `顺序不对，机关反震：损失当前生命的 ${PuzzleHealthPercent}%。线索都是真的，再想想。`;
    return finish(result, { floor, index, kind: SpotKind.Puzzle, solved: false });
  }
  const c = chapterOf(floor);
  markExplored(run, floor, index);
  run.puzzlesSolved = addCounter(run.puzzlesSolved, 1);
  addKeys(run, 1);
  const materials = addMaterials(hero, puzzleMaterials(c));
  const exp = expUnit(floor);
  result.text = `解开了${chapterText(c).puzzleTitle}！获得 1 把宝库钥匙、${materials} 份材料和 ${exp} 经验`;
  finish(result, { floor, index, kind: SpotKind.Puzzle, solved: true });
  hero.gainexp(exp);
  return result;
}

// ---- Camp cooking --------------------------------------------------------------------
// Recipes use two ingredients; a meal sets the boon for the next encounter
// (replacing the old one). needs is empty for the stew: any two different.
export const Recipes = [
  { id: 1, name: '烤鱼串', needs: [2, 0, 0, 0, 0] },
  { id: 2, name: '药草汤', needs: [0, 2, 0, 0, 0] },
  { id: 3, name: '岩盐烤饼', needs: [0, 0, 2, 0, 0] },
  { id: 4, name: '霜晶冰沙', needs: [0, 0, 0, 2, 0] },
  { id: 5, name: '星尘糖', needs: [0, 0, 0, 0, 2] },
  { id: 6, name: '远征杂烩', needs: [0, 0, 0, 0, 0] },
];
const RecipeBoons = [1, 2, 1, 3, 4, 2];
export const recipeDescription = (recipe) =>
  (recipe === 6 ? '任意两种不同食材各 1 份（用最多的两种）；' : '') + `祝福：${boonDescription(RecipeBoons[recipe - 1])}`;
// The recipe table with descriptions, like the C++ recipes().
export const recipes = () => Recipes.map((recipe) => ({ ...recipe, description: recipeDescription(recipe.id) }));
export const ingredientName = (ingredient) => IngredientNames[clamp(ingredient, 0, IngredientCount - 1)];

// The stew's two ingredients: the two most plentiful kinds, lower id on ties.
function stewIngredients(run) {
  const best = [-1, -1];
  for (let i = 0; i < IngredientCount; ++i) {
    if (run.ingredients[i] <= 0) continue;
    if (best[0] < 0 || run.ingredients[i] > run.ingredients[best[0]]) {
      best[1] = best[0];
      best[0] = i;
    } else if (best[1] < 0 || run.ingredients[i] > run.ingredients[best[1]]) {
      best[1] = i;
    }
  }
  return best;
}

export function canCook(hero, recipe) {
  if (!hero.isalive() || !Number.isInteger(recipe) || recipe < 1 || recipe > RecipeCount) return false;
  const run = hero.runState();
  if (recipe === 6) return stewIngredients(run)[1] >= 0;
  return Recipes[recipe - 1].needs.every((need, i) => run.ingredients[i] >= need);
}

export function cook(hero, recipe) {
  const result = { ok: false, text: '', lorePage: 0 };
  if (!canCook(hero, recipe)) return result;
  const run = hero.runState();
  if (recipe === 6) {
    for (const ingredient of stewIngredients(run)) --run.ingredients[ingredient];
  } else {
    Recipes[recipe - 1].needs.forEach((need, i) => { run.ingredients[i] -= need; });
  }
  run.mealsCooked = addCounter(run.mealsCooked, 1);
  // Meals only bless the next fight. Healing and MP stay with potions and rests:
  // a full MP bar before a boss is worth more than any blessing here.
  run.boon = RecipeBoons[recipe - 1];
  result.ok = true;
  result.text = `做好了${Recipes[recipe - 1].name}：祝福 ${boonDescription(run.boon)}（下一场战斗）`;
  return finish(result, { recipe, boon: run.boon });
}

// ---- Next encounter --------------------------------------------------------------------
export function boonDescription(boon) {
  switch (boon) {
    case 1: return `敌人攻击 -${100 - BoonAttackPercent}%`;
    case 2: return `敌人生命 -${100 - BoonHealthPercent}%`;
    case 3: return `敌人闪避 -${BoonDodgePoints} 个百分点`;
    case 4: return `敌人失去最危险的一个词缀（没有时攻击 -${100 - WeakenPercent}%）`;
    default: return '无';
  }
}
export function baneDescription(bane) {
  switch (bane) {
    case 1: return `敌人攻击 +${BaneAttackPercent - 100}%`;
    case 2: return `敌人生命 +${BaneHealthPercent - 100}%`;
    default: return '无';
  }
}

// Called by Campaign.makeEnemy: affix sources destroyed on this floor, the
// pending boon and bane, and the ally's ambush once the chapter's three quest
// items are found. notes (an array) receives one line per effect for previews.
export function applyToEncounter(hero, floor, monster, notes = null) {
  if (!validFloor(floor)) return;
  const run = hero.runState();
  const c = chapterOf(floor);
  // Without exploration nothing below draws random numbers or changes the enemy.
  if (run.explored[floor] & 0xffff) {
    generate(run.seed, floor).forEach((spot, i) => {
      if (spot.kind === SpotKind.AffixSource && ((run.explored[floor] >> i) & 1))
        stripAffix(monster, chapterText(c).affixSource, notes);
    });
  }
  switch (run.boon) {
    case 1:
      monster.resetattack(Math.max(1, idiv(monster.getattack() * BoonAttackPercent, 100)));
      notes?.push(`祝福：${boonDescription(1)}`);
      break;
    case 2:
      monster.resethp(Math.max(1, idiv(monster.getmaxhp() * BoonHealthPercent, 100)));
      notes?.push(`祝福：${boonDescription(2)}`);
      break;
    case 3:
      monster.setdodgeRate(Math.max(0, monster.getdodgeRate() - BoonDodgePoints));
      notes?.push(`祝福：${boonDescription(3)}`);
      break;
    case 4:
      stripAffix(monster, '祝福', notes);
      break;
    default:
      break;
  }
  if (run.bane === 1) {
    monster.resetattack(Math.max(1, idiv(monster.getattack() * BaneAttackPercent, 100)));
    notes?.push(`诅咒：${baneDescription(1)}`);
  } else if (run.bane === 2) {
    monster.resethp(Math.max(1, idiv(monster.getmaxhp() * BaneHealthPercent, 100)));
    notes?.push(`诅咒：${baneDescription(2)}`);
  }
  if (localOf(floor) === 10 && run.questItems[c] >= QuestItemsPerChapter) {
    monster.resethp(Math.max(1, idiv(monster.getmaxhp() * AmbushHealthPercent, 100)));
    notes?.push(`${Chapters[c].ally}的偷袭：生命 -${100 - AmbushHealthPercent}%`);
  }
}

// Called by Campaign.settleEncounter for every settled encounter.
export function finishEncounter(hero) {
  hero.runState().boon = 0;
  hero.runState().bane = 0;
}

// ---- Journal -----------------------------------------------------------------------------
export const loreTitle = (page) => lore(page).title;
export const loreText = (page) => lore(page).text;

// A chapter overview for the journal, same text as the terminal.
export function chapterSummary(hero, floor) {
  if (!validFloor(floor)) return '';
  const run = hero.runState();
  const c = chapterOf(floor);
  const plan = chapterPlan(run.seed, c);
  return `探索：钥匙 ${run.keys}；宝库 第${plan.vaultFloors[0]}层（1把）、第${plan.vaultFloors[1]}层（2把）；` +
    `谜题 第${plan.puzzleFloor}层；商人 第${plan.merchantFloors[0]}、${plan.merchantFloors[1]}层；` +
    `${Chapters[c].ally}的遗物 ${run.questItems[c]}/3；岔路 第${plan.forkFloors.join('、')}层\n食材：` +
    IngredientNames.map((name, i) => `${name}${run.ingredients[i]}`).join(' ') +
    `；下一场战斗：祝福 ${boonDescription(run.boon)}，诅咒 ${baneDescription(run.bane)}`;
}
