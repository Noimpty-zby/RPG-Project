// Procedural monsters for all 50 bestiary entries. Each archetype draws a
// 32x32 sprite facing right; the roster below picks the archetype, colors
// and accessories per enemy. Bosses are shown at double size in battle.
import { Grid, darken, lighten } from './pixel.js';
import { P } from './palette.js';
import { humanoidSide } from './humanoid.js';

function beast(o, pose) {
  const g = new Grid(32, 32);
  const fur = o.color;
  const shade = darken(fur, 0.3);
  const belly = o.belly || lighten(fur, 0.35);
  const lunge = pose === 'attack' ? 2 : 0;
  const bear = o.kind === 'bear';
  // Tail.
  if (!bear) g.line(5, 17, 1, 11, fur).line(5, 18, 2, 12, shade).px(1, 10, belly);
  // Back legs and body.
  g.rect(6, 22, 3, 8, shade).rect(10, 22, 3, 8, fur);
  if (bear) g.oval(3, 10, 22, 17, fur).oval(5, 18, 18, 8, belly);
  else g.oval(4, 13, 19, 12, fur).oval(7, 19, 14, 5, belly);
  g.rect(17 + lunge, 22, 3, 8, shade).rect(21 + lunge, 21, 3, 9, fur);
  g.rect(6, 29, 4, 2, darken(fur, 0.5)).rect(21 + lunge, 29, 4, 2, darken(fur, 0.5));
  // Spikes along the back.
  if (o.spikes) for (let x = 7; x <= 19; x += 4) g.poly([[x, 14], [x + 2, 8 + (x % 3)], [x + 3, 14]], o.spikes);
  const heads = o.twoHead ? [[0, -5], [-4, 3]] : [[0, 0]];
  for (const [dx, dy] of heads) {
    const hx = 18 + lunge + dx;
    const hy = (bear ? 7 : 8) + dy;
    // Ears.
    if (bear) g.oval(hx + 1, hy - 1, 4, 4, shade).oval(hx + 7, hy - 1, 4, 4, shade);
    else g.poly([[hx + 2, hy + 3], [hx + 3, hy - 3], [hx + 6, hy + 2]], shade).poly([[hx + 5, hy + 2], [hx + 7, hy - 3], [hx + 9, hy + 3]], fur);
    // Head and snout.
    g.oval(hx, hy, 11, 10, fur);
    g.rect(hx + 7, hy + 4, 6, 5, fur).rect(hx + 7, hy + 7, 6, 2, belly);
    g.px(hx + 12, hy + 4, P.black);
    g.rect(hx + 7, hy + 3, 2, 1, o.eye || P.yellow).px(hx + 8, hy + 2, darken(fur, 0.4));
    g.rect(hx + 8, hy + 9, 4, 1, P.black);
    g.px(hx + 9, hy + 9, P.white).px(hx + 11, hy + 9, P.white);
    if (o.mane) g.rect(hx - 1, hy + 2, 3, 8, o.mane).px(hx + 1, hy, o.mane);
  }
  if (o.armor) g.rect(8, 12, 12, 4, o.armor).rect(8, 12, 12, 1, lighten(o.armor, 0.4));
  g.outline(P.outline);
  return g;
}

function spider(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const shade = darken(c, 0.35);
  const reach = pose === 'attack' ? 2 : 0;
  // Legs behind.
  for (let i = 0; i < 4; i++) {
    const bx = 13 + i * 3;
    g.line(bx, 19, bx - 5 + i, 12 - (i % 2), shade).line(bx - 5 + i, 12 - (i % 2), bx - 8 + i * 2, 29, shade);
  }
  g.oval(2, 10, 15, 14, c).oval(4, 12, 9, 6, lighten(c, 0.15));
  if (o.mark) g.rect(7, 14, 3, 6, o.mark).rect(6, 16, 5, 2, o.mark);
  g.oval(14 + reach, 14, 10, 9, shade).oval(15 + reach, 14, 8, 7, c);
  for (let i = 0; i < 4; i++) {
    const bx = 15 + i * 2 + reach;
    g.line(bx, 21, bx + 4 + i, 16, c).line(bx + 4 + i, 16, bx + 7 + i, 29, c);
  }
  g.px(20 + reach, 16, o.eye || P.red).px(22 + reach, 16, o.eye || P.red).px(21 + reach, 17, o.eye || P.red)
    .px(23 + reach, 18, o.eye || P.red);
  g.line(23 + reach, 21, 25 + reach, 24, P.sand).line(21 + reach, 22, 22 + reach, 25, P.sand);
  g.outline(P.outline);
  return g;
}

