// Game flow from src/main.cpp: floors, routes, events, fights, settlement,
// revives and records. Scenes and menus call into this; it never draws.
import * as Campaign from './rules/campaign.js';
import { BattleSession } from './rules/combat.js';
import { chapterOf, MaxFloor } from './rules/config.js';
import * as Exploration from './rules/exploration.js';
import { ItemType } from './rules/item.js';
import { collect } from './rules/log.js';
import { Player } from './rules/player.js';
import { Profile, todayDate } from './rules/profile.js';
import { deserializeGame, saveSummary, serializeGame } from './rules/save.js';

const Keys = { profile: 'aincrad.profile.v1', manual: 'aincrad.save.manual.v1', auto: 'aincrad.save.auto.v1' };

function readStorage(key) {
  try {
    const text = localStorage.getItem(key);
    return text ? JSON.parse(text) : null;
  } catch {
    return undefined; // Unreadable or corrupt.
  }
}
function writeStorage(key, value) {
  try {
    localStorage.setItem(key, JSON.stringify(value));
    return true;
  } catch {
    return false;
  }
}
function storageAvailable() {
  try {
    localStorage.setItem('aincrad.probe', '1');
    localStorage.removeItem('aincrad.probe');
    return true;
  } catch {
    return false;
  }
}
function removeStorage(key) {
  try { localStorage.removeItem(key); } catch { /* ignore */ }
}

export class Game {
  constructor() {
    this.hero = null;
    this.floor = 1;
    this.profileWarning = '';
    this.profileWritable = true;
    this.profile = this.loadProfile();
    this.diedThisSession = false;
    this.runOver = false;
    this.listeners = new Set();
  }

  // ---- Profile and saves -----------------------------------------------------
  loadProfile() {
    if (!storageAvailable()) {
      this.profileWritable = false;
      this.profileWarning = '浏览器存储不可用（可能是隐私模式）：档案和存档无法保存。';
      return new Profile();
    }
    const data = readStorage(Keys.profile);
    if (data === null) return new Profile();
    try {
      if (data === undefined) throw new Error('档案无法解析');
      return Profile.parse(data);
    } catch (error) {
      if (error.newer) {
        this.profileWritable = false;
        this.profileWarning = '档案来自更新的游戏版本，本次不会写入。';
      } else {
        try { localStorage.setItem(`${Keys.profile}.broken`, localStorage.getItem(Keys.profile) ?? ''); } catch { /* ignore */ }
        this.profileWarning = `档案损坏（${error.message}），已另存为备份，将使用新档案。`;
      }
      return new Profile();
    }
  }
  saveProfile() {
    if (this.profileWritable) writeStorage(Keys.profile, this.profile.serialize());
  }
  slotInfo(slot) {
    const data = readStorage(Keys[slot]);
    if (!data) return null;
    return saveSummary(data);
  }
  save(slot = 'manual') {
    if (!this.hero || this.runOver) return false;
    return writeStorage(Keys[slot], serializeGame(this.hero, this.floor));
  }
  autosave() { return this.save('auto'); }
  load(slot) {
    const data = readStorage(Keys[slot]);
    if (!data) throw new Error('没有这个存档');
    const { hero, floor } = deserializeGame(data);
    this.hero = hero;
    this.floor = floor;
    this.diedThisSession = !hero.isalive();
    this.runOver = false;
    return true;
  }
  deleteSlot(slot) { removeStorage(Keys[slot]); }

  newGame(name, job, seed, challenge) {
    const hero = new Player(name || '旅人', job);
    const seedValue = seed > 0 ? seed >>> 0 : (Date.now() ^ (Math.random() * 0x7fffffff)) >>> 0;
    const { events } = collect(() => Campaign.initialize(hero, seedValue || 1, challenge));
    this.hero = hero;
    this.floor = 1;
    this.diedThisSession = false;
    this.runOver = false;
    this.enterFloor(1);
    return events;
  }

  // ---- Floors ------------------------------------------------------------------
  get run() { return this.hero.runState(); }
  get chapter() { return chapterOf(this.floor); }
  enterFloor(floor) {
    this.floor = floor;
    this.hero.setHighestFloor(floor);
    const unlocked = this.checkAchievements();
    this.autosave();
    return unlocked;
  }
  needsRoute() { return this.run.routes[this.chapter] === 0; }
  chooseRoute(route) {
    const ok = Campaign.chooseRoute(this.hero, this.floor, route);
    if (ok) this.autosave();
    return ok;
  }
  cleared(floor = this.floor) { return this.run.cleared[floor] !== 0; }
  canAdvance() { return this.cleared() && this.floor < MaxFloor; }
  advance() {
    if (!this.canAdvance()) return false;
    this.enterFloor(this.floor + 1);
    return true;
  }
  travel(target) {
    if (target < 1 || target > this.hero.getHighestFloor()) return false;
    this.enterFloor(target);
    return true;
  }
  eventPending() { return Campaign.eventPending(this.hero, this.floor); }
  resolveEvent(choice) {
    const { result, events } = collect(() => Campaign.resolveEvent(this.hero, this.floor, choice));
    if (result) this.autosave();
    return { ok: result, events };
  }

