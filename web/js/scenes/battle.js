// Side-view battle: runs the rules' BattleSession turn by turn and replays
// the events it emits as animations, numbers, sounds and screen shake.
import { Intent } from '../rules/enemy.js';
import { collect } from '../rules/log.js';
import { Job } from '../rules/jobs.js';
import { chapterOf } from '../rules/config.js';
import { Effects, tintGlow } from '../engine/effects.js';
import { easeInOut, easeOut, lerp } from '../engine/core.js';
import { Grid, tinted } from '../engine/pixel.js';
import { P } from '../engine/palette.js';
import * as Sprites from '../engine/sprites.js';
import { playMusic, sfx } from '../engine/audio.js';
import { drawBackdrop } from './backdrop.js';
import { BattleUI } from '../ui/battle_ui.js';

const StageW = 320;
const StageH = 180;

// Small pixel icons drawn above the enemy for its intent.
const IntentIcons = (() => {
  const make = (rows, palette) => new Grid(9, 9).stamp(0, 0, rows, palette).outline(P.outline).toCanvas();
  return {
    Attack: () => make(['......ww.', '.....ww..', '....ww...', '.r.ww....', '..ww.....', '.wrr.....', 'w..r.....', '.........', '.........'], { w: P.silver, r: P.brown }),
    Charge: () => make(['....yy...', '...yy....', '..yyyyy..', '....yy...', '...yy....', '..yy.....', '.........', '.........', '.........'], { y: P.yellow }),
    HeavyStrike: () => make(['.r..r..r.', '..rrrrr..', '.rryyyrr.', 'rryywyyrr', '.rryyyrr.', '..rrrrr..', '.r..r..r.', '.........', '.........'], { r: P.red, y: P.orange, w: P.white }),
    Wound: () => make(['...ggg...', '...gwg...', '.gggwggg.', '.gwwwwwg.', '.gggwggg.', '..rgwg...', '.r.ggg...', 'r........', '.........'], { g: P.green, w: P.white, r: P.red }),
    Shatter: () => make(['.ssssss..', '.sw.ssb..', '.s.s.sb..', '.ss.s.s..', '..ss.ss..', '...sss...', '....s....', '.........', '.........'], { s: P.steel, w: P.white, b: P.slate }),
    ReadPattern: () => make(['.........', '..ppppp..', '.pwwwwwp.', 'pwwrrrwwp', '.pwwwwwp.', '..ppppp..', '.........', '.........', '.........'], { p: P.magenta, w: P.white, r: P.red }),
    Recover: () => make(['.....zzz.', '.......z.', '......z..', '.zz..zzz.', '...z.....', '..z......', '.zz......', '.........', '.........'], { z: P.cyan }),
  };
})();
const iconCache = {};
const intentIcon = (kind) => (iconCache[kind] ??= IntentIcons[kind]?.());

const IntentNames = {
  Attack: '普通攻击', Charge: '蓄力', HeavyStrike: '重击', Wound: '压制治疗', Shatter: '破防', ReadPattern: '预判', Recover: '休整',
};

export class BattleScene {
  constructor(app, { monster, rules, battle, floor, onFinish }) {
    this.app = app;
    this.monster = monster;
    this.rules = rules;
    this.battle = battle;
    this.floor = floor;
    this.onFinish = onFinish;
    this.fx = new Effects();
    this.tweens = [];
    this.time = 0;
    this.hitstop = 0;
    this.theme = chapterOf(floor);
    this.boss = monster.isBoss();
    const job = app.game.hero.job();
    this.job = job;
    this.heroSprites = Sprites.heroBattle(job);
    this.enemySprites = Sprites.monster(monster.bestiaryEntry());
    this.hero = this.makeActor(this.heroSprites, 1);
    this.enemy = this.makeActor(this.enemySprites, this.boss ? 2 : 1);
    this.enemy.ox = 120;
    this.hero.ox = -120;
    this.intent = null;
    this.dim = 0;
  }

  makeActor(sprites, scale) {
    return { sprites, scale, ox: 0, oy: 0, pose: 'idle', flash: 0, alpha: 1, tint: null, tintTime: 0, rot: 0, dead: false, squash: 0 };
  }

  enter() {
    this.app.display.setMode('battle');
    this.app.hud.show(false);
    playMusic(this.boss ? 'boss' : 'battle');
    this.ui = new BattleUI(this.app, this);
    this.run();
  }
  exit() {
    this.ui?.destroy();
  }

  // ---- Layout -----------------------------------------------------------------------
  layout() {
    const view = this.app.display;
    const portrait = view.viewH > view.viewW * 1.1;
    if (portrait) {
      // Narrow screens: a tighter stage so the fighters stay large.
      const stageW = 200;
      const stageX = Math.floor((view.viewW - stageW) / 2);
      const groundY = Math.round(view.viewH * 0.5);
      return { stageX, stageY: groundY - 128, stageW, groundY, heroX: stageX + 52, enemyX: stageX + 150, w: view.viewW, h: view.viewH };
    }
    const stageX = Math.floor((view.viewW - StageW) / 2);
    const stageY = Math.floor((view.viewH - StageH) / 2) - 14;
    const groundY = stageY + 128;
    return { stageX, stageY, stageW: StageW, groundY, heroX: stageX + 96, enemyX: stageX + 226, w: view.viewW, h: view.viewH };
  }
  backdrop() {
    const L = this.layout();
    const key = `${L.w}x${L.h}x${L.groundY}`;
    if (this.backdropKey !== key) {
      this.backdropKey = key;
      this.backdropCanvas = drawBackdrop(this.theme, L.w, L.h, L.groundY);
    }
    return this.backdropCanvas;
  }
  // Center of an actor's body in screen space.
  heroPos() {
    const L = this.layout();
    return { x: L.heroX + this.hero.ox, y: L.groundY - 14 + this.hero.oy };
  }
  enemyPos() {
    const L = this.layout();
    const h = this.enemySprites.idle.height * this.enemy.scale;
    return { x: L.enemyX + this.enemy.ox, y: L.groundY - h / 2 + 2 + this.enemy.oy, top: L.groundY - h + this.enemy.oy };
  }

