// Top-down floor exploration: walk the generated map, talk to the merchant
// and allies, rest at the campfire and challenge the floor's guardian.
import { generateLayout, FLOOR } from '../engine/mapgen.js';
import { TILE } from '../engine/tiles.js';
import * as Sprites from '../engine/sprites.js';
import { makeCanvas } from '../engine/pixel.js';
import { ChapterThemes, P } from '../engine/palette.js';
import { Effects, tintGlow } from '../engine/effects.js';
import { sfx, playMusic } from '../engine/audio.js';
import { Bestiary } from '../rules/enemy.js';
import { chapterOf, MaxFloor } from '../rules/config.js';
import * as Campaign from '../rules/campaign.js';
import { Objective } from '../rules/combat.js';
import { modalOpen } from '../ui/dom.js';

const Dirs = { up: [0, -1], down: [0, 1], left: [-1, 0], right: [1, 0] };
const MoveTime = 0.14;

export class ExploreScene {
  constructor(app) {
    this.app = app;
    this.fx = new Effects();
    this.ambient = [];
    this.time = 0;
    this.chest = null;
    this.spawnAt = 'start';
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
    const layout = generateLayout({ seed: game.run.seed, floor, boss, propCount: set.props.length });
    this.layout = layout;
    const W = layout.w;
    const H = layout.h;
    const map = makeCanvas(W * TILE, H * TILE);
    const ctx = map.getContext('2d');
    const theme = ChapterThemes[this.theme];
    ctx.fillStyle = theme.wallTop;
    ctx.fillRect(0, 0, map.width, map.height);
    for (let y = 0; y < H; y++)
      for (let x = 0; x < W; x++) {
        const px = x * TILE;
        const py = y * TILE;
        if (layout.at(x, y) === FLOOR) {
          ctx.drawImage(set.floor[layout.variantAt(x, y)], px, py);
          if (layout.at(x, y - 1) !== FLOOR) {
            ctx.fillStyle = 'rgba(0,0,0,0.28)';
            ctx.fillRect(px, py, TILE, 4);
            ctx.fillStyle = 'rgba(0,0,0,0.12)';
            ctx.fillRect(px, py + 4, TILE, 3);
          }
          if (layout.at(x - 1, y) !== FLOOR) {
            ctx.fillStyle = 'rgba(0,0,0,0.15)';
            ctx.fillRect(px, py, 3, TILE);
          }
        } else if (layout.at(x, y + 1) === FLOOR) {
          ctx.drawImage(set.face[(x + y) % 3], px, py);
        } else {
          ctx.drawImage(set.top[(x * 7 + y * 3) % 3], px, py);
          // Dark rim where wall tops meet floor at the sides.
          ctx.fillStyle = 'rgba(0,0,0,0.35)';
          if (layout.at(x + 1, y) === FLOOR) ctx.fillRect(px + TILE - 2, py, 2, TILE);
          if (layout.at(x - 1, y) === FLOOR) ctx.fillRect(px, py, 2, TILE);
        }
      }
    this.mapCanvas = map;
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
    if (this.chest && this.chest.floor !== floor) this.chest = null;
    if (this.chest) add(this.chest.entity);
    // Player.
    const spawn = this.spawnAt === 'waystone' ? { x: layout.waystone.x + 1, y: layout.waystone.y + 1 } : layout.start;
    this.spawnAt = 'start';
    this.player = { tx: spawn.x, ty: spawn.y, x: spawn.x * TILE, y: spawn.y * TILE, dir: 'right', moving: null, step: 0, walk: 0 };
    this.camera = { x: this.player.x, y: this.player.y };
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
      const t = Math.min(1, p.moving.t / MoveTime);
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
    if (!dir) { p.walk = 0; return; }
    p.dir = dir;
    const [dx, dy] = Dirs[dir];
    const nx = p.tx + dx;
    const ny = p.ty + dy;
    if (this.walkable(nx, ny)) {
      p.moving = { fx: p.tx, fy: p.ty, t: 0 };
      p.tx = nx;
      p.ty = ny;
      p.step = p.step === 1 ? 2 : 1;
      p.walk = 1;
      if ((nx + ny) % 2 === 0) sfx('step');
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
      case 'stairs': return game.cleared() ? (game.floor === MaxFloor ? '踏入天顶之门' : '前往下一层') : '楼梯被封印';
      case 'prop': return null;
      default: return null;
    }
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
      default:
        break;
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

