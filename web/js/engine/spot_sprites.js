// Pixel art for floor exploration: caches, curios, gathering spots, affix
// sources, keys, vault doors, quest items, puzzle steles, journal pages and
// cracked walls, and for forks: the medicine chest, fog sources, lookouts,
// shortcuts and lane gates. Each sprite has a "spent" look for explored spots;
// colors follow the chapter accent so the same shapes fit every theme.
import { Grid, darken, lighten } from './pixel.js';
import { ChapterThemes, P } from './palette.js';

const make = (g) => {
  g.outline(P.outline);
  return g.toCanvas();
};

function sack(spent) {
  const g = new Grid(16, 16);
  if (spent) {
    g.oval(2, 10, 12, 5, darken(P.tan, 0.1)).oval(5, 10, 6, 2, P.plum).px(3, 12, P.sand).px(11, 13, darken(P.tan, 0.3));
    return make(g);
  }
  g.oval(3, 6, 10, 9, P.tan).oval(4, 11, 9, 4, darken(P.tan, 0.18)).rect(6, 3, 4, 4, P.tan).rect(5, 6, 6, 1, P.brown)
    .px(5, 2, P.tan).px(10, 2, P.tan).px(7, 9, P.sand).px(8, 8, P.sand);
  return make(g);
}

function crate(accent, spent) {
  const g = new Grid(16, 16);
  const wood = P.clay;
  if (spent) {
    g.rect(2, 7, 12, 8, wood).rect(3, 7, 10, 3, P.black).rect(2, 11, 12, 1, P.steel)
      .rect(4, 7, 1, 8, darken(wood, 0.25)).rect(11, 7, 1, 8, darken(wood, 0.25)).line(1, 6, 6, 3, lighten(wood, 0.2));
    return make(g);
  }
  g.rect(2, 5, 12, 10, wood).rect(2, 5, 12, 2, lighten(wood, 0.2)).rect(2, 9, 12, 1, P.steel)
    .rect(4, 5, 1, 10, darken(wood, 0.25)).rect(11, 5, 1, 10, darken(wood, 0.25)).rect(7, 8, 2, 3, accent).px(7, 9, P.black);
  return make(g);
}

function shrine(accent, spent) {
  const g = new Grid(16, 32);
  g.rect(2, 26, 12, 5, P.slate).rect(2, 26, 12, 1, P.steel).rect(4, 11, 8, 15, P.steel).rect(4, 11, 1, 15, lighten(P.steel, 0.2))
    .poly([[1, 12], [8, 5], [15, 12]], P.slate).rect(6, 14, 4, 7, P.ink).rect(7, 18, 2, 3, P.sand).rect(5, 23, 6, 1, P.slate);
  if (!spent) g.px(7, 17, accent).px(8, 16, P.yellow).px(8, 17, accent);
  else g.px(8, 17, P.steel);
  return make(g);
}

function relic(accent, spent) {
  const g = new Grid(16, 24);
  g.rect(4, 16, 8, 7, P.slate).rect(3, 15, 10, 2, P.steel).rect(5, 19, 6, 1, P.ink);
  const orb = spent ? darken(accent, 0.55) : accent;
  g.oval(5, 6, 6, 6, orb).px(6, 7, spent ? P.steel : P.white);
  if (!spent) g.px(3, 5, accent).px(12, 9, accent).px(8, 3, P.white);
  else g.line(8, 7, 9, 10, P.ink);
  return make(g);
}

