// Tiny pixel-art toolkit: draw shapes on a color grid, add outlines, and
// bake the result into a canvas. All sprites in the game are built with it.

export class Grid {
  constructor(w, h) {
    this.w = w;
    this.h = h;
    this.data = new Array(w * h).fill(null);
  }
  inside(x, y) { return x >= 0 && y >= 0 && x < this.w && y < this.h; }
  get(x, y) { return this.inside(x, y) ? this.data[y * this.w + x] : null; }
  px(x, y, c) {
    x = Math.round(x);
    y = Math.round(y);
    if (c !== undefined && this.inside(x, y)) this.data[y * this.w + x] = c;
    return this;
  }
  rect(x, y, w, h, c) {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) this.px(x + i, y + j, c);
    return this;
  }
  // Filled ellipse inside the box [x, x+w) x [y, y+h).
  oval(x, y, w, h, c) {
    const cx = x + w / 2 - 0.5;
    const cy = y + h / 2 - 0.5;
    const rx = w / 2;
    const ry = h / 2;
    for (let j = 0; j < h; j++)
      for (let i = 0; i < w; i++) {
        const dx = (x + i - cx) / rx;
        const dy = (y + j - cy) / ry;
        if (dx * dx + dy * dy <= 1.0) this.px(x + i, y + j, c);
      }
    return this;
  }
  line(x0, y0, x1, y1, c) {
    x0 = Math.round(x0); y0 = Math.round(y0); x1 = Math.round(x1); y1 = Math.round(y1);
    const dx = Math.abs(x1 - x0);
    const dy = -Math.abs(y1 - y0);
    const sx = x0 < x1 ? 1 : -1;
    const sy = y0 < y1 ? 1 : -1;
    let err = dx + dy;
    for (;;) {
      this.px(x0, y0, c);
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return this;
  }
  thick(x0, y0, x1, y1, c, width = 2) {
    for (let i = 0; i < width; i++) for (let j = 0; j < width; j++) this.line(x0 + i, y0 + j, x1 + i, y1 + j, c);
    return this;
  }
  poly(points, c) {
    let minY = Infinity;
    let maxY = -Infinity;
    for (const [, y] of points) { minY = Math.min(minY, y); maxY = Math.max(maxY, y); }
    for (let y = Math.floor(minY); y <= Math.ceil(maxY); y++) {
      const xs = [];
      for (let i = 0; i < points.length; i++) {
        const [x0, y0] = points[i];
        const [x1, y1] = points[(i + 1) % points.length];
        if ((y0 <= y + 0.5 && y1 > y + 0.5) || (y1 <= y + 0.5 && y0 > y + 0.5))
          xs.push(x0 + ((y + 0.5 - y0) / (y1 - y0)) * (x1 - x0));
      }
      xs.sort((a, b) => a - b);
      for (let k = 0; k + 1 < xs.length; k += 2)
        for (let x = Math.round(xs[k]); x < Math.round(xs[k + 1]); x++) this.px(x, y, c);
    }
    return this;
  }
  // Paint string rows: each char maps through the palette; '.' and ' ' skip.
  stamp(x, y, rows, palette) {
    rows.forEach((row, j) => {
      for (let i = 0; i < row.length; i++) {
        const ch = row[i];
        if (ch !== '.' && ch !== ' ' && palette[ch]) this.px(x + i, y + j, palette[ch]);
      }
    });
    return this;
  }
  // Recolor existing pixels of one color within a region.
  replace(from, to, x = 0, y = 0, w = this.w, h = this.h) {
    for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (this.get(i, j) === from) this.px(i, j, to);
    return this;
  }
  // Darken pixels on the side facing away from the light (bottom/right).
  shadeEdges(dark, side = 'right') {
    const copy = this.data.slice();
    for (let y = 0; y < this.h; y++)
      for (let x = 0; x < this.w; x++) {
        if (!copy[y * this.w + x]) continue;
        const nx = side === 'right' ? x + 1 : x - 1;
        const below = copy[(y + 1) * this.w + x];
        const beside = nx >= 0 && nx < this.w ? copy[y * this.w + nx] : null;
        if (y + 1 >= this.h || !below || !beside) {
          if (dark[copy[y * this.w + x]]) this.data[y * this.w + x] = dark[copy[y * this.w + x]];
        }
      }
    return this;
  }
  // 1px outline around all opaque pixels (outside the shape).
  outline(color = '#1a1220') {
    const copy = this.data.slice();
    const at = (x, y) => (x >= 0 && y >= 0 && x < this.w && y < this.h ? copy[y * this.w + x] : null);
    for (let y = 0; y < this.h; y++)
      for (let x = 0; x < this.w; x++) {
        if (at(x, y)) continue;
        if (at(x - 1, y) || at(x + 1, y) || at(x, y - 1) || at(x, y + 1)) this.data[y * this.w + x] = color;
      }
    return this;
  }
  mirrored() {
    const g = new Grid(this.w, this.h);
    for (let y = 0; y < this.h; y++) for (let x = 0; x < this.w; x++) g.data[y * this.w + x] = this.get(this.w - 1 - x, y);
    return g;
  }
  clone() {
    const g = new Grid(this.w, this.h);
    g.data = this.data.slice();
    return g;
  }
  toCanvas() {
    const canvas = makeCanvas(this.w, this.h);
    const ctx = canvas.getContext('2d');
    const image = ctx.createImageData(this.w, this.h);
    for (let i = 0; i < this.data.length; i++) {
      const c = this.data[i];
      if (!c) continue;
      const [r, g, b, a] = parseColor(c);
      image.data[i * 4] = r;
      image.data[i * 4 + 1] = g;
      image.data[i * 4 + 2] = b;
      image.data[i * 4 + 3] = a;
    }
    ctx.putImageData(image, 0, 0);
    return canvas;
  }
}

