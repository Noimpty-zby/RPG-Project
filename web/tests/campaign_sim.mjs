// Port of tests/sim/campaign_sim.cpp. With the same policies and seeds it must
// print byte-for-byte the same CSV as the C++ build; parity_test.mjs checks it.
import * as Campaign from '../js/rules/campaign.js';
import { ActionType, BattleSession, Objective } from '../js/rules/combat.js';
import { Intent } from '../js/rules/enemy.js';
import { ItemEffect, ItemType, JobReq } from '../js/rules/item.js';
import { Job } from '../js/rules/jobs.js';
import { Player } from '../js/rules/player.js';
import * as Exploration from '../js/rules/exploration.js';
import { restCost } from '../js/rules/config.js';

function equipUpgrades(hero) {
  for (const type of [ItemType.Weapon, ItemType.Armor]) {
    let best = -1;
    let value = 0;
    for (const eq of hero.getSaveEquipments()) if (eq.getType() === type) value = eq.getValue();
    const items = hero.getSaveBackpack();
    for (let i = 0; i < items.length; ++i) {
      const item = items[i];
      if (item.getType() === type && item.getValue() > value && item.getDurability() > 0 &&
        item.getLevelRequirement() <= hero.getlevel() &&
        (item.getJobRequirement() === JobReq.AnyJob || item.getJobRequirement() === Campaign.jobOf(hero))) {
        best = i + 1;
        value = item.getValue();
      }
    }
    if (best > 0) hero.equipItem(best);
  }
}

export const options = { exploring: true };
// Both policies explore every floor on arrival like a careful player: take
// everything free, solve the puzzle, open vaults when keys allow, pay for a
// curio's sure result only when it is cheap, and skip gambles at low health.
function explore(hero, floor, spend) {
  const { SpotKind } = Exploration;
  if (!options.exploring) return;
  const run = hero.runState();
  for (const view of Exploration.survey(hero, floor)) {
    const index = view.index;
    const healthy = hero.gethp() * 100 >= hero.getmaxhp() * 60;
    switch (view.spot.kind) {
      case SpotKind.Puzzle:
        if (!view.done) Exploration.solvePuzzle(hero, floor, Exploration.puzzleSolution(run.seed, floor));
        break;
      case SpotKind.Merchant: {
        const goods = Exploration.merchantGoods(hero, floor);
        for (let i = 0; i < goods.length; ++i) {
          const good = goods[i];
          const coins = hero.getcoin();
          const wanted = good.kind === 1 ? run.keys < 2 && coins >= good.price * 4 : coins >= good.price * 8;
          if (!good.sold && wanted && Exploration.interact(hero, floor, index, i + 1).ok)
            spend.merchant += coins - hero.getcoin();
        }
        break;
      }
      case SpotKind.Curio: {
        if (view.done) break;
        const list = Exploration.options(hero, floor, index);
        const variant = view.spot.variant;
        let choice = 1;
        if (list.length === 2 && list[1].available &&
          ((variant === 0 && run.materials >= 6) ||
            (variant === 1 && hero.getcoin() >= restCost(floor) * 5) ||
            (variant === 2 && hero.getmp() * 100 >= hero.getmaxmp() * 80)))
          choice = 2;
        if (choice === 2 || healthy) Exploration.interact(hero, floor, index, choice);
        break;
      }
      case SpotKind.AffixSource:
        if (!view.done) Exploration.interact(hero, floor, index, run.materials >= 4 ? 2 : healthy ? 1 : 0);
        break;
      case SpotKind.Vault:
        if (!view.done && run.keys >= view.spot.variant) Exploration.interact(hero, floor, index, 1);
        break;
      default:
        if (!view.done) Exploration.interact(hero, floor, index, 1);
        break;
    }
  }
}
// Cook a blessing before an elite or boss, or when ingredients pile up.
function cook(hero, floor) {
  if (!options.exploring) return;
  const run = hero.runState();
  const total = run.ingredients.reduce((sum, count) => sum + count, 0);
  const major = floor % 10 === 0 || Campaign.isEliteFloor(floor);
  if (run.boon === 0 && (major || total >= 8))
    for (const recipe of [5, 1, 3, 2, 4, 6])
      if (Exploration.cook(hero, recipe).ok) break;
}

