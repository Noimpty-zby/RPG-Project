// Procedural humanoids: 32x32 side-view battlers (facing right) and 16x16
// top-down walkers. Heroes, NPCs and humanoid enemies share these builders.
import { Grid, darken, lighten } from './pixel.js';
import { P } from './palette.js';

const DEFAULT = {
  skin: P.skin, hair: P.brown, hairStyle: 'short', hat: null, hatColor: P.steel, hatAccent: P.red,
  torso: P.red, trim: P.gold, belt: P.brown, legs: P.ink, boots: P.plum, robe: null, cape: null, scarf: null,
  shield: null, weapon: 'sword', weaponColor: P.silver, weaponAccent: P.gold, eye: P.black, mask: null,
  bulk: 0, beard: null, glow: null, pose: 'idle', offhand: null,
};

// Weapon drawn from the hand point (hx, hy) in the given pose.
function drawWeapon(g, o, hx, hy) {
  const c = o.weaponColor;
  const a = o.weaponAccent;
  const dark = darken(c, 0.35);
  const pose = o.pose;
  const forward = pose === 'attack';
  const raised = pose === 'cast';
  switch (o.weapon) {
    case 'sword':
    case 'greatsword': {
      const len = o.weapon === 'greatsword' ? 13 : 10;
      if (forward) {
        g.rect(hx - 1, hy - 1, 2, 3, P.brown);
        g.rect(hx + 1, hy - 3, 1, 7, a);
        g.rect(hx + 2, hy, len, 2, c).rect(hx + 2, hy + 1, len, 1, dark).px(hx + 2 + len, hy, c);
      } else if (raised) {
        g.rect(hx, hy, 1, 3, P.brown);
        g.rect(hx - 2, hy - 1, 5, 1, a);
        g.rect(hx, hy - 1 - len, 2, len, c).rect(hx + 1, hy - 1 - len, 1, len, dark);
      } else {
        g.line(hx - 1, hy + 2, hx, hy + 1, P.brown);
        g.line(hx - 1, hy - 1, hx + 2, hy + 2, a);
        for (let i = 1; i <= len; i++) {
          g.px(hx + i, hy - i, c);
          g.px(hx + i + 1, hy - i, dark);
        }
        g.px(hx + len + 1, hy - len - 1, lighten(c, 0.4));
      }
      break;
    }
    case 'staff': {
      const top = raised ? hy - 16 : forward ? hy - 9 : hy - 14;
      const x = forward ? hx + 2 : hx;
      g.rect(x, top, 1, hy - top + 8, P.brown).rect(x + 1, top + 2, 1, hy - top + 6, darken(P.brown, 0.3));
      g.oval(x - 2, top - 4, 5, 5, a).px(x - 1, top - 3, lighten(a, 0.6));
      g.px(x - 2, top - 1, c).px(x + 2, top - 1, c);
      break;
    }
    case 'dagger': {
      if (forward) {
        g.rect(hx, hy, 2, 1, P.brown).rect(hx + 2, hy - 1, 1, 3, a).rect(hx + 3, hy, 5, 1, c).px(hx + 8, hy, lighten(c, 0.5));
      } else {
        g.rect(hx, hy, 1, 2, P.brown).rect(hx - 1, hy - 1, 3, 1, a);
        g.line(hx, hy - 2, hx + 3, hy - 5, c);
        g.px(hx + 1, hy - 2, dark);
      }
      break;
    }
    case 'bow': {
      const x = hx + 1;
      for (let i = -7; i <= 7; i++) g.px(x + Math.round(2.5 - Math.abs(i) * 0.33), hy + i, c);
      g.line(x, hy - 7, x, hy + 7, P.silver);
      if (forward) g.line(x - 6, hy, x + 4, hy, P.sand);
      break;
    }
    case 'axe':
    case 'hammer': {
      const top = raised ? hy - 12 : hy - 9;
      g.rect(hx, top, 1, hy - top + 5, P.brown);
      if (o.weapon === 'axe') g.poly([[hx + 1, top], [hx + 6, top - 2], [hx + 6, top + 6], [hx + 1, top + 4]], c)
        .line(hx + 6, top - 2, hx + 6, top + 6, lighten(c, 0.4));
      else g.rect(hx - 3, top - 1, 7, 5, c).rect(hx - 3, top + 3, 7, 1, dark);
      break;
    }
    case 'spear': {
      if (forward) {
        g.rect(hx - 6, hy, 18, 1, P.brown).poly([[hx + 12, hy - 2], [hx + 16, hy], [hx + 12, hy + 3]], c);
      } else {
        g.line(hx - 2, hy + 6, hx + 6, hy - 12, P.brown);
        g.poly([[hx + 5, hy - 12], [hx + 9, hy - 17], [hx + 8, hy - 11]], c);
      }
      break;
    }
    case 'scythe': {
      g.line(hx - 1, hy + 6, hx + 3, hy - 14, P.brown);
      g.line(hx + 3, hy - 14, hx - 6, hy - 12, c).line(hx + 3, hy - 13, hx - 5, hy - 11, dark);
      break;
    }
    case 'book': {
      g.rect(hx - 1, hy - 3, 5, 6, c).rect(hx, hy - 2, 3, 4, P.sand).rect(hx - 1, hy - 3, 1, 6, a);
      if (raised) g.oval(hx - 1, hy - 9, 5, 5, a);
      break;
    }
    case 'bell': {
      g.line(hx, hy, hx, hy - 4, P.brown).oval(hx - 3, hy - 9, 7, 6, a).rect(hx - 3, hy - 5, 7, 2, a).px(hx, hy - 3, P.brown);
      break;
    }
    case 'lantern': {
      g.line(hx, hy, hx, hy + 3, P.steel).rect(hx - 2, hy + 3, 5, 5, P.ink).rect(hx - 1, hy + 4, 3, 3, P.yellow);
      break;
    }
    case 'fan': {
      for (let i = 0; i < 5; i++) g.line(hx, hy, hx + 2 + i, hy - 6 + i, i % 2 ? c : a);
      break;
    }
    case 'mirror': {
      g.rect(hx, hy, 1, 3, P.gold).oval(hx - 3, hy - 7, 7, 8, P.gold).oval(hx - 2, hy - 6, 5, 6, P.cyan)
        .px(hx - 1, hy - 5, P.white);
      break;
    }
    case 'orb': {
      g.oval(hx - 1, hy - 5, 5, 5, a).px(hx, hy - 4, lighten(a, 0.6));
      break;
    }
    default:
      break;
  }
}

