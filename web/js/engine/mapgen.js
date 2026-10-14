// Procedural floor layouts: an entrance camp on the left, a chain of rooms,
// and the guardian's room with the stairs on the right. Layouts use their
// own RNG so they never disturb the rules' random stream.
//
// Ordinary floors are compact: one or two rooms between the camp and the
// guardian. Fork floors (rules/exploration.js branches()) are larger: a hall,
// then one lane per branch into the guardian's room. The patrol lane runs
// straight across the middle; the fog and key lanes loop over and under the
// middle through a room of their own. Lanes are one tile wide, so a barrier
// at each end (layout.lanes[].entry/exit) closes them; the scene opens the
// lane of the branch taken.
//
// Exploration spots (from rules/exploration.js generate()) are placed by zone:
// middle rooms, the guardian's room, nooks behind cracked walls (hidden),
// locked side rooms (vaults) and the fog lane's room (branch). Nooks and
// vaults are cut into the wall above a room or corridor and stay concealed
// until their crack or door is opened. A shortcut is the fog lane's exit.
import { VisualRng } from '../rules/random.js';
import { hash2 } from './tiles.js';

export const WALL = 0;
export const FLOOR = 1;
// Mirror Exploration.Zone, Exploration.Branch and SpotKind.Shortcut.
const Zone = { Middle: 0, Guardian: 1, Hidden: 2, Vault: 3, Branch: 4 };
export const Branch = { None: 0, Patrol: 1, Fog: 2, Locked: 3 };
const ShortcutKind = 13;
const Steps = [[1, 0], [-1, 0], [0, 1], [0, -1]];

// Rooms and lanes of a fork floor; lanes get their tiles once the map exists.
function forkRooms(rng, H, fork) {
  const midY = Math.floor(H / 2);
  const entrance = { x: 2, y: midY - 3, w: 9, h: 7, kind: 'entrance' };
  const hall = { x: entrance.x + entrance.w + 3, y: midY - 4, w: 9 + rng.int(2), h: 9, kind: 'middle' };
  const span = 16 + rng.int(3);
  const guardianRoom = { x: hall.x + hall.w + span, y: midY - 4, w: 9, h: 9, kind: 'guardian' };
  const sides = rng.chance(0.5) ? ['top', 'bottom'] : ['bottom', 'top'];
  let next = 0;
  const lanes = fork.map((branch) => {
    const lane = { branch, side: branch === Branch.Patrol ? 'middle' : sides[next++], room: null };
    if (lane.side !== 'middle') {
      const w = 7;
      const h = 5;
      lane.room = { x: hall.x + hall.w + Math.floor((span - w) / 2), y: lane.side === 'top' ? 2 : H - 2 - h, w, h,
        kind: branch === Branch.Fog ? 'fog' : 'locked' };
    }
    return lane;
  });
  const rooms = [entrance, hall, ...lanes.filter((l) => l.room).map((l) => l.room), guardianRoom];
  return { rooms, lanes, hall, guardianRoom, midY };
}