const colorCache = new Map();
export function parseColor(c) {
  let rgba = colorCache.get(c);
  if (rgba) return rgba;
  let hex = c.replace('#', '');
  if (hex.length === 3) hex = hex.split('').map((h) => h + h).join('');
  const r = parseInt(hex.slice(0, 2), 16);
  const g = parseInt(hex.slice(2, 4), 16);
  const b = parseInt(hex.slice(4, 6), 16);
  const a = hex.length === 8 ? parseInt(hex.slice(6, 8), 16) : 255;
  rgba = [r, g, b, a];
  colorCache.set(c, rgba);
  return rgba;
}

export function makeCanvas(w, h) {
  if (typeof OffscreenCanvas !== 'undefined' && typeof document === 'undefined') return new OffscreenCanvas(w, h);
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  return canvas;
}

// Mix two hex colors; t=0 gives a, t=1 gives b.
export function mix(a, b, t) {
  const [r1, g1, b1] = parseColor(a);
  const [r2, g2, b2] = parseColor(b);
  const h = (v) => Math.round(v).toString(16).padStart(2, '0');
  return `#${h(r1 + (r2 - r1) * t)}${h(g1 + (g2 - g1) * t)}${h(b1 + (b2 - b1) * t)}`;
}
export const lighten = (c, t = 0.25) => mix(c, '#ffffff', t);
export const darken = (c, t = 0.3) => mix(c, '#000000', t);

// A solid-color silhouette of a canvas, used for hit flashes and shadows.
const tintCache = new WeakMap();
export function tinted(canvas, color) {
  let byColor = tintCache.get(canvas);
  if (!byColor) tintCache.set(canvas, (byColor = new Map()));
  let result = byColor.get(color);
  if (result) return result;
  result = makeCanvas(canvas.width, canvas.height);
  const ctx = result.getContext('2d');
  ctx.drawImage(canvas, 0, 0);
  ctx.globalCompositeOperation = 'source-in';
  ctx.fillStyle = color;
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  byColor.set(color, result);
  return result;
}

export function flipped(canvas) {
  const result = makeCanvas(canvas.width, canvas.height);
  const ctx = result.getContext('2d');
  ctx.translate(canvas.width, 0);
  ctx.scale(-1, 1);
  ctx.drawImage(canvas, 0, 0);
  return result;
}
