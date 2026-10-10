// Browser-only rules: JSON saves, profile parsing and achievements.
//   node web/tests/rules_test.mjs
import * as Campaign from '../js/rules/campaign.js';
import { BattleSession } from '../js/rules/combat.js';
import { Job } from '../js/rules/jobs.js';
import { Player } from '../js/rules/player.js';
import { Profile, Achievements } from '../js/rules/profile.js';
import { deserializeGame, serializeGame } from '../js/rules/save.js';
import { HelpTopics } from '../js/rules/help.js';
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
  Profile.parse({ version: 2 });
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
check(Achievements.length === 17, '17 achievements');
check(HelpTopics.every((topic) => topic.lines().length > 0), 'help pages render');

if (failures) {
  console.error(`${failures} rules check(s) failed`);
  process.exit(1);
}
console.log('Rules tests passed: save round trips and validation, profile parsing, achievements, help.');
