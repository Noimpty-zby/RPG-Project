// Particles, slashes, rings, projectiles, floating numbers and screen shake.
// Coordinates are virtual pixels in whatever space the owning scene draws.
import { Grid, makeCanvas } from './pixel.js';
import { easeOut, lerp } from './core.js';

const Digits = {
  '0': ['01110', '10001', '10011', '10101', '11001', '10001', '01110'],
  '1': ['00100', '01100', '00100', '00100', '00100', '00100', '01110'],
  '2': ['01110', '10001', '00001', '00110', '01000', '10000', '11111'],
  '3': ['11110', '00001', '00001', '01110', '00001', '00001', '11110'],
  '4': ['00010', '00110', '01010', '10010', '11111', '00010', '00010'],
  '5': ['11111', '10000', '11110', '00001', '00001', '10001', '01110'],
  '6': ['00110', '01000', '10000', '11110', '10001', '10001', '01110'],
  '7': ['11111', '00001', '00010', '00100', '01000', '01000', '01000'],
  '8': ['01110', '10001', '10001', '01110', '10001', '10001', '01110'],
  '9': ['01110', '10001', '10001', '01111', '00001', '00010', '01100'],
  '+': ['00000', '00100', '00100', '11111', '00100', '00100', '00000'],
  '-': ['00000', '00000', '00000', '11111', '00000', '00000', '00000'],
};
const numberCache = new Map();
// A pixel-font number baked with an outline.
export function numberSprite(text, color) {
  const id = `${text}|${color}`;
  let canvas = numberCache.get(id);
  if (canvas) return canvas;
  const chars = [...text].filter((c) => Digits[c]);
  const g = new Grid(chars.length * 6 + 2, 9);
  chars.forEach((c, i) => {
    Digits[c].forEach((row, y) => {
      for (let x = 0; x < 5; x++) if (row[x] === '1') g.px(1 + i * 6 + x, 1 + y, color);
    });
  });
  g.outline('#181425');
  canvas = g.toCanvas();
  numberCache.set(id, canvas);
  return canvas;
}

let glowSprite = null;
function glow() {
  if (glowSprite) return glowSprite;
  glowSprite = makeCanvas(32, 32);
  const ctx = glowSprite.getContext('2d');
  const gradient = ctx.createRadialGradient(16, 16, 0, 16, 16, 16);
  gradient.addColorStop(0, 'rgba(255,255,255,1)');
  gradient.addColorStop(1, 'rgba(255,255,255,0)');
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, 32, 32);
  return glowSprite;
}

export class Effects {
  constructor() {
    this.items = [];
    this.shakeTime = 0;
    this.shakePower = 0;
    this.flashTime = 0;
    this.flashMax = 0;
    this.flashColor = '#ffffff';
  }
  clear() { this.items = []; this.shakeTime = 0; this.flashTime = 0; }
  add(effect) {
    effect.age = 0;
    this.items.push(effect);
    return effect;
  }
  shake(power = 3, time = 0.25) {
    this.shakePower = Math.max(this.shakePower * (this.shakeTime > 0 ? 1 : 0), power);
    this.shakeTime = Math.max(this.shakeTime, time);
  }
  flash(color = '#ffffff', time = 0.15) {
    this.flashColor = color;
    this.flashTime = time;
    this.flashMax = time;
  }
  shakeOffset() {
    if (this.shakeTime <= 0) return { x: 0, y: 0 };
    const p = this.shakePower * Math.min(1, this.shakeTime * 6);
    return { x: Math.round((Math.random() * 2 - 1) * p), y: Math.round((Math.random() * 2 - 1) * p) };
  }
  update(dt) {
    this.shakeTime = Math.max(0, this.shakeTime - dt);
    this.flashTime = Math.max(0, this.flashTime - dt);
    for (const e of this.items) {
      e.age += dt;
      if (e.update) e.update(e, dt);
    }
    this.items = this.items.filter((e) => e.age < e.life);
  }
  draw(ctx, layer = 'world') {
    for (const e of this.items) if ((e.layer || 'world') === layer) e.draw(ctx, e);
  }
  drawFlash(ctx, w, h) {
    if (this.flashTime <= 0) return;
    ctx.globalAlpha = (this.flashTime / this.flashMax) * 0.6;
    ctx.fillStyle = this.flashColor;
    ctx.fillRect(0, 0, w, h);
    ctx.globalAlpha = 1;
  }

