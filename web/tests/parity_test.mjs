// Checks that the JavaScript rules reproduce the C++ build exactly:
//   node web/tests/parity_test.mjs [reference.csv]
// The golden values come from tests/unit/random_tests.cpp. With a reference
// CSV from the C++ campaign_sim, the full 1,296-run simulation must match it.
import { readFileSync } from 'node:fs';
import * as Random from '../js/rules/random.js';
import * as Campaign from '../js/rules/campaign.js';
import { generateLoot } from '../js/rules/world.js';
import { fnv1a, utf8 } from '../js/rules/item.js';
import { Player } from '../js/rules/player.js';
import * as Exploration from '../js/rules/exploration.js';
import { collect } from '../js/rules/log.js';
import { campaignCsv } from './campaign_sim.mjs';

let failures = 0;
function check(condition, message) {
  if (!condition) {
    failures++;
    console.error(`FAIL: ${message}`);
  }
}

// PCG32 reference outputs for seed 42.
Random.seed(42);
const golden = [0x713066ea, 0x3c7a0d56, 0xf424216a, 0x25c89145];
for (let i = 0; i < golden.length; i++) {
  const value = Random.next();
  check(value === golden[i], `PCG32 output ${i} is 0x${value.toString(16)}`);
}
Random.seed(42);
check(Random.next() === golden[0], 'reseeding must restart the sequence');

Random.seed(7);
const buckets = new Array(10).fill(0);
for (let i = 0; i < 100000; i++) buckets[Random.below(10)]++;
check(buckets.every((n) => n > 9500 && n < 10500), 'below(10) is visibly uneven');
check(Random.below(0) === 0 && Random.below(1) === 0 && Random.below(-5) === 0, 'degenerate bounds');

// Whole-campaign generation fingerprint, identical to random_tests.cpp.
class Fingerprint {
  constructor() { this.hash = 14695981039346656037n; }
  add(value) {
    const text = typeof value === 'string' ? value : String(value);
    this.hash = fnv1a(utf8(text), this.hash);
    this.hash = fnv1a([0xff], this.hash);
  }
  addItem(item) {
    this.add(item.getName());
    this.add(item.getValue());
    this.add(item.getDurability());
    this.add(item.getLevelRequirement());
    this.add(item.getTier());
    this.add(item.getEffect());
  }
}
function campaignFingerprint() {
  const print = new Fingerprint();
  for (const seed of [1, 777]) {
    for (let job = 0; job < 3; job++) {
      const hero = new Player('golden', job);
      Campaign.initialize(hero, seed, 0);
      for (const item of hero.getSaveEquipments()) print.addItem(item);
      for (const item of hero.getSaveBackpack()) print.addItem(item);
      for (let floor = 1; floor <= 100; floor++) {
        Campaign.chooseRoute(hero, floor, (Math.trunc(floor / 10) % 3) + 1);
        const monster = Campaign.makeEnemy(hero, floor);
        print.add(monster.getname());
        print.add(monster.getmaxhp());
        print.add(monster.getattack());
        print.add(monster.getdodgeRate());
        for (const affix of monster.getAffixes()) print.add(affix);
        Campaign.seedStream(hero, floor, 900);
        const loot = generateLoot(hero, floor, true);
        if (loot) print.addItem(loot);
        if (floor % 10 === 1) for (const item of Campaign.shopStock(hero, floor)) print.addItem(item);
      }
    }
  }
  return print.hash;
}
const fingerprint = campaignFingerprint();
check(fingerprint === 0x60a12f556174ccd9n, `campaign fingerprint is 0x${fingerprint.toString(16)}`);
check(campaignFingerprint() === fingerprint, 'generation must not depend on earlier calls');