function worm(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const shade = darken(c, 0.3);
  const rise = pose === 'attack' ? -2 : 0;
  const pts = [[6, 29], [8, 25], [11, 21], [14, 17], [17, 14], [20, 11]];
  g.oval(1, 27, 16, 5, darken(o.ground || P.brown, 0.1));
  pts.forEach(([x, y], i) => {
    const r = 9 - Math.floor(i / 2);
    g.oval(x - r / 2, y - r / 2 + (i > 2 ? rise : 0), r, r, i % 2 ? c : shade);
    g.px(x, y - r / 2 + 1 + (i > 2 ? rise : 0), lighten(c, 0.3));
  });
  const hx = 18;
  const hy = 4 + rise;
  g.oval(hx, hy, 11, 11, c).oval(hx + 4, hy + 3, 6, 6, P.plum);
  for (let a = 0; a < 6; a++) g.px(hx + 7 + Math.round(Math.cos(a) * 3), hy + 6 + Math.round(Math.sin(a) * 3), P.sand);
  g.px(hx + 3, hy + 2, o.eye || P.yellow).px(hx + 2, hy + 4, o.eye || P.yellow);
  if (o.drip) g.px(hx + 6, hy + 11, o.drip).px(hx + 6, hy + 13, o.drip).px(hx + 9, hy + 12, o.drip);
  g.outline(P.outline);
  return g;
}

function plant(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const leaf = o.leaf || lighten(c, 0.2);
  const shade = darken(c, 0.35);
  const open = pose === 'attack';
  if (o.treant) {
    // Trunk body with branch arms and a canopy crown.
    g.rect(10, 12, 12, 18, c).rect(10, 12, 3, 18, shade).line(15, 14, 15, 28, shade);
    g.rect(7, 28, 4, 3, c).rect(21, 28, 4, 3, c);
    g.line(21, 16, 28, 10, c).line(21, 17, 28, 11, shade).line(11, 17, 4, 12, c);
    g.oval(4, 0, 24, 13, leaf).oval(6, 2, 8, 6, lighten(leaf, 0.2)).oval(17, 4, 9, 6, darken(leaf, 0.2));
    g.rect(17, 16, 2, 2, o.eye || P.yellow).rect(22, 16, 1, 2, o.eye || P.yellow);
    g.rect(17, 21, 6, 1, P.black);
    g.oval(26, 8, 4, 4, o.eye || P.yellow);
  } else {
    // Vine stalk with a snapping bulb.
    g.oval(4, 26, 22, 5, shade);
    g.line(12, 29, 10, 20, c).line(13, 29, 11, 20, c).line(11, 20, 15, 12, c).line(12, 20, 16, 12, c);
    g.poly([[11, 22], [4, 18], [10, 25]], leaf).poly([[13, 17], [21, 15], [15, 20]], leaf);
    const hx = 13;
    const hy = 3;
    g.oval(hx, hy, 14, 11, c).oval(hx + 1, hy + 1, 6, 4, lighten(c, 0.25));
    if (open) g.poly([[hx + 7, hy + 5], [hx + 16, hy + 1], [hx + 16, hy + 10]], P.plum);
    else g.rect(hx + 8, hy + 5, 6, 2, P.plum);
    for (let i = 0; i < 3; i++) g.px(hx + 9 + i * 2, hy + 4, P.sand).px(hx + 9 + i * 2, hy + 7, P.sand);
    g.px(hx + 5, hy + 3, o.eye || P.yellow);
    if (o.spots) g.px(hx + 3, hy + 7, o.spots).px(hx + 6, hy + 8, o.spots).px(hx + 2, hy + 4, o.spots);
  }
  g.outline(P.outline);
  return g;
}

