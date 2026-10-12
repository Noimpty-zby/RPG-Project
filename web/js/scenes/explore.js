// Top-down floor exploration: walk the generated map, talk to the merchant
// and allies, rest at the campfire, search the floor's exploration spots and
// challenge the floor's guardian.
import { generateLayout, FLOOR } from '../engine/mapgen.js';
import { TILE } from '../engine/tiles.js';
import * as Sprites from '../engine/sprites.js';
import { lighten, makeCanvas } from '../engine/pixel.js';
import { ChapterThemes, P } from '../engine/palette.js';
import { Effects, tintGlow } from '../engine/effects.js';
import { sfx, playMusic } from '../engine/audio.js';
import { Bestiary } from '../rules/enemy.js';
import { chapterOf, MaxFloor } from '../rules/config.js';
import * as Campaign from '../rules/campaign.js';
import { Objective } from '../rules/combat.js';
import * as Exploration from '../rules/exploration.js';
import { chapterText } from '../rules/exploration_text.js';
import { modalOpen } from '../ui/dom.js';

const Dirs = { up: [0, -1], down: [0, 1], left: [-1, 0], right: [1, 0] };
const MoveTime = 0.14;
const { SpotKind } = Exploration;
// Picked up when explored, so nothing stays on the map.
const PickedUp = [SpotKind.Key, SpotKind.QuestItem, SpotKind.Lore];

export class ExploreScene {
  constructor(app) {
    this.app = app;
    this.fx = new Effects();
    this.ambient = [];
    this.time = 0;
    this.chest = null;
    this.spawnAt = 'start';
    // Cracked walls broken this session ("seed:floor:crack"); a crack whose
    // nook holds an explored spot is open anyway.
    this.openedCracks = new Set();
    this.status = { remaining: 0 };
  }

  get game() { return this.app.game; }

  enter() {
    const { game } = this;
    this.app.display.setMode('explore');
    this.build();
    const resumed = this.resume;
    if (resumed) {
      Object.assign(this.player, { tx: resumed.tx, ty: resumed.ty, x: resumed.tx * TILE, y: resumed.ty * TILE, dir: resumed.dir });
      this.camera = { x: this.player.x, y: this.player.y };
      this.resume = null;
    }
    this.fx.clear();
    this.ambient = [];
    if (this.pendingChest?.length) this.placeChest(this.pendingChest);
    this.pendingChest = null;
    const theme = ChapterThemes[this.theme];
    playMusic(theme.dark >= 0.3 ? 'dark' : 'explore');
    this.app.hud.show(true);
    this.app.hud.refresh();
    this.lastPrompt = null;
    this.bumpCooldown = 0;
    // Arrival hooks: route choice, revive, banner.
    setTimeout(() => this.app.menus.onArrive(this), 50);
    this.banner = resumed ? null : { text: `第 ${game.floor} 层`, sub: Campaign.chapter(game.floor).name, t: 0 };
  }
  exit() {
    this.app.hud.prompt(null);
  }

  // Build the floor layout and bake the static map canvas.
  build() {
    const { game } = this;
    const floor = game.floor;
    this.floorNumber = floor;
    this.theme = chapterOf(floor);
    const boss = floor % 10 === 0;
    const set = Sprites.tiles(this.theme);
    const layout = generateLayout({ seed: game.run.seed, floor, boss, propCount: set.props.length,
      spots: Exploration.generate(game.run.seed, floor) });
    this.layout = layout;
    const views = Exploration.survey(game.hero, floor);
    this.status = { remaining: views.filter((v) => !v.done && Exploration.countsTowardCompletion(v.spot.kind)).length };
    this.openWalls(views);
    this.bakeMap();
    // Entities.
    const run = game.run;
    const local = Campaign.localFloor(floor);
    const entry = Bestiary.entryForFloor(floor);
    const rules = Campaign.rulesFor(game.hero, floor);
    this.rules = rules;
    this.entities = [];
    const add = (e) => { this.entities.push(e); return e; };
    add({ type: 'waystone', ...layout.waystone, solid: true, name: '传送石', light: P.cyan });
    add({ type: 'campfire', ...layout.campfire, solid: true, name: '营火', light: P.orange });
    add({ type: 'stall', x: layout.stall.x, y: layout.stall.y, solid: true, w: 2 });
    add({ type: 'merchant', ...layout.merchant, solid: true, name: '商人', dir: 'down' });
    add({ type: 'sign', ...layout.sign, solid: true, name: '告示牌' });
    if (Campaign.eventPending(game.hero, floor) || (local >= 5 && run.events[this.theme] !== 0))
      add({ type: 'ally', ...layout.ally, solid: true, name: Campaign.chapter(floor).ally, dir: 'down' });
    if (run.routes[this.theme] === 1 && (local === 2 || local === 7))
      add({ type: 'device', ...layout.device, solid: true, name: '装置' });
    this.guardian = add({ type: 'guardian', ...layout.guardian, solid: true, entry, boss, elite: rules.elite,
      objective: rules.objective, name: Bestiary.entryName(entry) });
    this.stairs = add({ type: 'stairs', ...layout.stairs, solid: false, name: floor === MaxFloor ? '天顶之门' : '楼梯' });
    for (const prop of layout.props) add({ type: 'prop', ...prop, solid: true, def: set.props[prop.kind] });
    for (const view of views) {
      const place = layout.spots[view.index];
      const hidden = place && place.crack >= 0 && !layout.cracks[place.crack].open;
      if (place && !hidden && !(view.done && PickedUp.includes(view.spot.kind))) add(this.spotEntity(view, place));
    }
    layout.cracks.forEach((crack, i) => add(crack.open ? { type: 'rubble', x: crack.x, y: crack.y, solid: false }
      : { type: 'crack', x: crack.x, y: crack.y, solid: true, crack: i, name: '墙上的裂缝' }));
    // An emptied chest at the back of each opened vault.
    for (const vault of layout.vaults) {
      if (!vault.open) continue;
      const back = Math.min(...vault.cells.filter(([x]) => x === vault.x).map(([, y]) => y));
      add({ type: 'decor', x: vault.x, y: back, solid: false });
    }
    if (this.chest && this.chest.floor !== floor) this.chest = null;
    if (this.chest) add(this.chest.entity);
    // Player.
    const spawn = this.spawnAt === 'waystone' ? { x: layout.waystone.x + 1, y: layout.waystone.y + 1 } : layout.start;
    this.spawnAt = 'start';
    this.player = { tx: spawn.x, ty: spawn.y, x: spawn.x * TILE, y: spawn.y * TILE, dir: 'right', moving: null, step: 0, walk: 0 };
    this.camera = { x: this.player.x, y: this.player.y };
    this.route = null;
  }