// Every exploration text, option, puzzle, merchant good and result for two
// seeds, identical to random_tests.cpp.
function explorationFingerprint() {
  const print = new Fingerprint();
  for (const seed of [1, 777]) {
    const hero = new Player('golden', 0);
    Campaign.initialize(hero, seed, 0);
    hero.setHighestFloor(100);
    hero.gaincoin(500);
    hero.runState().materials = 3;
    hero.runState().keys = 1;
    for (let floor = 1; floor <= 100; floor++) {
      for (const view of Exploration.survey(hero, floor)) {
        print.add(view.spot.kind);
        print.add(view.spot.variant);
        print.add(view.spot.zone);
        print.add(view.name);
        print.add(view.description);
        for (const option of Exploration.options(hero, floor, view.index)) {
          print.add(option.id);
          print.add(option.label);
          print.add(option.detail);
          print.add(option.available ? 1 : 0);
          print.add(option.reason);
        }
      }
      for (const good of Exploration.merchantGoods(hero, floor)) {
        print.add(good.name);
        print.add(good.description);
        print.add(good.price);
        print.add(good.sold ? 1 : 0);
      }
      if (Exploration.hasPuzzle(hero, floor)) {
        const puzzle = Exploration.puzzle(hero, floor);
        print.add(puzzle.title);
        print.add(puzzle.prompt);
        for (const element of puzzle.elements) print.add(element);
        for (const clue of puzzle.clues) print.add(clue);
        for (const value of Exploration.puzzleSolution(seed, floor)) print.add(value);
      }
      print.add(Exploration.chapterSummary(hero, floor));
      print.add(Exploration.loreTitle(floor));
      print.add(Exploration.loreText(floor));
    }
    for (let floor = 1; floor <= 20; floor++) {
      for (const view of Exploration.survey(hero, floor)) {
        if (view.done || view.spot.kind === Exploration.SpotKind.Puzzle) continue;
        const result = Exploration.interact(hero, floor, view.index, 1);
        print.add(result.ok ? 1 : 0);
        print.add(result.text);
        print.add(result.lorePage);
      }
      if (Exploration.hasPuzzle(hero, floor))
        print.add(Exploration.solvePuzzle(hero, floor, Exploration.puzzleSolution(seed, floor)).text);
      const notes = [];
      Campaign.makeEnemy(hero, floor, notes);
      for (const note of notes) print.add(note);
    }
    for (const recipe of Exploration.recipes()) {
      print.add(recipe.name);
      print.add(recipe.description);
      const result = Exploration.cook(hero, recipe.id);
      print.add(result.ok ? 1 : 0);
      print.add(result.text);
    }
    const run = hero.runState();
    for (const value of [hero.getcoin(), hero.gethp(), hero.getmp(), hero.getlevel(), run.keys, run.materials,
      run.boon, run.bane, run.spotsExplored, run.vaultsOpened, run.puzzlesSolved, run.mealsCooked]) print.add(value);
    for (const value of run.ingredients) print.add(value);
    for (const value of run.questItems) print.add(value);
  }
  return print.hash;
}
const explorationPrint = collect(() => explorationFingerprint()).result;
check(explorationPrint === 0x5f1125db8e2b21aen, `exploration fingerprint is 0x${explorationPrint.toString(16)}`);
check(collect(() => explorationFingerprint()).result === explorationPrint, 'exploration must not depend on earlier calls');

const reference = process.argv[2];
if (reference) {
  const expected = readFileSync(reference, 'utf8');
  const started = Date.now();
  const actual = campaignCsv(24);
  if (actual !== expected) {
    const a = actual.split('\n');
    const e = expected.split('\n');
    const line = a.findIndex((row, i) => row !== e[i]);
    check(false, `campaign_sim CSV differs at line ${line + 1}:\n  js:  ${a[line]}\n  c++: ${e[line]}`);
  } else {
    console.log(`campaign_sim parity: ${actual.trim().split('\n').length - 1} runs identical to C++ ` +
      `(${Date.now() - started} ms)`);
  }
}

if (failures) {
  console.error(`${failures} parity check(s) failed`);
  process.exit(1);
}
console.log('Parity tests passed: PCG32 golden sequence, unbiased bounds, campaign and exploration fingerprints' +
  (reference ? ', full campaign simulation.' : '.'));