export function generateLayout({ seed, floor, boss, propCount, spots = [], fork = [] }) {
  const rng = new VisualRng((seed * 2654435761) ^ (floor * 40503) ^ 0x5bd1e995);
  const forked = fork.length > 0 && !boss;
  const H = boss ? 18 : forked ? 30 : 20;
  let rooms;
  let lanes = [];
  let plan = null;
  if (forked) {
    plan = forkRooms(rng, H, fork);
    ({ rooms, lanes } = plan);
  } else {
    rooms = [];
    const entrance = { x: 2, y: 3 + rng.int(H - 13), w: 9, h: 7, kind: 'entrance' };
    rooms.push(entrance);
    // Nothing to choose between on an ordinary floor, so the walk stays short:
    // one or two rooms, close together, low enough for nooks in their top wall.
    const gap = boss ? 3 : 2;
    let cursor = entrance.x + entrance.w + gap + rng.int(2);
    const middle = boss ? 1 : 1 + rng.int(2);
    for (let i = 0; i < middle; i++) {
      const w = (middle === 1 && !boss ? 7 : 5) + rng.int(4);
      const h = 5 + rng.int(3);
      const y = boss ? 2 + rng.int(H - h - 4) : 4 + rng.int(Math.max(1, H - h - 6));
      rooms.push({ x: cursor, y, w, h, kind: 'middle' });
      cursor += w + gap + rng.int(2);
    }
    const gw = boss ? 12 : 9;
    const gh = boss ? 11 : 8;
    rooms.push({ x: cursor, y: 2 + rng.int(Math.max(1, H - gh - 4)), w: gw, h: gh, kind: 'guardian' });
  }
  const entrance = rooms[0];
  const guardianRoom = rooms[rooms.length - 1];
  const W = guardianRoom.x + guardianRoom.w + 3;
  const tiles = new Uint8Array(W * H);
  const at = (x, y) => (x < 0 || y < 0 || x >= W || y >= H ? WALL : tiles[y * W + x]);
  const carve = (x, y) => {
    if (x > 0 && y > 1 && x < W - 1 && y < H - 1) tiles[y * W + x] = FLOOR;
  };
  for (const r of rooms) for (let y = r.y; y < r.y + r.h; y++) for (let x = r.x; x < r.x + r.w; x++) carve(x, y);
  const center = (r) => ({ x: r.x + Math.floor(r.w / 2), y: r.y + Math.floor(r.h / 2) });
  const chain = forked ? [rooms[0], rooms[1]] : rooms;
  for (let i = 0; i + 1 < chain.length; i++) {
    const a = center(chain[i]);
    const b = center(chain[i + 1]);
    const midX = chain[i].x + chain[i].w + 1;
    for (let x = a.x; x <= midX; x++) { carve(x, a.y); carve(x, a.y + 1); }
    const [y0, y1] = a.y < b.y ? [a.y, b.y] : [b.y, a.y];
    for (let y = y0; y <= y1 + 1; y++) { carve(midX, y); carve(midX + 1, y); }
    for (let x = midX; x <= b.x; x++) { carve(x, b.y); carve(x, b.y + 1); }
  }
  if (forked) carveLanes(plan, carve);
  // Decorative alcoves; fork floors have lanes instead, which alcoves could breach.
  for (const r of forked ? [] : rooms.slice(1, -1)) {
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
    lanes,
    // The fork's signpost stands in the hall, facing the lanes' doorways.
    forkSign: forked ? { x: plan.hall.x + plan.hall.w - 3, y: plan.midY - 1 } : null,
  };
  // Stall occupies two tiles of floor in the entrance room.
  const reserved = new Set();
  const key = (x, y) => y * W + x;
  const reserve = (p, radius = 1) => {
    for (let dy = -radius; dy <= radius; dy++) for (let dx = -radius; dx <= radius; dx++) reserved.add(key(p.x + dx, p.y + dy));
  };
  for (const p of [layout.start, layout.waystone, layout.sign, layout.campfire, layout.merchant, layout.ally,
    layout.device, layout.guardian, layout.stairs]) reserve(p);
  // Lane ends and the tiles in front of them stay clear, and so does the row
  // a lane crosses its room on.
  for (const lane of lanes) {
    for (const p of [lane.entry, lane.exit, lane.entryFront, lane.exitFront]) reserved.add(key(p.x, p.y));
    if (lane.room) for (let x = lane.room.x; x < lane.room.x + lane.room.w; x++) reserved.add(key(x, lane.room.y + 2));
    for (const p of lane.tiles) reserved.add(key(p.x, p.y));
  }
  if (layout.forkSign) reserve(layout.forkSign, 0);
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
  if (layout.forkSign) blocking.add(key(layout.forkSign.x, layout.forkSign.y));
  // Things the player must be able to walk up to, and tiles they must reach.
  // Those marked late lie past a fork. Everything else must be reachable from
  // the entrance with every lane closed; late things must be reachable from
  // the stairs with only the lanes' hall ends closed, which also keeps the
  // guardian's room in one piece for whichever lane leads into it (lanes and
  // the row each crosses its room on stay clear, so lanes cannot break).
  const stairsFront = { x: layout.stairs.x - 1, y: layout.stairs.y, late: true };
  const mustNear = [{ ...layout.stairs, late: true }, { ...layout.guardian, late: true }, layout.merchant, layout.ally,
    layout.campfire, layout.waystone];
  const mustReach = [stairsFront];
  for (const lane of lanes) mustReach.push(lane.entryFront, { ...lane.exitFront, late: true });
  if (layout.forkSign) mustNear.push(layout.forkSign);
  const entries = new Set(lanes.map((lane) => key(lane.entry.x, lane.entry.y)));
  const exits = new Set(lanes.map((lane) => key(lane.exit.x, lane.exit.y)));
  const flood = (from, blocked, closed) => {
    const seen = new Set([key(from.x, from.y)]);
    const queue = [from];
    while (queue.length) {
      const p = queue.shift();
      for (const [dx, dy] of Steps) {
        const n = { x: p.x + dx, y: p.y + dy };
        const k = key(n.x, n.y);
        if (seen.has(k) || at(n.x, n.y) !== FLOOR || blocked.has(k) || closed.has(k)) continue;
        seen.add(k);
        queue.push(n);
      }
    }
    return seen;
  };
  const reachable = (blocked) => {
    const early = flood(layout.start, blocked, lanes.length ? new Set([...entries, ...exits]) : entries);
    const late = lanes.length ? flood(stairsFront, blocked, entries) : early;
    const seen = (p) => (p.late ? late : early);
    const near = (p) => Steps.some(([dx, dy]) => seen(p).has(key(p.x + dx, p.y + dy)));
    return mustNear.every(near) && mustReach.every((p) => seen(p).has(key(p.x, p.y)));
  };
  placeSpots({ layout, rooms, spots, rng, at, key, carve, reserved, blocking, mustNear, mustReach, reachable, forked });
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

// One-tile lanes. Each lane records its tiles, its two ends (entry on the
// hall's side, exit on the guardian's), the floor tile in front of each end
// and, for the fog lane, the tiles the fog fills (the way in, up to its room).
function carveLanes({ lanes, hall, guardianRoom, midY }, carve) {
  const path = (points) => {
    const out = [];
    for (let i = 0; i + 1 < points.length; i++) {
      const a = points[i];
      const b = points[i + 1];
      const dx = Math.sign(b.x - a.x);
      const dy = Math.sign(b.y - a.y);
      for (let x = a.x, y = a.y; ; x += dx, y += dy) {
        if (i === 0 || x !== a.x || y !== a.y) {
          carve(x, y);
          out.push({ x, y });
        }
        if (x === b.x && y === b.y) break;
      }
    }
    return out;
  };
  const hallEdge = hall.x + hall.w;
  for (const lane of lanes) {
    if (lane.side === 'middle') {
      lane.tiles = path([{ x: hallEdge, y: midY }, { x: guardianRoom.x - 1, y: midY }]);
      // The patrol stands just past the hall's doorway.
      lane.entry = { x: hallEdge + 1, y: midY };
      lane.entryFront = { x: hallEdge, y: midY };
      lane.exit = { x: guardianRoom.x - 1, y: midY };
      lane.exitFront = { x: guardianRoom.x, y: midY };
      lane.fogTiles = [];
      continue;
    }
    // Out of the hall's right wall beside the middle lane, then over or under it.
    const top = lane.side === 'top';
    const room = lane.room;
    const row = room.y + 2;
    const doorY = top ? midY - 3 : midY + 3;
    const outX = guardianRoom.x + 2;
    const hallDoor = { x: hallEdge, y: doorY };
    const guardDoor = { x: outX, y: top ? guardianRoom.y - 1 : guardianRoom.y + guardianRoom.h };
    const way = path([hallDoor, { x: hallEdge + 1, y: doorY }, { x: hallEdge + 1, y: row }, { x: room.x - 1, y: row }]);
    const back = path([{ x: room.x + room.w, y: row }, { x: outX, y: row }, guardDoor]);
    lane.tiles = [...way, ...back];
    lane.entry = hallDoor;
    lane.entryFront = { x: hallEdge - 1, y: doorY };
    lane.exit = guardDoor;
    lane.exitFront = { x: outX, y: top ? guardianRoom.y : guardianRoom.y + guardianRoom.h - 1 };
    lane.fogTiles = lane.branch === Branch.Fog ? way.slice(1) : [];
  }
}

function shuffle(list, rng) {
  for (let i = list.length - 1; i > 0; i--) {
    const j = rng.int(i + 1);
    [list[i], list[j]] = [list[j], list[i]];
  }
  return list;
}

// Places exploration spots. Fills layout.spots (one entry per spot index:
// { index, x, y, crack, vault, fallback, shortcut }), layout.cracks ({ x, y,
// cells, spots }: a cracked wall at (x, y) hiding a nook), layout.vaults
// ({ x, y, cells, spot }: a locked door at (x, y)) and layout.concealed (keys
// of nook and vault cells, drawn as wall until opened). Cracks and doors are
// wall tiles; the scene turns them into floor when they open. A spot whose
// zone has no room left falls back to an ordinary room tile (fallback: true).
// On fork floors everything that must be reachable before a branch is taken
// stays on the hall's side, and a shortcut takes the fog lane's exit.
function placeSpots({ layout, rooms, spots, rng, at, key, carve, reserved, blocking, mustNear, mustReach, reachable, forked }) {
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
  const laneTiles = new Set(layout.lanes.flatMap((lane) => lane.tiles.map((p) => key(p.x, p.y))));
  const regionOf = (face) => (laneTiles.has(key(face.x, face.y + 1)) ? 'lane' : roomAt(face.x, face.y + 1)?.kind ?? 'corridor');
  // Past a fork only the guardian's own things; the rest stays before it.
  const before = ['middle', 'corridor', 'entrance'];
  const preferred = (order) => [...faces].filter((face) => !forked || order.includes(regionOf(face))).sort((a, b) => {
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
  // Pools of room kinds tried in turn; null means any room. Fork floors never
  // move a spot across the fork.
  const placeFree = (kinds, near = null) => {
    const late = forked && (kinds.includes('guardian') || kinds.includes('fog'));
    const pools = !forked ? [kinds, ['middle', 'guardian', 'entrance'], null]
      : kinds.includes('middle') ? [kinds, before] : [kinds];
    for (const pool of pools) {
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
        mustNear.push({ ...t, late });
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
  const rank = (zone) => [Zone.Vault, Zone.Hidden, Zone.Guardian, Zone.Branch, Zone.Middle].indexOf(zone);
  order.sort((a, b) => rank(a.zone) - rank(b.zone) || a.index - b.index);
  const fogLane = layout.lanes.find((lane) => lane.branch === Branch.Fog);
  for (const spot of order) {
    let placed = null;
    if (spot.kind === ShortcutKind && fogLane) {
      // The crack or the raised bridge closes the fog lane's far end.
      placed = { ...fogLane.exit, crack: -1, vault: false, fallback: false, shortcut: true };
      fogLane.exitSpot = spot.index;
    } else if (spot.zone === Zone.Vault) {
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
      for (const face of preferred(forked ? before : ['middle', 'corridor', 'guardian', 'entrance'])) {
        const cells = cut(face, 0, 1);
        if (!cells) continue;
        layout.cracks.push({ x: face.x, y: face.y, cells, spots: [spot.index] });
        placed = { x: face.x, y: face.y - 1, crack: layout.cracks.length - 1, vault: false, fallback: false };
        break;
      }
    }
    if (!placed) {
      // Affix sources stand beside the guardian they empower; branch spots
      // fill the fog lane's room.
      const tile = spot.zone === Zone.Guardian ? placeFree(['guardian'], layout.guardian)
        : spot.zone === Zone.Branch && fogLane ? placeFree(['fog']) : placeFree(['middle']);
      if (tile) placed = { ...tile, crack: -1, vault: false, fallback: spot.zone === Zone.Hidden || spot.zone === Zone.Vault };
    }
    if (placed) layout.spots[spot.index] = { index: spot.index, ...placed };
  }
}