  // Bake the static map. Concealed nook and vault cells draw as wall.
  bakeMap() {
    const layout = this.layout;
    const set = Sprites.tiles(this.theme);
    const W = layout.w;
    const H = layout.h;
    const open = (x, y) => layout.at(x, y) === FLOOR && !layout.concealed.has(layout.key(x, y));
    const map = makeCanvas(W * TILE, H * TILE);
    const ctx = map.getContext('2d');
    const theme = ChapterThemes[this.theme];
    ctx.fillStyle = theme.wallTop;
    ctx.fillRect(0, 0, map.width, map.height);
    for (let y = 0; y < H; y++)
      for (let x = 0; x < W; x++) {
        const px = x * TILE;
        const py = y * TILE;
        if (open(x, y)) {
          ctx.drawImage(set.floor[layout.variantAt(x, y)], px, py);
          if (!open(x, y - 1)) {
            ctx.fillStyle = 'rgba(0,0,0,0.28)';
            ctx.fillRect(px, py, TILE, 4);
            ctx.fillStyle = 'rgba(0,0,0,0.12)';
            ctx.fillRect(px, py + 4, TILE, 3);
          }
          if (!open(x - 1, y)) {
            ctx.fillStyle = 'rgba(0,0,0,0.15)';
            ctx.fillRect(px, py, 3, TILE);
          }
        } else if (open(x, y + 1)) {
          ctx.drawImage(set.face[(x + y) % 3], px, py);
        } else {
          ctx.drawImage(set.top[(x * 7 + y * 3) % 3], px, py);
          // Dark rim where wall tops meet floor at the sides.
          ctx.fillStyle = 'rgba(0,0,0,0.35)';
          if (open(x + 1, y)) ctx.fillRect(px + TILE - 2, py, 2, TILE);
          if (open(x - 1, y)) ctx.fillRect(px, py, 2, TILE);
        }
      }
    this.mapCanvas = map;
  }

  // Open cracks broken this session or hiding an explored spot, and vaults
  // already opened: their wall tile becomes floor and their cells show.
  openWalls(views) {
    const layout = this.layout;
    const reveal = (opening) => {
      layout.setFloor(opening.x, opening.y);
      for (const [x, y] of opening.cells) layout.concealed.delete(layout.key(x, y));
    };
    layout.cracks.forEach((crack, i) => {
      crack.open = this.openedCracks.has(this.crackKey(i)) || crack.spots.some((index) => views[index].done);
      if (crack.open) reveal(crack);
    });
    for (const vault of layout.vaults) {
      vault.open = views[vault.spot].done;
      if (vault.open) reveal(vault);
    }
  }
  crackKey(i) { return `${this.game.run.seed}:${this.floorNumber}:${i}`; }

  spotEntity(view, place) {
    const { spot, done } = view;
    const accent = ChapterThemes[this.theme].accent;
    const glows = !done && (spot.kind === SpotKind.AffixSource || spot.kind === SpotKind.Puzzle ||
      (spot.kind === SpotKind.Curio && spot.variant > 0));
    return {
      type: 'spot', x: place.x, y: place.y, index: view.index, kind: spot.kind, variant: spot.variant, zone: spot.zone,
      name: view.name, done, door: place.vault && !place.fallback, fallback: place.fallback,
      // An opened vault door and an emptied cache can be walked through.
      solid: !(done && (place.vault && !place.fallback || spot.kind === SpotKind.Cache)),
      light: glows ? accent : null,
    };
  }

  breakCrack(e) {
    this.openedCracks.add(this.crackKey(e.crack));
    sfx('shatter');
    this.fx.shake(2, 0.15);
    const stone = lighten(ChapterThemes[this.theme].wall, 0.2);
    this.fx.burst(e.x * TILE + 8, e.y * TILE + 10, { color: [stone, P.steel], count: 18, speed: 50, gravity: 120 });
    this.refreshEntities();
    this.app.toast('墙后藏着一处隐秘的角落。');
  }