  // ---- Battles -------------------------------------------------------------------
  startBattle() {
    const hero = this.hero;
    const notes = [];
    const monster = Campaign.makeEnemy(hero, this.floor, notes);
    const rules = Campaign.rulesFor(hero, this.floor);
    this.profile.recordEncounter(this.floor, monster);
    Campaign.seedStream(hero, this.floor, 1200 + hero.runState().attempts[this.floor]);
    const battle = new BattleSession(hero, monster, rules);
    this.firstFinalClear = this.floor === MaxFloor && !this.cleared(MaxFloor);
    return { monster, rules, battle, notes };
  }
  // Settle a finished battle; returns what happened for the result screen.
  finishBattle(monster, battle) {
    const hero = this.hero;
    battle.close();
    const report = battle.report;
    const outcome = { floor: this.floor, won: battle.won(), damageTaken: report.damageTaken, potionsUsed: report.potionsUsed,
      interrupts: report.interrupts, hpAfter: hero.gethp(), maxHp: hero.getmaxhp() };
    const review = Campaign.battleReview(battle);
    const { result: won, events } = collect(() => Campaign.settleEncounter(hero, monster, battle, this.floor));
    if (won) this.profile.recordVictory(this.floor);
    const achievements = this.checkAchievements(outcome);
    this.saveProfile();
    const worn = hero.getSaveEquipments().filter((item) => item.getDurability() <= 3)
      .map((item) => `【耐久告急】${item.getDisplayName()} 还能用 ${item.getDurability()} 场，可在营火处修理。`);
    let summit = false;
    if (won && this.firstFinalClear) {
      this.recordRun(true);
      summit = true;
    }
    if (!hero.isalive()) this.diedThisSession = true;
    // A death is saved too, so reloading cannot undo it (the next load offers revive or ends the run).
    this.autosave();
    return { won, events, achievements, review, worn, summit, timeout: report.timeout, alive: hero.isalive() };
  }
  // Revive choices after a death; null when the run is over.
  reviveOptions() {
    if (this.hero.isalive()) return null;
    if (this.run.challenge === 2) return null;
    return { free: this.hero.getfreerevives(), paid: Math.max(1, Math.trunc(this.hero.getcoin() / 3)), coins: this.hero.getcoin() };
  }
  revive(useFree) {
    const { result, events } = collect(() => Campaign.revive(this.hero, useFree));
    if (result) this.autosave();
    return { ok: result, events };
  }
  // Ends the run after a death (no revive chosen or possible).
  endRun() {
    if (this.diedThisSession && !this.runOver) this.recordRun(false);
    this.runOver = true;
    this.deleteSlot('auto');
  }
  recordRun(cleared) {
    const hero = this.hero;
    const run = hero.runState();
    this.profile.addRecord({
      date: todayDate(), job: hero.getjob(), challenge: run.challenge, cleared, floor: hero.getHighestFloor(),
      level: hero.getlevel(), battles: run.battles, turns: run.turns, deaths: run.deaths, potions: run.potionsUsed,
      seed: run.seed, name: hero.getname(),
    });
    this.saveProfile();
  }
  checkAchievements(outcome = null) {
    if (!this.hero) return [];
    const unlocked = this.profile.checkAchievements(this.hero, outcome);
    if (unlocked.length) {
      this.saveProfile();
      for (const listener of this.listeners) listener({ type: 'achievements', unlocked });
    }
    return unlocked;
  }
  onEvent(listener) { this.listeners.add(listener); }

  // ---- Camp and inventory (wrappers that persist and report) -------------------
  act(fn) {
    const { result, events } = collect(fn);
    this.checkAchievements();
    return { ok: Boolean(result), result, events };
  }
  rest() { return this.act(() => Campaign.rest(this.hero, this.floor)); }
  claimOrder() { return this.act(() => Campaign.claimOrder(this.hero, this.floor)); }
  setLootTarget(target) {
    if ((target === 1 || target === 2) && target !== this.run.lootTarget) {
      this.run.lootTarget = target;
      this.run.pity = 0;
      return true;
    }
    return false;
  }
  shopStock() { return Campaign.shopStock(this.hero, this.floor); }
  purchasePrice(item) { return Campaign.purchasePrice(this.hero, this.floor, item); }
  purchase(slot) { return this.act(() => Campaign.purchase(this.hero, this.floor, slot)); }
  refreshShop() { return this.act(() => Campaign.refreshShop(this.hero, this.floor)); }
  journal() { return Campaign.journal(this.hero, this.floor); }

  // ---- Exploration -------------------------------------------------------------
  // Each returns { ok, text, lorePage, newPage, events }. Progress is saved at
  // once: spot results come from the seed, so reloading would not change them.
  explore(index, option) {
    return this.exploreResult(() => Exploration.interact(this.hero, this.floor, index, option));
  }
  solvePuzzle(order) {
    return this.exploreResult(() => Exploration.solvePuzzle(this.hero, this.floor, order));
  }
  cook(recipe) {
    return this.exploreResult(() => Exploration.cook(this.hero, recipe));
  }
  exploreResult(fn) {
    const { result, events } = collect(fn);
    const newPage = result.ok && result.lorePage > 0 && this.profile.recordLore(result.lorePage);
    if (newPage) this.saveProfile();
    if (result.ok) {
      this.checkAchievements();
      this.autosave();
    }
    return { ...result, newPage, events };
  }
  potionCount() {
    return this.hero.getSaveBackpack().filter((i) => i.getType() === ItemType.Potion).reduce((n, i) => n + i.getQuantity(), 0);
  }
}