function drawHair(g, o, back) {
  const hair = o.hair;
  const shade = darken(hair, 0.25);
  if (o.hairStyle === 'none' || o.hairStyle === 'bald') return;
  if (back) {
    if (o.hairStyle === 'long') g.rect(10, 8, 5, 11, shade).rect(11, 8, 3, 10, hair);
    if (o.hairStyle === 'tail') g.rect(8, 9, 3, 6, shade).px(8, 15, shade);
    return;
  }
  g.oval(10, 3, 13, 7, hair);
  g.rect(10, 6, 5, 6, hair);
  g.rect(10, 10, 3, 3, shade);
  if (o.hairStyle === 'spiky') {
    g.px(12, 2, hair).px(15, 1, hair).px(15, 2, hair).px(18, 2, hair).px(21, 3, hair);
  }
  g.rect(18, 6, 5, 1, lighten(hair, 0.2));
  g.px(21, 7, hair).px(22, 7, hair);
}

function drawHat(g, o) {
  const c = o.hatColor;
  const a = o.hatAccent;
  const dark = darken(c, 0.3);
  switch (o.hat) {
    case 'helmet':
      g.oval(10, 2, 14, 8, c).rect(10, 6, 6, 8, c).rect(10, 10, 5, 4, dark);
      g.rect(16, 7, 7, 1, dark).rect(16, 8, 1, 5, c).rect(18, 2, 2, 4, lighten(c, 0.3));
      if (a) g.rect(14, 0, 3, 3, a).rect(11, 1, 4, 2, a);
      break;
    case 'wizard':
      g.rect(8, 7, 18, 2, c).rect(9, 9, 16, 1, dark);
      g.poly([[11, 8], [22, 8], [17, -2], [12, 0]], c);
      g.line(12, 7, 20, 7, a).px(16, 3, P.yellow).px(15, 4, P.yellow).px(17, 4, P.yellow).px(16, 5, P.yellow);
      g.px(11, -1, c).px(10, 1, dark);
      break;
    case 'hood':
      g.oval(9, 2, 15, 13, c).rect(9, 9, 6, 8, c).rect(9, 11, 5, 6, dark).rect(11, 3, 9, 1, lighten(c, 0.2));
      g.oval(15, 6, 8, 9, o.glow ? darken(c, 0.55) : o.skin);
      if (!o.glow) g.rect(15, 6, 8, 2, c);
      break;
    case 'cap':
      g.oval(10, 2, 13, 6, c).rect(17, 6, 8, 2, dark).rect(11, 4, 10, 1, a);
      break;
    case 'crown':
      g.rect(11, 2, 11, 3, P.gold).px(11, 1, P.gold).px(16, 0, P.gold).px(16, 1, P.gold).px(21, 1, P.gold)
        .px(16, 3, P.red).px(13, 3, P.blue).px(19, 3, P.blue);
      break;
    case 'horned':
      g.oval(10, 2, 14, 9, c).rect(10, 7, 14, 3, c).rect(17, 8, 7, 1, dark);
      g.line(11, 3, 7, -1, P.sand).line(12, 3, 8, -1, P.sand).line(21, 3, 25, -1, P.sand).line(22, 3, 26, 0, P.sand);
      break;
    case 'bandana':
      g.rect(10, 4, 13, 3, c).px(9, 6, c).px(8, 7, c).px(9, 8, dark).px(14, 5, a).px(18, 5, a);
      break;
    case 'tophat':
      g.rect(9, 6, 15, 2, c).rect(12, -1, 9, 7, c).rect(12, 4, 9, 1, a);
      break;
    case 'jester':
      g.oval(10, 2, 13, 7, c).poly([[11, 4], [5, 1], [12, 7]], c).poly([[21, 3], [27, 0], [22, 7]], a)
        .px(5, 1, P.yellow).px(27, 0, P.yellow);
      break;
    case 'miner':
      g.oval(10, 2, 13, 8, c).rect(9, 7, 15, 2, dark).rect(18, 3, 3, 3, P.yellow).px(19, 4, P.white);
      break;
    case 'fur':
      g.oval(9, 1, 15, 9, c).rect(9, 7, 15, 3, lighten(c, 0.3));
      break;
    case 'circlet':
      g.rect(11, 5, 12, 1, P.gold).px(19, 4, P.cyan);
      break;
    case 'halo':
      g.oval(11, -1, 11, 3, P.yellow).oval(13, 0, 7, 1, null);
      break;
    case 'mask':
      break;
    default:
      break;
  }
}