  // ---- Factories -------------------------------------------------------------
  particle(o) {
    const p = {
      x: o.x, y: o.y, vx: o.vx ?? 0, vy: o.vy ?? 0, ax: o.ax ?? 0, ay: o.ay ?? 0, drag: o.drag ?? 0,
      life: o.life ?? 0.6, size: o.size ?? 2, color: o.color ?? '#ffffff', glow: o.glow ?? false, shrink: o.shrink ?? true,
      layer: o.layer, square: o.square ?? true,
      update(e, dt) {
        e.vx += e.ax * dt;
        e.vy += e.ay * dt;
        if (e.drag) { e.vx *= 1 - e.drag * dt; e.vy *= 1 - e.drag * dt; }
        e.x += e.vx * dt;
        e.y += e.vy * dt;
      },
      draw(ctx, e) {
        const t = e.age / e.life;
        const size = e.shrink ? Math.max(1, e.size * (1 - t * 0.8)) : e.size;
        if (e.glow) {
          ctx.globalAlpha = (1 - t) * 0.5;
          ctx.globalCompositeOperation = 'lighter';
          ctx.filter = 'none';
          const s = size * 4;
          ctx.drawImage(tintGlow(e.color), e.x - s / 2, e.y - s / 2, s, s);
          ctx.globalCompositeOperation = 'source-over';
        }
        ctx.globalAlpha = 1 - t * t;
        ctx.fillStyle = e.color;
        ctx.fillRect(Math.round(e.x - size / 2), Math.round(e.y - size / 2), Math.round(size), Math.round(size));
        ctx.globalAlpha = 1;
      },
    };
    return this.add(p);
  }
  burst(x, y, o = {}) {
    const count = o.count ?? 12;
    for (let i = 0; i < count; i++) {
      const angle = (o.angle ?? 0) + (Math.random() - 0.5) * (o.spread ?? Math.PI * 2);
      const speed = (o.speed ?? 60) * (0.4 + Math.random() * 0.8);
      const colors = Array.isArray(o.color) ? o.color : [o.color ?? '#ffffff'];
      this.particle({ x: x + (Math.random() - 0.5) * (o.jitter ?? 2), y: y + (Math.random() - 0.5) * (o.jitter ?? 2),
        vx: Math.cos(angle) * speed, vy: Math.sin(angle) * speed, ay: o.gravity ?? 0, drag: o.drag ?? 2,
        life: (o.life ?? 0.5) * (0.6 + Math.random() * 0.6), size: o.size ?? 2, color: colors[i % colors.length],
        glow: o.glow ?? false, layer: o.layer });
    }
  }
  ring(x, y, o = {}) {
    return this.add({
      x, y, life: o.life ?? 0.4, r0: o.r0 ?? 2, r1: o.r1 ?? 24, color: o.color ?? '#ffffff', width: o.width ?? 2, layer: o.layer,
      squash: o.squash ?? 1,
      draw(ctx, e) {
        const t = easeOut(e.age / e.life);
        ctx.globalAlpha = 1 - t;
        ctx.strokeStyle = e.color;
        ctx.lineWidth = Math.max(1, e.width * (1 - t));
        ctx.beginPath();
        ctx.ellipse(e.x, e.y, lerp(e.r0, e.r1, t), lerp(e.r0, e.r1, t) * e.squash, 0, 0, Math.PI * 2);
        ctx.stroke();
        ctx.globalAlpha = 1;
      },
    });
  }
  slash(x, y, o = {}) {
    return this.add({
      x, y, life: o.life ?? 0.25, radius: o.radius ?? 18, angle: o.angle ?? -0.6, sweep: o.sweep ?? 2.2,
      color: o.color ?? '#ffffff', width: o.width ?? 4, dir: o.dir ?? 1, layer: o.layer, edge: o.edge ?? '#ffffff',
      draw(ctx, e) {
        const t = e.age / e.life;
        const head = easeOut(Math.min(1, t * 1.6));
        const tail = easeOut(Math.max(0, t * 1.6 - 0.6));
        const a0 = e.angle + e.sweep * tail * e.dir;
        const a1 = e.angle + e.sweep * head * e.dir;
        ctx.globalAlpha = 1 - Math.max(0, t - 0.6) / 0.4;
        ctx.lineCap = 'round';
        ctx.strokeStyle = e.color;
        ctx.lineWidth = e.width;
        ctx.beginPath();
        ctx.arc(e.x, e.y, e.radius, Math.min(a0, a1), Math.max(a0, a1));
        ctx.stroke();
        ctx.strokeStyle = e.edge;
        ctx.lineWidth = Math.max(1, e.width / 3);
        ctx.beginPath();
        ctx.arc(e.x, e.y, e.radius + e.width / 3, Math.min(a0, a1), Math.max(a0, a1));
        ctx.stroke();
        ctx.globalAlpha = 1;
      },
    });
  }
  line(x0, y0, x1, y1, o = {}) {
    return this.add({
      life: o.life ?? 0.3, color: o.color ?? '#ffffff', width: o.width ?? 2, layer: o.layer,
      draw(ctx, e) {
        const t = e.age / e.life;
        const grow = Math.min(1, t * 4);
        ctx.globalAlpha = 1 - t;
        ctx.strokeStyle = e.color;
        ctx.lineWidth = e.width * (1 - t * 0.5);
        ctx.beginPath();
        ctx.moveTo(x0, y0);
        ctx.lineTo(lerp(x0, x1, grow), lerp(y0, y1, grow));
        ctx.stroke();
        ctx.globalAlpha = 1;
      },
    });
  }
  beam(x0, y0, x1, y1, o = {}) {
    return this.add({
      life: o.life ?? 0.45, color: o.color ?? '#b06aff', core: o.core ?? '#ffffff', width: o.width ?? 8, layer: o.layer,
      draw(ctx, e) {
        const t = e.age / e.life;
        const w = e.width * Math.sin(Math.min(1, t * 1.3) * Math.PI);
        ctx.globalAlpha = 0.9;
        ctx.strokeStyle = e.color;
        ctx.lineWidth = Math.max(1, w);
        ctx.beginPath();
        ctx.moveTo(x0, y0);
        ctx.lineTo(x1, y1);
        ctx.stroke();
        ctx.strokeStyle = e.core;
        ctx.lineWidth = Math.max(1, w / 3);
        ctx.stroke();
        ctx.globalAlpha = 1;
      },
    });
  }
  // A projectile that resolves when it lands. trail: particle color.
  projectile(x0, y0, x1, y1, o = {}) {
    return new Promise((resolve) => {
      this.add({
        life: o.time ?? 0.35, arc: o.arc ?? 0, size: o.size ?? 4, color: o.color ?? '#feae34', core: o.core ?? '#ffffff',
        trail: o.trail ?? o.color, sprite: o.sprite ?? null, layer: o.layer, fx: this, done: false,
        update(e) {
          const t = Math.min(1, e.age / e.life);
          e.x = lerp(x0, x1, t);
          e.y = lerp(y0, y1, t) - Math.sin(t * Math.PI) * e.arc;
          if (e.trail && Math.random() < 0.9)
            e.fx.particle({ x: e.x, y: e.y, vx: (Math.random() - 0.5) * 20, vy: (Math.random() - 0.5) * 20, life: 0.3,
              size: e.size * 0.6, color: e.trail, layer: e.layer });
          if (t >= 1 && !e.done) { e.done = true; resolve(); }
        },
        draw(ctx, e) {
          if (e.x === undefined) return;
          if (e.sprite) {
            ctx.drawImage(e.sprite, Math.round(e.x - e.sprite.width / 2), Math.round(e.y - e.sprite.height / 2));
            return;
          }
          ctx.globalCompositeOperation = 'lighter';
          const s = e.size * 4;
          ctx.drawImage(tintGlow(e.color), e.x - s / 2, e.y - s / 2, s, s);
          ctx.globalCompositeOperation = 'source-over';
          ctx.fillStyle = e.color;
          ctx.fillRect(Math.round(e.x - e.size / 2), Math.round(e.y - e.size / 2), e.size, e.size);
          ctx.fillStyle = e.core;
          ctx.fillRect(Math.round(e.x - 1), Math.round(e.y - 1), 2, 2);
        },
      });
    });
  }
  // Floating pixel-font number.
  number(x, y, value, color, o = {}) {
    const sprite = numberSprite(String(value), color);
    const scale = o.big ? 2 : 1;
    return this.add({
      x, y, life: o.life ?? 0.9, vx: (Math.random() - 0.5) * 16, layer: o.layer ?? 'top',
      draw(ctx, e) {
        const t = e.age / e.life;
        const pop = t < 0.12 ? 1 + (0.12 - t) * 5 : 1;
        const rise = easeOut(Math.min(1, t * 2)) * 14;
        ctx.globalAlpha = t > 0.7 ? 1 - (t - 0.7) / 0.3 : 1;
        const w = sprite.width * scale * pop;
        const h = sprite.height * scale * pop;
        ctx.drawImage(sprite, Math.round(e.x + e.vx * t - w / 2), Math.round(e.y - rise - h / 2), Math.round(w), Math.round(h));
        ctx.globalAlpha = 1;
      },
    });
  }
  // Floating text label drawn with the UI font (crisp at any scale).
  label(x, y, text, color = '#ffffff', o = {}) {
    return this.add({
      x, y, life: o.life ?? 1.0, size: o.size ?? 8, layer: o.layer ?? 'top',
      draw(ctx, e) {
        const t = e.age / e.life;
        const rise = easeOut(Math.min(1, t * 2)) * (o.rise ?? 10);
        ctx.globalAlpha = t > 0.75 ? 1 - (t - 0.75) / 0.25 : 1;
        ctx.font = `bold ${e.size}px "Zpix", "PingFang SC", "Microsoft YaHei", "Noto Sans SC", sans-serif`;
        ctx.textAlign = 'center';
        ctx.lineWidth = 2.5;
        ctx.strokeStyle = '#181425';
        ctx.strokeText(text, e.x, e.y - rise);
        ctx.fillStyle = color;
        ctx.fillText(text, e.x, e.y - rise);
        ctx.globalAlpha = 1;
      },
    });
  }
  // Generic timed draw callback: draw(ctx, t) with t in 0..1.
  custom(life, draw, layer) {
    return this.add({ life, layer, draw(ctx, e) { draw(ctx, e.age / e.life, e); } });
  }
}

const glowCache = new Map();
function tintGlow(color) {
  let canvas = glowCache.get(color);
  if (canvas) return canvas;
  canvas = makeCanvas(32, 32);
  const ctx = canvas.getContext('2d');
  ctx.drawImage(glow(), 0, 0);
  ctx.globalCompositeOperation = 'source-in';
  ctx.fillStyle = color;
  ctx.fillRect(0, 0, 32, 32);
  glowCache.set(color, canvas);
  return canvas;
}
export { tintGlow };
