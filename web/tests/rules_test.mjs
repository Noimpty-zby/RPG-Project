// Browser-only rules: JSON saves, profile parsing, achievements and the
// texts that must agree with the rules.
//   node web/tests/rules_test.mjs
import * as Campaign from '../js/rules/campaign.js';
import { ActionType, BattleSession, Objective } from '../js/rules/combat.js';
import { Affix, Enemy, Intent } from '../js/rules/enemy.js';
import { collect } from '../js/rules/log.js';
import { Job } from '../js/rules/jobs.js';
import { Player } from '../js/rules/player.js';
import { Profile, Achievements } from '../js/rules/profile.js';
import { deserializeGame, serializeGame } from '../js/rules/save.js';
import { Item, ItemType } from '../js/rules/item.js';
import { HelpTopics } from '../js/rules/help.js';
import * as Exploration from '../js/rules/exploration.js';
import { choose } from './campaign_sim.mjs';

let failures = 0;
function check(condition, message) {
  if (!condition) {
    failures++;
    console.error(`FAIL: ${message}`);
  }
}
const clone = (value) => JSON.parse(JSON.stringify(value));
function rejects(data, message) {
  try {
    deserializeGame(data);
    check(false, `accepted invalid save: ${message}`);
  } catch {
    // Expected.
  }
}

// Play a few floors so the save has gear, skills, routes and history.
function playedHero(job) {
  const hero = new Player('存档测试', job);
  Campaign.initialize(hero, 2024, 0);
  for (let floor = 1; floor <= 12; floor++) {
    hero.setHighestFloor(floor);
    Campaign.chooseRoute(hero, floor, 2);
    if (Campaign.eventPending(hero, floor)) Campaign.resolveEvent(hero, floor, 2);
    const monster = Campaign.makeEnemy(hero, floor);
    Campaign.seedStream(hero, floor, 1200 + hero.runState().attempts[floor]);
    const battle = new BattleSession(hero, monster, Campaign.rulesFor(hero, floor));
    const state = { cursor: 0 };
    while (battle.beginTurn()) battle.act(choose(hero, monster, battle, true, state));
    battle.close();
    Campaign.settleEncounter(hero, monster, battle, floor);
    if (!hero.isalive()) Campaign.revive(hero, true);
    if (hero.getSaveBackpack().some((i) => i.getType() !== 3)) hero.toggleItemLock(1);
  }
  hero.chooseSpecialization(1, 2);
  hero.levelUpSkill(1);
  return hero;
}

for (const job of [Job.Warrior, Job.Mage, Job.Assassin]) {
  const hero = playedHero(job);
  const data = serializeGame(hero, 12);
  const text = JSON.stringify(data);
  const { hero: loaded, floor } = deserializeGame(JSON.parse(text));
  check(floor === 12, 'floor restored');
  const again = serializeGame(loaded, floor);
  delete again.savedAt;
  const first = clone(data);
  delete first.savedAt;
  check(JSON.stringify(again) === JSON.stringify(first), `job ${job}: save round trip must be lossless`);
  check(loaded.getSkill(1).getSpecialization() === hero.getSkill(1).getSpecialization(), 'specialization restored');
  check(loaded.getattack() === hero.getattack() && loaded.getmaxhp() === hero.getmaxhp(), 'stats restored');
  check(Campaign.shopStock(loaded, 12).map((i) => i.getName()).join() === Campaign.shopStock(hero, 12).map((i) => i.getName()).join(),
    'shop stock depends only on saved state');

  rejects({ ...clone(data), version: 99 }, 'newer version');
  rejects({ ...clone(data), floor: 50 }, 'floor above highest');
  const badItem = clone(data);
  badItem.backpack.push({ ...badItem.equipment[0], tier: 9 });
  rejects(badItem, 'invalid tier');
  const badId = clone(data);
  badId.equipment[0].id = 'item-0000000000000000';
  rejects(badId, 'template id mismatch');
  const badRun = clone(data);
  badRun.run.cleared[90] = 1;
  rejects(badRun, 'cleared floor beyond highest');
  const badSkill = clone(data);
  badSkill.skills[0].rank = 5;
  rejects(badSkill, 'rank above cap');
  const badBonus = clone(data);
  badBonus.hero.attack = 1;
  rejects(badBonus, 'equipment bonus above attack');
  rejects(null, 'empty');
}