  // Re-check entity states after menus change the run (events, clears).
  refreshEntities() {
    const keepPlayer = this.player;
    const chest = this.chest;
    this.build();
    this.player = keepPlayer;
    this.chest = chest;
  }

  entityAt(x, y) {
    return this.entities.find((e) => e.x === x && e.y === y || (e.w === 2 && e.y === y && e.x + 1 === x)) || null;
  }
  walkable(x, y) {
    if (this.layout.at(x, y) !== FLOOR) return false;
    const e = this.entityAt(x, y);
    if (e && e.type === 'stairs') return this.game.cleared();
    return !e || !e.solid;
  }
  // Landmarks for quick travel, most useful first. The map holds no fights
  // besides the guardian, so every walk is safe to automate.
  travelTargets() {
    const { game } = this;
    const byType = (type) => this.entities.find((e) => e.type === type);
    const list = [];
    const add = (entity, label, primary = false) => { if (entity) list.push({ entity, label, primary }); };
    if (this.chest && !this.chest.entity.opened) add(this.chest.entity, '宝箱', true);
    if (game.cleared()) add(this.stairs, game.floor === MaxFloor ? '天顶之门' : '楼梯（下一层）', true);
    else add(this.guardian, `守卫：${this.guardian.name}`, true);
    add(byType('campfire'), '营火');
    add(byType('merchant'), '商人');
    add(byType('waystone'), '传送石');
    add(byType('sign'), '告示牌');
    add(byType('ally'), byType('ally')?.name);
    add(byType('device'), '装置');
    if (game.cleared()) add(this.guardian, `再次挑战：${this.guardian.name}`);
    // Unexplored spots and unbroken cracks, left to right.
    const soldOut = () => Exploration.merchantGoods(game.hero, game.floor).every((good) => good.sold);
    const spots = this.entities.filter((e) => e.type === 'crack' ||
      (e.type === 'spot' && (e.kind === SpotKind.Merchant ? !soldOut() : !e.done))).sort((a, b) => a.x - b.x || a.y - b.y);
    for (const e of spots) add(e, e.type === 'crack' ? '墙上的裂缝' : `探索：${e.name}`);
    return list;
  }
  // Walk the shortest path to an entity, then use it. Stairs are walked onto;
  // anything else is approached from a free side and faced.
  travelTo(entity) {
    const route = this.pathTo(entity);
    if (!route) {
      sfx('error');
      this.app.toast('走不到那里。');
      return;
    }
    this.route = { ...route, target: entity };
  }
  pathTo(entity) {
    const { w } = this.layout;
    const p = this.player;
    const key = (x, y) => y * w + x;
    const goals = new Map();
    if (entity.type === 'stairs') {
      if (this.walkable(entity.x, entity.y)) goals.set(key(entity.x, entity.y), null);
    } else {
      const tiles = entity.w === 2 ? [[entity.x, entity.y], [entity.x + 1, entity.y]] : [[entity.x, entity.y]];
      for (const [tx, ty] of tiles)
        for (const [dir, [dx, dy]] of Object.entries(Dirs)) {
          const nx = tx - dx;
          const ny = ty - dy;
          if (this.walkable(nx, ny) || (nx === p.tx && ny === p.ty)) goals.set(key(nx, ny), dir);
        }
    }
    const from = new Map([[key(p.tx, p.ty), null]]);
    const queue = [[p.tx, p.ty]];
    while (queue.length) {
      const [x, y] = queue.shift();
      if (goals.has(key(x, y))) {
        const steps = [];
        for (let k = key(x, y); from.get(k); k = from.get(k).prev) steps.unshift(from.get(k).dir);
        return { steps, face: goals.get(key(x, y)) };
      }
      for (const [dir, [dx, dy]] of Object.entries(Dirs)) {
        const nx = x + dx;
        const ny = y + dy;
        if (!from.has(key(nx, ny)) && this.walkable(nx, ny)) {
          from.set(key(nx, ny), { prev: key(x, y), dir });
          queue.push([nx, ny]);
        }
      }
    }
    return null;
  }

  facing() {
    const [dx, dy] = Dirs[this.player.dir];
    return this.entityAt(this.player.tx + dx, this.player.ty + dy);
  }

  update(dt) {
    this.time += dt;
    this.fx.update(dt);
    this.bumpCooldown = Math.max(0, this.bumpCooldown - dt);
    if (this.banner) {
      this.banner.t += dt;
      if (this.banner.t > 2.6) this.banner = null;
    }
    const input = this.app.input;
    const keys = input.take();
    const busy = modalOpen() || this.app.busy;
    if (!busy) {
      for (const key of keys) {
        if (key === 'action') this.interact(this.facing());
        if (key === 'menu') this.app.menus.openMain();
        if (key === 'travel') this.app.menus.openTravel(this);
      }
    }
    this.updatePlayer(dt, busy);
    this.updateAmbient(dt);
    // Camera follows the player smoothly.
    const view = this.app.display;
    const targetX = this.player.x + TILE / 2 - view.viewW / 2;
    const targetY = this.player.y + TILE / 2 - view.viewH / 2;
    const maxX = this.layout.w * TILE - view.viewW;
    const maxY = this.layout.h * TILE - view.viewH;
    const clampX = maxX < 0 ? maxX / 2 : Math.max(0, Math.min(maxX, targetX));
    const clampY = maxY < 0 ? maxY / 2 : Math.max(0, Math.min(maxY, targetY));
    this.camera.x += (clampX - this.camera.x) * Math.min(1, dt * 10);
    this.camera.y += (clampY - this.camera.y) * Math.min(1, dt * 10);
    // Interaction prompt.
    const target = busy ? null : this.facing();
    const label = target ? this.promptFor(target) : null;
    if (label !== this.lastPrompt) {
      this.lastPrompt = label;
      this.app.hud.prompt(label);
    }
  }

