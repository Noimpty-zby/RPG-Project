// Procedural floor layouts: an entrance camp on the left, a chain of rooms,
// and the guardian's room with the stairs on the right. Layouts use their
// own RNG so they never disturb the rules' random stream.
//
// Exploration spots (from rules/exploration.js generate()) are placed by zone:
// middle rooms, the guardian's room, nooks behind cracked walls (hidden) and
// locked side rooms (vaults). Nooks and vaults are cut into the wall above a
// room or corridor and stay concealed until their crack or door is opened.
import { VisualRng } from '../rules/random.js';
import { hash2 } from './tiles.js';

export const WALL = 0;
export const FLOOR = 1;
// Mirrors Exploration.Zone.
const Zone = { Middle: 0, Guardian: 1, Hidden: 2, Vault: 3 };
const Steps = [[1, 0], [-1, 0], [0, 1], [0, -1]];

export function generateLayout({ seed, floor, boss, propCount, spots = [] }) {
  const rng = new VisualRng((seed * 2654435761) ^ (floor * 40503) ^ 0x5bd1e995);
  const H = boss ? 18 : 20;
  const rooms = [];
  const entrance = { x: 2, y: 3 + rng.int(H - 13), w: 9, h: 7, kind: 'entrance' };
  rooms.push(entrance);
  let cursor = entrance.x + entrance.w + 3 + rng.int(2);
  const middle = boss ? 1 : 2 + rng.int(2);
  for (let i = 0; i < middle; i++) {
    const w = 5 + rng.int(4);
    const h = 5 + rng.int(3);
    rooms.push({ x: cursor, y: 2 + rng.int(H - h - 4), w, h, kind: 'middle' });
    cursor += w + 3 + rng.int(2);
  }
  const gw = boss ? 12 : 9;
  const gh = boss ? 11 : 8;
  const guardianRoom = { x: cursor, y: 2 + rng.int(Math.max(1, H - gh - 4)), w: gw, h: gh, kind: 'guardian' };
  rooms.push(guardianRoom);
  const W = guardianRoom.x + guardianRoom.w + 3;
  const tiles = new Uint8Array(W * H);
  const at = (x, y) => (x < 0 || y < 0 || x >= W || y >= H ? WALL : tiles[y * W + x]);
  const carve = (x, y) => {
    if (x > 0 && y > 1 && x < W - 1 && y < H - 1) tiles[y * W + x] = FLOOR;
  };
  for (const r of rooms) for (let y = r.y; y < r.y + r.h; y++) for (let x = r.x; x < r.x + r.w; x++) carve(x, y);
  const center = (r) => ({ x: r.x + Math.floor(r.w / 2), y: r.y + Math.floor(r.h / 2) });
  for (let i = 0; i + 1 < rooms.length; i++) {
    const a = center(rooms[i]);
    const b = center(rooms[i + 1]);
    const midX = rooms[i].x + rooms[i].w + 1;
    for (let x = a.x; x <= midX; x++) { carve(x, a.y); carve(x, a.y + 1); }
    const [y0, y1] = a.y < b.y ? [a.y, b.y] : [b.y, a.y];
    for (let y = y0; y <= y1 + 1; y++) { carve(midX, y); carve(midX + 1, y); }
    for (let x = midX; x <= b.x; x++) { carve(x, b.y); carve(x, b.y + 1); }
  }
  // Decorative alcoves.
  for (const r of rooms.slice(1, -1)) {
    if (!rng.chance(0.6)) continue;
    const up = rng.chance(0.5);
    const ax = r.x + 1 + rng.int(Math.max(1, r.w - 4));
    const ay = up ? r.y - 3 : r.y + r.h;
    for (let y = ay; y < ay + 3; y++) for (let x = ax; x < ax + 3; x++) carve(x, y);
  }

  const e = center(entrance);
  const g = center(guardianRoom);
  const layout = {
    w: W, h: H, tiles, rooms, theme: 0,
    start: { x: entrance.x + 1, y: e.y },
    waystone: { x: entrance.x + 1, y: entrance.y + 1 },
    sign: { x: entrance.x + 3, y: entrance.y },
    campfire: { x: entrance.x + 4, y: e.y },
    stall: { x: entrance.x + 6, y: entrance.y },
    merchant: { x: entrance.x + 7, y: entrance.y + 1 },
    ally: rooms.length > 2 ? center(rooms[1]) : { x: entrance.x + 6, y: entrance.y + entrance.h - 2 },
    device: { x: guardianRoom.x + 1, y: guardianRoom.y + 1 },
    guardian: { x: guardianRoom.x + guardianRoom.w - 4, y: g.y },
    stairs: { x: guardianRoom.x + guardianRoom.w - 2, y: g.y },
    props: [],
  };
  // Stall occupies two tiles of floor in the entrance room.
  const reserved = new Set();
  const key = (x, y) => y * W + x;
  const reserve = (p, radius = 1) => {
    for (let dy = -radius; dy <= radius; dy++) for (let dx = -radius; dx <= radius; dx++) reserved.add(key(p.x + dx, p.y + dy));
  };
  for (const p of [layout.start, layout.waystone, layout.sign, layout.campfire, layout.merchant, layout.ally,
    layout.device, layout.guardian, layout.stairs]) reserve(p);
  reserve({ x: layout.stall.x, y: layout.stall.y }, 1);
  reserve({ x: layout.stall.x + 1, y: layout.stall.y }, 1);
  // Keep the guardian's approach open.
  for (let x = guardianRoom.x; x < guardianRoom.x + guardianRoom.w; x++) { reserved.add(key(x, g.y)); reserved.add(key(x, g.y + 1)); }

  layout.solidAt = (x, y, extra = null) => {
    if (at(x, y) !== FLOOR) return true;
    if (extra && extra.has(key(x, y))) return true;
    return false;
  };
  const blocking = new Set([key(layout.stall.x, layout.stall.y), key(layout.stall.x + 1, layout.stall.y),
    key(layout.waystone.x, layout.waystone.y), key(layout.campfire.x, layout.campfire.y), key(layout.sign.x, layout.sign.y)]);
  // Things the player must be able to walk up to, and tiles they must reach.
  const mustNear = [layout.stairs, layout.guardian, layout.merchant, layout.ally, layout.campfire, layout.waystone];
  const mustReach = [{ x: layout.stairs.x - 1, y: layout.stairs.y }];
  const reachable = (blocked) => {
    const seen = new Set([key(layout.start.x, layout.start.y)]);
    const queue = [layout.start];
    while (queue.length) {
      const p = queue.shift();
      for (const [dx, dy] of Steps) {
        const n = { x: p.x + dx, y: p.y + dy };
        const k = key(n.x, n.y);
        if (seen.has(k) || at(n.x, n.y) !== FLOOR || blocked.has(k)) continue;
        seen.add(k);
        queue.push(n);
      }
    }
    const near = (p) => Steps.some(([dx, dy]) => seen.has(key(p.x + dx, p.y + dy)));
    return mustNear.every(near) && mustReach.every((p) => seen.has(key(p.x, p.y)));
  };
  placeSpots({ layout, rooms, spots, rng, at, key, carve, reserved, blocking, mustNear, mustReach, reachable });
  // Props: prefer spots along room walls, never on reserved tiles.
  const candidates = [];
  for (const r of rooms) {
    for (let y = r.y; y < r.y + r.h; y++)
      for (let x = r.x; x < r.x + r.w; x++) {
        const nearWall = at(x - 1, y) === WALL || at(x + 1, y) === WALL || at(x, y - 1) === WALL || at(x, y + 1) === WALL;
        if (!reserved.has(key(x, y)) && (nearWall || rng.chance(0.12))) candidates.push({ x, y });
      }
  }
  for (let i = candidates.length - 1; i > 0; i--) {
    const j = rng.int(i + 1);
    [candidates[i], candidates[j]] = [candidates[j], candidates[i]];
  }
  const target = boss ? 6 : 10 + rng.int(6);
  for (const c of candidates) {
    if (layout.props.length >= target) break;
    const k = key(c.x, c.y);
    if (blocking.has(k)) continue;
    blocking.add(k);
    if (!reachable(blocking)) {
      blocking.delete(k);
      continue;
    }
    layout.props.push({ x: c.x, y: c.y, kind: rng.int(propCount) });
  }
  layout.blocking = blocking;
  layout.at = at;
  layout.key = key;
  // Opening a crack or vault door turns its wall tile into floor.
  layout.setFloor = (x, y) => { tiles[y * W + x] = FLOOR; };
  layout.variantAt = (x, y) => {
    const v = hash2(x, y, seed + floor * 7);
    return v < 0.08 ? 3 : Math.floor(v * 37) % 3;
  };
  return layout;
}

