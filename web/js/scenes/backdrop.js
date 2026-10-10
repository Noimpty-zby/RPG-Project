// Battle backdrops: a sky, a silhouette layer and a ground strip per chapter.
import { makeCanvas, darken, lighten, mix } from '../engine/pixel.js';
import { ChapterThemes, P } from '../engine/palette.js';
import { hash2, TILE } from '../engine/tiles.js';
import * as Sprites from '../engine/sprites.js';

const rand = (i, s = 0) => hash2(i, s, 97);

export function drawBackdrop(theme, w, h, groundY) {
  const canvas = makeCanvas(w, h);
  const ctx = canvas.getContext('2d');
  const t = ChapterThemes[theme];
  const sky = ctx.createLinearGradient(0, 0, 0, groundY);
  sky.addColorStop(0, t.sky[0]);
  sky.addColorStop(1, t.sky[1]);
  ctx.fillStyle = sky;
  ctx.fillRect(0, 0, w, groundY);
  const far = t.far;
  const mid = t.mid;
  const fill = (color, points) => {
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(points[0][0], points[0][1]);
    for (const [x, y] of points.slice(1)) ctx.lineTo(x, y);
    ctx.closePath();
    ctx.fill();
  };
  const ridge = (color, base, amp, step, seed) => {
    const pts = [[0, groundY]];
    for (let x = 0; x <= w + step; x += step) pts.push([x, base - rand(x, seed) * amp]);
    pts.push([w, groundY]);
    fill(color, pts);
  };
  const stars = (count, color = '#ffffff') => {
    for (let i = 0; i < count; i++) {
      ctx.fillStyle = rand(i, 3) < 0.2 ? P.yellow : color;
      ctx.fillRect(Math.floor(rand(i, 1) * w), Math.floor(rand(i, 2) * groundY * 0.8), 1, 1);
    }
  };
  switch (t.key) {
    case 'wild':
      ctx.fillStyle = '#fff6d0';
      ctx.beginPath(); ctx.arc(w * 0.78, groundY * 0.25, 12, 0, Math.PI * 2); ctx.fill();
      for (let i = 0; i < 5; i++) {
        ctx.fillStyle = 'rgba(255,255,255,0.75)';
        const cx = rand(i, 4) * w;
        const cy = 18 + rand(i, 5) * 30;
        ctx.fillRect(cx, cy, 26, 5); ctx.fillRect(cx + 5, cy - 4, 14, 5);
      }
      ridge(far, groundY - 40, 30, 24, 1);
      ridge(mid, groundY - 14, 14, 10, 2);
      for (let x = 4; x < w; x += 18 + rand(x, 6) * 20) {
        const ty = groundY - 16 - rand(x, 7) * 8;
        ctx.fillStyle = '#2f5a2a'; ctx.fillRect(x + 4, ty + 10, 3, 8);
        ctx.fillStyle = '#3e7a34'; ctx.beginPath(); ctx.arc(x + 5, ty + 6, 8, 0, Math.PI * 2); ctx.fill();
      }
      break;
    case 'forest':
      for (let layer = 0; layer < 2; layer++) {
        const color = layer ? mid : far;
        for (let x = -10; x < w + 10; x += 14 + rand(x, layer) * 10) {
          const top = 10 + rand(x, layer + 9) * 40 + layer * 20;
          ctx.fillStyle = darken(color, 0.2);
          ctx.fillRect(x + 6, top, 5 + layer * 2, groundY - top);
          ctx.fillStyle = color;
          ctx.beginPath(); ctx.arc(x + 8, top, 14 + layer * 4, 0, Math.PI * 2); ctx.fill();
        }
      }
      for (let i = 0; i < 20; i++) {
        ctx.fillStyle = 'rgba(216,255,107,0.8)';
        ctx.fillRect(rand(i, 11) * w, 20 + rand(i, 12) * (groundY - 30), 1, 1);
      }
      break;
    case 'library': {
      for (let x = 0; x < w; x += 46) {
        ctx.fillStyle = far;
        ctx.fillRect(x, 10, 36, groundY - 10);
        for (let row = 0; row < 6; row++) {
          ctx.fillStyle = darken(far, 0.35);
          ctx.fillRect(x + 2, 14 + row * 18, 32, 14);
          for (let b = 0; b < 8; b++) {
            ctx.fillStyle = [P.wine, P.navy, P.forest, P.gold, P.violet][(b + row + x) % 5];
            ctx.globalAlpha = 0.55;
            ctx.fillRect(x + 3 + b * 4, 16 + row * 18 + (b % 3), 3, 12 - (b % 3));
            ctx.globalAlpha = 1;
          }
        }
        ctx.fillStyle = mid;
        ctx.fillRect(x + 36, 0, 10, groundY);
      }
      const water = ctx.createLinearGradient(0, groundY - 30, 0, groundY);
      water.addColorStop(0, 'rgba(44,232,245,0)');
      water.addColorStop(1, 'rgba(44,232,245,0.25)');
      ctx.fillStyle = water;
      ctx.fillRect(0, groundY - 30, w, 30);
      break;
    }
    case 'mine':
      ridge(far, groundY * 0.35, 20, 12, 3);
      fill(t.sky[0], [[0, 0], [w, 0], [w, 18], [0, 24]]);
      for (let x = 10; x < w; x += 70) {
        ctx.fillStyle = P.brown; ctx.fillRect(x, 20, 6, groundY - 20); ctx.fillRect(x + 50, 20, 6, groundY - 20); ctx.fillRect(x - 4, 18, 64, 6);
        ctx.fillStyle = '#fee761'; ctx.fillRect(x + 26, 26, 4, 5);
        const glow = ctx.createRadialGradient(x + 28, 28, 0, x + 28, 28, 30);
        glow.addColorStop(0, 'rgba(254,231,97,0.35)'); glow.addColorStop(1, 'rgba(254,231,97,0)');
        ctx.fillStyle = glow; ctx.fillRect(x - 2, 0, 60, 60);
      }
      ridge(mid, groundY - 6, 12, 8, 4);
      for (let i = 0; i < 12; i++) {
        ctx.fillStyle = P.orange;
        ctx.fillRect(rand(i, 13) * w, groundY - 10 - rand(i, 14) * 40, 2, 2);
      }
      break;
    case 'frost':
      ridge(far, groundY - 50, 40, 20, 5);
      ridge(lighten(far, 0.5), groundY - 50 + 4, 2, 20, 5);
      ridge(mid, groundY - 18, 16, 12, 6);
      for (let x = 0; x < w; x += 22) {
        const ty = groundY - 26 - rand(x, 8) * 10;
        fill('#2a5a4a', [[x, ty + 24], [x + 7, ty], [x + 14, ty + 24]]);
        fill(P.white, [[x + 4, ty + 10], [x + 7, ty], [x + 10, ty + 10]]);
      }
      for (let i = 0; i < 60; i++) {
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(rand(i, 15) * w, rand(i, 16) * groundY, 1, 1);
      }
      break;
    case 'forge': {
      for (let x = 0; x < w; x += 40) {
        ctx.fillStyle = far; ctx.fillRect(x + 8, 0, 14, groundY);
        ctx.fillStyle = darken(far, 0.3); ctx.fillRect(x + 8, 0, 3, groundY);
        ctx.fillStyle = P.slate; ctx.fillRect(x, 30 + (x % 3) * 10, 40, 4);
      }
      const lava = ctx.createLinearGradient(0, groundY - 50, 0, groundY);
      lava.addColorStop(0, 'rgba(255,122,26,0)');
      lava.addColorStop(1, 'rgba(255,122,26,0.55)');
      ctx.fillStyle = lava;
      ctx.fillRect(0, groundY - 50, w, 50);
      ctx.fillStyle = mid;
      ridge(mid, groundY - 8, 8, 16, 7);
      break;
    }
    case 'theater':
      for (let x = 0; x < w; x += 8) {
        ctx.fillStyle = (x / 8) % 2 ? '#6a1a3a' : '#7a2244';
        ctx.fillRect(x, 0, 8, groundY);
      }
      ctx.fillStyle = P.gold; ctx.fillRect(0, 8, w, 3);
      for (let x = 0; x < w; x += 16) { ctx.fillStyle = '#7a2244'; ctx.beginPath(); ctx.arc(x + 8, 11, 8, 0, Math.PI); ctx.fill(); }
      for (const cx of [w * 0.3, w * 0.7]) {
        const beam = ctx.createLinearGradient(cx, 0, cx, groundY);
        beam.addColorStop(0, 'rgba(255,240,200,0.35)'); beam.addColorStop(1, 'rgba(255,240,200,0.05)');
        ctx.fillStyle = beam;
        fill(beam, [[cx - 6, 0], [cx + 6, 0], [cx + 40, groundY], [cx - 40, groundY]]);
      }
      break;
    case 'observatory':
      stars(140);
      ctx.strokeStyle = 'rgba(254,231,97,0.25)';
      ctx.lineWidth = 1;
      ctx.beginPath(); ctx.arc(w / 2, groundY + 40, groundY, Math.PI, Math.PI * 2); ctx.stroke();
      ctx.beginPath(); ctx.arc(w / 2, groundY + 40, groundY * 0.7, Math.PI, Math.PI * 2); ctx.stroke();
      ctx.fillStyle = '#d0d0f0';
      ctx.beginPath(); ctx.arc(w * 0.2, 30, 10, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = t.sky[0];
      ctx.beginPath(); ctx.arc(w * 0.2 + 4, 27, 9, 0, Math.PI * 2); ctx.fill();
      ridge(far, groundY - 8, 6, 30, 9);
      break;
    case 'ruins':
      ctx.fillStyle = '#ffd8a0';
      ctx.beginPath(); ctx.arc(w * 0.5, groundY - 10, 26, 0, Math.PI * 2); ctx.fill();
      ridge(far, groundY - 22, 12, 20, 10);
      for (let x = 6; x < w; x += 34 + rand(x, 17) * 20) {
        const hgt = 30 + rand(x, 18) * 50;
        ctx.fillStyle = mid; ctx.fillRect(x, groundY - hgt, 10, hgt);
        ctx.fillStyle = lighten(mid, 0.1); ctx.fillRect(x - 2, groundY - hgt, 14, 4);
      }
      break;
    case 'zenith':
      stars(80, P.cyan);
      for (let i = 0; i < 6; i++) {
        const x = rand(i, 19) * w;
        const y = 20 + rand(i, 20) * (groundY - 60);
        ctx.fillStyle = far; ctx.fillRect(x, y, 30, 6);
        ctx.fillStyle = P.cyan; ctx.fillRect(x + 4, y + 6, 22, 1);
      }
      ctx.strokeStyle = 'rgba(44,232,245,0.35)';
      for (let i = 0; i < 6; i++) {
        ctx.beginPath(); ctx.moveTo(0, groundY - 10 - i * 14); ctx.lineTo(w, groundY - 30 - i * 10); ctx.stroke();
      }
      break;
    default:
      break;
  }
  // Ground made of the chapter's floor tiles, darkening with depth.
  const set = Sprites.tiles(theme);
  for (let y = groundY; y < h; y += TILE)
    for (let x = 0; x < w; x += TILE) ctx.drawImage(set.floor[(Math.floor(x / TILE) + Math.floor(y / TILE)) % 3], x, y);
  const shade = ctx.createLinearGradient(0, groundY, 0, h);
  shade.addColorStop(0, 'rgba(0,0,0,0)');
  shade.addColorStop(1, 'rgba(0,0,0,0.55)');
  ctx.fillStyle = shade;
  ctx.fillRect(0, groundY, w, h - groundY);
  ctx.fillStyle = mix(t.ground, '#000000', 0.45);
  ctx.fillRect(0, groundY, w, 2);
  ctx.fillStyle = 'rgba(255,255,255,0.08)';
  ctx.fillRect(0, groundY + 2, w, 1);
  return canvas;
}