// Profile: serialize/parse round trip, validation and achievements.
const profile = new Profile();
const hero = playedHero(Job.Warrior);
for (let floor = 1; floor <= 12; floor++) {
  const monster = Campaign.makeEnemy(hero, floor);
  profile.recordEncounter(floor, monster);
  profile.recordVictory(floor);
}
const unlocked = profile.checkAchievements(hero, { floor: 10, won: true, damageTaken: 0, potionsUsed: 0, interrupts: 3, hpAfter: 1, maxHp: 100 });
const keys = unlocked.map((a) => a.key);
check(keys.includes('first_chapter') && keys.includes('flawless_boss') && keys.includes('interrupt_master') && keys.includes('last_stand'),
  `expected achievements, got ${keys}`);
check(profile.checkAchievements(hero).length === 0, 'achievements unlock once');
profile.addRecord({ date: '2026-10-10', job: 'Warrior', challenge: 0, cleared: false, floor: 12, level: hero.getlevel(),
  battles: 12, turns: 50, deaths: 0, potions: 2, seed: 2024, name: '多行\n名字' });
const parsed = Profile.parse(JSON.parse(JSON.stringify(profile.serialize())));
check(JSON.stringify(parsed.serialize()) === JSON.stringify(profile.serialize()), 'profile round trip');
check(parsed.records()[0].name === '多行 名字', 'record names are single line');
check(parsed.discovered() === profile.discovered() && parsed.discovered() > 0, 'bestiary restored');
for (let i = 0; i < 25; i++) parsed.addRecord({ ...parsed.records()[0], floor: 30 + i });
check(parsed.records().length === 20, 'records keep the newest 20');
try {
  Profile.parse({ version: 3 });
  check(false, 'newer profile must be refused');
} catch (error) {
  check(error.newer === true, 'newer profile is flagged, not treated as corrupt');
}
for (const bad of [{}, { version: 1, bestiary: [{ id: 0, seen: 1, won: 2, firstFloor: 1 }] },
  { version: 1, bestiary: [{ id: 0, seen: 1, won: 0, firstFloor: 55 }] }, { version: 1, records: [{ date: 'x' }] }]) {
  try {
    Profile.parse(bad);
    check(false, `accepted invalid profile ${JSON.stringify(bad)}`);
  } catch {
    // Expected.
  }
}
check(Achievements.length === 23, '23 achievements');

// Interrupt texts must match what the enemy then does: ordinary fights and
// bosses rest, scarce-supply and elite fights do not.
for (const c of [{ floor: 4, boss: false, difficulty: 0, elite: false }, { floor: 4, boss: false, difficulty: 1, elite: true },
  { floor: 4, boss: false, difficulty: 1, elite: false }, { floor: 4, boss: false, difficulty: 2, elite: true },
  { floor: 10, boss: true, difficulty: 0, elite: false }, { floor: 10, boss: true, difficulty: 1, elite: false }]) {
  const label = `floor ${c.floor} difficulty ${c.difficulty}${c.elite ? ' elite' : ''}`;
  const fighter = new Player('打断测试', Job.Warrior);
  const monster = new Enemy('fixture', 100000, 0, 0, 500, 500, c.floor);
  monster.configureEncounter(c.floor, c.boss);
  monster.setdodgeRate(0);
  const battle = new BattleSession(fighter, monster, { objective: Objective.Defeat, targetTurns: 4, difficulty: c.difficulty,
    bossSabotaged: false, elite: c.elite });
  for (let i = 0; i < 30 && !monster.getIntent().interruptible; i++) {
    battle.beginTurn();
    battle.act({ type: ActionType.Guard });
  }
  check(monster.getIntent().interruptible, `${label}: fixture reaches a charge`);
  const hint = monster.getTacticalHint();
  const intentText = monster.describeIntent();
  battle.beginTurn();
  const { events } = collect(() => battle.act({ type: ActionType.Attack }));
  battle.close();
  const rests = monster.getIntent().kind === Intent.Recover;
  const log = events.find((e) => e.type === 'interrupt')?.text ?? '';
  check(battle.report.interrupts === 1 && rests === (c.difficulty === 0), `${label}: only unpressured enemies rest`);
  check(rests === intentText.includes('敌人随后休整一回合') && rests !== intentText.includes('不休整'),
    `${label}: charge intent promises rest exactly when it follows`);
  check(rests === log.includes('下回合敌人休整') && rests !== log.includes('不休整'),
    `${label}: interrupt log promises rest exactly when it follows`);
  check(c.boss || rests !== hint.includes('不休整'), `${label}: tactical hint matches the interrupt rule`);
}