// 32x32 battler facing right.
export function humanoidSide(options) {
  const o = { ...DEFAULT, ...options };
  const g = new Grid(32, 32);
  const torsoShade = darken(o.torso, 0.28);
  const skinShade = darken(o.skin, 0.18);
  const bulk = o.bulk;
  // Cape and long hair behind everything.
  if (o.cape) g.poly([[12, 16], [19, 16], [17, 29], [7, 30]], o.cape).line(7, 30, 12, 16, darken(o.cape, 0.3));
  drawHair(g, o, true);
  if (o.scarf) g.rect(8, 15, 6, 2, o.scarf).rect(6, 16, 3, 2, o.scarf).px(5, 18, o.scarf);
  // Back arm and shield.
  g.rect(11 - bulk, 17, 2, 6, torsoShade).rect(11 - bulk, 22, 2, 2, skinShade);
  if (o.offhand === 'dagger') g.line(11 - bulk, 24, 8, 27, o.weaponColor);
  // Legs.
  if (!o.robe) {
    g.rect(13, 24, 3, 5, darken(o.legs, 0.2)).rect(17, 24, 3, 5, o.legs);
    g.rect(13, 28, 4, 3, darken(o.boots, 0.2)).rect(17, 28, 4, 3, o.boots).px(20, 28, lighten(o.boots, 0.2));
  } else {
    g.poly([[11, 22], [21, 22], [23, 31], [9, 31]], o.robe);
    g.rect(9, 30, 15, 1, darken(o.robe, 0.3)).line(16, 23, 18, 30, darken(o.robe, 0.2));
    g.rect(18, 30, 4, 1, o.boots);
  }
  // Torso.
  g.rect(12 - bulk, 15, 9 + bulk * 2, 9, o.torso);
  g.rect(12 - bulk, 15, 2, 9, torsoShade);
  g.rect(12 - bulk, 23, 9 + bulk * 2, 1, o.belt).px(18, 23, o.trim);
  g.line(17, 16, 17, 22, o.trim);
  if (o.armor) g.rect(14, 16, 6, 5, o.armor).rect(14, 16, 6, 1, lighten(o.armor, 0.3)).px(16, 18, darken(o.armor, 0.3));
  if (bulk) g.rect(10, 15, 4, 3, o.armor || o.trim).rect(19, 15, 4, 3, o.armor || o.trim);
  // Head.
  g.oval(11, 4, 12, 12, o.skin);
  g.rect(11, 9, 3, 6, skinShade);
  if (o.hat !== 'hood') drawHair(g, o, false);
  g.rect(18, 9, 1, 2, o.eye).rect(21, 9, 1, 2, o.eye);
  if (o.glow) g.px(18, 9, o.glow).px(21, 9, o.glow);
  g.px(20, 12, darken(o.skin, 0.12));
  if (o.mask) g.rect(16, 11, 7, 4, o.mask).rect(16, 11, 7, 1, darken(o.mask, 0.3));
  if (o.beard) g.rect(17, 12, 6, 4, o.beard).px(19, 16, o.beard);
  drawHat(g, o);
  if (o.hat === 'hood') {
    g.rect(18, 9, 1, 2, o.glow || o.eye).rect(21, 9, 1, 2, o.glow || o.eye);
    if (o.mask) g.rect(16, 11, 7, 4, o.mask);
  }
  if (o.scarf) g.rect(14, 14, 8, 2, o.scarf).rect(14, 15, 8, 1, darken(o.scarf, 0.25));
  // Shield on the near side.
  if (o.shield) {
    g.oval(6, 15, 8, 11, o.shield).oval(7, 16, 6, 9, lighten(o.shield, 0.15)).px(9, 20, o.trim).px(10, 20, o.trim);
  }
  // Front arm and weapon.
  let hx;
  let hy;
  if (o.pose === 'attack') {
    g.rect(18, 17, 6, 3, o.torso).rect(18, 19, 6, 1, torsoShade);
    hx = 24; hy = 18;
    g.rect(hx, hy - 1, 2, 3, o.skin);
  } else if (o.pose === 'cast') {
    g.rect(18, 11, 3, 7, o.torso).rect(20, 11, 1, 7, torsoShade);
    hx = 19; hy = 10;
    g.rect(hx - 1, hy - 1, 3, 2, o.skin);
  } else {
    g.rect(18, 16, 3, 6, o.torso).rect(20, 16, 1, 6, torsoShade);
    hx = 20; hy = 22;
    g.rect(hx - 1, hy, 3, 2, o.skin);
  }
  drawWeapon(g, o, hx, hy);
  g.outline(P.outline);
  return g;
}

