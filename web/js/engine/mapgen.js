// Procedural floor layouts: an entrance camp on the left, a chain of rooms,
// and the guardian's room with the stairs on the right. Layouts use their
// own RNG so they never disturb the rules' random stream.
import { VisualRng } from '../rules/random.js';
import { hash2 } from './tiles.js';

export const WALL = 0;
export const FLOOR = 1;

export function generateLayout({ seed, floor, boss, propCount }) {
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
  const reachable = (blocked) => {
    const seen = new Set([key(layout.start.x, layout.start.y)]);
    const queue = [layout.start];
    while (queue.length) {
      const p = queue.shift();
      for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const n = { x: p.x + dx, y: p.y + dy };
        const k = key(n.x, n.y);
        if (seen.has(k) || at(n.x, n.y) !== FLOOR || blocked.has(k)) continue;
        seen.add(k);
        queue.push(n);
      }
    }
    const near = (p) => [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([dx, dy]) => seen.has(key(p.x + dx, p.y + dy)));
    return near(layout.stairs) && near(layout.guardian) && near(layout.merchant) && near(layout.ally) && near(layout.campfire) &&
      near(layout.waystone) && seen.has(key(layout.stairs.x - 1, layout.stairs.y));
  };
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
  layout.variantAt = (x, y) => {
    const v = hash2(x, y, seed + floor * 7);
    return v < 0.08 ? 3 : Math.floor(v * 37) % 3;
  };
  return layout;
}