function slime(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const squash = pose === 'attack' ? 2 : 0;
  g.oval(4 - squash, 10 + squash, 24 + squash * 2, 21 - squash, c);
  g.rect(4 - squash, 24, 24 + squash * 2, 6, c);
  g.oval(6, 12 + squash, 8, 5, lighten(c, 0.4)).px(8, 13 + squash, P.white);
  g.rect(4, 28, 24, 2, darken(c, 0.3));
  if (o.kind === 'mirror') {
    g.line(10, 22, 18, 12, P.white).line(14, 26, 22, 16, lighten(c, 0.5));
  }
  if (o.kind === 'water') g.poly([[8, 12], [14, 4], [20, 10], [26, 6], [24, 14]], c).px(14, 5, lighten(c, 0.5));
  if (o.kind === 'ink') for (let i = 0; i < 4; i++) g.line(6 + i * 6, 29, 4 + i * 6, 31, darken(c, 0.4));
  g.rect(17, 16 + squash, 2, 3, o.eye || P.black).rect(22, 16 + squash, 2, 3, o.eye || P.black);
  g.px(17, 16 + squash, P.white).px(22, 16 + squash, P.white);
  g.rect(18, 22, 5, 1, darken(c, 0.45));
  g.outline(P.outline);
  return g;
}

function ghost(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const shade = darken(c, 0.25);
  const lean = pose === 'attack' ? 2 : 0;
  g.oval(9 + lean, 3, 15, 15, c);
  g.poly([[9 + lean, 11], [24 + lean, 11], [21, 22], [16, 28], [12, 23], [6, 27]], c);
  g.line(6, 27, 12, 23, shade).line(12, 23, 16, 28, shade);
  g.rect(9 + lean, 11, 3, 8, shade);
  g.oval(16 + lean, 7, 4, 5, darken(c, 0.7)).oval(21 + lean, 7, 3, 5, darken(c, 0.7));
  g.px(17 + lean, 8, o.eye || P.cyan).px(22 + lean, 8, o.eye || P.cyan);
  g.oval(18 + lean, 13, 4, 3, darken(c, 0.6));
  // Arms.
  g.line(22 + lean, 14, 27 + lean, 17, c).line(11 + lean, 14, 7, 18, shade);
  if (o.item === 'quill') g.line(27 + lean, 17, 30, 10, P.white).line(28 + lean, 16, 30, 11, P.silver);
  if (o.item === 'book') g.rect(25 + lean, 15, 5, 5, P.brown).rect(26 + lean, 16, 3, 3, P.sand);
  if (o.stars) for (const [x, y] of [[12, 16], [15, 20], [19, 15], [13, 6]]) g.px(x, y, o.stars);
  g.outline(o.outline || P.outline);
  return g;
}