function shuffle(list, rng) {
  for (let i = list.length - 1; i > 0; i--) {
    const j = rng.int(i + 1);
    [list[i], list[j]] = [list[j], list[i]];
  }
  return list;
}

// Places exploration spots. Fills layout.spots (one entry per spot index:
// { index, x, y, crack, vault, fallback }), layout.cracks ({ x, y, cells,
// spots }: a cracked wall at (x, y) hiding a nook), layout.vaults ({ x, y,
// cells, spot }: a locked door at (x, y)) and layout.concealed (keys of nook
// and vault cells, drawn as wall until opened). Cracks and doors are wall
// tiles; the scene turns them into floor when they open. A spot whose zone
// has no room left falls back to an ordinary room tile (fallback: true).
function placeSpots({ layout, rooms, spots, rng, at, key, carve, reserved, blocking, mustNear, mustReach, reachable }) {
  layout.spots = [];
  layout.cracks = [];
  layout.vaults = [];
  layout.concealed = new Set();
  if (!spots.length) return;
  const { w: W, h: H } = layout;
  const claimed = new Set();
  const roomAt = (x, y) => rooms.find((r) => x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) || null;
  const solidWall = (x0, y0, x1, y1) => {
    for (let y = y0; y <= y1; y++)
      for (let x = x0; x <= x1; x++) if (at(x, y) !== WALL || claimed.has(key(x, y))) return false;
    return true;
  };
  const claim = (x0, y0, x1, y1) => {
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) claimed.add(key(x, y));
  };
  // Cuts go into a straight wall face: wall at (x, y) with floor below it and
  // below both neighbors, so the crack or door reads as part of the wall.
  const faces = [];
  for (let y = 3; y < H - 1; y++)
    for (let x = 2; x < W - 2; x++)
      if (at(x, y) === WALL && at(x - 1, y) === WALL && at(x + 1, y) === WALL &&
        at(x, y + 1) === FLOOR && at(x - 1, y + 1) === FLOOR && at(x + 1, y + 1) === FLOOR && !reserved.has(key(x, y + 1)))
        faces.push({ x, y });
  shuffle(faces, rng);
  const regionOf = (face) => roomAt(face.x, face.y + 1)?.kind ?? 'corridor';
  const preferred = (order) => [...faces].sort((a, b) => {
    const rank = (face) => { const i = order.indexOf(regionOf(face)); return i < 0 ? order.length : i; };
    return rank(a) - rank(b);
  });
  // Opens a pocket of cells above a face; returns false if it does not fit.
  const cut = (face, half, depth) => {
    const { x, y } = face;
    if (y - depth < 2 || x - half - 1 < 1 || x + half + 1 > W - 2) return false;
    if (claimed.has(key(x, y)) || !solidWall(x - half - 1, y - depth - 1, x + half + 1, y - 1)) return false;
    const approach = { x, y: y + 1 };
    if (blocking.has(key(approach.x, approach.y))) return false;
    mustReach.push(approach);
    if (!reachable(blocking)) {
      mustReach.pop();
      return false;
    }
    const cells = [];
    for (let cy = y - depth; cy < y; cy++)
      for (let cx = x - half; cx <= x + half; cx++) {
        carve(cx, cy);
        cells.push([cx, cy]);
        layout.concealed.add(key(cx, cy));
      }
    claim(x - half - 1, y - depth - 1, x + half + 1, y);
    reserved.add(key(approach.x, approach.y));
    return cells;
  };
  // An ordinary room tile: wall-side tiles first (or, with near, the tiles
  // closest to that point), never next to another spot.
  const spotTiles = new Set();
  const distance = (a, b) => Math.abs(a.x - b.x) + Math.abs(a.y - b.y);
  const placeFree = (kinds, near = null) => {
    for (const pool of [kinds, ['middle', 'guardian', 'entrance'], null]) {
      const tiles = [];
      for (let y = 2; y < H - 1; y++)
        for (let x = 1; x < W - 1; x++) {
          if (at(x, y) !== FLOOR || layout.concealed.has(key(x, y))) continue;
          if (pool && !pool.includes(roomAt(x, y)?.kind)) continue;
          tiles.push({ x, y, wall: Steps.some(([dx, dy]) => at(x + dx, y + dy) === WALL) });
        }
      shuffle(tiles, rng).sort(near ? (a, b) => distance(a, near) - distance(b, near) : (a, b) => Number(b.wall) - Number(a.wall));
      for (const t of tiles) {
        const k = key(t.x, t.y);
        if (reserved.has(k) || blocking.has(k) || Steps.some(([dx, dy]) => spotTiles.has(key(t.x + dx, t.y + dy)))) continue;
        blocking.add(k);
        mustNear.push(t);
        if (reachable(blocking)) {
          spotTiles.add(k);
          return { x: t.x, y: t.y };
        }
        blocking.delete(k);
        mustNear.pop();
      }
    }
    return null;
  };
  const order = spots.map((spot, index) => ({ ...spot, index }));
  const rank = (zone) => [Zone.Vault, Zone.Hidden, Zone.Guardian, Zone.Middle].indexOf(zone);
  order.sort((a, b) => rank(a.zone) - rank(b.zone) || a.index - b.index);
  for (const spot of order) {
    let placed = null;
    if (spot.zone === Zone.Vault) {
      // A 3x2 room, else a narrower one where the walls are tight.
      for (const [half, depth] of [[1, 2], [1, 1], [0, 2]]) {
        for (const face of preferred(['middle', 'corridor', 'guardian', 'entrance'])) {
          const cells = cut(face, half, depth);
          if (!cells) continue;
          layout.vaults.push({ x: face.x, y: face.y, cells, spot: spot.index });
          placed = { x: face.x, y: face.y, crack: -1, vault: true, fallback: false };
          break;
        }
        if (placed) break;
      }
    } else if (spot.zone === Zone.Hidden) {
      for (const face of preferred(['middle', 'corridor', 'guardian', 'entrance'])) {
        const cells = cut(face, 0, 1);
        if (!cells) continue;
        layout.cracks.push({ x: face.x, y: face.y, cells, spots: [spot.index] });
        placed = { x: face.x, y: face.y - 1, crack: layout.cracks.length - 1, vault: false, fallback: false };
        break;
      }
    }
    if (!placed) {
      // Affix sources stand beside the guardian they empower.
      const tile = spot.zone === Zone.Guardian ? placeFree(['guardian'], layout.guardian) : placeFree(['middle']);
      if (tile) placed = { ...tile, crack: -1, vault: false, fallback: spot.zone === Zone.Hidden || spot.zone === Zone.Vault };
    }
    if (placed) layout.spots[spot.index] = { index: spot.index, ...placed };
  }
}