function prepare(hero, floor, build, spend) {
  Campaign.claimOrder(hero, floor);
  cook(hero, floor);
  equipUpgrades(hero);
  for (let slot = 1; slot <= hero.getSkillCount(); ++slot) {
    const ability = hero.getSkill(slot);
    if (ability.canUpgrade(hero.getlevel(), hero.getHighestFloor()) &&
      hero.getcoin() >= ability.getlevelUpCost() + 150) hero.levelUpSkill(slot);
  }
  for (let slot = 1; slot <= 2; ++slot)
    if (hero.getSkill(slot).getSpecialization() === 0) hero.chooseSpecialization(slot, build);
  const stock = Campaign.shopStock(hero, floor);
  for (let slot = 1; slot <= 2; ++slot) {
    let value = 0;
    for (const eq of hero.getSaveEquipments()) if (eq.getType() === stock[slot - 1].getType()) value = eq.getValue();
    if (stock[slot - 1].getLevelRequirement() <= hero.getlevel() && stock[slot - 1].getValue() > value)
      Campaign.purchase(hero, floor, slot);
  }
  equipUpgrades(hero);
  if (hero.gethp() * 100 < hero.getmaxhp() * 65) {
    if (!hero.potionIndex()) {
      Campaign.purchase(hero, floor, 3);
      Campaign.purchase(hero, floor, 4);
    }
    if (hero.gethp() * 100 < hero.getmaxhp() * 40) {
      const coins = hero.getcoin();
      if (Campaign.rest(hero, floor)) spend.rest += coins - hero.getcoin();
    }
    if (!hero.potionIndex() && Campaign.refreshShop(hero, floor)) {
      Campaign.purchase(hero, floor, 3);
      Campaign.purchase(hero, floor, 4);
    }
    for (let use = 0; use < 3 && hero.gethp() * 100 < hero.getmaxhp() * 65 && hero.potionIndex(); ++use)
      hero.useItem(hero.potionIndex());
  }
  if (floor % 10 === 0) {
    for (let refreshes = 0; hero.potionCount() < 4 && refreshes < 5; ++refreshes) {
      Campaign.purchase(hero, floor, 3);
      Campaign.purchase(hero, floor, 4);
      if (hero.potionCount() >= 4 || !Campaign.refreshShop(hero, floor)) break;
    }
  }
  for (let slot = 1; slot <= hero.getSaveEquipments().length; ++slot)
    for (let cost = hero.enhanceCost(slot); cost > 0 && cost * 4 <= hero.getcoin(); cost = hero.enhanceCost(slot)) {
      if (!hero.enhanceEquipment(slot)) break;
      spend.enhance += cost;
    }
  for (let slot = 1; slot <= hero.getSaveEquipments().length; ++slot) {
    const item = hero.getSaveEquipments()[slot - 1];
    const cost = hero.repairCost(slot);
    if (item.getDurability() * 4 <= item.getMaxDurability() && cost > 0 && cost * 3 <= hero.getcoin()) {
      hero.repairEquipment(slot);
      spend.repair += cost;
    }
  }
  if (hero.runState().materials >= 10 && hero.getSaveEquipments().length)
    hero.modifyEquipment(1, ItemEffect.SkillEfficiency);
}

const Sequences = { [Job.Warrior]: [3, 2, 4, 5, 1], [Job.Mage]: [2, 4, 5, 1, 3], [Job.Assassin]: [5, 2, 3, 4, 1] };