function golem(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const shade = darken(c, 0.3);
  const light = lighten(c, 0.25);
  const punch = pose === 'attack' ? 3 : 0;
  // Legs.
  g.rect(9, 23, 5, 8, shade).rect(17, 23, 5, 8, c);
  // Torso.
  g.rect(6, 10, 19, 15, c).rect(6, 10, 19, 2, light).rect(6, 10, 3, 15, shade);
  if (o.core) g.oval(13, 14, 6, 6, o.core).px(15, 15, P.white);
  if (o.plate) g.rect(11, 13, 9, 8, o.plate).rect(12, 14, 7, 6, lighten(o.plate, 0.3)).line(15, 14, 15, 19, darken(o.plate, 0.3));
  if (o.cracks) g.line(8, 12, 12, 18, o.cracks).line(20, 20, 23, 14, o.cracks).line(17, 11, 15, 15, o.cracks);
  if (o.circuit) g.rect(8, 17, 6, 1, o.circuit).rect(13, 17, 1, 5, o.circuit).rect(18, 12, 1, 6, o.circuit).rect(18, 12, 5, 1, o.circuit);
  // Head.
  g.rect(12, 3, 9, 8, c).rect(12, 3, 9, 2, light).rect(12, 3, 2, 8, shade);
  g.rect(16, 6, 2, 2, o.eye || P.cyan).rect(19, 6, 2, 2, o.eye || P.cyan);
  if (o.horns) g.line(12, 4, 9, 0, o.horns).line(20, 4, 23, 0, o.horns);
  // Arms: back arm then front arm with a fist or tool.
  g.rect(2, 11, 5, 13, shade).rect(1, 22, 6, 5, shade);
  g.rect(23 + punch, 11, 5, 11, c).rect(22 + punch, 20, 8, 7, light);
  if (o.tool === 'hammer') g.rect(28 + punch, 14, 2, 12, P.brown).rect(25 + punch, 9, 8, 6, P.steel).rect(25 + punch, 13, 8, 2, P.slate);
  if (o.tool === 'book') g.rect(23 + punch, 20, 8, 9, P.wine).rect(24 + punch, 21, 6, 7, P.sand).px(26 + punch, 24, P.cyan);
  if (o.moss) g.px(8, 10, o.moss).px(9, 10, o.moss).px(20, 3, o.moss).px(24, 11, o.moss).px(7, 15, o.moss);
  g.outline(P.outline);
  return g;
}

function eye(o, pose) {
  const g = new Grid(32, 32);
  const c = o.color;
  const look = pose === 'attack' ? 2 : 0;
  if (o.wings) {
    g.poly([[10, 14], [0, 4], [3, 14], [0, 22], [10, 18]], o.wings).poly([[22, 14], [31, 4], [29, 14], [31, 22], [22, 18]], o.wings);
  }
  if (o.ring === 'gear') {
    for (let a = 0; a < 12; a++) {
      const x = 16 + Math.round(Math.cos((a / 12) * Math.PI * 2) * 13);
      const y = 15 + Math.round(Math.sin((a / 12) * Math.PI * 2) * 13);
      g.rect(x - 1, y - 1, 3, 3, P.steel);
    }
    g.oval(4, 3, 25, 25, P.slate).oval(6, 5, 21, 21, null);
  }
  if (o.ring === 'star') for (let a = 0; a < 8; a++) {
    const ang = (a / 8) * Math.PI * 2;
    g.line(16 + Math.cos(ang) * 9, 15 + Math.sin(ang) * 9, 16 + Math.cos(ang) * 15, 15 + Math.sin(ang) * 15, o.accent || P.yellow);
  }
  if (o.ring === 'lantern') g.rect(5, 2, 22, 2, P.ink).rect(5, 2, 2, 26, P.ink).rect(25, 2, 2, 26, P.ink).rect(5, 26, 22, 2, P.ink)
    .rect(15, 0, 2, 3, P.ink);
  if (o.ring === 'tendril') for (let i = 0; i < 5; i++) g.line(10 + i * 3, 24, 8 + i * 4, 31, darken(c, 0.4));
  if (o.ring === 'corona') g.oval(1, 0, 30, 30, o.accent || P.violet).oval(3, 2, 26, 26, null);
  g.oval(7, 6, 19, 19, c).oval(9, 8, 15, 15, lighten(c, 0.15));
  g.oval(14 + look, 11, 9, 9, o.iris || P.red).oval(17 + look, 13, 4, 5, P.black).px(16 + look, 12, P.white);
  g.rect(7, 14, 3, 3, darken(c, 0.3));
  if (o.lids) g.rect(8, 6, 17, 3, o.lids).rect(8, 23, 17, 2, o.lids);
  g.outline(P.outline);
  return g;
}