  // ---- Tweens and timing --------------------------------------------------------------
  wait(seconds) { return this.app.loop.wait(seconds); }
  tween(obj, to, duration, ease = easeInOut) {
    return new Promise((resolve) => {
      const from = {};
      for (const key of Object.keys(to)) from[key] = obj[key];
      this.tweens.push({ obj, from, to, duration, t: 0, ease, resolve });
    });
  }
  freeze(seconds) { this.hitstop = Math.max(this.hitstop, seconds); }

  update(dt) {
    this.time += dt;
    if (this.hitstop > 0) {
      this.hitstop -= dt;
      return;
    }
    this.fx.update(dt);
    for (const tw of this.tweens) {
      tw.t += dt;
      const k = tw.ease(Math.min(1, tw.t / tw.duration));
      for (const key of Object.keys(tw.to)) tw.obj[key] = lerp(tw.from[key], tw.to[key], k);
    }
    const done = this.tweens.filter((tw) => tw.t >= tw.duration);
    this.tweens = this.tweens.filter((tw) => tw.t < tw.duration);
    for (const tw of done) tw.resolve();
    for (const actor of [this.hero, this.enemy]) {
      actor.flash = Math.max(0, actor.flash - dt);
      actor.tintTime = Math.max(0, actor.tintTime - dt);
    }
    // Ambient effects tied to the enemy's intent.
    const pos = this.enemyPos();
    const kind = this.intent?.kind;
    if (!this.enemy.dead && (kind === Intent.Charge || kind === Intent.HeavyStrike) && Math.random() < dt * 30) {
      const a = Math.random() * Math.PI * 2;
      const r = 26 * this.enemy.scale;
      const color = kind === Intent.HeavyStrike ? P.red : P.yellow;
      this.fx.particle({ x: pos.x + Math.cos(a) * r, y: pos.y + Math.sin(a) * r, vx: -Math.cos(a) * r * 2.2, vy: -Math.sin(a) * r * 2.2, life: 0.4, size: 2, color, glow: true });
    }
    if (!this.enemy.dead && kind === Intent.Recover && Math.random() < dt * 6) {
      this.fx.particle({ x: pos.x + 6, y: pos.top - 2, vx: 8, vy: -12, life: 1, size: 1, color: P.cyan });
    }
    this.ui?.tick(dt);
  }

  // ---- Drawing ----------------------------------------------------------------------
  draw(ctx) {
    const L = this.layout();
    const shake = this.fx.shakeOffset();
    ctx.save();
    ctx.translate(shake.x, shake.y);
    ctx.drawImage(this.backdrop(), 0, 0);
    if (this.dim > 0) {
      ctx.fillStyle = `rgba(8,6,20,${this.dim})`;
      ctx.fillRect(0, 0, L.w, L.h);
    }
    this.fx.draw(ctx, 'back');
    this.drawActor(ctx, this.enemy, L.enemyX, L.groundY, true);
    this.drawActor(ctx, this.hero, L.heroX, L.groundY, false);
    this.fx.draw(ctx, 'world');
    this.drawIntent(ctx);
    this.fx.draw(ctx, 'top');
    ctx.restore();
    this.fx.drawFlash(ctx, L.w, L.h);
  }

