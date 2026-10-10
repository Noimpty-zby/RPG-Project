// Sprite registry: builds every canvas once and caches it.
import { Grid, darken } from './pixel.js';
import { P, TierColors } from './palette.js';
import { HeroLooks, NpcLooks, humanoidFrames, walker } from './humanoid.js';
import { monsterGrid } from './monsters.js';
import {
  campfireFrames, chestFrames, deviceSprite, floorTile, signSprite, stairsSprite, stallSprite, themeProps,
  wallFace, wallTop, waystoneFrames,
} from './tiles.js';

const cache = new Map();
function memo(key, build) {
  let value = cache.get(key);
  if (value === undefined) {
    value = build();
    cache.set(key, value);
  }
  return value;
}

export const heroBattle = (job) => memo(`hero-battle-${job}`, () => {
  const frames = humanoidFrames(HeroLooks[job]);
  return { idle: frames.idle.toCanvas(), attack: frames.attack.toCanvas(), cast: frames.cast.toCanvas() };
});

// Walk frames: { down: [3], up: [3], right: [3], left: [3] }.
function walkerSet(look) {
  const set = {};
  for (const dir of ['down', 'up', 'side']) {
    const frames = [0, 1, 2].map((step) => walker(look, dir, step));
    if (dir === 'side') {
      set.right = frames.map((g) => g.toCanvas());
      set.left = frames.map((g) => g.mirrored().toCanvas());
    } else set[dir] = frames.map((g) => g.toCanvas());
  }
  return set;
}
export const heroWalker = (job) => memo(`hero-walk-${job}`, () => walkerSet(HeroLooks[job]));
export const merchantWalker = () => memo('merchant', () => walkerSet(NpcLooks.merchant));
export const allyWalker = (chapter) => memo(`ally-${chapter}`, () => walkerSet(NpcLooks.allies[chapter]));

export const monster = (entry) => memo(`monster-${entry}`, () => ({
  idle: monsterGrid(entry, 'idle').toCanvas(),
  attack: monsterGrid(entry, 'attack').toCanvas(),
}));

export const tiles = (theme) => memo(`tiles-${theme}`, () => ({
  floor: [0, 1, 2, 3].map((v) => floorTile(theme, v)),
  face: [0, 1, 2].map((v) => wallFace(theme, v)),
  top: [0, 1, 2].map((v) => wallTop(theme, v)),
  props: themeProps(theme),
  stairs: stairsSprite(theme, false),
  sealed: stairsSprite(theme, true),
  device: deviceSprite(theme, false),
  deviceBroken: deviceSprite(theme, true),
}));
export const objects = () => memo('objects', () => ({
  campfire: campfireFrames(),
  waystone: waystoneFrames(),
  chest: chestFrames(),
  stall: stallSprite(),
  sign: signSprite(),
}));