// Gathering spots by ingredient: 0 fish, 1 herb, 2 ore, 3 frost crystal, 4 stardust.
function gather(ingredient, spent) {
  const g = new Grid(16, 16);
  switch (ingredient) {
    case 0:
      g.oval(1, 6, 14, 9, P.navy).oval(2, 7, 12, 7, spent ? darken(P.blue, 0.2) : P.blue).px(4, 9, P.white).px(11, 12, P.white);
      if (!spent) g.rect(6, 10, 4, 1, P.silver).px(10, 9, P.silver).px(10, 11, P.silver).px(6, 10, P.white);
      break;
    case 1:
      if (spent) {
        g.line(5, 15, 5, 12, P.forest).line(8, 15, 8, 11, P.forest).line(11, 15, 11, 12, P.forest).rect(4, 14, 9, 1, P.brown);
      } else {
        g.oval(2, 6, 7, 6, P.forest).oval(7, 4, 7, 7, P.green).oval(4, 9, 9, 6, P.forest).line(8, 15, 8, 9, P.pine)
          .px(5, 7, P.pink).px(10, 5, P.white).px(11, 9, P.pink).px(4, 11, P.white);
      }
      break;
    case 2: {
      const rock = '#6a5a5a';
      g.oval(2, 5, 12, 10, rock).oval(3, 5, 9, 5, lighten(rock, 0.15)).rect(3, 13, 10, 1, darken(rock, 0.3));
      if (spent) g.px(6, 9, darken(rock, 0.4)).px(9, 11, darken(rock, 0.4)).line(5, 8, 7, 10, darken(rock, 0.3));
      else g.px(5, 8, P.orange).px(6, 9, P.gold).px(9, 7, P.orange).px(10, 11, P.gold).px(7, 12, P.orange).px(11, 9, P.yellow);
      break;
    }
    case 3:
      if (spent) {
        g.rect(4, 12, 3, 3, '#8ad0f0').rect(9, 13, 3, 2, '#8ad0f0').rect(3, 14, 10, 1, P.slate);
      } else {
        g.poly([[3, 15], [5, 5], [7, 15]], '#8ad0f0').poly([[6, 15], [9, 2], [12, 15]], lighten('#8ad0f0', 0.3))
          .poly([[10, 15], [13, 8], [14, 15]], '#8ad0f0').line(9, 4, 9, 13, P.white).px(5, 8, P.white);
      }
      break;
    default:
      g.oval(2, 10, 12, 5, spent ? P.ink : P.violet).oval(4, 9, 8, 3, spent ? P.night : lighten(P.violet, 0.2));
      if (!spent) g.px(5, 10, P.yellow).px(9, 9, P.white).px(11, 11, P.yellow).px(7, 7, P.yellow).px(8, 6, P.white).px(8, 8, P.yellow);
      break;
  }
  return make(g);
}

function source(accent, spent) {
  const g = new Grid(16, 32);
  g.rect(2, 28, 12, 3, P.ink).rect(2, 28, 12, 1, P.slate);
  if (spent) {
    g.poly([[4, 28], [5, 20], [7, 22], [9, 18], [11, 21], [12, 28]], P.night).px(3, 27, P.ink).px(13, 26, P.ink).px(8, 24, P.ink);
    return make(g);
  }
  g.poly([[4, 28], [5, 9], [8, 4], [11, 9], [12, 28]], P.night).line(8, 6, 8, 26, darken(P.night, 0.3))
    .rect(6, 12, 4, 1, accent).rect(7, 16, 2, 1, accent).rect(6, 20, 4, 1, accent).px(8, 13, P.white)
    .oval(6, 0, 4, 5, accent).px(7, 1, P.yellow).px(8, 2, P.white);
  return make(g);
}

function key() {
  const g = new Grid(16, 16);
  g.stamp(1, 6, [
    '..yyy.........',
    '.y...y........',
    '.y...yyyyyyyy.',
    '.y...y....y.y.',
    '..yyy.........',
  ], { y: P.gold });
  g.px(3, 7, P.yellow).px(8, 8, P.yellow);
  return make(g);
}

function questItem(accent) {
  const g = new Grid(16, 16);
  g.rect(3, 7, 10, 7, P.brown).rect(3, 7, 10, 3, lighten(P.brown, 0.15)).rect(7, 6, 2, 8, accent).px(7, 9, P.gold).px(8, 9, P.gold)
    .rect(5, 4, 6, 3, P.brown).rect(6, 5, 4, 1, P.plum);
  return make(g);
}

function stele(accent, spent) {
  const g = new Grid(16, 32);
  const rune = spent ? P.green : accent;
  g.rect(1, 27, 14, 4, P.slate).rect(1, 27, 14, 1, P.steel).rect(3, 7, 10, 20, P.steel).oval(3, 3, 10, 8, P.steel)
    .rect(3, 7, 1, 20, lighten(P.steel, 0.2)).rect(12, 7, 1, 20, darken(P.steel, 0.2));
  for (const [y, w] of [[9, 6], [13, 4], [17, 6], [21, 3]]) g.rect(8 - Math.floor(w / 2), y, w, 1, rune);
  g.px(5, 11, rune).px(10, 15, rune).px(6, 19, rune);
  return make(g);
}

function lorePage() {
  const g = new Grid(16, 16);
  g.poly([[4, 3], [12, 3], [12, 13], [9, 14], [4, 14]], P.sand).poly([[9, 14], [12, 13], [9, 12]], P.tan)
    .rect(5, 5, 6, 1, P.clay).rect(5, 7, 5, 1, P.clay).rect(5, 9, 6, 1, P.clay).rect(5, 11, 3, 1, P.clay).px(8, 3, P.steel);
  return make(g);
}

