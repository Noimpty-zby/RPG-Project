// Canvas display with integer pixel scaling, the frame loop, input and
// awaitable timers that pause with the game.

export class Display {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.scale = 3;
    this.viewW = 320;
    this.viewH = 180;
    this.mode = 'explore';
    this.resize();
    addEventListener('resize', () => this.resize());
  }
  // Scenes pick how many virtual pixels they want to show.
  setMode(mode) {
    this.mode = mode;
    this.resize();
  }
  resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 3);
    const cssW = window.innerWidth;
    const cssH = window.innerHeight;
    const w = Math.floor(cssW * dpr);
    const h = Math.floor(cssH * dpr);
    this.canvas.width = w;
    this.canvas.height = h;
    this.canvas.style.width = `${cssW}px`;
    this.canvas.style.height = `${cssH}px`;
    const portrait = h > w * 1.1;
    let scale;
    if (this.mode === 'battle') scale = portrait ? Math.floor(w / 210) : Math.floor(Math.min(w / 320, h / 200));
    else if (portrait) scale = Math.floor(w / 220);
    else scale = Math.floor(Math.min(w / 340, h / 200));
    this.scale = Math.max(1, scale);
    this.viewW = Math.ceil(w / this.scale);
    this.viewH = Math.ceil(h / this.scale);
    this.dpr = dpr;
    this.ctx.imageSmoothingEnabled = false;
  }
  // Begin a frame in virtual-pixel coordinates.
  begin() {
    const ctx = this.ctx;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = '#181425';
    ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    ctx.setTransform(this.scale, 0, 0, this.scale, 0, 0);
    return ctx;
  }
  // Convert virtual coordinates to CSS pixels (for DOM overlays).
  toCss(x, y) {
    return { x: (x * this.scale) / this.dpr, y: (y * this.scale) / this.dpr };
  }
}

export class Input {
  constructor() {
    this.down = new Set();
    this.queue = [];
    this.enabled = true;
    addEventListener('keydown', (e) => {
      // A menu already handled this key (e.g. Esc closing a dialog).
      if (e.defaultPrevented) return;
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement || e.target instanceof HTMLSelectElement) return;
      const key = Input.normalize(e.key);
      if (['up', 'down', 'left', 'right', 'action', 'menu'].includes(key) || /^[0-9]$/.test(key)) e.preventDefault();
      if (!this.down.has(key)) this.queue.push(key);
      this.down.add(key);
    });
    addEventListener('keyup', (e) => this.down.delete(Input.normalize(e.key)));
    addEventListener('blur', () => this.down.clear());
  }
  static normalize(key) {
    const map = {
      ArrowUp: 'up', w: 'up', W: 'up', ArrowDown: 'down', s: 'down', S: 'down', ArrowLeft: 'left', a: 'left', A: 'left',
      ArrowRight: 'right', d: 'right', D: 'right', ' ': 'action', Enter: 'action', e: 'action', E: 'action',
      Escape: 'menu', m: 'menu', M: 'menu', Tab: 'tab',
    };
    return map[key] ?? key;
  }
  // Virtual buttons (touch pad) feed the same state.
  press(key) { if (!this.down.has(key)) this.queue.push(key); this.down.add(key); }
  release(key) { this.down.delete(key); }
  tap(key) { this.queue.push(key); }
  held(key) { return this.enabled && this.down.has(key); }
  // Keys pressed since the last call.
  take() {
    const keys = this.queue;
    this.queue = [];
    return this.enabled ? keys : [];
  }
  clear() { this.queue = []; this.down.clear(); }
}

// Frame loop with scene switching. Scenes implement update(dt) and draw(ctx).
export class Loop {
  constructor(display) {
    this.display = display;
    this.scene = null;
    this.time = 0;
    this.timers = [];
    this.last = performance.now();
    this.running = false;
    this.timeScale = 1; // Debug/test speed-up; gameplay never changes it.
  }
  setScene(scene) {
    if (this.scene && this.scene.exit) this.scene.exit();
    this.scene = scene;
    this.timers = [];
    if (scene.enter) scene.enter();
  }
  start() {
    if (this.running) return;
    this.running = true;
    const frame = (now) => {
      const dt = Math.min(0.05, (now - this.last) / 1000) * this.timeScale;
      this.last = now;
      this.time += dt;
      this.step(dt);
      requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
  }
  step(dt) {
    const due = [];
    this.timers = this.timers.filter((t) => {
      t.left -= dt;
      if (t.left <= 0) { due.push(t); return false; }
      return true;
    });
    for (const t of due) t.resolve();
    if (this.scene) {
      this.scene.update?.(dt);
      const ctx = this.display.begin();
      this.scene.draw?.(ctx);
    }
  }
  wait(seconds) {
    return new Promise((resolve) => this.timers.push({ left: seconds, resolve }));
  }
}

export const easeOut = (t) => 1 - (1 - t) * (1 - t);
export const easeIn = (t) => t * t;
export const easeInOut = (t) => (t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2);
export const lerp = (a, b, t) => a + (b - a) * t;