// ---- Item icons (16x16) -------------------------------------------------------
function iconGrid(kind, tier) {
  const g = new Grid(16, 16);
  const metal = [P.steel, P.silver, '#9ad0ff', '#e0a0ff'][tier] ?? P.steel;
  const accent = [P.brown, P.green, P.blue, P.magenta][tier] ?? P.brown;
  const gold = tier >= 2 ? P.gold : P.tan;
  switch (kind) {
    case 'sword': g.line(3, 12, 12, 3, metal).line(4, 12, 13, 3, darken(metal, 0.3)).line(2, 10, 6, 14, gold).line(1, 14, 3, 12, P.brown); break;
    case 'hammer': g.line(3, 13, 9, 7, P.brown).rect(7, 2, 7, 6, metal).rect(7, 6, 7, 2, darken(metal, 0.3)); break;
    case 'axe': g.line(3, 13, 10, 4, P.brown).poly([[8, 3], [14, 1], [14, 9], [10, 7]], metal); break;
    case 'spear': g.line(2, 14, 11, 5, P.brown).poly([[10, 3], [14, 1], [12, 6]], metal); break;
    case 'staff': g.line(3, 14, 10, 5, P.brown).oval(9, 1, 6, 6, accent).px(11, 2, P.white); break;
    case 'book': g.rect(3, 2, 10, 12, accent).rect(5, 3, 7, 10, P.sand).rect(3, 2, 2, 12, darken(accent, 0.3)).px(8, 7, gold); break;
    case 'beads': for (let a = 0; a < 10; a++) g.oval(7 + Math.round(Math.cos(a * 0.63) * 5) - 1, 7 + Math.round(Math.sin(a * 0.63) * 5) - 1, 3, 3, a % 3 ? metal : accent); break;
    case 'dagger': g.line(4, 11, 11, 4, metal).line(3, 9, 6, 12, gold).line(2, 13, 4, 11, P.brown); break;
    case 'bow': for (let i = -6; i <= 6; i++) g.px(5 + Math.round(Math.abs(i) * 0.6), 8 + i, P.brown); g.line(8, 2, 8, 14, P.silver).line(4, 8, 13, 8, metal); break;
    case 'dart': g.poly([[8, 2], [11, 8], [8, 14], [5, 8]], metal).px(8, 8, accent); break;
    case 'shield': g.oval(2, 1, 12, 14, accent).oval(4, 3, 8, 10, metal).px(8, 8, gold); break;
    case 'helmet': g.oval(2, 2, 12, 11, metal).rect(2, 9, 12, 3, darken(metal, 0.2)).rect(8, 7, 5, 2, P.black); break;
    case 'legs': g.rect(3, 2, 10, 4, metal).rect(3, 6, 4, 8, metal).rect(9, 6, 4, 8, metal).rect(3, 13, 4, 2, P.brown).rect(9, 13, 4, 2, P.brown); break;
    case 'hat': g.rect(1, 11, 14, 3, accent).poly([[4, 12], [12, 12], [9, 1]], accent).px(8, 8, gold); break;
    case 'belt': g.rect(1, 6, 14, 4, P.brown).rect(6, 5, 5, 6, gold).rect(7, 6, 3, 4, P.brown); break;
    case 'robe': g.poly([[5, 1], [11, 1], [14, 15], [2, 15]], accent).line(8, 2, 8, 14, gold); break;
    case 'cloak': g.poly([[4, 1], [12, 1], [15, 15], [1, 15]], accent).rect(5, 1, 6, 2, gold); break;
    case 'glove': g.rect(4, 6, 8, 8, accent).rect(4, 2, 2, 5, accent).rect(6, 1, 2, 5, accent).rect(8, 1, 2, 5, accent).rect(10, 3, 2, 5, accent); break;
    case 'amulet': g.line(3, 1, 8, 7, gold).line(13, 1, 8, 7, gold).oval(5, 6, 7, 8, accent).px(7, 8, P.white); break;
    case 'potion': {
      const liquid = [P.pink, P.red, P.crimson, P.yellow][tier] ?? P.red;
      g.rect(6, 1, 4, 2, P.brown).rect(6, 3, 4, 3, P.silver).oval(3, 5, 10, 10, P.silver).oval(4, 7, 8, 7, liquid).px(6, 8, P.white);
      break;
    }
    case 'armor':
      g.poly([[2, 3], [6, 2], [8, 4], [10, 2], [14, 3], [13, 14], [3, 14]], metal).rect(7, 4, 2, 10, darken(metal, 0.25))
        .rect(3, 3, 3, 3, accent).rect(10, 3, 3, 3, accent).rect(4, 11, 8, 1, gold);
      break;
    default: g.rect(3, 3, 10, 10, metal); break;
  }
  g.outline(P.outline);
  return g;
}
export const itemIcon = (kind, tier) => memo(`icon-${kind}-${tier}`, () => iconGrid(kind, tier).toCanvas());
export const iconUrl = (kind, tier) => memo(`icon-url-${kind}-${tier}`, () => {
  const canvas = itemIcon(kind, tier);
  return canvas.toDataURL ? canvas.toDataURL() : '';
});
export const tierColor = (tier) => TierColors[tier] ?? TierColors[0];
export const spriteUrl = (key, canvas) => memo(`url-${key}`, () => canvas.toDataURL());