export function humanoidFrames(options) {
  return {
    idle: humanoidSide({ ...options, pose: 'idle' }),
    attack: humanoidSide({ ...options, pose: 'attack' }),
    cast: humanoidSide({ ...options, pose: 'cast' }),
  };
}

// 16x16 top-down walker. dir: 'down' | 'up' | 'side' (faces right).
// step: 0 idle, 1 and 2 alternate legs.
export function walker(options, dir, step) {
  const o = { ...DEFAULT, ...options };
  const g = new Grid(16, 16);
  const torsoShade = darken(o.torso, 0.28);
  const skinShade = darken(o.skin, 0.2);
  const hair = o.hair;
  const hairShade = darken(hair, 0.25);
  const lift = step === 0 ? 0 : 1;
  // Legs.
  const legL = step === 1 ? -1 : 0;
  const legR = step === 2 ? -1 : 0;
  if (o.robe) {
    g.rect(4, 11, 8, 4, o.robe).rect(4, 14, 8, 1, darken(o.robe, 0.3));
    if (step) g.px(step === 1 ? 5 : 10, 15, o.boots);
  } else if (dir === 'side') {
    g.rect(6 + (step === 1 ? -1 : 0), 12, 2, 3 + legL, darken(o.legs, 0.2)).rect(8 + (step === 2 ? 1 : 0), 12, 2, 3 + legR, o.legs);
    g.px(6 + (step === 1 ? -1 : 0), 14 + legL, o.boots).px(9 + (step === 2 ? 1 : 0), 14 + legR, o.boots);
  } else {
    g.rect(5, 12, 2, 3 + legL, o.legs).rect(9, 12, 2, 3 + legR, o.legs);
    g.rect(5, 14 + legL, 2, 1, o.boots).rect(9, 14 + legR, 2, 1, o.boots);
  }
  if (o.cape && dir !== 'down') g.rect(4, 9, 8, 5, o.cape).rect(4, 13, 8, 1, darken(o.cape, 0.3));
  // Body.
  g.rect(4, 9 - (lift ? 0 : 0), 8, 4, o.torso).rect(4, 12, 8, 1, o.belt);
  if (dir === 'down') g.line(8, 9, 8, 11, o.trim);
  if (o.armor && dir !== 'up') g.rect(5, 9, 6, 2, o.armor);
  g.rect(3, 9, 1, 3, torsoShade).rect(12, 9, 1, 3, torsoShade);
  g.px(3, 12, o.skin).px(12, 12, o.skin);
  // Head.
  g.oval(3, 1, 10, 9, o.skin);
  if (dir === 'up') {
    g.oval(3, 1, 10, 9, hair).rect(3, 5, 10, 3, hairShade);
  } else if (dir === 'down') {
    if (o.hairStyle !== 'bald' && o.hairStyle !== 'none') {
      g.oval(3, 0, 10, 6, hair).rect(3, 3, 2, 4, hair).rect(11, 3, 2, 4, hair).rect(4, 2, 8, 1, lighten(hair, 0.15));
    }
    g.rect(5, 6, 1, 2, o.glow || o.eye).rect(10, 6, 1, 2, o.glow || o.eye);
    g.px(4, 8, skinShade).px(11, 8, skinShade);
    if (o.mask) g.rect(4, 7, 8, 2, o.mask);
    if (o.beard) g.rect(5, 7, 6, 2, o.beard);
  } else {
    if (o.hairStyle !== 'bald' && o.hairStyle !== 'none') g.oval(3, 0, 9, 6, hair).rect(3, 3, 4, 5, hair).rect(3, 6, 2, 2, hairShade);
    g.rect(10, 6, 1, 2, o.glow || o.eye);
    if (o.mask) g.rect(8, 7, 5, 2, o.mask);
    if (o.beard) g.rect(8, 7, 4, 2, o.beard);
  }
  // Hats.
  const c = o.hatColor;
  const a = o.hatAccent;
  switch (o.hat) {
    case 'helmet':
      g.oval(2, 0, 12, 6, c).rect(2, 3, 12, 2, c).rect(2, 5, 12, 1, darken(c, 0.3));
      if (a) g.rect(7, -1, 2, 2, a).px(7, 0, a);
      if (dir === 'up') g.rect(3, 5, 10, 3, c);
      break;
    case 'wizard':
      g.rect(1, 4, 14, 2, c).poly([[4, 5], [12, 5], [9, -3], [5, -1]], c).rect(4, 4, 8, 1, a);
      break;
    case 'hood':
      g.oval(2, 0, 12, 10, c).rect(2, 5, 12, 4, c);
      if (dir === 'down') g.oval(4, 3, 8, 6, o.skin).rect(5, 6, 1, 2, o.glow || o.eye).rect(10, 6, 1, 2, o.glow || o.eye);
      if (dir === 'side') g.oval(7, 3, 6, 6, o.skin).rect(10, 6, 1, 2, o.glow || o.eye);
      if (o.mask && dir === 'down') g.rect(4, 7, 8, 2, o.mask);
      if (o.mask && dir === 'side') g.rect(8, 7, 5, 2, o.mask);
      break;
    case 'cap':
      g.oval(3, 0, 10, 5, c).rect(3, 3, 10, 1, a);
      if (dir === 'down') g.rect(4, 4, 8, 1, darken(c, 0.3));
      if (dir === 'side') g.rect(10, 3, 4, 1, darken(c, 0.3));
      break;
    case 'crown':
    case 'circlet':
      g.rect(4, 1, 8, 1, P.gold).px(4, 0, P.gold).px(8, 0, P.gold).px(11, 0, P.gold);
      break;
    case 'miner':
      g.oval(3, 0, 10, 5, c).rect(2, 3, 12, 1, darken(c, 0.3));
      if (dir !== 'up') g.rect(dir === 'side' ? 10 : 7, 1, 2, 2, P.yellow);
      break;
    case 'fur':
      g.oval(2, 0, 12, 5, c).rect(2, 3, 12, 2, lighten(c, 0.3));
      break;
    case 'bandana':
      g.rect(3, 2, 10, 2, c);
      break;
    case 'straw':
      g.rect(0, 3, 16, 2, P.tan).oval(4, 0, 8, 4, P.sand).rect(4, 3, 8, 1, P.red);
      break;
    default:
      break;
  }
  // Items carried.
  if (o.pack) g.rect(dir === 'side' ? 1 : 3, 7, dir === 'side' ? 4 : 10, 6, o.pack).rect(dir === 'side' ? 1 : 3, 7, dir === 'side' ? 4 : 10, 1, lighten(o.pack, 0.2));
  if (dir === 'down' && o.pack) {
    // Pack straps visible from the front.
    g.px(4, 9, darken(o.pack, 0.3)).px(11, 9, darken(o.pack, 0.3));
  }
  if (o.weapon && dir !== 'up') {
    const x = dir === 'side' ? 12 : 13;
    if (o.weapon === 'staff') g.rect(x, 2, 1, 13, P.brown).oval(x - 1, 0, 3, 3, o.weaponAccent);
    else if (o.weapon === 'sword' || o.weapon === 'greatsword') g.rect(x, 6, 1, 6, o.weaponColor).px(x - 1, 11, o.weaponAccent).px(x + 1, 11, o.weaponAccent);
    else if (o.weapon === 'dagger') g.rect(x, 10, 1, 3, o.weaponColor);
    else if (o.weapon === 'spear') g.rect(x, 1, 1, 14, P.brown).px(x, 0, o.weaponColor);
  }
  if (o.weapon && dir === 'up' && (o.weapon === 'sword' || o.weapon === 'greatsword' || o.weapon === 'staff'))
    g.line(4, 13, 11, 6, o.weapon === 'staff' ? P.brown : o.weaponColor);
  if (o.shield && dir === 'up') g.oval(4, 8, 8, 6, o.shield).px(8, 10, o.trim);
  if (o.shield && dir === 'side') g.oval(1, 8, 5, 6, o.shield);
  g.outline(P.outline);
  return g;
}