// A dodge costs the hit's damage and MP, never the correct read.
{
  const fighter = new Player('闪避测试', Job.Warrior);
  const monster = new Enemy('fixture', 100000, 0, 0, 500, 500, 10);
  monster.configureEncounter(10, true);
  monster.setdodgeRate(100);
  fighter.setmp(0);
  const battle = new BattleSession(fighter, monster);
  battle.beginTurn();
  const hp = monster.gethp();
  const mp = fighter.getmp();
  const { events } = collect(() => battle.act({ type: ActionType.Attack }));
  battle.close();
  check(battle.report.interrupts === 1 && monster.getIntent().kind === Intent.Recover,
    'a dodged plain attack still interrupts the charge');
  check(monster.gethp() === hp && fighter.getmp() === mp &&
    events.some((e) => e.type === 'interrupt' && e.text.includes('普攻虽被闪避，仍打断了蓄力')),
  'a dodged interrupt deals no damage, earns no MP and says why it still worked');
}

// Poison alone is no sign of stalling: the advice needs a Defeat fight where
// the player guarded while nothing called for it.
function poisonFight(floor, boss, rules, pick) {
  const fighter = new Player('剧毒测试', Job.Warrior);
  const monster = new Enemy('fixture', 100000, 0, 0, 500, 500, floor);
  monster.configureEncounter(floor, boss);
  for (let i = 0; i < 1000 && !monster.hasAffix(Affix.POISONOUS); i++) monster.setAffix(100);
  monster.setdodgeRate(0);
  const battle = new BattleSession(fighter, monster, { objective: Objective.Defeat, targetTurns: 4, difficulty: 0,
    bossSabotaged: false, elite: false, ...rules });
  for (let turn = 0; turn < 30 && battle.beginTurn(); turn++) {
    const action = pick(battle, monster, turn);
    if (!action) break;
    battle.act({ type: action });
  }
  battle.close();
  return { battle, advised: Campaign.battleReview(battle).some((line) => line.includes('格挡等待')) };
}
const waiting = (kind) => kind === Intent.Attack || kind === Intent.Charge || kind === Intent.Recover;
const survived = poisonFight(1, false, { objective: Objective.Survive, targetTurns: 5 }, () => ActionType.Guard);
check(survived.battle.won() && survived.battle.report.poisonDamage > 0 && !survived.advised,
  'waiting out a survival objective is not called stalling');
const assault = poisonFight(10, true, {}, (b, m, turn) => (turn < 6 ? ActionType.Attack : null));
check(assault.battle.report.poisonDamage > 0 && assault.battle.report.waitingGuards === 0 && !assault.advised,
  'attacking every turn is not called stalling');
const stall = poisonFight(1, false, {}, (b, m) => (b.report.waitingGuards >= 2 ? null
  : waiting(m.getIntent().kind) ? ActionType.Guard : ActionType.Attack));
check(stall.battle.report.waitingGuards === 2 && stall.advised, 'guarding while nothing threatens in a poisoned Defeat fight is stalling');
const warrior = new Player('预览测试', Job.Warrior);
check(warrior.getSkill(3).getPreview(warrior).includes('无视护盾'), 'Shield Counter preview states it ignores shields');

