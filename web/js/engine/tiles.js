// Chapter-themed tiles and props for the top-down floors. Everything is drawn
// procedurally with the theme palette and a per-tile hash for variety.
import { Grid, darken, lighten, mix } from './pixel.js';
import { ChapterThemes, P } from './palette.js';

export const TILE = 16;

export function hash2(x, y, seed = 0) {
  let h = Math.imul(x, 374761393) + Math.imul(y, 668265263) + Math.imul(seed, 2147483647);
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

// Floor tile texture for a theme.
export function floorTile(theme, variant) {
  const t = ChapterThemes[theme];
  const g = new Grid(TILE, TILE);
  const base = t.floor[variant % t.floor.length];
  g.rect(0, 0, TILE, TILE, base);
  const light = lighten(base, 0.12);
  const dark = darken(base, 0.15);
  const r = (i) => hash2(variant * 31 + i, i * 7, theme);
  switch (t.key) {
    case 'wild':
      for (let i = 0; i < 6; i++) {
        const x = Math.floor(r(i) * 15);
        const y = Math.floor(r(i + 9) * 14) + 1;
        g.px(x, y, light).px(x, y - 1, lighten(base, 0.22)).px(x + 1, y, dark);
      }
      if (variant === 3) g.px(5, 6, P.yellow).px(10, 11, P.white).px(11, 11, P.pink);
      break;
    case 'forest':
      for (let i = 0; i < 5; i++) g.px(Math.floor(r(i) * 16), Math.floor(r(i + 5) * 16), dark);
      if (variant % 2) g.rect(3, 9, 3, 2, darken(base, 0.3)).px(4, 8, '#5a8a3a');
      if (variant === 3) g.px(8, 4, '#c0e080').px(12, 12, '#9be36b');
      break;
    case 'library':
      g.rect(0, 0, TILE, 1, dark).rect(0, 0, 1, TILE, dark).rect(8, 0, 1, 8, dark).rect(0, 8, TILE, 1, dark).rect(4, 8, 1, 8, dark);
      g.px(2, 2, light).px(11, 4, light).px(6, 12, light);
      if (variant === 3) g.oval(4, 4, 9, 6, mix(base, P.cyan, 0.3));
      break;
    case 'mine':
      for (let i = 0; i < 8; i++) g.px(Math.floor(r(i) * 16), Math.floor(r(i + 3) * 16), i % 2 ? dark : light);
      if (variant === 2) g.oval(5, 6, 4, 3, darken(base, 0.25)).px(6, 6, P.tan);
      if (variant === 3) g.rect(0, 6, 16, 1, P.slate).rect(0, 10, 16, 1, P.slate).rect(3, 5, 2, 7, P.brown).rect(11, 5, 2, 7, P.brown);
      break;
    case 'frost':
      for (let i = 0; i < 4; i++) g.px(Math.floor(r(i) * 16), Math.floor(r(i + 4) * 16), P.white);
      g.px(Math.floor(r(9) * 16), Math.floor(r(10) * 16), darken(base, 0.1));
      if (variant === 3) g.line(2, 12, 12, 3, lighten(base, 0.3)).line(6, 13, 9, 10, P.white);
      break;
    case 'forge':
      g.rect(0, 0, TILE, 1, dark).rect(0, 0, 1, TILE, dark).rect(0, 15, TILE, 1, darken(base, 0.3)).rect(15, 0, 1, TILE, darken(base, 0.3));
      g.px(2, 2, light).px(13, 2, light).px(2, 13, light).px(13, 13, light);
      if (variant === 3) g.line(4, 8, 11, 8, '#7a2a10').px(7, 8, P.orange);
      break;
    case 'theater': {
      const checker = (variant % 2) === 0;
      g.rect(0, 0, 8, 8, checker ? base : dark).rect(8, 8, 8, 8, checker ? base : dark)
        .rect(8, 0, 8, 8, checker ? dark : base).rect(0, 8, 8, 8, checker ? dark : base);
      g.px(3, 3, light).px(11, 11, light);
      if (variant === 3) g.px(7, 7, P.gold).px(8, 8, P.gold);
      break;
    }
    case 'observatory':
      g.rect(0, 0, TILE, 1, dark).rect(0, 0, 1, TILE, dark);
      g.px(Math.floor(r(1) * 14) + 1, Math.floor(r(2) * 14) + 1, P.white);
      if (variant >= 2) g.px(Math.floor(r(3) * 14) + 1, Math.floor(r(4) * 14) + 1, P.yellow);
      if (variant === 3) g.line(3, 8, 12, 8, mix(base, P.yellow, 0.3)).line(8, 3, 8, 12, mix(base, P.yellow, 0.3));
      break;
    case 'ruins':
      g.rect(0, 0, TILE, 1, dark).rect(0, 0, 1, TILE, dark);
      if (variant % 2) g.line(3, 3, 7, 9, darken(base, 0.3)).line(7, 9, 6, 14, darken(base, 0.3));
      g.px(10, 4, light).px(12, 11, light);
      if (variant === 3) g.rect(9, 9, 4, 3, darken(base, 0.2)).px(10, 9, P.slate);
      break;
    case 'zenith':
      g.rect(0, 0, TILE, 1, dark).rect(0, 0, 1, TILE, dark);
      if (variant % 2) g.rect(0, 8, 6, 1, mix(base, P.cyan, 0.35)).rect(6, 8, 1, 8, mix(base, P.cyan, 0.35)).px(6, 8, P.cyan);
      if (variant === 3) g.rect(9, 3, 4, 4, mix(base, P.cyan, 0.2)).px(10, 4, P.cyan);
      break;
    default:
      break;
  }
  return g.toCanvas();
}

// Wall face (the visible front of a wall above a floor tile).
export function wallFace(theme, variant) {
  const t = ChapterThemes[theme];
  const g = new Grid(TILE, TILE);
  const c = t.wall;
  const light = lighten(c, 0.15);
  const dark = darken(c, 0.3);
  g.rect(0, 0, TILE, TILE, c);
  switch (t.key) {
    case 'wild':
    case 'forest':
      // Earth cliff with roots and grass lip.
      g.rect(0, 0, TILE, 3, t.floor[0]).rect(0, 3, TILE, 1, darken(t.floor[0], 0.3));
      for (let i = 0; i < 5; i++) g.line(i * 4 + (variant % 3), 5, i * 4 + 1, 13, dark);
      g.px(3, 9, light).px(11, 7, light);
      break;
    case 'library':
      // Bookshelf face.
      for (let row = 0; row < 3; row++) {
        g.rect(0, row * 5 + 1, TILE, 1, darken(c, 0.4));
        for (let b = 0; b < 7; b++) {
          const colors = [P.wine, P.navy, P.forest, P.gold, P.violet, P.rust];
          g.rect(1 + b * 2, row * 5 + 2, 2, 4, colors[(b + row + variant) % colors.length]);
        }
      }
      break;
    case 'frost':
      g.rect(0, 0, TILE, 4, P.white).rect(0, 4, TILE, 1, lighten(c, 0.4));
      g.line(2, 6, 5, 14, light).line(10, 5, 13, 13, light).px(8, 9, P.cyan);
      break;
    case 'theater':
      // Velvet curtain folds.
      for (let x = 0; x < TILE; x += 4) g.rect(x, 0, 2, TILE, lighten(c, 0.12)).rect(x + 3, 0, 1, TILE, dark);
      g.rect(0, 0, TILE, 2, P.gold);
      break;
    case 'observatory':
      g.rect(0, 0, TILE, 2, lighten(c, 0.3));
      g.px(4, 6, P.white).px(11, 9, P.yellow).px(7, 12, P.white);
      g.rect(0, 15, TILE, 1, dark);
      break;
    case 'zenith':
      g.rect(0, 0, TILE, 2, lighten(c, 0.25)).rect(3, 5, 10, 1, mix(c, P.cyan, 0.5)).rect(3, 5, 1, 6, mix(c, P.cyan, 0.5))
        .px(12, 5, P.cyan).rect(0, 15, TILE, 1, dark);
      break;
    default:
      // Brick / stone blocks.
      for (let row = 0; row < 4; row++) {
        g.rect(0, row * 4, TILE, 1, dark);
        const off = (row + variant) % 2 ? 0 : 4;
        for (let x = off; x < TILE; x += 8) g.rect(x, row * 4, 1, 4, dark);
        g.px((off + 3) % 16, row * 4 + 1, light);
      }
      g.rect(0, 0, TILE, 1, lighten(c, 0.3));
      break;
  }
  return g.toCanvas();
}

// Top of a wall (seen from above): the impassable terrain between rooms.
export function wallTop(theme, variant = 0) {
  const t = ChapterThemes[theme];
  const g = new Grid(TILE, TILE);
  const base = t.wallTop;
  const light = lighten(base, 0.14);
  const dark = darken(base, 0.25);
  const r = (i) => hash2(variant * 13 + i, i * 5 + 1, theme + 40);
  g.rect(0, 0, TILE, TILE, base);
  switch (t.key) {
    case 'wild':
    case 'forest':
      // Dense canopy: overlapping leaf clumps.
      for (let i = 0; i < 5; i++) {
        const x = Math.floor(r(i) * 12) - 2;
        const y = Math.floor(r(i + 7) * 12) - 2;
        g.oval(x, y, 8, 7, i % 2 ? light : dark).oval(x + 1, y + 1, 4, 3, i % 2 ? lighten(base, 0.25) : base);
      }
      break;
    case 'library':
      for (let y = 0; y < TILE; y += 4) g.rect(0, y, TILE, 1, dark);
      g.rect(Math.floor(r(1) * 14), 1, 1, 3, dark).rect(Math.floor(r(2) * 14), 9, 1, 3, dark);
      break;
    case 'frost':
      for (let i = 0; i < 4; i++) g.px(Math.floor(r(i) * 16), Math.floor(r(i + 3) * 16), '#8ab0d0');
      g.oval(Math.floor(r(9) * 8), Math.floor(r(10) * 8), 7, 5, lighten(base, 0.3));
      break;
    case 'observatory':
    case 'zenith':
      for (let i = 0; i < 3; i++) g.px(Math.floor(r(i) * 16), Math.floor(r(i + 3) * 16), i ? t.accent : '#ffffff');
      break;
    case 'theater':
      for (let x = 0; x < TILE; x += 4) g.rect(x, 0, 1, TILE, dark);
      g.px(2, 2, t.accent).px(10, 10, t.accent);
      break;
    case 'forge':
      g.rect(0, 0, TILE, 1, light).rect(0, 15, TILE, 1, dark).px(3, 3, light).px(12, 12, light);
      if (variant === 1) g.line(2, 9, 13, 7, '#7a2410');
      break;
    default:
      // Rock: speckles and a crack.
      for (let i = 0; i < 6; i++) g.px(Math.floor(r(i) * 16), Math.floor(r(i + 6) * 16), i % 2 ? light : dark);
      if (variant === 2) g.line(3, 4, 7, 9, dark).line(7, 9, 6, 13, dark);
      break;
  }
  return g.toCanvas();
}

// ---- Props -----------------------------------------------------------------
// Each prop: { w, h (in px), solid, draw(g) } built at load time per theme.
function treeProp(leaf, trunk, tall = true) {
  const g = new Grid(16, 32);
  g.rect(6, 20, 4, 10, trunk).rect(6, 20, 1, 10, darken(trunk, 0.3));
  g.oval(0, 2, 16, 14, leaf).oval(1, tall ? 10 : 8, 14, 12, darken(leaf, 0.12)).oval(3, 3, 7, 5, lighten(leaf, 0.2));
  g.outline(P.outline);
  return g;
}
function rockProp(c) {
  const g = new Grid(16, 16);
  g.oval(1, 5, 14, 10, c).oval(3, 5, 8, 5, lighten(c, 0.2)).rect(1, 12, 14, 2, darken(c, 0.25));
  g.outline(P.outline);
  return g;
}
function crystalProp(c) {
  const g = new Grid(16, 16);
  g.poly([[3, 15], [5, 4], [8, 15]], c).poly([[7, 15], [10, 1], [13, 15]], lighten(c, 0.15)).poly([[10, 15], [13, 8], [15, 15]], darken(c, 0.2));
  g.line(10, 3, 10, 13, lighten(c, 0.5));
  g.outline(P.outline);
  return g;
}
function shelfProp(theme) {
  const g = new Grid(16, 32);
  g.rect(0, 4, 16, 28, P.brown).rect(1, 5, 14, 26, darken(P.brown, 0.3));
  for (let row = 0; row < 4; row++) {
    g.rect(1, 11 + row * 6, 14, 1, P.brown);
    for (let b = 0; b < 6; b++) g.rect(2 + b * 2, 6 + row * 6, 2, 5, [P.wine, P.navy, P.forest, P.gold, P.violet][(b * 3 + row + theme) % 5]);
  }
  g.rect(0, 2, 16, 2, lighten(P.brown, 0.2));
  g.outline(P.outline);
  return g;
}
function pillarProp(c, broken) {
  const g = new Grid(16, 32);
  g.rect(3, broken ? 14 : 4, 10, broken ? 16 : 26, c).rect(3, broken ? 14 : 4, 2, broken ? 16 : 26, lighten(c, 0.2)).rect(11, broken ? 14 : 4, 2, broken ? 16 : 26, darken(c, 0.25));
  g.rect(1, 28, 14, 3, darken(c, 0.15));
  if (!broken) g.rect(1, 2, 14, 3, lighten(c, 0.1));
  else g.poly([[3, 14], [6, 10], [9, 13], [13, 11], [13, 14]], c);
  g.outline(P.outline);
  return g;
}
function mushroomProp() {
  const g = new Grid(16, 16);
  g.rect(4, 9, 2, 5, P.sand).oval(1, 5, 8, 5, P.red).px(3, 6, P.white).px(6, 7, P.white);
  g.rect(10, 10, 2, 4, P.sand).oval(8, 7, 6, 4, P.violet).px(10, 8, P.cyan);
  g.outline(P.outline);
  return g;
}
function glowPlantProp(c) {
  const g = new Grid(16, 16);
  g.line(8, 15, 8, 6, P.forest).line(8, 10, 4, 6, P.forest).line(8, 9, 12, 5, P.forest);
  g.oval(6, 3, 5, 5, c).px(7, 4, P.white).oval(2, 4, 4, 4, c).oval(11, 3, 4, 4, c);
  g.outline(P.outline);
  return g;
}
function bushProp(c) {
  const g = new Grid(16, 16);
  g.oval(1, 4, 14, 11, c).oval(3, 5, 6, 4, lighten(c, 0.2)).px(5, 9, P.red).px(10, 7, P.red);
  g.outline(P.outline);
  return g;
}
function cartProp() {
  const g = new Grid(16, 16);
  g.poly([[1, 4], [15, 4], [13, 12], [3, 12]], P.slate).rect(1, 4, 14, 2, P.steel).rect(3, 2, 10, 3, P.orange).px(5, 2, P.gold).px(9, 1, P.yellow);
  g.oval(2, 11, 4, 4, P.ink).oval(10, 11, 4, 4, P.ink);
  g.outline(P.outline);
  return g;
}
function beamProp() {
  const g = new Grid(16, 32);
  g.rect(2, 4, 3, 28, P.brown).rect(11, 4, 3, 28, P.brown).rect(0, 2, 16, 4, lighten(P.brown, 0.1)).rect(2, 4, 1, 28, darken(P.brown, 0.3));
  g.rect(7, 6, 2, 3, P.ink).rect(6, 9, 4, 4, P.yellow).px(7, 10, P.white);
  g.outline(P.outline);
  return g;
}
function pineSnowProp() {
  const g = new Grid(16, 32);
  g.rect(7, 24, 2, 7, P.brown);
  for (let i = 0; i < 4; i++) g.poly([[8, 1 + i * 6], [1 + i, 10 + i * 5], [15 - i, 10 + i * 5]], i % 2 ? '#2a5a4a' : '#356a58');
  g.line(4, 9, 8, 2, P.white).line(3, 15, 8, 8, P.white).line(12, 20, 8, 14, P.white).px(7, 1, P.white);
  g.outline(P.outline);
  return g;
}
function anvilProp() {
  const g = new Grid(16, 16);
  g.rect(2, 5, 12, 3, P.slate).rect(0, 5, 3, 2, P.slate).rect(5, 8, 6, 3, P.ink).rect(3, 11, 10, 3, P.slate).rect(2, 5, 12, 1, P.steel);
  g.outline(P.outline);
  return g;
}
function barrelProp(c = P.brown) {
  const g = new Grid(16, 16);
  g.oval(3, 2, 10, 13, c).rect(3, 5, 10, 1, P.slate).rect(3, 11, 10, 1, P.slate).oval(4, 2, 8, 3, lighten(c, 0.2));
  g.outline(P.outline);
  return g;
}
function pipeProp() {
  const g = new Grid(16, 32);
  g.rect(5, 0, 6, 32, P.slate).rect(5, 0, 2, 32, P.steel).rect(4, 6, 8, 2, P.ink).rect(4, 20, 8, 2, P.ink).px(9, 12, P.orange);
  g.outline(P.outline);
  return g;
}
function mirrorProp() {
  const g = new Grid(16, 32);
  g.oval(1, 2, 14, 24, P.gold).oval(3, 4, 10, 20, '#9ab0d0').line(5, 8, 9, 4, P.white).line(6, 14, 11, 8, lighten('#9ab0d0', 0.4));
  g.rect(5, 26, 6, 5, P.gold);
  g.outline(P.outline);
  return g;
}
function chairProp() {
  const g = new Grid(16, 16);
  g.rect(3, 2, 10, 7, P.wine).rect(3, 9, 10, 3, P.red).rect(3, 12, 2, 3, P.brown).rect(11, 12, 2, 3, P.brown).rect(3, 2, 10, 1, P.gold);
  g.outline(P.outline);
  return g;
}
function telescopeProp() {
  const g = new Grid(16, 32);
  g.line(4, 31, 8, 18, P.brown).line(12, 31, 8, 18, P.brown).line(8, 31, 8, 18, P.brown);
  g.thick(2, 16, 13, 4, P.gold, 3).rect(12, 2, 3, 3, P.steel).px(3, 16, P.cyan);
  g.outline(P.outline);
  return g;
}
function orreryProp() {
  const g = new Grid(16, 32);
  g.rect(6, 20, 4, 10, P.gold).rect(3, 29, 10, 2, P.gold);
  g.oval(1, 4, 14, 14, null).oval(5, 8, 6, 6, P.yellow).px(6, 9, P.white);
  for (let a = 0; a < 16; a++) g.px(8 + Math.round(Math.cos(a / 2.55) * 7), 11 + Math.round(Math.sin(a / 2.55) * 3), P.steel);
  g.oval(13, 9, 3, 3, P.cyan).oval(0, 11, 3, 3, P.red);
  g.outline(P.outline);
  return g;
}
function statueProp(c) {
  const g = new Grid(16, 32);
  g.rect(2, 26, 12, 5, darken(c, 0.2)).rect(1, 25, 14, 2, c);
  g.oval(5, 4, 6, 6, c).rect(4, 10, 8, 12, c).rect(4, 10, 2, 12, lighten(c, 0.2)).rect(3, 12, 2, 8, c).rect(11, 12, 2, 8, darken(c, 0.2));
  g.rect(4, 22, 3, 4, c).rect(9, 22, 3, 4, c).line(12, 8, 14, 20, P.steel);
  g.outline(P.outline);
  return g;
}
function bannerProp(c) {
  const g = new Grid(16, 32);
  g.rect(7, 0, 2, 32, P.brown).rect(2, 2, 12, 14, c).poly([[2, 16], [8, 20], [14, 16]], c).px(8, 8, P.gold).px(7, 9, P.gold).px(9, 9, P.gold);
  g.outline(P.outline);
  return g;
}
function pylonProp() {
  const g = new Grid(16, 32);
  g.poly([[4, 31], [8, 3], [12, 31]], '#1a2a4a').line(8, 6, 8, 28, P.cyan).px(8, 4, P.white).oval(5, 0, 6, 6, P.cyan).px(7, 1, P.white);
  g.outline(P.outline);
  return g;
}
function obeliskProp() {
  const g = new Grid(16, 32);
  g.poly([[3, 31], [5, 6], [8, 2], [11, 6], [13, 31]], '#20325a');
  for (let y = 9; y < 28; y += 5) g.rect(6, y, 4, 1, P.cyan);
  g.outline(P.outline);
  return g;
}
function rubbleProp(c) {
  const g = new Grid(16, 16);
  g.oval(1, 9, 7, 6, c).oval(6, 7, 8, 8, darken(c, 0.1)).oval(10, 11, 5, 4, lighten(c, 0.1)).px(8, 9, lighten(c, 0.3));
  g.outline(P.outline);
  return g;
}
function candleProp() {
  const g = new Grid(16, 32);
  g.rect(7, 12, 2, 18, P.gold).rect(4, 28, 8, 2, P.gold).rect(3, 12, 10, 1, P.gold);
  g.rect(3, 8, 2, 4, P.sand).rect(7, 7, 2, 5, P.sand).rect(11, 8, 2, 4, P.sand);
  g.outline(P.outline);
  return g;
}
function iceRockProp() {
  const g = new Grid(16, 16);
  g.oval(1, 4, 14, 11, '#8ab0d0').oval(2, 3, 12, 6, P.white).px(9, 9, P.cyan).line(4, 10, 7, 13, lighten('#8ab0d0', 0.3));
  g.outline(P.outline);
  return g;
}

// Solid props block movement; decals are flat floor details drawn under entities.
export function themeProps(theme) {
  const t = ChapterThemes[theme];
  const make = (grid, solid, flicker = null) => ({ canvas: grid.toCanvas(), w: grid.w, h: grid.h, solid, flicker });
  switch (t.key) {
    case 'wild': return [make(treeProp('#4e8f3a', P.brown), true), make(treeProp('#5a9a40', P.brown), true),
      make(rockProp('#8a8a7a'), true), make(bushProp('#3e7a34'), true), make(bushProp('#4e8a3a'), true)];
    case 'forest': return [make(treeProp('#2a5a3a', '#4a3424'), true), make(treeProp('#1e4a30', '#3a2a1c'), true),
      make(mushroomProp(), false), make(glowPlantProp('#9be36b'), false, '#9be36b'), make(rockProp('#4a5a4a'), true)];
    case 'library': return [make(shelfProp(theme), true), make(shelfProp(theme + 3), true), make(candleProp(), true, '#feae34'),
      make(pillarProp('#6a7a8a', false), true), make(barrelProp('#5a3a2a'), true)];
    case 'mine': return [make(beamProp(), true, '#fee761'), make(cartProp(), true), make(crystalProp(P.orange), true, '#f77622'),
      make(rockProp('#7a4a3a'), true), make(barrelProp(), true)];
    case 'frost': return [make(pineSnowProp(), true), make(pineSnowProp(), true), make(crystalProp('#8ad0f0'), true, '#2ce8f5'),
      make(iceRockProp(), true), make(rockProp('#9ab0c0'), true)];
    case 'forge': return [make(anvilProp(), true), make(pipeProp(), true), make(barrelProp('#5a3030'), true),
      make(crystalProp('#ff7a1a'), true, '#ff7a1a'), make(rockProp('#4a3a3a'), true)];
    case 'theater': return [make(mirrorProp(), true, '#f6757a'), make(chairProp(), true), make(chairProp(), true),
      make(statueProp('#c0a8b8'), true), make(bannerProp(P.wine), true)];
    case 'observatory': return [make(telescopeProp(), true), make(orreryProp(), true, '#fee761'), make(crystalProp('#9a7aff'), true, '#b06aff'),
      make(pillarProp('#3a3e6a', false), true), make(rockProp('#3a3a5a'), true)];
    case 'ruins': return [make(pillarProp('#9a9488', true), true), make(pillarProp('#9a9488', false), true), make(statueProp('#a8a296'), true),
      make(bannerProp(P.violet), true), make(rubbleProp('#8a8478'), true)];
    case 'zenith': return [make(pylonProp(), true, '#2ce8f5'), make(obeliskProp(), true, '#2ce8f5'), make(crystalProp('#2ce8f5'), true, '#2ce8f5'),
      make(rubbleProp('#2a3a5a'), true), make(rockProp('#20325a'), true)];
    default: return [make(rockProp(P.steel), true)];
  }
}

// ---- Interactable objects ---------------------------------------------------
export function campfireFrames() {
  return [0, 1, 2].map((frame) => {
    const g = new Grid(16, 16);
    g.rect(2, 12, 12, 3, P.brown).line(2, 14, 13, 11, darken(P.brown, 0.2)).line(2, 11, 13, 14, P.brown);
    const flames = [[5, 4, 6, 9], [4, 5, 7, 8], [5, 3, 6, 10]][frame];
    g.oval(flames[0], flames[1], flames[2], flames[3], P.orange).oval(flames[0] + 1, flames[1] + 3, flames[2] - 2, flames[3] - 3, P.yellow)
      .px(8, flames[1] + 5, P.white);
    for (let i = 0; i < 6; i++) g.px(2 + i * 2, 15, P.slate);
    g.outline(P.outline);
    return g.toCanvas();
  });
}
export function waystoneFrames() {
  return [0, 1, 2, 3].map((frame) => {
    const g = new Grid(16, 32);
    g.rect(2, 26, 12, 5, P.slate).rect(2, 26, 12, 1, P.steel).rect(4, 24, 8, 2, P.slate);
    const y = 6 + [0, -1, -2, -1][frame];
    g.poly([[8, y], [13, y + 8], [8, y + 16], [3, y + 8]], P.cyan).poly([[8, y + 2], [11, y + 8], [8, y + 14]], lighten(P.cyan, 0.4))
      .px(7, y + 5, P.white);
    g.outline(P.outline);
    return g.toCanvas();
  });
}
export function stairsSprite(theme, sealed) {
  const t = ChapterThemes[theme];
  const g = new Grid(16, 16);
  const c = lighten(t.wall, 0.2);
  for (let i = 0; i < 4; i++) g.rect(1 + i, i * 4, 14 - i * 2, 4, i % 2 ? darken(c, 0.1) : lighten(c, 0.15)).rect(1 + i, i * 4 + 3, 14 - i * 2, 1, darken(c, 0.4));
  g.rect(0, 0, 1, 16, darken(c, 0.4)).rect(15, 0, 1, 16, darken(c, 0.4));
  if (sealed) for (let i = 0; i < 16; i += 3) g.line(0, i, 15, i + 6, '#b06aff');
  g.outline(P.outline);
  return g.toCanvas();
}
export function chestFrames() {
  const closed = new Grid(16, 16);
  closed.rect(1, 6, 14, 9, P.brown).rect(1, 4, 14, 4, lighten(P.brown, 0.15)).rect(1, 8, 14, 1, P.gold).rect(7, 7, 2, 4, P.gold)
    .rect(1, 6, 1, 9, P.gold).rect(14, 6, 1, 9, P.gold).px(7, 9, P.black);
  closed.outline(P.outline);
  const open = new Grid(16, 16);
  open.rect(1, 8, 14, 7, P.brown).rect(2, 8, 12, 2, P.black).rect(1, 2, 14, 4, lighten(P.brown, 0.15)).rect(1, 5, 14, 1, P.gold)
    .rect(1, 8, 1, 7, P.gold).rect(14, 8, 1, 7, P.gold).px(5, 8, P.yellow).px(9, 8, P.yellow);
  open.outline(P.outline);
  return [closed.toCanvas(), open.toCanvas()];
}
export function stallSprite() {
  const g = new Grid(32, 32);
  g.rect(1, 16, 30, 12, P.brown).rect(1, 16, 30, 2, lighten(P.brown, 0.2)).rect(3, 28, 3, 3, P.brown).rect(26, 28, 3, 3, P.brown);
  for (let x = 0; x < 32; x += 6) g.rect(x, 2, 6, 8, (x / 6) % 2 ? P.red : P.white);
  g.rect(0, 10, 32, 1, P.wine).rect(2, 10, 2, 7, P.brown).rect(28, 10, 2, 7, P.brown);
  g.rect(5, 13, 4, 4, P.red).rect(11, 12, 3, 5, P.blue).oval(17, 12, 5, 5, P.green).rect(24, 13, 4, 4, P.gold);
  g.outline(P.outline);
  return g.toCanvas();
}
// Sabotage target on route floors 2 and 7: a chapter device that breaks once cleared.
export function deviceSprite(theme, broken) {
  const t = ChapterThemes[theme];
  const g = new Grid(16, 32);
  if (broken) {
    g.rect(2, 24, 12, 6, P.slate).poly([[2, 24], [5, 18], [8, 23], [11, 17], [14, 24]], P.ink).px(6, 22, P.orange).px(10, 26, P.orange);
  } else {
    g.rect(3, 22, 10, 9, P.slate).rect(3, 22, 10, 1, P.steel).rect(6, 6, 4, 16, P.ink).oval(2, 0, 12, 10, t.accent)
      .oval(5, 2, 4, 4, lighten(t.accent, 0.5)).rect(4, 25, 8, 2, t.accent);
  }
  g.outline(P.outline);
  return g.toCanvas();
}
export function signSprite() {
  const g = new Grid(16, 16);
  g.rect(7, 8, 2, 8, P.brown).rect(1, 2, 14, 7, P.tan).rect(1, 2, 14, 1, P.sand).rect(3, 4, 10, 1, P.brown).rect(3, 6, 7, 1, P.brown);
  g.outline(P.outline);
  return g.toCanvas();
}