// Look descriptions for heroes and map NPCs.
export const HeroLooks = [
  { hair: P.brown, hairStyle: 'short', hat: 'helmet', hatColor: P.steel, hatAccent: P.red, torso: P.wine, armor: P.silver,
    trim: P.gold, legs: P.ink, boots: P.brown, shield: P.navy, weapon: 'greatsword', weaponColor: P.silver, weaponAccent: P.gold, bulk: 0 },
  { hair: P.silver, hairStyle: 'long', hat: 'wizard', hatColor: P.navy, hatAccent: P.gold, torso: P.navy, robe: '#2a5aa8',
    trim: P.cyan, legs: P.navy, boots: P.plum, weapon: 'staff', weaponColor: P.cyan, weaponAccent: P.cyan },
  { hair: P.black, hairStyle: 'short', hat: 'hood', hatColor: '#45456a', torso: '#34344e', trim: P.red, legs: '#2a2a40',
    boots: P.black, scarf: P.red, mask: '#2a2a40', weapon: 'dagger', offhand: 'dagger', weaponColor: P.silver, weaponAccent: P.red },
];

export const NpcLooks = {
  merchant: { hair: P.tan, hat: 'cap', hatColor: P.forest, hatAccent: P.gold, torso: P.clay, trim: P.sand, legs: P.brown,
    boots: P.plum, pack: P.brown, weapon: null, beard: P.tan },
  // The wandering merchant met on two floors per chapter.
  wanderer: { hair: P.silver, hat: 'straw', torso: P.violet, trim: P.gold, legs: P.ink, boots: P.brown, pack: P.clay,
    weapon: null, beard: P.silver },
  allies: [
    { hat: 'hood', hatColor: P.forest, torso: P.pine, trim: P.green, legs: P.brown, weapon: 'dagger', hair: P.brown },
    { hat: 'straw', hair: P.brown, hairStyle: 'long', torso: P.green, trim: P.sand, legs: P.brown, pack: P.tan, weapon: null },
    { hair: P.silver, hairStyle: 'bald', torso: P.brown, robe: '#6a4a36', trim: P.gold, weapon: 'staff', weaponAccent: P.cyan, beard: P.silver },
    { hat: 'miner', hatColor: P.gold, torso: P.slate, trim: P.orange, legs: P.ink, weapon: null, hair: P.black, beard: P.brown },
    { hat: 'fur', hatColor: P.brown, torso: P.navy, trim: P.silver, legs: P.ink, weapon: 'spear', hair: P.brown },
    { hair: P.black, hairStyle: 'bald', torso: P.rust, armor: P.brown, trim: P.gold, legs: P.plum, weapon: 'hammer', beard: P.black },
    { hair: P.white, hairStyle: 'long', torso: P.slate, cape: P.ink, trim: P.silver, legs: P.ink, weapon: null },
    { hat: 'wizard', hatColor: P.violet, hatAccent: P.yellow, torso: P.violet, robe: P.violet, trim: P.yellow, weapon: 'staff', weaponAccent: P.yellow },
    { hat: 'circlet', hair: P.yellow, hairStyle: 'long', torso: P.white, cape: P.red, trim: P.gold, legs: P.silver, weapon: null },
    { hat: 'helmet', hatColor: P.blue, hatAccent: P.cyan, torso: P.navy, armor: P.steel, trim: P.cyan, legs: P.ink, weapon: 'sword', shield: P.blue },
  ],
};