  drawActor(ctx, actor, baseX, groundY, isEnemy) {
    const sprite = actor.sprites[actor.pose] || actor.sprites.idle;
    const s = actor.scale;
    const w = sprite.width * s;
    const h = sprite.height * s;
    const bob = actor.dead ? 0 : Math.round(Math.sin(this.time * 3 + (isEnemy ? 1 : 0)) * 0.8);
    const x = Math.round(baseX + actor.ox - w / 2);
    const y = Math.round(groundY - h + actor.oy + bob + 1);
    // Shadow.
    ctx.globalAlpha = 0.3 * actor.alpha;
    ctx.fillStyle = '#000';
    ctx.beginPath();
    ctx.ellipse(baseX + actor.ox, groundY + 1, w * 0.32, 3 * s, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.globalAlpha = actor.alpha;
    // Elite / boss aura.
    if (isEnemy && !actor.dead && (this.monster.elite || this.boss)) {
      ctx.globalCompositeOperation = 'lighter';
      ctx.globalAlpha = actor.alpha * (0.25 + Math.sin(this.time * 3) * 0.08);
      ctx.drawImage(tintGlow(this.boss ? '#fee761' : '#e43b44'), x - w * 0.3, y - h * 0.2, w * 1.6, h * 1.4);
      ctx.globalCompositeOperation = 'source-over';
      ctx.globalAlpha = actor.alpha;
    }
    ctx.save();
    if (actor.rot) {
      ctx.translate(x + w / 2, y + h);
      ctx.rotate(actor.rot);
      ctx.translate(-(x + w / 2), -(y + h));
    }
    const squashY = 1 - actor.squash;
    ctx.drawImage(sprite, x, y + h * (1 - squashY), w, h * squashY);
    if (actor.tintTime > 0 && actor.tint) {
      ctx.globalAlpha = actor.alpha * Math.min(0.7, actor.tintTime * 2);
      ctx.drawImage(tinted(sprite, actor.tint), x, y + h * (1 - squashY), w, h * squashY);
    }
    if (actor.flash > 0) {
      ctx.globalAlpha = actor.alpha * Math.min(1, actor.flash * 8);
      ctx.drawImage(tinted(sprite, '#ffffff'), x, y + h * (1 - squashY), w, h * squashY);
    }
    ctx.restore();
    ctx.globalAlpha = 1;
  }

  drawIntent(ctx) {
    if (!this.intent || this.enemy.dead || this.battle.finished()) return;
    const pos = this.enemyPos();
    const icon = intentIcon(this.intent.kind);
    if (!icon) return;
    const y = Math.round(pos.top - 16 + Math.sin(this.time * 4) * 1.5);
    const x = Math.round(pos.x);
    const label = this.intentLabel();
    ctx.font = 'bold 6px "PingFang SC", "Microsoft YaHei", "Noto Sans SC", sans-serif';
    const tw = Math.ceil(ctx.measureText(label).width);
    const w = tw + 16;
    ctx.fillStyle = 'rgba(14,11,22,0.85)';
    ctx.fillRect(x - w / 2, y - 1, w, 11);
    ctx.fillStyle = this.intentDanger() ? P.red : this.intent.kind === Intent.Recover ? P.green : P.steel;
    ctx.fillRect(x - w / 2, y + 10, w, 1);
    ctx.drawImage(icon, x - w / 2 + 1, y);
    ctx.fillStyle = '#ffffff';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText(label, x - w / 2 + 12, y + 5);
    ctx.textBaseline = 'alphabetic';
  }
  intentLabel() {
    const i = this.intent;
    const name = IntentNames[i.kind];
    if (i.kind === Intent.ReadPattern && i.predictedAction !== -99) return `${name}：${this.actionName(i.predictedAction)}`;
    if (i.damagePercent > 0) return `${name} ${i.damagePercent}%`;
    return name;
  }
  intentDanger() {
    const k = this.intent?.kind;
    return k === Intent.HeavyStrike || k === Intent.Charge || k === Intent.Wound || k === Intent.Shatter ||
      (k === Intent.ReadPattern && this.intent.predictedAction !== -99);
  }
  actionName(id) {
    if (id === 0) return '普攻';
    if (id === -1) return '格挡';
    if (id === -2) return '药水';
    const skill = this.app.game.hero.getSkill(id);
    return skill ? skill.getskillname() : '?';
  }

  // ---- Battle flow ----------------------------------------------------------------------
  async run() {
    const L = this.layout();
    this.fx.label(L.stageX + L.stageW / 2, L.stageY + 40, this.boss ? '— BOSS —' : this.monster.elite ? '— 精英 —' : '— 遭遇 —',
      this.boss ? P.yellow : this.monster.elite ? P.red : P.white, { size: 10, life: 1.6, rise: 4 });
    sfx('encounter');
    await Promise.all([this.tween(this.enemy, { ox: 0 }, 0.5, easeOut), this.tween(this.hero, { ox: 0 }, 0.45, easeOut)]);
    this.ui.showIntro();
    for (;;) {
      const { result: alive, events } = collect(() => this.battle.beginTurn());
      await this.play(events);
      if (!alive || this.battle.finished()) break;
      this.intent = this.battle.intent;
      this.ui.setIntent(this.intent);
      this.ui.refresh();
      let acted = false;
      while (!acted) {
        const action = await this.ui.waitForAction();
        const { result, events: actEvents } = collect(() => this.battle.act(action));
        if (!result) {
          sfx('error');
          this.ui.flashHint('行动未执行，没有推进回合。');
          continue;
        }
        acted = true;
        this.ui.lock();
        this.intent = null;
        await this.play(actEvents, action);
        this.intent = null;
        this.ui.refresh();
      }
      if (this.battle.finished()) break;
      await this.wait(0.15);
    }
    await this.finish();
  }

  async finish() {
    const battle = this.battle;
    const won = battle.won();
    if (won && this.monster.isalive()) {
      // Objective completed: the guardian retreats.
      this.fx.label(this.enemyPos().x, this.enemyPos().top - 4, '目标完成！', P.yellow, { size: 10, life: 1.4 });
      await this.tween(this.enemy, { ox: 160, alpha: 0 }, 0.6);
    }
    if (!won && this.app.game.hero.isalive()) {
      this.fx.label(this.heroPos().x, this.heroPos().y - 24, battle.report.timeout ? '目标超时' : '撤退', P.red, { size: 10, life: 1.4 });
      await this.tween(this.hero, { ox: -150, alpha: 0 }, 0.6);
    }
    sfx(won ? 'victory' : 'defeat');
    playMusic(null);
    await this.wait(0.5);
    const result = this.app.game.finishBattle(this.monster, battle);
    this.ui.showResult(result, () => this.onFinish(result));
  }

  // ---- Event playback ---------------------------------------------------------------------
  async play(events, action = null) {
    const queue = [...events];
    let pendingCrit = false;
    let castInfo = null;
    let attackApproached = false;
    let poisonNext = false;
    while (queue.length) {
      const e = queue.shift();
      this.ui.log(e);
      switch (e.type) {
        case 'intent':
          break;
        case 'poison': {
          const p = this.heroPos();
          poisonNext = true;
          sfx('poison');
          this.fx.burst(p.x, p.y, { color: [P.green, '#9be36b'], count: 14, speed: 30, gravity: -40, life: 0.7 });
          await this.wait(0.2);
          break;
        }
        case 'attack': {
          pendingCrit = e.crit;
          await this.heroLunge();
          attackApproached = true;
          const p = this.enemyPos();
          this.fx.slash(p.x - 4, p.y, { radius: 14, angle: -2.2, sweep: 2.4, color: e.crit ? P.yellow : '#ffffff', width: e.crit ? 5 : 3 });
          sfx(e.crit ? 'crit' : 'slash');
          if (e.crit) {
            this.freeze(0.09);
            this.fx.label(p.x, p.top - 2, '暴击！', P.yellow, { size: 10 });
            this.fx.flash('#fff6c0', 0.12);
          }
          break;
        }
        case 'cast':
          castInfo = e;
          await this.castSkill(e);
          attackApproached = Math.abs(this.hero.ox) > 1 || this.hero.alpha < 1;
          break;
        case 'strike':
          if (castInfo) await this.skillHit(castInfo, e);
          break;
        case 'followUp': {
          const p = this.enemyPos();
          await this.fx.projectile(p.x + 40, p.top - 60, p.x, p.y, { color: P.orange, size: 5, time: 0.25 });
          this.fx.burst(p.x, p.y, { color: [P.orange, P.yellow, P.red], count: 24, speed: 80, glow: true });
          sfx('explode');
          this.fx.shake(4, 0.25);
          break;
        }
        case 'damage':
          await this.showDamage(e, pendingCrit, poisonNext);
          pendingCrit = false;
          poisonNext = false;
          break;
        case 'dodge':
          await this.showDodge(e.side);
          pendingCrit = false;
          break;
        case 'attackHit': {
          const p = this.heroPos();
          this.fx.number(p.x - 10, p.y - 8, `+${e.mana}`, '#5ad0ff');
          this.ui.refresh();
          break;
        }
        case 'mana': {
          const p = this.heroPos();
          sfx('mana');
          this.fx.number(p.x - 10, p.y - 8, `+${e.amount}`, '#5ad0ff');
          this.fx.burst(p.x, p.y, { color: ['#5ad0ff', P.white], count: 8, speed: 25, gravity: -30 });
          break;
        }
        case 'heal': {
          const p = this.heroPos();
          if (e.amount > 0) {
            sfx('heal');
            this.fx.number(p.x, p.y - 14, `+${e.amount}`, P.green);
            for (let i = 0; i < 10; i++)
              this.fx.particle({ x: p.x + (Math.random() - 0.5) * 18, y: p.y + 8, vy: -30 - Math.random() * 20, life: 0.8, size: 2, color: i % 2 ? P.green : P.white, glow: true });
            if (e.suppressed) this.fx.label(p.x, p.y - 30, '抑疗', P.red, { size: 8 });
            if (castInfo && this.job === Job.Assassin && castInfo.slot === 4) await this.drainStream();
          }
          this.ui.refresh();
          await this.wait(0.15);
          break;
        }
        case 'potion': {
          const p = this.heroPos();
          const icon = Sprites.itemIcon('potion', 1);
          this.fx.custom(0.6, (ctx, t) => {
            ctx.globalAlpha = 1 - t;
            ctx.drawImage(icon, Math.round(p.x - 8), Math.round(p.y - 30 - t * 12));
            ctx.globalAlpha = 1;
          }, 'top');
          await this.wait(0.3);
          break;
        }
        case 'guard': {
          await this.showGuard(e.pierced);
          this.ui.refresh();
          break;
        }
        case 'weaken': {
          const p = this.enemyPos();
          this.fx.label(p.x, p.top + 4, `攻击 -${e.percent}%`, '#5ad0ff', { size: 8, life: 1.2 });
          for (let i = 0; i < 8; i++)
            this.fx.particle({ x: p.x + (Math.random() - 0.5) * 20, y: p.top + 4, vy: 30, life: 0.6, size: 2, color: '#5ad0ff' });
          this.enemy.tint = '#5ad0ff';
          this.enemy.tintTime = 0.6;
          this.ui.refresh();
          break;
        }
        case 'interrupt': {
          const p = this.enemyPos();
          sfx('interrupt');
          this.freeze(0.12);
          this.fx.flash('#ffffff', 0.15);
          this.fx.shake(5, 0.3);
          this.fx.label(p.x, p.top - 6, '打断！', P.yellow, { size: 14, life: 1.4 });
          this.fx.ring(p.x, p.y, { r0: 4, r1: 40 * this.enemy.scale, color: P.yellow, width: 3 });
          this.intent = null;
          await this.tween(this.enemy, { ox: 10, rot: 0.15 }, 0.1);
          await this.tween(this.enemy, { ox: 0, rot: 0 }, 0.25);
          break;
        }
        case 'defeated':
          if (attackApproached) await this.heroReturn();
          attackApproached = false;
          await this.enemyDeath();
          break;
        case 'blast': {
          const p = this.enemyPos();
          sfx('explode');
          this.fx.burst(p.x, p.y, { color: [P.orange, P.yellow, P.red, P.white], count: 50, speed: 120, glow: true, life: 0.8 });
          this.fx.ring(p.x, p.y, { r0: 4, r1: 70, color: P.orange, width: 4, life: 0.5 });
          this.fx.flash('#ffb060', 0.25);
          this.fx.shake(7, 0.4);
          await this.wait(0.25);
          break;
        }
        case 'enemyAction':
          if (attackApproached) { await this.heroReturn(); attackApproached = false; }
          await this.enemyAct(e);
          break;
        case 'enemyHeal': {
          const p = this.enemyPos();
          sfx('heal');
          this.fx.number(p.x, p.top, `+${e.amount}`, P.green);
          this.fx.burst(p.x, p.y, { color: [P.green, P.white], count: 12, speed: 30, gravity: -40 });
          this.ui.refresh();
          await this.wait(0.3);
          break;
        }
        case 'objective':
          this.ui.refresh();
          break;
        case 'timeout':
          this.ui.refresh();
          break;
        default:
          break;
      }
    }
    if (attackApproached) await this.heroReturn();
    if (castInfo) { this.hero.pose = 'idle'; this.dim = 0; }
    this.ui.refresh();
  }

  async heroLunge(distance = null) {
    const L = this.layout();
    const target = distance ?? (L.enemyX - L.heroX - 30 * this.enemy.scale);
    this.hero.pose = 'attack';
    await this.tween(this.hero, { ox: target }, 0.14, easeOut);
  }
  async heroReturn() {
    this.hero.pose = 'idle';
    await this.tween(this.hero, { ox: 0, oy: 0, alpha: 1 }, 0.18, easeInOut);
  }

  async showDamage(e, crit, poison = false) {
    if (e.side === 'enemy') {
      const p = this.enemyPos();
      this.enemy.flash = 0.12;
      this.fx.number(p.x + (Math.random() - 0.5) * 8, p.top + 6, e.amount, crit ? P.yellow : '#ffffff', { big: crit });
      this.fx.burst(p.x - 6, p.y, { color: ['#ffffff', P.yellow], count: crit ? 16 : 8, speed: crit ? 90 : 60, angle: 0, spread: 2.4 });
      if (e.amount > 0) sfx('hit');
      this.fx.shake(crit ? 4 : 2, 0.15);
      this.tween(this.enemy, { ox: 6 }, 0.05).then(() => this.tween(this.enemy, { ox: 0 }, 0.15));
      this.ui.refresh();
      await this.wait(crit ? 0.22 : 0.14);
    } else {
      const p = this.heroPos();
      this.hero.flash = 0.12;
      this.fx.number(p.x, p.y - 18, e.amount, poison ? '#9be36b' : '#ff6b6b');
      if (e.guarded) this.fx.label(p.x, p.y - 34, '格挡减半', P.cyan, { size: 8 });
      this.fx.burst(p.x + 6, p.y, { color: poison ? [P.green] : [P.red, '#ffffff'], count: 8, speed: 50, angle: Math.PI, spread: 2.4 });
      if (!poison && e.amount > 0) sfx('hit');
      this.fx.shake(e.amount > this.app.game.hero.getmaxhp() * 0.15 ? 5 : 2, 0.2);
      this.tween(this.hero, { ox: -6 }, 0.05).then(() => this.tween(this.hero, { ox: this.hero.ox < -2 ? 0 : this.hero.ox }, 0.15));
      this.ui.refresh();
      if (!this.app.game.hero.isalive()) await this.heroDeath();
      await this.wait(0.16);
    }
  }
  async showDodge(side) {
    const actor = side === 'enemy' ? this.enemy : this.hero;
    const p = side === 'enemy' ? this.enemyPos() : this.heroPos();
    sfx('dodge');
    this.fx.label(p.x, (side === 'enemy' ? p.top : p.y - 24), '闪避', P.cyan, { size: 9 });
    const dir = side === 'enemy' ? 1 : -1;
    const base = actor.ox;
    await this.tween(actor, { ox: base + 14 * dir, alpha: 0.5 }, 0.08);
    await this.tween(actor, { ox: base, alpha: 1 }, 0.2);
  }
  async showGuard(pierced) {
    const p = this.heroPos();
    sfx('guard');
    const color = pierced ? P.red : P.cyan;
    this.fx.custom(0.9, (ctx, t) => {
      ctx.globalAlpha = (1 - t) * 0.8;
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(p.x + 6, p.y, 18 + t * 4, -1.1, 1.1);
      ctx.stroke();
      ctx.globalAlpha = (1 - t) * 0.25;
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(p.x + 6, p.y, 18 + t * 4, -1.1, 1.1);
      ctx.fill();
      ctx.globalAlpha = 1;
    }, 'world');
    this.fx.label(p.x, p.y - 26, pierced ? '格挡（将被破防）' : '格挡', color, { size: 8 });
    await this.wait(0.3);
  }
  async heroDeath() {
    this.hero.dead = true;
    sfx('defeat');
    await this.tween(this.hero, { rot: -1.4, oy: 2 }, 0.5, easeOut);
  }
  async enemyDeath() {
    const p = this.enemyPos();
    this.enemy.dead = true;
    this.intent = null;
    this.freeze(0.12);
    this.enemy.flash = 0.5;
    this.fx.shake(this.boss ? 8 : 4, 0.4);
    await this.wait(0.2);
    const sprite = this.enemySprites.idle;
    // Dissolve into pixels.
    const s = this.enemy.scale;
    const data = sprite.getContext('2d').getImageData(0, 0, sprite.width, sprite.height).data;
    const x0 = p.x - (sprite.width * s) / 2;
    const y0 = p.top;
    for (let y = 0; y < sprite.height; y += 2)
      for (let x = 0; x < sprite.width; x += 2) {
        const i = (y * sprite.width + x) * 4;
        if (data[i + 3] < 128) continue;
        if (Math.random() < 0.5) continue;
        const color = `#${[data[i], data[i + 1], data[i + 2]].map((v) => v.toString(16).padStart(2, '0')).join('')}`;
        this.fx.particle({ x: x0 + x * s, y: y0 + y * s, vx: (Math.random() - 0.3) * 40, vy: -20 - Math.random() * 50, ay: 30,
          life: 0.8 + Math.random() * 0.6, size: 2 * s, color, shrink: true });
      }
    sfx('explode');
    await this.tween(this.enemy, { alpha: 0 }, 0.15);
    await this.wait(0.4);
  }

  async enemyAct(e) {
    const p = this.heroPos();
    const ep = this.enemyPos();
    const L = this.layout();
    const reach = -(L.enemyX - L.heroX - 26 - 10 * this.enemy.scale);
    switch (e.kind) {
      case 'Interrupted':
        this.fx.label(ep.x, ep.top - 4, '被打断', P.cyan, { size: 8 });
        await this.wait(0.3);
        return;
      case Intent.Charge: {
        sfx('charge');
        this.fx.label(ep.x, ep.top - 4, '蓄力中…', P.yellow, { size: 9, life: 1.2 });
        this.fx.ring(ep.x, ep.y, { r0: 40 * this.enemy.scale, r1: 6, color: P.yellow, width: 2, life: 0.6 });
        await this.tween(this.enemy, { squash: 0.12 }, 0.3);
        await this.tween(this.enemy, { squash: 0 }, 0.2);
        return;
      }
      case Intent.Recover:
        this.fx.label(ep.x, ep.top - 4, '休整', P.green, { size: 9 });
        await this.wait(0.4);
        return;
      default:
        break;
    }
    if (e.damagePercent <= 0) return;
    const heavy = e.kind === Intent.HeavyStrike;
    if (heavy) {
      sfx('charge');
      await this.tween(this.enemy, { ox: 16, oy: -10 }, 0.25, easeOut);
      this.fx.flash('#ff2020', 0.1);
    }
    this.enemy.pose = 'attack';
    await this.tween(this.enemy, { ox: reach, oy: 0 }, heavy ? 0.12 : 0.16, easeOut);
    switch (e.kind) {
      case Intent.HeavyStrike:
        sfx('heavy');
        this.freeze(0.1);
        this.fx.shake(9, 0.45);
        this.fx.ring(p.x, p.y, { r0: 6, r1: 50, color: P.red, width: 4 });
        this.fx.burst(p.x, p.y, { color: [P.red, P.orange, P.yellow], count: 30, speed: 110, glow: true });
        break;
      case Intent.Wound:
        sfx('slash');
        for (let i = 0; i < 3; i++) this.fx.line(p.x + 10 - i * 4, p.y - 14, p.x - 6 - i * 4, p.y + 12, { color: '#3e8948', width: 2, life: 0.45 });
        this.fx.label(p.x, p.y - 30, '治疗压制', P.green, { size: 8 });
        break;
      case Intent.Shatter:
        sfx('shatter');
        for (let i = 0; i < 14; i++)
          this.fx.particle({ x: p.x + 12, y: p.y, vx: -30 - Math.random() * 60, vy: (Math.random() - 0.5) * 80, ay: 120, life: 0.7, size: 2, color: i % 2 ? P.cyan : P.silver });
        this.fx.label(p.x, p.y - 30, '破防！', P.red, { size: 9 });
        break;
      case Intent.ReadPattern:
        sfx('read');
        if (e.predicted === true) {
          this.fx.label(p.x, p.y - 32, '被识破！', P.red, { size: 10 });
          this.fx.shake(5, 0.25);
        } else if (e.predicted === false) this.fx.label(p.x, p.y - 32, '预判落空', P.green, { size: 9 });
        this.fx.slash(p.x + 4, p.y, { radius: 16, angle: -1, sweep: 2.4, dir: -1, color: P.magenta, width: 3 });
        break;
      default:
        sfx('slash');
        this.fx.slash(p.x + 4, p.y, { radius: 14, angle: -1, sweep: 2.2, dir: -1, color: '#ffffff', width: 3 });
        break;
    }
    // The damage event follows; return after it plays.
    this.enemy.pose = 'idle';
    this.wait(heavy ? 0.26 : 0.16).then(() => this.tween(this.enemy, { ox: 0, oy: 0 }, 0.25, easeInOut));
  }

  async drainStream() {
    const ep = this.enemyPos();
    const hp = this.heroPos();
    const runs = [];
    for (let i = 0; i < 8; i++)
      runs.push(this.wait(i * 0.04).then(() => this.fx.projectile(ep.x + (Math.random() - 0.5) * 12, ep.y + (Math.random() - 0.5) * 12, hp.x, hp.y,
        { color: P.crimson, size: 2, time: 0.3, arc: 10 + Math.random() * 10, trail: P.wine })));
    await Promise.all(runs);
  }

  // ---- Skills ---------------------------------------------------------------------------
  // Plays the wind-up of a skill and returns when its first hit should land.
  async castSkill(e) {
    const L = this.layout();
    const hp = this.heroPos();
    const ep = this.enemyPos();
    const job = this.job;
    const slot = e.slot;
    const name = e.name;
    this.fx.label(hp.x, hp.y - 30, name, P.yellow, { size: 9, life: 1.1 });
    const near = L.enemyX - L.heroX - 30 * this.enemy.scale;
    if (job === Job.Warrior) {
      if (slot === 2) {
        sfx('charge');
        this.fx.ring(hp.x, hp.y, { r0: 4, r1: 40, color: P.gold, width: 3, life: 0.4 });
        await this.wait(0.2);
      }
      if (slot === 3) {
        sfx('guard');
        this.fx.ring(hp.x + 10, hp.y, { r0: 2, r1: 20, color: P.cyan, width: 3, squash: 1.4 });
      }
      if (slot === 4) {
        sfx('charge');
        for (let i = 0; i < 24; i++)
          this.fx.particle({ x: hp.x + (Math.random() - 0.5) * 20, y: hp.y + 14, vy: -50 - Math.random() * 40, life: 0.6, size: 2, color: i % 2 ? P.red : P.crimson, glow: true });
        this.hero.tint = P.red;
        this.hero.tintTime = 0.8;
        await this.wait(0.25);
      }
      if (slot === 5) {
        sfx('charge');
        this.dim = 0.35;
        this.hero.pose = 'cast';
        await this.tween(this.hero, { oy: -24 }, 0.2, easeOut);
        this.fx.ring(hp.x, hp.y - 24, { r0: 30, r1: 4, color: P.gold, width: 2 });
        await this.wait(0.15);
      }
      await this.heroLunge(near);
      this.hero.pose = 'attack';
      return;
    }
    if (job === Job.Mage) {
      this.hero.pose = 'cast';
      const orb = { x: hp.x + 6, y: hp.y - 26 };
      const circle = (color) => this.fx.custom(0.6, (ctx, t) => {
        ctx.globalAlpha = Math.sin(t * Math.PI);
        ctx.strokeStyle = color;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.ellipse(hp.x, L.groundY, 18 + t * 4, 5, 0, 0, Math.PI * 2);
        ctx.stroke();
        for (let i = 0; i < 6; i++) {
          const a = t * 4 + i;
          ctx.fillStyle = color;
          ctx.fillRect(hp.x + Math.cos(a) * 18 - 1, L.groundY + Math.sin(a) * 5 - 1, 2, 2);
        }
        ctx.globalAlpha = 1;
      }, 'back');
      if (slot === 1) {
        circle(P.orange);
        sfx('fire');
        this.fx.burst(orb.x, orb.y, { color: [P.orange, P.yellow], count: 10, speed: 20, glow: true });
        await this.wait(0.2);
        await this.fx.projectile(orb.x, orb.y, ep.x, ep.y, { color: P.orange, size: 6, time: 0.32, arc: 18, trail: P.yellow });
      } else if (slot === 2) {
        circle(P.cyan);
        sfx('ice');
        await this.wait(0.25);
        for (let i = 0; i < 10; i++) {
          const a = (i / 10) * Math.PI * 2;
          this.fx.line(ep.x + Math.cos(a) * 34, ep.y + Math.sin(a) * 24, ep.x + Math.cos(a) * 8, ep.y + Math.sin(a) * 6, { color: '#8ad0f0', width: 2, life: 0.4 });
        }
        await this.wait(0.15);
      } else if (slot === 3) {
        circle(P.green);
        sfx('poison');
        for (let i = 0; i < 18; i++)
          this.fx.particle({ x: ep.x + (Math.random() - 0.5) * 40, y: ep.y + (Math.random() - 0.5) * 30, vx: (Math.random() - 0.5) * 10, vy: -6,
            life: 1.2, size: 6 + Math.random() * 6, color: i % 3 ? '#3e8948' : '#9be36b', shrink: false });
        await this.wait(0.3);
      } else if (slot === 4) {
        circle('#b06aff');
        sfx('arcane');
        this.fx.burst(orb.x, orb.y, { color: ['#b06aff', P.white], count: 16, speed: 30, glow: true, gravity: 0 });
        await this.wait(0.3);
        this.fx.beam(orb.x, orb.y, ep.x, ep.y, { color: '#b06aff', width: 10, life: 0.5 });
        this.fx.shake(4, 0.3);
        await this.wait(0.12);
      } else if (slot === 5) {
        circle(P.orange);
        sfx('charge');
        this.dim = 0.45;
        await this.wait(0.3);
        const meteors = [];
        for (let i = 0; i < 4; i++)
          meteors.push(this.wait(i * 0.1).then(() => this.fx.projectile(ep.x + 60 + i * 10, ep.top - 90, ep.x + (i - 1.5) * 8, ep.y, { color: P.orange, size: 7, time: 0.35, trail: P.red })
            .then(() => {
              sfx('explode');
              this.fx.burst(ep.x + (i - 1.5) * 8, ep.y, { color: [P.orange, P.yellow, P.red], count: 20, speed: 80, glow: true });
              this.fx.shake(4, 0.2);
            })));
        await meteors[0];
      }
      return;
    }
    // Assassin.
    if (slot === 1) {
      sfx('dodge');
      await this.heroLunge(near + 6);
    } else if (slot === 2) {
      sfx('dodge');
      for (let i = 0; i < 4; i++)
        this.fx.custom(0.4, (ctx, t) => {
          ctx.globalAlpha = (1 - t) * 0.4;
          ctx.drawImage(tinted(this.heroSprites.attack, '#68386c'), Math.round(L.heroX + near * (i / 4) - 16), L.groundY - 31);
          ctx.globalAlpha = 1;
        }, 'world');
      this.hero.alpha = 0.3;
      await this.heroLunge(L.enemyX - L.heroX + 26 * this.enemy.scale);
      this.hero.alpha = 1;
    } else if (slot === 3) {
      sfx('dodge');
      await this.heroLunge(near + 4);
    } else if (slot === 4) {
      this.hero.tint = P.crimson;
      this.hero.tintTime = 0.6;
      await this.heroLunge(near);
    } else if (slot === 5) {
      this.dim = 0.6;
      sfx('charge');
      await this.tween(this.hero, { alpha: 0 }, 0.15);
      await this.wait(0.2);
      this.fx.line(ep.x - 60, ep.y + 4, ep.x + 60, ep.y - 4, { color: '#ffffff', width: 3, life: 0.5 });
      this.fx.flash('#ffffff', 0.2);
      await this.wait(0.15);
      this.hero.ox = L.enemyX - L.heroX + 30 * this.enemy.scale;
      this.hero.pose = 'attack';
      await this.tween(this.hero, { alpha: 1 }, 0.1);
    }
  }

  // One landing hit of a skill (before its damage number).
  async skillHit(cast, strike) {
    const ep = this.enemyPos();
    const job = this.job;
    const slot = cast.slot;
    const crit = { Warrior: [0, P.orange, P.gold, P.cyan, P.red, P.yellow], Mage: [], Assassin: [0, '#ffffff', '#b06aff', '#ffffff', P.crimson, '#ffffff'] };
    if (job === Job.Warrior) {
      const color = crit.Warrior[slot];
      sfx(slot === 3 ? 'guard' : 'slash');
      if (slot === 5) {
        for (let i = 0; i < 4; i++) {
          this.fx.slash(ep.x, ep.y, { radius: 16 + i * 2, angle: -2 + i * 1.3, sweep: 2.4, color: i % 2 ? P.yellow : '#ffffff', width: 3, life: 0.2 });
          sfx('slash');
          await this.wait(0.06);
        }
        this.fx.slash(ep.x - 8, ep.y, { radius: 30, angle: -1.9, sweep: 3, color: P.gold, width: 7, life: 0.4 });
        this.fx.shake(6, 0.35);
        this.freeze(0.1);
        await this.tween(this.hero, { oy: 0 }, 0.1);
        this.dim = 0;
      } else {
        this.fx.slash(ep.x - 4, ep.y, { radius: slot === 4 ? 22 : 18, angle: -2.3, sweep: 2.6, color, width: slot === 4 ? 6 : 4 });
        if (slot === 2) this.fx.slash(ep.x - 4, ep.y, { radius: 18, angle: 2.3, sweep: -2.6, color, width: 4 });
        if (slot === 3) this.fx.ring(ep.x - 8, ep.y, { r0: 4, r1: 26, color: P.cyan, width: 3 });
        this.fx.shake(slot === 4 ? 5 : 3, 0.2);
        if (slot === 4) this.freeze(0.07);
      }
      return;
    }
    if (job === Job.Mage) {
      if (slot === 1) {
        sfx('explode');
        this.fx.burst(ep.x, ep.y, { color: [P.orange, P.yellow, P.red], count: 26, speed: 90, glow: true });
        this.fx.ring(ep.x, ep.y, { r0: 4, r1: 30, color: P.orange, width: 3 });
        this.fx.shake(3, 0.2);
      } else if (slot === 2) {
        sfx('ice');
        this.fx.ring(ep.x, ep.y, { r0: 4, r1: 36, color: P.cyan, width: 3 });
        this.fx.burst(ep.x, ep.y, { color: [P.cyan, P.white, '#8ad0f0'], count: 24, speed: 70 });
        this.enemy.tint = '#8ad0f0';
        this.enemy.tintTime = 1.2;
      } else if (slot === 3) {
        sfx('poison');
        this.fx.burst(ep.x, ep.y, { color: [P.green, '#9be36b'], count: 16, speed: 40, gravity: -30 });
        this.enemy.tint = P.green;
        this.enemy.tintTime = 0.6;
      } else if (slot === 4) {
        this.fx.burst(ep.x, ep.y, { color: ['#b06aff', P.white, P.magenta], count: 30, speed: 100, glow: true });
        this.fx.ring(ep.x, ep.y, { r0: 2, r1: 40, color: '#b06aff', width: 4 });
        this.freeze(0.08);
      } else if (slot === 5) {
        this.fx.flash('#ffb060', 0.25);
        this.fx.shake(8, 0.5);
        this.freeze(0.1);
        await this.wait(0.25);
        this.dim = 0;
      }
      return;
    }
    // Assassin.
    const color = crit.Assassin[slot];
    if (slot === 1) {
      sfx('slash');
      this.fx.line(ep.x - 30, ep.y + 2, ep.x + 20, ep.y - 2, { color: '#ffffff', width: 3, life: 0.25 });
    } else if (slot === 3) {
      sfx('slash');
      const a = -2.6 + (strike.hit ?? 0) * 1.7;
      this.fx.slash(ep.x, ep.y, { radius: 14, angle: a, sweep: 2, color: strike.hit ? P.silver : '#ffffff', width: 3, life: 0.18 });
      await this.wait(0.05);
    } else if (slot === 5) {
      sfx('crit');
      this.freeze(0.14);
      this.fx.burst(ep.x, ep.y, { color: ['#ffffff', P.crimson], count: 30, speed: 110 });
      this.dim = 0;
    } else {
      sfx('slash');
      this.fx.slash(ep.x - 2, ep.y, { radius: 16, angle: -2.3, sweep: 2.6, color, width: 4 });
      if (slot === 2) this.fx.burst(ep.x, ep.y, { color: ['#68386c', '#b06aff'], count: 16, speed: 60 });
    }
    this.fx.shake(3, 0.15);
  }
}