function thing(o, pose) {
  const g = new Grid(32, 32);
  const bob = pose === 'attack' ? -1 : 0;
  if (o.kind === 'book') {
    g.poly([[4, 12 + bob], [16, 16 + bob], [16, 28 + bob], [4, 24 + bob]], o.color)
      .poly([[16, 16 + bob], [28, 12 + bob], [28, 24 + bob], [16, 28 + bob]], darken(o.color, 0.2));
    g.poly([[6, 13 + bob], [16, 17 + bob], [26, 13 + bob], [16, 10 + bob]], P.sand);
    for (let i = 0; i < 4; i++) g.line(7 + i, 14 + bob + i, 15, 18 + bob, P.silver);
    g.oval(18, 3 + bob, 8, 8, o.accent || P.cyan).oval(20, 5 + bob, 4, 4, P.black).px(21, 5 + bob, P.white);
    for (const [x, y] of [[3, 8], [28, 6], [24, 2], [8, 4]]) g.px(x, y, o.accent || P.cyan);
  } else if (o.kind === 'blade') {
    g.rect(14, 2, 4, 21, o.color).rect(17, 2, 1, 21, darken(o.color, 0.3)).poly([[14, 2], [16, -2], [18, 2]], o.color);
    g.rect(9, 22, 14, 3, o.accent || P.gold).rect(15, 25, 2, 5, P.brown).rect(14, 29, 4, 2, o.accent || P.gold);
    g.oval(13, 22, 6, 3, o.accent || P.gold).px(16, 23, P.red);
    for (let i = 0; i < 6; i++) g.px(10 + (i * 5) % 12, 6 + i * 3, lighten(o.color, 0.5));
  } else if (o.kind === 'core') {
    g.oval(2, 2, 28, 28, darken(o.color, 0.5)).oval(4, 4, 24, 24, null);
    g.rect(15, 0, 2, 32, P.steel).rect(0, 15, 32, 2, P.steel);
    g.poly([[16, 5], [24, 16], [16, 27], [8, 16]], o.color).poly([[16, 8], [21, 16], [16, 24], [11, 16]], lighten(o.color, 0.3));
    g.px(15, 13, P.white).px(16, 12, P.white);
    for (let a = 0; a < 6; a++) g.oval(15 + Math.cos(a) * 12, 15 + Math.sin(a) * 12, 3, 3, o.accent || P.orange);
  }
  g.outline(P.outline);
  return g;
}

const builders = { beast, spider, worm, plant, slime, ghost, golem, eye, thing };

function humanoid(o, pose) { return humanoidSide({ ...o, pose }); }