// Drawn over the wall face tile that holds the door.
function vaultDoor(locks, wall, spent) {
  const g = new Grid(16, 16);
  const frame = darken(wall, 0.35);
  if (spent) {
    // Open: the posts and the swung-back leaf; the doorway itself is floor.
    g.rect(0, 0, 2, 16, frame).rect(14, 0, 2, 16, frame).rect(2, 0, 3, 13, P.brown).px(3, 4, P.steel).px(3, 9, P.steel);
    return make(g);
  }
  g.rect(2, 1, 12, 15, frame);
  g.rect(3, 2, 10, 14, P.brown).rect(5, 2, 1, 14, darken(P.brown, 0.3)).rect(10, 2, 1, 14, darken(P.brown, 0.3))
    .rect(3, 5, 10, 1, P.steel).rect(3, 12, 10, 1, P.steel);
  const xs = locks === 2 ? [4, 9] : [7];
  for (const x of xs) g.rect(x, 7, 3, 3, P.gold).px(x + 1, 8, P.black);
  if (locks === 2) g.rect(2, 1, 12, 1, P.gold);
  return make(g);
}

// A vault with no wall to sit in: an iron strongbox.
function strongbox(locks, spent) {
  const g = new Grid(16, 16);
  if (spent) {
    g.rect(1, 8, 14, 7, P.slate).rect(2, 8, 12, 2, P.black).rect(1, 3, 14, 4, P.steel).rect(1, 6, 14, 1, P.gold);
    return make(g);
  }
  g.rect(1, 6, 14, 9, P.slate).rect(1, 4, 14, 4, P.steel).rect(1, 8, 14, 1, P.gold);
  const xs = locks === 2 ? [4, 10] : [7];
  for (const x of xs) g.rect(x, 7, 2, 4, P.gold).px(x, 9, P.black);
  return make(g);
}

function crack() {
  const g = new Grid(16, 16);
  const line = '#181425cc';
  g.line(8, 1, 6, 5, line).line(6, 5, 9, 9, line).line(9, 9, 7, 14, line).line(6, 5, 3, 7, line).line(9, 9, 12, 11, line)
    .px(10, 4, '#ffffff40').px(5, 11, '#ffffff40');
  return g.toCanvas();
}

function rubble(wall) {
  const g = new Grid(16, 16);
  const stone = lighten(wall, 0.15);
  g.rect(2, 12, 3, 2, stone).rect(7, 13, 2, 2, stone).rect(11, 11, 3, 3, stone).px(5, 14, darken(stone, 0.3)).px(10, 14, darken(stone, 0.3));
  g.outline(P.outline);
  return g.toCanvas();
}

// The fog on each chapter's far branch: miasma, spores, mildew, mine gas,
// frost, smoke, illusion, stardust, curse, energy.
export const FogColors = ['#a88ec8', '#d6e06a', '#b0c49a', '#c9c84a', '#e6f4ff', '#8a8078', '#f0b0e8', '#a8b8ff',
  '#5a3a6a', '#2ce8f5'];

function medkit(spent) {
  const g = new Grid(16, 16);
  if (spent) {
    g.rect(2, 8, 12, 7, P.silver).rect(3, 8, 10, 3, P.black).rect(2, 12, 12, 1, P.steel).line(1, 7, 6, 4, P.white);
    return make(g);
  }
  g.rect(2, 6, 12, 9, P.white).rect(2, 6, 12, 2, P.silver).rect(2, 13, 12, 2, P.silver).rect(6, 3, 4, 3, P.steel)
    .rect(7, 8, 2, 5, P.red).rect(5, 10, 6, 1, P.red).rect(5, 9, 6, 1, P.red);
  return make(g);
}

// A vent or pool breathing out the chapter's fog; sealed with a slab when shut.
function fogSource(fog, spent) {
  const g = new Grid(16, 16);
  g.oval(1, 8, 14, 7, P.ink).oval(2, 9, 12, 5, P.night);
  if (spent) {
    g.rect(3, 9, 10, 4, P.slate).rect(3, 9, 10, 1, P.steel).px(5, 11, P.ink).px(10, 11, P.ink);
    return make(g);
  }
  g.oval(4, 10, 8, 3, darken(fog, 0.35)).oval(5, 4, 4, 4, fog).oval(8, 1, 5, 5, lighten(fog, 0.2)).px(6, 11, fog)
    .px(10, 12, lighten(fog, 0.3)).px(9, 3, P.white);
  return make(g);
}

// A wooden lookout tower; a flag flies once it has been climbed.
function lookout(accent, spent) {
  const g = new Grid(16, 32);
  const wood = P.clay;
  g.rect(3, 12, 1, 19, darken(wood, 0.25)).rect(12, 12, 1, 19, darken(wood, 0.25)).line(3, 30, 12, 16, wood)
    .line(12, 30, 3, 16, wood).rect(2, 10, 12, 3, wood).rect(2, 10, 12, 1, lighten(wood, 0.2))
    .rect(2, 6, 1, 4, darken(wood, 0.25)).rect(13, 6, 1, 4, darken(wood, 0.25)).rect(2, 6, 12, 1, darken(wood, 0.1));
  for (let y = 15; y < 30; y += 3) g.rect(6, y, 4, 1, lighten(wood, 0.1));
  g.rect(6, 13, 1, 18, darken(wood, 0.3)).rect(9, 13, 1, 18, darken(wood, 0.3));
  if (spent) g.rect(8, 0, 1, 6, P.steel).rect(9, 0, 4, 3, accent);
  return make(g);
}