// The upgrade preview predicts exactly what an upgrade does, names what blocks
// it, and leaves the skill untouched.
{
  const hero = new Player('升阶测试', Job.Warrior);
  hero.loadStats(100, 0, 100000, 100, 1000, 100, 1000, 1000, 0, 3);
  hero.setHighestFloor(30);
  const strike = hero.getSkill(1);
  const damage = strike.getScaledDamage(hero);
  const cost = strike.getActualCost(hero);
  const preview = strike.getUpgradePreview(hero);
  check(strike.getskilllevel() === 1 && strike.getScaledDamage(hero) === damage, 'upgrade preview leaves the skill untouched');
  check(hero.levelUpSkill(1) && strike.getScaledDamage(hero) === damage + 15 && strike.getActualCost(hero) === cost + 3 &&
    preview.includes(`伤害 ${damage}→${damage + 15}`) && preview.includes(`MP ${cost}→${cost + 3}`), `upgrade preview matches the upgrade: ${preview}`);
  check(strike.getUpgradePreview(hero).includes('到达第40层后可升'), 'rank-capped preview names the floor that lifts the cap');
  strike.restoreProgress(5, 0);
  check(strike.getUpgradePreview(hero).includes('已满阶'), 'rank-5 preview says it is maxed');
  const assassin = new Player('吸血测试', Job.Assassin);
  assassin.loadStats(100, 0, 100000, 100, 1000, 100, 1000, 1000, 0, 3);
  assassin.setHighestFloor(100);
  check(!assassin.getSkill(4).getUpgradePreview(assassin).includes('升阶不增加实际治疗'), 'uncapped lifesteal upgrade adds healing');
  assassin.loadStats(100, 0, 100000, 100, 1000, 2000, 1000, 1000, 0, 3);
  check(assassin.getSkill(4).getUpgradePreview(assassin).includes('升阶不增加实际治疗'), 'capped lifesteal says the upgrade adds no healing');
}

// The potion plan avoids overshooting, consumes nothing, and drinking follows it.
{
  const hero = new Player('喝药测试', Job.Warrior);
  hero.loadStats(50, 0, 10000, 400, 1000, 100, 1000, 1000, 0, 3);
  hero.sethp(100);
  hero.addItem(new Item('中药', 1, ItemType.Potion, 300, 0, 1, 2));
  hero.addItem(new Item('小药', 1, ItemType.Potion, 50, 0, 1, 5));
  hero.addItem(new Item('大药', 1, ItemType.Potion, 500, 0, 1, 1));
  const plan = hero.planPotions(1000);
  check(plan.restored === 900 && plan.overflow === 0 &&
    JSON.stringify(plan.uses) === JSON.stringify([{ slot: 3, count: 1 }, { slot: 1, count: 1 }, { slot: 2, count: 2 }]),
  `potion plan takes the largest that fits, then smaller ones: ${JSON.stringify(plan)}`);
  check(hero.gethp() === 100 && hero.getSaveBackpack()[0].getQuantity() === 2, 'planning drinks nothing');
  check(hero.drinkPotionsTo(1000) === 900 && hero.gethp() === 1000 && hero.getSaveBackpack().length === 2 &&
    hero.getSaveBackpack()[0].getQuantity() === 1 && hero.getSaveBackpack()[1].getQuantity() === 3, 'drinking follows the plan');
  hero.sethp(980);
  const spill = hero.planPotions(1000);
  check(spill.restored === 20 && spill.overflow === 30, 'when every potion overflows, the smallest is used');
}

// The camp summary names the next hard fight and warns only when that fight
// is on this floor and the hero lacks half MP or a ready skill.
{
  const hero = new Player('出战测试', Job.Warrior);
  hero.loadStats(50, 0, 10000, 400, 1000, 100, 80, 80, 0, 3);
  hero.setHighestFloor(100);
  const elite = Campaign.eliteFloor(7);
  const ahead = Campaign.readiness(hero, 71);
  check(ahead.majorFloor === elite && !ahead.warning && ahead.summary.includes(`还有 ${elite - 71} 层`), 'readiness names the next elite');
  check(!Campaign.readiness(hero, elite).warning && Campaign.readiness(hero, elite).summary.includes('技能全部就绪'), 'rested hero needs no warning');
  hero.setmp(20);
  check(Campaign.readiness(hero, elite).warning.includes('MP 不到一半'), 'low MP before an elite warns');
  hero.setmp(80);
  hero.getSkill(3).resetcooldown();
  const cooling = Campaign.readiness(hero, elite);
  check(cooling.warning.includes('1 个技能仍在冷却') && cooling.summary.includes('盾反 4'), 'cooling skill before an elite warns');
  hero.runState().cleared[elite] = 1;
  check(Campaign.readiness(hero, elite).majorFloor === 80 && !Campaign.readiness(hero, elite).warning, 'cleared elite gives way to the boss');
  hero.runState().cleared[80] = 1;
  check(Campaign.readiness(hero, elite).majorFloor === 0, 'fully cleared chapter has no major floor');
}
check(HelpTopics.every((topic) => topic.lines().length > 0), 'help pages render');