  updatePlayer(dt, busy) {
    const p = this.player;
    if (p.moving) {
      p.moving.t += dt;
      const t = Math.min(1, p.moving.t / (p.moving.fast ? MoveTime / 2 : MoveTime));
      p.x = (p.moving.fx + (p.tx - p.moving.fx) * t) * TILE;
      p.y = (p.moving.fy + (p.ty - p.moving.fy) * t) * TILE;
      if (t >= 1) {
        p.moving = null;
        this.onStep();
      }
      return;
    }
    if (busy) return;
    const input = this.app.input;
    let dir = null;
    for (const d of ['up', 'down', 'left', 'right']) if (input.held(d)) dir = d;
    if (dir) this.route = null;
    else if (this.route) {
      const route = this.route;
      dir = route.steps.shift();
      if (!dir) {
        this.route = null;
        if (route.face) p.dir = route.face;
        if (route.target.type !== 'stairs') this.interact(route.target);
        return;
      }
    }
    if (!dir) { p.walk = 0; return; }
    p.dir = dir;
    const [dx, dy] = Dirs[dir];
    const nx = p.tx + dx;
    const ny = p.ty + dy;
    if (this.walkable(nx, ny)) {
      p.moving = { fx: p.tx, fy: p.ty, t: 0, fast: Boolean(this.route) };
      p.tx = nx;
      p.ty = ny;
      p.step = p.step === 1 ? 2 : 1;
      p.walk = 1;
      if ((nx + ny) % 2 === 0) sfx('step');
    } else if (this.route) {
      this.route = null;
    } else if (this.bumpCooldown <= 0) {
      this.bumpCooldown = 0.35;
      const e = this.entityAt(nx, ny);
      if (e && (e.type === 'guardian' || e.type === 'stairs')) this.interact(e);
    }
  }

  onStep() {
    const p = this.player;
    if (p.tx === this.stairs.x && p.ty === this.stairs.y) this.interact(this.stairs);
  }

  promptFor(e) {
    const { game } = this;
    switch (e.type) {
      case 'waystone': return '传送 / 存档';
      case 'campfire': return '营火：休息与整备';
      case 'merchant': case 'stall': return '商店';
      case 'sign': return '查看楼层告示';
      case 'ally': return game.eventPending() ? `与${e.name}交谈（事件）` : `与${e.name}交谈`;
      case 'device': return game.cleared() ? '已破坏的装置' : '调查装置';
      case 'guardian': return game.cleared() ? `再次挑战 ${e.name}` : `挑战 ${e.name}`;
      case 'chest': return e.opened ? null : '打开宝箱';
      case 'spot': return this.spotPrompt(e);
      case 'crack': return '敲开墙上的裂缝';
      case 'stairs': return game.cleared() ? (game.floor === MaxFloor ? '踏入天顶之门' : '前往下一层') : '楼梯被封印';
      case 'prop': return null;
      default: return null;
    }
  }

  spotPrompt(e) {
    if (e.kind === SpotKind.Merchant) return `${e.name}${e.done ? '（已光顾）' : ''}`;
    if (e.done) return e.solid ? `${e.name}（已探索）` : null;
    if (e.kind === SpotKind.Vault) return `${e.name}：需要 ${e.variant} 把钥匙（现有 ${this.game.run.keys}）`;
    if (e.kind === SpotKind.Puzzle) return `解谜：${e.name}`;
    return `探索：${e.name}`;
  }

  async interact(e) {
    if (!e || this.app.busy) return;
    const menus = this.app.menus;
    switch (e.type) {
      case 'waystone': menus.openWaystone(this); break;
      case 'campfire': menus.openCamp(this); break;
      case 'merchant': case 'stall': menus.openShop(); break;
      case 'sign': menus.openJournal(); break;
      case 'ally': await menus.talkAlly(this, e); break;
      case 'device': menus.inspectDevice(this); break;
      case 'guardian': await menus.challenge(this); break;
      case 'chest': this.openChest(e); break;
      case 'spot': menus.openSpot(this, e); break;
      case 'crack': this.breakCrack(e); break;
      case 'stairs': await this.goStairs(); break;
      default: break;
    }
  }

  async goStairs() {
    const { game } = this;
    if (!game.cleared()) {
      sfx('error');
      this.app.toast('楼梯被封印：先完成本层目标。');
      return;
    }
    if (game.floor === MaxFloor) {
      this.app.showEnding();
      return;
    }
    sfx('stairs');
    await this.app.transition(() => {
      game.advance();
      this.chest = null;
      this.enter();
    });
  }