// The fog lane's far end seen from the guardian's room: a cracked wall chunk
// (variant 0) or a raised drawbridge (variant 1), and how each looks opened.
function shortcut(variant, wall, spent) {
  const g = new Grid(16, 16);
  if (variant === 0) {
    const stone = lighten(wall, 0.12);
    if (spent) {
      g.rect(1, 12, 4, 3, stone).rect(10, 11, 4, 4, stone).rect(6, 13, 3, 2, darken(stone, 0.2)).outline(P.outline);
      return g.toCanvas();
    }
    g.rect(0, 0, 16, 16, stone).rect(0, 0, 16, 2, lighten(stone, 0.15)).rect(0, 14, 16, 2, darken(stone, 0.25))
      .line(8, 1, 6, 5, P.black).line(6, 5, 9, 9, P.black).line(9, 9, 7, 14, P.black).line(6, 5, 3, 7, P.black)
      .line(9, 9, 12, 11, P.black).px(10, 4, '#ffffff60');
    return g.toCanvas();
  }
  const wood = P.brown;
  if (spent) {
    g.rect(1, 0, 14, 16, wood);
    for (let y = 1; y < 16; y += 3) g.rect(1, y, 14, 1, darken(wood, 0.3));
    g.rect(0, 0, 1, 16, P.steel).rect(15, 0, 1, 16, P.steel);
    return g.toCanvas();
  }
  g.rect(1, 2, 14, 13, wood).rect(1, 2, 14, 1, lighten(wood, 0.2));
  for (let x = 3; x < 15; x += 3) g.rect(x, 3, 1, 12, darken(wood, 0.3));
  g.line(1, 2, 0, 0, P.steel).line(14, 2, 15, 0, P.steel).rect(6, 7, 4, 3, P.steel).px(7, 8, P.black).px(8, 8, P.black);
  return make(g);
}

// Lane barriers: an iron gate with a lock (key lane) and a spiked barricade
// (patrol lane's far end). Open gates are drawn as their posts.
function laneGate(wall, open) {
  const g = new Grid(16, 16);
  const frame = darken(wall, 0.35);
  g.rect(0, 0, 2, 16, frame).rect(14, 0, 2, 16, frame);
  if (open) {
    g.rect(2, 0, 12, 2, P.slate);
    return make(g);
  }
  g.rect(2, 1, 12, 15, P.slate);
  for (let x = 3; x < 14; x += 2) g.rect(x, 1, 1, 15, P.steel);
  g.rect(2, 5, 12, 1, P.ink).rect(2, 11, 12, 1, P.ink).rect(6, 7, 4, 4, P.gold).px(7, 9, P.black).px(8, 9, P.black);
  return make(g);
}
function barricade() {
  const g = new Grid(16, 16);
  const wood = P.clay;
  g.line(1, 15, 6, 3, wood).line(14, 15, 9, 3, wood).line(4, 15, 12, 4, darken(wood, 0.2)).line(12, 15, 4, 4, darken(wood, 0.2))
    .rect(1, 9, 14, 2, P.brown).px(6, 3, P.silver).px(9, 3, P.silver).px(12, 4, P.silver).px(4, 4, P.silver);
  return make(g);
}

// Sprites for one chapter; ingredients are by index, curios by variant.
export function spotSprites(theme) {
  const t = ChapterThemes[theme];
  const accent = t.accent;
  const both = (build) => ({ live: build(false), spent: build(true) });
  return {
    cache: both(sack),
    curios: [both((s) => crate(accent, s)), both((s) => shrine(accent, s)), both((s) => relic(accent, s))],
    gather: [0, 1, 2, 3, 4].map((i) => both((s) => gather(i, s))),
    source: both((s) => source(accent, s)),
    key: key(),
    questItem: questItem(accent),
    stele: both((s) => stele(accent, s)),
    lore: lorePage(),
    door: [1, 2].map((locks) => both((s) => vaultDoor(locks, t.wall, s))),
    strongbox: [1, 2].map((locks) => both((s) => strongbox(locks, s))),
    crack: crack(),
    rubble: rubble(t.wall),
    medkit: both(medkit),
    fogSource: both((s) => fogSource(FogColors[theme], s)),
    lookout: both((s) => lookout(accent, s)),
    shortcuts: [0, 1].map((variant) => both((s) => shortcut(variant, t.wall, s))),
    gate: both((s) => laneGate(t.wall, s)),
    barricade: barricade(),
  };
}