export function choose(hero, monster, battle, aware, state) {
  const intent = monster.getIntent();
  const potion = hero.potionIndex();
  const emergency = hero.gethp() * 100 < hero.getmaxhp() * 28;
  if (potion && (emergency || (aware && intent.healingPercent === 100 && hero.gethp() * 100 < hero.getmaxhp() * 55)))
    return { type: ActionType.Potion, index: potion };
  if (aware && intent.interruptible) return { type: ActionType.Attack };
  if (aware && battle.objective() === Objective.Escape && battle.turns() >= 2) return { type: ActionType.Guard };
  if (aware && intent.kind === Intent.HeavyStrike &&
    Math.trunc(monster.getattack() * intent.damagePercent / 100) * 10 >= hero.gethp() * 3 &&
    monster.gethp() > hero.getattack())
    return { type: ActionType.Guard };
  const sequence = Sequences[hero.job()];
  for (let i = 0; i < 5; ++i) {
    const slot = sequence[state.cursor++ % 5];
    if (hero.getSkill(slot).canUse(hero) && (!aware || slot !== intent.predictedAction))
      return { type: ActionType.Skill, index: slot };
  }
  return { type: ActionType.Attack };
}

export function run(job, seed, route, challenge, aware) {
  const hero = new Player('sim', job);
  Campaign.initialize(hero, seed, challenge);
  let floor = 1;
  let timeouts = 0;
  let objectiveWins = 0;
  const spend = { enhance: 0, repair: 0, rest: 0, merchant: 0 };
  for (; floor <= 100; ++floor) {
    hero.setHighestFloor(floor);
    Campaign.chooseRoute(hero, floor, route);
    if (Campaign.eventPending(hero, floor))
      Campaign.resolveEvent(hero, floor, hero.getcoin() >= restCost(floor) ? 1 : 2);
    explore(hero, floor, spend);
    let won = false;
    for (let retry = 0; retry < 3 && !won; ++retry) {
      prepare(hero, floor, (seed % 2) + 1, spend);
      const monster = Campaign.makeEnemy(hero, floor);
      Campaign.seedStream(hero, floor, 1200 + hero.runState().attempts[floor]);
      const rules = Campaign.rulesFor(hero, floor);
      const battle = new BattleSession(hero, monster, rules);
      const state = { cursor: 0 };
      while (battle.beginTurn()) {
        const action = choose(hero, monster, battle, aware, state);
        if (!battle.act(action)) throw new Error('simulation submitted invalid action');
      }
      battle.close();
      timeouts += battle.report.timeout ? 1 : 0;
      if (battle.turns() > battle.turnLimit()) throw new Error('encounter exceeded its turn bound');
      if (battle.won() && monster.isalive()) ++objectiveWins;
      won = Campaign.settleEncounter(hero, monster, battle, floor);
      if (!won && !hero.isalive()) {
        if (!Campaign.revive(hero, hero.getfreerevives() > 0)) break;
      }
    }
    if (!won) break;
  }
  const r = hero.runState();
  return { floor, level: hero.getlevel(), coins: hero.getcoin(), turns: r.turns, potions: r.potionsUsed,
    deaths: r.deaths, upgrades: r.usefulDrops, drought: r.longestGearDrought, timeouts, objectiveWins,
    explored: r.spotsExplored };
}

export function campaignCsv(seeds = 24) {
  const names = ['Warrior', 'Mage', 'Assassin'];
  let csv = 'job,seed,route,challenge,policy,ending_floor,level,coins,turns,potions,deaths,gear_upgrades,longest_drought,timeouts,objective_wins,explored\n';
  for (let job = 0; job < 3; ++job)
    for (let seed = 1; seed <= seeds; ++seed)
      for (let route = 1; route <= 3; ++route)
        for (let challenge = 0; challenge < 3; ++challenge)
          for (let aware = 0; aware < 2; ++aware) {
            const r = run(job, seed, route, challenge, aware !== 0);
            csv += [names[job], seed, route, challenge, aware ? 'aware' : 'fixed', r.floor, r.level, r.coins, r.turns,
              r.potions, r.deaths, r.upgrades, r.drought, r.timeouts, r.objectiveWins, r.explored].join(',') + '\n';
          }
  return csv;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const index = process.argv.indexOf('--seeds');
  if (process.argv.includes('--no-explore')) options.exploring = false;
  process.stdout.write(campaignCsv(index > 0 ? Number(process.argv[index + 1]) : 24));
}