  // A chest left where the guardian stood, holding the battle's rewards.
  placeChest(items) {
    if (!items.length) return;
    const spot = [[-1, 0], [0, -1], [0, 1], [-1, 1], [-1, -1]].map(([dx, dy]) => ({ x: this.guardian.x + dx, y: this.guardian.y + dy }))
      .find((s) => this.layout.at(s.x, s.y) === FLOOR && !this.entityAt(s.x, s.y) &&
        !(s.x === this.player.tx && s.y === this.player.ty));
    if (!spot) return;
    const entity = { type: 'chest', ...spot, solid: true, opened: false, items };
    this.chest = { floor: this.game.floor, entity };
    this.entities.push(entity);
    this.fx.burst(spot.x * TILE + 8, spot.y * TILE + 8, { color: [P.yellow, P.white], count: 20, speed: 50 });
  }
  openChest(e) {
    if (e.opened) return;
    e.opened = true;
    sfx('chest');
    this.fx.burst(e.x * TILE + 8, e.y * TILE + 4, { color: [P.yellow, P.gold, P.white], count: 30, speed: 70, gravity: 80, glow: true });
    this.app.menus.showChest(e.items);
  }

  updateAmbient(dt) {
    const theme = ChapterThemes[this.theme];
    const view = this.app.display;
    const cx = this.camera.x;
    const cy = this.camera.y;
    const rate = { leaf: 3, firefly: 4, bubble: 4, dust: 6, snow: 18, ember: 10, sparkle: 5, star: 4, ash: 8, circuit: 5 }[theme.ambient] ?? 3;
    if (Math.random() < rate * dt) {
      const x = cx + Math.random() * view.viewW;
      const y = cy + Math.random() * view.viewH;
      const kinds = {
        leaf: { x, y: cy - 4, vx: 10 + Math.random() * 10, vy: 14 + Math.random() * 8, color: Math.random() < 0.5 ? '#9be36b' : '#feae34', life: 6, size: 2 },
        firefly: { x, y, vx: (Math.random() - 0.5) * 8, vy: (Math.random() - 0.5) * 8, color: '#d8ff6b', life: 4, size: 1, glow: true },
        bubble: { x, y: cy + view.viewH, vx: 0, vy: -14 - Math.random() * 8, color: '#8ad0f0', life: 6, size: 2 },
        dust: { x, y, vx: 3, vy: -2, color: '#e4a672', life: 5, size: 1 },
        snow: { x, y: cy - 4, vx: -6 + Math.random() * 4, vy: 18 + Math.random() * 10, color: '#ffffff', life: 7, size: Math.random() < 0.3 ? 2 : 1 },
        ember: { x, y: cy + view.viewH, vx: (Math.random() - 0.5) * 10, vy: -20 - Math.random() * 14, color: Math.random() < 0.5 ? '#ff7a1a' : '#fee761', life: 5, size: 1, glow: true },
        sparkle: { x, y, vx: 0, vy: -3, color: '#f6757a', life: 1.5, size: 1, glow: true },
        star: { x, y, vx: 0, vy: 0, color: Math.random() < 0.5 ? '#fee761' : '#ffffff', life: 1.8, size: 1, glow: true },
        ash: { x, y: cy - 4, vx: 6, vy: 10 + Math.random() * 6, color: '#a8a296', life: 7, size: 1 },
        circuit: { x, y: cy + view.viewH, vx: 0, vy: -24, color: '#2ce8f5', life: 4, size: 1, glow: true },
      };
      const o = kinds[theme.ambient];
      if (o) this.ambient.push({ ...o, age: 0, sway: Math.random() * 6 });
    }
    for (const a of this.ambient) {
      a.age += dt;
      a.x += (a.vx + Math.sin(this.time * 2 + a.sway) * 4) * dt;
      a.y += a.vy * dt;
    }
    this.ambient = this.ambient.filter((a) => a.age < a.life);
  }

  draw(ctx) {
    const view = this.app.display;
    const shake = this.fx.shakeOffset();
    const camX = Math.round(this.camera.x) - shake.x;
    const camY = Math.round(this.camera.y) - shake.y;
    ctx.fillStyle = '#0e0b16';
    ctx.fillRect(0, 0, view.viewW, view.viewH);
    ctx.save();
    ctx.translate(-camX, -camY);
    ctx.drawImage(this.mapCanvas, 0, 0);
    // Entities sorted by their base row.
    const drawables = this.entities.map((e) => ({ e, y: e.y * TILE + (e.type === 'stairs' ? -100 : 0) }));
    drawables.push({ e: { type: 'player' }, y: this.player.y + 0.5 });
    drawables.sort((a, b) => a.y - b.y);
    for (const { e } of drawables) this.drawEntity(ctx, e);
    this.fx.draw(ctx, 'world');
    for (const a of this.ambient) {
      const t = a.age / a.life;
      ctx.globalAlpha = Math.min(1, (1 - t) * 2, t * 6);
      if (a.glow) {
        ctx.globalCompositeOperation = 'lighter';
        ctx.drawImage(tintGlow(a.color), a.x - 4, a.y - 4, 8, 8);
        ctx.globalCompositeOperation = 'source-over';
      }
      ctx.fillStyle = a.color;
      ctx.fillRect(Math.round(a.x), Math.round(a.y), a.size, a.size);
    }
    ctx.globalAlpha = 1;
    ctx.restore();
    this.drawLighting(ctx, camX, camY);
    ctx.save();
    ctx.translate(-camX, -camY);
    this.fx.draw(ctx, 'top');
    this.drawMarkers(ctx);
    ctx.restore();
    this.drawVignette(ctx);
    this.drawBanner(ctx);
  }