// [archetype, options] for each bestiary entry (chapter * 5 + slot; slot 4 = boss).
const Roster = [
  // 1 荒野前哨
  ['humanoid', { hat: 'helmet', hatColor: P.steel, hatAccent: null, torso: P.navy, armor: P.steel, trim: P.gold, legs: P.ink, weapon: 'bell', weaponAccent: P.gold, shield: null }],
  ['beast', { color: '#8a5a3a', belly: P.tan, spikes: P.pine, eye: P.red }],
  ['humanoid', { hat: 'bandana', hatColor: P.red, hatAccent: P.white, hair: P.black, torso: P.brown, trim: P.tan, legs: P.plum, weapon: 'axe', weaponColor: P.steel, shield: '#6a4a3a', beard: P.black }],
  ['humanoid', { hat: 'hood', hatColor: P.forest, torso: P.pine, trim: P.tan, legs: P.brown, weapon: 'bow', weaponColor: P.clay, glow: P.yellow }],
  ['humanoid', { hat: 'helmet', hatColor: P.slate, hatAccent: P.red, torso: P.navy, armor: P.steel, trim: P.gold, legs: P.ink, bulk: 1, weapon: 'bell', weaponAccent: P.gold, shield: P.slate, glow: P.red }],
  // 2 幽林回廊
  ['humanoid', { hat: 'hood', hatColor: '#3e6a3a', torso: P.forest, robe: '#2f5a32', trim: P.green, weapon: 'staff', weaponAccent: P.green, glow: P.green }],
  ['plant', { color: '#5a7a2a', leaf: P.green, eye: P.red, spots: P.magenta }],
  ['spider', { color: '#5a4030', mark: P.green, eye: P.green }],
  ['eye', { color: '#2a3a2a', iris: P.green, ring: 'tendril', lids: '#1c3324' }],
  ['plant', { treant: true, color: '#6a4a2a', leaf: '#3e8948', eye: P.yellow }],
  // 3 沉没书库
  ['thing', { kind: 'book', color: P.navy, accent: P.cyan }],
  ['slime', { color: '#2a3a6a', kind: 'water', eye: P.cyan }],
  ['golem', { color: '#7a8088', plate: P.sand, eye: P.cyan, moss: P.green }],
  ['ghost', { color: '#b8d0e0', item: 'quill', eye: P.navy }],
  ['golem', { color: '#5a6070', plate: P.wine, tool: 'book', eye: P.cyan, horns: P.gold }],
  // 4 赤砂矿井
  ['humanoid', { hat: 'miner', hatColor: P.gold, torso: P.slate, trim: P.orange, legs: P.ink, weapon: 'hammer', weaponColor: P.steel, beard: P.brown }],
  ['worm', { color: '#8ab83a', eye: P.red, drip: P.yellow, ground: '#7a4028' }],
  ['golem', { color: '#a0583a', cracks: P.orange, eye: P.yellow }],
  ['eye', { color: '#e0c890', iris: P.orange, ring: 'lantern' }],
  ['humanoid', { hat: 'miner', hatColor: P.orange, torso: P.rust, armor: P.brown, trim: P.yellow, legs: P.plum, bulk: 1, weapon: 'hammer', weaponColor: P.slate, beard: P.black, glow: P.orange }],
  // 5 寒霜关隘
  ['humanoid', { hat: 'wizard', hatColor: '#5a9ad0', hatAccent: P.white, torso: '#3a7ab0', robe: '#4a8ac0', trim: P.white, weapon: 'staff', weaponAccent: P.cyan, hair: P.white, hairStyle: 'long' }],
  ['beast', { color: '#dfe8f0', belly: P.white, eye: P.cyan, spikes: '#8ad0f0' }],
  ['beast', { kind: 'bear', color: '#a8b8c8', belly: P.white, eye: P.blue, armor: '#6ad0f0' }],
  ['ghost', { color: '#e8f0f8', eye: P.slate }],
  ['beast', { color: '#9ab8d8', belly: P.white, eye: P.cyan, spikes: P.cyan, twoHead: true, mane: P.white }],
  // 6 熔炉要塞
  ['humanoid', { hat: 'helmet', hatColor: P.ink, hatAccent: P.orange, torso: P.wine, armor: '#4a4040', trim: P.orange, legs: P.black, weapon: 'spear', weaponColor: P.orange }],
  ['humanoid', { hat: 'bandana', hatColor: P.orange, hair: P.black, torso: P.rust, trim: P.yellow, legs: P.plum, weapon: 'scythe', weaponColor: P.orange, mask: P.ink }],
  ['golem', { color: '#6a6060', core: P.orange, tool: 'hammer', eye: P.orange }],
  ['eye', { color: '#c0b8a8', iris: P.orange, ring: 'gear' }],
  ['thing', { kind: 'core', color: P.orange, accent: P.yellow }],
  // 7 镜影剧场
  ['humanoid', { hair: P.magenta, hairStyle: 'long', torso: P.pink, robe: P.magenta, trim: P.gold, weapon: 'fan', weaponColor: P.pink, weaponAccent: P.white, mask: P.white }],
  ['humanoid', { hat: 'jester', hatColor: P.red, hatAccent: P.violet, hair: P.white, torso: P.violet, trim: P.yellow, legs: P.red, weapon: 'dagger', mask: P.white }],
  ['humanoid', { hair: P.silver, torso: P.slate, cape: P.wine, trim: P.cyan, legs: P.ink, weapon: 'mirror', mask: P.silver }],
  ['slime', { color: '#a8b8d0', kind: 'mirror', eye: P.magenta }],
  ['humanoid', { hat: 'tophat', hatColor: P.black, hatAccent: P.red, hair: P.white, torso: P.black, cape: P.red, trim: P.gold, legs: P.black, bulk: 1, weapon: 'mirror', mask: P.white, glow: P.red }],
  // 8 星蚀观测站
  ['humanoid', { hat: 'hood', hatColor: P.violet, torso: P.violet, robe: '#4a2a6a', trim: P.yellow, weapon: 'orb', weaponAccent: P.yellow, glow: P.yellow }],
  ['ghost', { color: '#7a5aa8', eye: P.yellow, stars: P.yellow }],
  ['golem', { color: '#3a3448', cracks: '#b06aff', eye: '#d0a0ff' }],
  ['eye', { color: '#e0e0f0', iris: P.navy, ring: 'star', accent: P.yellow }],
  ['eye', { color: '#2a2440', iris: P.yellow, ring: 'corona', accent: '#6a3aa8', lids: '#14163a' }],
  // 9 王庭废墟
  ['humanoid', { hat: 'helmet', hatColor: P.silver, hatAccent: P.violet, torso: P.violet, armor: P.silver, trim: P.gold, legs: P.slate, weapon: 'sword', shield: P.violet }],
  ['humanoid', { hat: 'hood', hatColor: P.wine, torso: P.black, trim: P.red, legs: P.plum, bulk: 1, weapon: 'axe', weaponColor: P.steel, mask: P.black, glow: P.red }],
  ['humanoid', { hat: 'helmet', hatColor: P.steel, hatAccent: null, torso: P.slate, armor: P.steel, trim: P.silver, legs: P.ink, weapon: 'sword', weaponColor: '#8a8a8a' }],
  ['humanoid', { hat: 'circlet', hair: P.black, hairStyle: 'tail', torso: P.navy, robe: '#2a2a5a', trim: P.gold, weapon: 'book', weaponColor: P.wine, weaponAccent: P.gold, beard: P.black }],
  ['humanoid', { hat: 'crown', hair: P.white, hairStyle: 'long', torso: P.wine, robe: '#6a1a2a', trim: P.gold, bulk: 1, weapon: 'staff', weaponAccent: P.red, beard: P.white, glow: P.red }],
  // 10 天顶之门
  ['golem', { color: '#2a3a5a', circuit: P.cyan, eye: P.cyan }],
  ['thing', { kind: 'blade', color: '#d0e8ff', accent: P.cyan }],
  ['golem', { color: '#c8d0e0', circuit: P.blue, eye: P.blue, horns: P.cyan }],
  ['eye', { color: '#f0f0ff', iris: P.cyan, wings: P.white }],
  ['humanoid', { hat: 'halo', hair: P.yellow, hairStyle: 'long', torso: P.white, armor: P.gold, cape: P.cyan, trim: P.cyan, legs: P.silver, bulk: 1, weapon: 'greatsword', weaponColor: P.cyan, weaponAccent: P.gold, glow: P.cyan, wings: true }],
];

function addWings(g) {
  const w = new Grid(32, 32);
  w.poly([[13, 15], [2, 4], [4, 12], [0, 15], [5, 20], [12, 21]], P.white).line(2, 4, 12, 18, P.silver).line(4, 12, 12, 19, P.silver);
  w.outline(P.outline);
  for (let i = 0; i < g.data.length; i++) if (g.data[i]) w.data[i] = g.data[i];
  return w;
}

export function monsterGrid(entry, pose = 'idle') {
  const [type, options] = Roster[entry];
  const builder = type === 'humanoid' ? humanoid : builders[type];
  let g = builder(options, pose);
  if (options.wings && type === 'humanoid') g = addWings(g);
  return g.mirrored(); // Monsters face left, toward the hero.
}

export function monsterArchetype(entry) { return Roster[entry][0]; }
export const RosterSize = Roster.length;