// Exploration in saves: version 2 keeps every record; version 1 saves load
// without exploration; contradictory records are rejected whole.
{
  const hero = new Player('探索存档', Job.Mage);
  Campaign.initialize(hero, 4321, 0);
  hero.setHighestFloor(20);
  collect(() => {
    for (let floor = 1; floor <= 20; floor++)
      for (const view of Exploration.survey(hero, floor))
        if (!view.done && view.spot.kind !== Exploration.SpotKind.Puzzle) Exploration.interact(hero, floor, view.index, 1);
  });
  hero.runState().boon = 3;
  const data = serializeGame(hero, 20);
  check(data.version === 2, 'saves are version 2');
  const { hero: loaded } = deserializeGame(clone(data));
  const fields = ['explored', 'keys', 'ingredients', 'boon', 'bane', 'questItems', 'spotsExplored', 'vaultsOpened',
    'puzzlesSolved', 'mealsCooked'];
  check(fields.every((f) => JSON.stringify(loaded.runState()[f]) === JSON.stringify(hero.runState()[f])),
    'exploration round trips');
  check(hero.runState().spotsExplored > 50, 'twenty floors hold many spots');
  const v1 = clone(data);
  v1.version = 1;
  for (const f of fields) delete v1.run[f];
  const old = deserializeGame(v1).hero.runState();
  check(old.spotsExplored === 0 && old.keys === 0 && old.explored.every((m) => m === 0), 'version 1 saves load without exploration');
  const v1extra = clone(v1);
  v1extra.run.keys = 1;
  rejects(v1extra, 'version 1 with exploration fields');
  for (const f of fields) {
    const missing = clone(data);
    delete missing.run[f];
    rejects(missing, `version 2 without ${f}`);
  }
  const mutate = (fn, message) => { const bad = clone(data); fn(bad.run); rejects(bad, message); };
  mutate((r) => { r.explored[1] |= 1 << 15; }, 'spot bits beyond the floor');
  mutate((r) => { r.explored[21] = 1; }, 'records above the highest floor');
  mutate((r) => { r.spotsExplored += 1; }, 'explored counter mismatch');
  mutate((r) => { r.vaultsOpened += 1; }, 'vault counter mismatch');
  mutate((r) => { r.questItems = new Array(10).fill(0); r.questItems[0] = 3; }, 'quest mismatch');
  mutate((r) => { r.keys = 100; }, 'too many keys');
  mutate((r) => { r.boon = 5; }, 'unknown boon');
  mutate((r) => { r.ingredients = [0, 0, 0]; }, 'short ingredient list');
  const plain = Array.from({ length: 20 }, (_, i) => i + 1)
    .find((f) => !Exploration.generate(4321, f).some((s) => s.kind === Exploration.SpotKind.Merchant));
  mutate((r) => { r.explored[plain] |= 1 << Exploration.MerchantBit; }, 'goods without a merchant');
}

// Journal pages live in the profile: recorded once, round trip, malformed data refused.
{
  const profile = Profile.parse({ version: 1 });
  check(profile.loreCount() === 0 && profile.recordLore(37) && !profile.recordLore(37) && !profile.recordLore(101),
    'pages are recorded once');
  const copy = Profile.parse(JSON.parse(JSON.stringify(profile.serialize())));
  check(copy.loreFound(37) && copy.loreCount() === 1, 'pages round trip');
  for (const lore of ['01', '2'.repeat(100), 7]) {
    try {
      Profile.parse({ version: 2, lore });
      check(false, 'malformed journal accepted');
    } catch {
      // Expected.
    }
  }
  const hero = new Player('成就', Job.Warrior);
  Campaign.initialize(hero, 1, 0);
  Object.assign(hero.runState(), { spotsExplored: 300, vaultsOpened: 10, puzzlesSolved: 10, mealsCooked: 20 });
  for (let page = 1; page <= 10; page++) profile.recordLore(page);
  profile.checkAchievements(hero);
  check(['explorer', 'vault_hunter', 'puzzle_master', 'gourmet', 'lore_keeper'].every((k) => profile.unlocked(k)) &&
    !profile.unlocked('lore_complete'), 'exploration achievements');
}

if (failures) {
  console.error(`${failures} rules check(s) failed`);
  process.exit(1);
}
console.log('Rules tests passed: save round trips and validation, exploration saves, journal pages, profile parsing, achievements, help, dodged interrupts, interrupt and review texts, upgrade previews, readiness, potion plans.');