  drawEntity(ctx, e) {
    const t = this.time;
    const objects = Sprites.objects();
    const set = Sprites.tiles(this.theme);
    const px = e.x * TILE;
    const py = e.y * TILE;
    const shadow = (x, y, w = 12) => {
      ctx.fillStyle = 'rgba(0,0,0,0.25)';
      ctx.beginPath();
      ctx.ellipse(x, y, w / 2, 2.5, 0, 0, Math.PI * 2);
      ctx.fill();
    };
    switch (e.type) {
      case 'player': {
        const p = this.player;
        const frames = Sprites.heroWalker(this.game.hero.job())[p.dir];
        const frame = p.moving ? frames[p.step] : frames[0];
        shadow(p.x + 8, p.y + 15);
        ctx.drawImage(frame, Math.round(p.x), Math.round(p.y) - (p.moving ? 1 : 0));
        break;
      }
      case 'waystone': {
        const frames = objects.waystone;
        shadow(px + 8, py + 15, 14);
        ctx.drawImage(frames[Math.floor(t * 4) % frames.length], px, py - 16);
        break;
      }
      case 'campfire': {
        const frames = objects.campfire;
        ctx.drawImage(frames[Math.floor(t * 8) % frames.length], px, py);
        if (Math.random() < 0.15) this.fx.particle({ x: px + 8 + (Math.random() - 0.5) * 6, y: py + 6, vy: -18, vx: (Math.random() - 0.5) * 6, life: 0.8, size: 1, color: Math.random() < 0.5 ? P.yellow : P.orange });
        break;
      }
      case 'stall': ctx.drawImage(objects.stall, px, py - 16); break;
      case 'sign': ctx.drawImage(objects.sign, px, py); break;
      case 'merchant':
      case 'ally': {
        const set2 = e.type === 'merchant' ? Sprites.merchantWalker() : Sprites.allyWalker(this.theme);
        const near = Math.abs(this.player.tx - e.x) + Math.abs(this.player.ty - e.y) <= 2;
        let dir = 'down';
        if (near) {
          const dx = this.player.tx - e.x;
          const dy = this.player.ty - e.y;
          dir = Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 'right' : 'left') : (dy > 0 ? 'down' : 'up');
        }
        shadow(px + 8, py + 15);
        const bob = Math.floor(t * 2 + e.x) % 2;
        ctx.drawImage(set2[dir][0], px, py - bob);
        break;
      }
      case 'device': {
        const broken = this.game.cleared();
        ctx.drawImage(broken ? set.deviceBroken : set.device, px, py - 16);
        if (!broken && Math.random() < 0.08) this.fx.particle({ x: px + 8, y: py - 12, vy: -10, vx: (Math.random() - 0.5) * 10, life: 0.8, size: 1, color: ChapterThemes[this.theme].accent, glow: true });
        break;
      }
      case 'stairs': {
        const sealed = !this.game.cleared();
        ctx.drawImage(sealed ? set.sealed : set.stairs, px, py);
        if (sealed) {
          ctx.globalAlpha = 0.35 + Math.sin(t * 3) * 0.15;
          ctx.globalCompositeOperation = 'lighter';
          ctx.drawImage(tintGlow('#b06aff'), px - 8, py - 8, 32, 32);
          ctx.globalCompositeOperation = 'source-over';
          ctx.globalAlpha = 1;
        } else if (Math.random() < 0.1) {
          this.fx.particle({ x: px + 4 + Math.random() * 8, y: py + 12, vy: -16, life: 0.9, size: 1, color: P.yellow, glow: true });
        }
        break;
      }
      case 'guardian': {
        const sprite = Sprites.monster(e.entry).idle;
        const cleared = this.game.cleared();
        const bob = Math.round(Math.sin(t * 2.4) * 1);
        const x = px + 8 - sprite.width / 2;
        const y = py + 16 - sprite.height + bob;
        shadow(px + 8, py + 15, 20);
        if (e.elite || e.boss) {
          ctx.globalCompositeOperation = 'lighter';
          ctx.globalAlpha = 0.4 + Math.sin(t * 4) * 0.15;
          ctx.drawImage(tintGlow(e.boss ? '#fee761' : '#e43b44'), px - 16, py - 24, 48, 48);
          ctx.globalAlpha = 1;
          ctx.globalCompositeOperation = 'source-over';
        }
        if (cleared) ctx.globalAlpha = 0.55;
        ctx.drawImage(sprite, Math.round(x), Math.round(y));
        ctx.globalAlpha = 1;
        break;
      }
      case 'chest': {
        const [closed, open] = objects.chest;
        shadow(px + 8, py + 15);
        ctx.drawImage(e.opened ? open : closed, px, py);
        if (!e.opened && Math.random() < 0.06) this.fx.particle({ x: px + 4 + Math.random() * 8, y: py + 4, vy: -8, life: 0.8, size: 1, color: P.yellow, glow: true });
        break;
      }
      case 'prop': {
        const def = e.def;
        ctx.drawImage(def.canvas, px + (TILE - def.w) / 2, py + TILE - def.h);
        break;
      }
      case 'spot': this.drawSpot(ctx, e, px, py, shadow); break;
      case 'crack':
        ctx.drawImage(Sprites.spots(this.theme).crack, px, py);
        // Grit trickling from the crack hints at the space behind it.
        if (Math.random() < 0.02) this.fx.particle({ x: px + 5 + Math.random() * 6, y: py + 6 + Math.random() * 6, vy: 14, life: 0.7, size: 1, color: '#c0cbdc' });
        break;
      case 'rubble': ctx.drawImage(Sprites.spots(this.theme).rubble, px, py); break;
      case 'decor': ctx.drawImage(Sprites.objects().chest[1], px, py); break;
      default:
        break;
    }
  }

  // The canvas for a spot entity in its current state.
  spotSprite(e) {
    const art = Sprites.spots(this.theme);
    const look = (pair) => (e.done ? pair.spent : pair.live);
    switch (e.kind) {
      case SpotKind.Cache: return look(art.cache);
      case SpotKind.Curio: return look(art.curios[e.variant]);
      case SpotKind.Gather: return look(art.gather[chapterText(this.theme).gatherIngredient[e.variant]]);
      case SpotKind.AffixSource: return look(art.source);
      case SpotKind.Vault: return look(e.door ? art.door[e.variant - 1] : art.strongbox[e.variant - 1]);
      case SpotKind.Merchant: return Sprites.wandererWalker().down[0];
      case SpotKind.Puzzle: return look(art.stele);
      case SpotKind.Key: return art.key;
      case SpotKind.QuestItem: return art.questItem;
      case SpotKind.Lore: return art.lore;
      default: return null;
    }
  }

  drawSpot(ctx, e, px, py, shadow) {
    const t = this.time;
    if (e.kind === SpotKind.Merchant) {
      const frames = Sprites.wandererWalker();
      const dx = this.player.tx - e.x;
      const dy = this.player.ty - e.y;
      const near = Math.abs(dx) + Math.abs(dy) <= 2;
      const dir = !near ? 'down' : Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 'right' : 'left') : (dy > 0 ? 'down' : 'up');
      shadow(px + 8, py + 15);
      ctx.drawImage(frames[dir][0], px, py - (Math.floor(t * 2 + e.x) % 2));
      return;
    }
    const sprite = this.spotSprite(e);
    if (!sprite) return;
    if (e.door) {
      ctx.drawImage(sprite, px, py);
      return;
    }
    const float = PickedUp.includes(e.kind);
    const bob = float ? Math.round(Math.sin(t * 3 + e.x)) - 1 : 0;
    shadow(px + 8, py + 15, sprite.height > 16 ? 14 : 10);
    ctx.drawImage(sprite, px + (TILE - sprite.width) / 2, py + TILE - sprite.height + bob);
    if (!e.done && Math.random() < 0.04) {
      this.fx.particle({ x: px + 3 + Math.random() * 10, y: py + 2 + Math.random() * 8, vy: -8, life: 0.7, size: 1,
        color: e.light || P.yellow, glow: true });
    }
  }

  // Floating "!" and tags above the guardian and pending allies.
  drawMarkers(ctx) {
    const t = this.time;
    const g = this.guardian;
    const sprite = Sprites.monster(g.entry).idle;
    const top = g.y * TILE + 16 - sprite.height - 6 + Math.sin(t * 3) * 1.5;
    const cx = g.x * TILE + 8;
    let tag = null;
    if (this.game.cleared()) tag = ['已通过', '#63c74d'];
    else if (g.boss) tag = ['BOSS', '#f77622'];
    else if (g.elite) tag = ['精英', '#e43b44'];
    else if (g.objective !== Objective.Defeat) tag = [Campaign.objectiveName(g.objective), '#b55088'];
    if (tag) this.drawTag(ctx, cx, top, tag[0], tag[1]);
    for (const e of this.entities) {
      if (e.type === 'ally' && this.game.eventPending()) this.drawTag(ctx, e.x * TILE + 8, e.y * TILE - 6 + Math.sin(t * 4) * 1.5, '!', '#feae34');
      if (e.type === 'chest' && !e.opened) this.drawTag(ctx, e.x * TILE + 8, e.y * TILE - 4 + Math.sin(t * 4) * 1.5, '!', '#feae34');
      if (e.type !== 'spot' || e.done) continue;
      const bob = Math.sin(t * 4 + e.x) * 1.5;
      if (e.kind === SpotKind.QuestItem) this.drawTag(ctx, e.x * TILE + 8, e.y * TILE - 2 + bob, '!', '#feae34');
      if (e.kind === SpotKind.Puzzle) this.drawTag(ctx, e.x * TILE + 8, e.y * TILE - 18 + bob, '?', '#68386c');
      // Keys a vault needs, once the player is close enough to read the lock.
      const near = Math.abs(this.player.tx - e.x) + Math.abs(this.player.ty - e.y) <= 3;
      if (e.kind === SpotKind.Vault && near) {
        const enough = this.game.run.keys >= e.variant;
        this.drawTag(ctx, e.x * TILE + 8, e.y * TILE - 2 + bob, `钥匙×${e.variant}`, enough ? '#3e8948' : '#a22633');
      }
    }
  }
  drawTag(ctx, x, y, text, color) {
    ctx.font = 'bold 7px "PingFang SC", "Microsoft YaHei", "Noto Sans SC", sans-serif';
    const w = Math.ceil(ctx.measureText(text).width) + 6;
    ctx.fillStyle = '#181425';
    ctx.fillRect(Math.round(x - w / 2) - 1, Math.round(y - 8) - 1, w + 2, 11);
    ctx.fillStyle = color;
    ctx.fillRect(Math.round(x - w / 2), Math.round(y - 8), w, 9);
    ctx.fillStyle = '#ffffff';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, Math.round(x), Math.round(y - 3.5));
    ctx.textBaseline = 'alphabetic';
  }

  drawLighting(ctx, camX, camY) {
    const theme = ChapterThemes[this.theme];
    if (!theme.dark) return;
    const view = this.app.display;
    if (!this.lightCanvas || this.lightCanvas.width !== view.viewW || this.lightCanvas.height !== view.viewH) {
      this.lightCanvas = makeCanvas(view.viewW, view.viewH);
    }
    const lc = this.lightCanvas.getContext('2d');
    lc.globalCompositeOperation = 'source-over';
    lc.clearRect(0, 0, view.viewW, view.viewH);
    lc.fillStyle = `rgba(8,6,20,${theme.dark})`;
    lc.fillRect(0, 0, view.viewW, view.viewH);
    lc.globalCompositeOperation = 'destination-out';
    const hole = (x, y, r) => {
      const gradient = lc.createRadialGradient(x, y, 0, x, y, r);
      gradient.addColorStop(0, 'rgba(0,0,0,1)');
      gradient.addColorStop(0.6, 'rgba(0,0,0,0.7)');
      gradient.addColorStop(1, 'rgba(0,0,0,0)');
      lc.fillStyle = gradient;
      lc.fillRect(x - r, y - r, r * 2, r * 2);
    };
    const flicker = 1 + Math.sin(this.time * 9) * 0.04;
    hole(this.player.x + 8 - camX, this.player.y + 8 - camY, 72 * flicker);
    for (const e of this.entities) {
      if (e.light) hole(e.x * TILE + 8 - camX, e.y * TILE + 8 - camY, 56 * flicker);
      else if (e.type === 'prop' && e.def.flicker) hole(e.x * TILE + 8 - camX, e.y * TILE - camY, 34 * flicker);
      else if (e.type === 'guardian') hole(e.x * TILE + 8 - camX, e.y * TILE - camY, 40);
      else if (e.type === 'spot' && !e.done) hole(e.x * TILE + 8 - camX, e.y * TILE + 8 - camY, 22);
      else if (e.type === 'stairs') hole(e.x * TILE + 8 - camX, e.y * TILE + 8 - camY, 30);
    }
    ctx.drawImage(this.lightCanvas, 0, 0);
    // Colored glow from light sources on top.
    ctx.globalCompositeOperation = 'lighter';
    for (const e of this.entities) {
      const color = e.light || (e.type === 'prop' && e.def.flicker);
      if (!color) continue;
      ctx.globalAlpha = 0.18 * flicker;
      ctx.drawImage(tintGlow(color), e.x * TILE + 8 - camX - 28, e.y * TILE + 4 - camY - 28, 56, 56);
    }
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }

  drawVignette(ctx) {
    const view = this.app.display;
    if (!this.vignette || this.vignette.width !== view.viewW || this.vignette.height !== view.viewH) {
      this.vignette = makeCanvas(view.viewW, view.viewH);
      const vc = this.vignette.getContext('2d');
      const r = Math.max(view.viewW, view.viewH) * 0.75;
      const gradient = vc.createRadialGradient(view.viewW / 2, view.viewH / 2, r * 0.45, view.viewW / 2, view.viewH / 2, r);
      gradient.addColorStop(0, 'rgba(0,0,0,0)');
      gradient.addColorStop(1, 'rgba(0,0,0,0.45)');
      vc.fillStyle = gradient;
      vc.fillRect(0, 0, view.viewW, view.viewH);
    }
    ctx.drawImage(this.vignette, 0, 0);
  }

  drawBanner(ctx) {
    if (!this.banner) return;
    const view = this.app.display;
    const t = this.banner.t;
    const alpha = t < 0.3 ? t / 0.3 : t > 2.1 ? Math.max(0, (2.6 - t) / 0.5) : 1;
    ctx.globalAlpha = alpha;
    const y = view.viewH * 0.32;
    ctx.fillStyle = 'rgba(14,11,22,0.6)';
    ctx.fillRect(0, y - 16, view.viewW, 30);
    ctx.fillStyle = ChapterThemes[this.theme].accent;
    ctx.fillRect(0, y - 16, view.viewW, 1);
    ctx.fillRect(0, y + 13, view.viewW, 1);
    ctx.textAlign = 'center';
    ctx.fillStyle = '#fee761';
    ctx.font = 'bold 12px "PingFang SC", "Microsoft YaHei", "Noto Sans SC", sans-serif';
    ctx.fillText(this.banner.text, view.viewW / 2, y - 2);
    ctx.fillStyle = '#ead4aa';
    ctx.font = '7px "PingFang SC", "Microsoft YaHei", "Noto Sans SC", sans-serif';
    ctx.fillText(this.banner.sub, view.viewW / 2, y + 9);
    ctx.globalAlpha = 1;
  }
}

