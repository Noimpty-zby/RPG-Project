// Map layouts with exploration spots: every spot gets a tile, can be walked
// up to (after breaking its crack), and nooks stay sealed until opened.
//   node web/tests/layout_test.mjs
import { FLOOR, generateLayout } from '../js/engine/mapgen.js';
import * as Exploration from '../js/rules/exploration.js';
import { next, seed as seedRandom } from '../js/rules/random.js';

let failures = 0;
function check(condition, message) {
  if (!condition) {
    failures++;
    if (failures <= 20) console.error(`FAIL: ${message}`);
  }
}
const Steps = [[1, 0], [-1, 0], [0, 1], [0, -1]];

// Tiles reachable from the entrance, walking around solid things.
function walk(layout, blocked) {
  const seen = new Set([layout.key(layout.start.x, layout.start.y)]);
  const queue = [layout.start];
  while (queue.length) {
    const p = queue.shift();
    for (const [dx, dy] of Steps) {
      const x = p.x + dx;
      const y = p.y + dy;
      const k = layout.key(x, y);
      if (seen.has(k) || layout.at(x, y) !== FLOOR || blocked.has(k)) continue;
      seen.add(k);
      queue.push({ x, y });
    }
  }
  return seen;
}

let spotsTotal = 0;
let fallbacks = 0;
for (let seedValue = 1; seedValue <= 60; seedValue++) {
  for (let floor = 1; floor <= 100; floor++) {
    const spots = Exploration.generate(seedValue, floor);
    const args = { seed: seedValue, floor, boss: floor % 10 === 0, propCount: 5, spots };
    const layout = generateLayout(args);
    const where = `seed ${seedValue} floor ${floor}`;
    const again = generateLayout(args);
    check(JSON.stringify(again.spots) === JSON.stringify(layout.spots), `${where}: layout is not deterministic`);
    const key = layout.key;
    const fixed = [layout.start, layout.waystone, layout.sign, layout.campfire, layout.merchant, layout.ally,
      layout.guardian, layout.stairs, layout.stall, { x: layout.stall.x + 1, y: layout.stall.y }].map((p) => key(p.x, p.y));
    const used = new Set();
    // Solid things on the map: the entrance camp, props and room spots.
    const blocked = new Set(layout.blocking);
    spots.forEach((spot, index) => {
      spotsTotal++;
      const place = layout.spots[index];
      check(place, `${where}: spot ${index} has no tile`);
      if (!place) return;
      if (place.fallback) fallbacks++;
      const k = key(place.x, place.y);
      check(!used.has(k), `${where}: two spots share ${place.x},${place.y}`);
      check(!fixed.includes(k), `${where}: spot ${index} sits on a landmark`);
      used.add(k);
      if (place.vault) check(spot.zone === Exploration.Zone.Vault, `${where}: spot ${index} placed as a vault`);
      if (place.crack >= 0) check(spot.zone === Exploration.Zone.Hidden, `${where}: spot ${index} placed in a nook`);
    });
    // Sealed: crack and door approaches are reachable, nooks and vaults are not.
    let reach = walk(layout, blocked);
    for (const opening of [...layout.cracks, ...layout.vaults]) {
      check(layout.at(opening.x, opening.y) !== FLOOR, `${where}: crack or door at ${opening.x},${opening.y} is open`);
      check(reach.has(key(opening.x, opening.y + 1)), `${where}: cannot reach the wall at ${opening.x},${opening.y}`);
      for (const [x, y] of opening.cells) {
        check(!reach.has(key(x, y)), `${where}: concealed cell ${x},${y} is reachable`);
        check(layout.concealed.has(key(x, y)), `${where}: cell ${x},${y} is not concealed`);
      }
    }
    for (const p of [layout.guardian, layout.campfire, layout.waystone, layout.merchant])
      check(Steps.some(([dx, dy]) => reach.has(key(p.x + dx, p.y + dy))), `${where}: landmark at ${p.x},${p.y} is cut off`);
    check(reach.has(key(layout.stairs.x - 1, layout.stairs.y)), `${where}: stairs are cut off`);
    // Opened: every spot can be walked up to.
    for (const opening of [...layout.cracks, ...layout.vaults]) layout.setFloor(opening.x, opening.y);
    reach = walk(layout, blocked);
    spots.forEach((spot, index) => {
      const place = layout.spots[index];
      if (!place) return;
      check(Steps.some(([dx, dy]) => reach.has(key(place.x + dx, place.y + dy))), `${where}: spot ${index} cannot be reached`);
    });
  }
}
check(fallbacks / spotsTotal < 0.01, `too many spots fell back to plain rooms: ${fallbacks}/${spotsTotal}`);

// Layouts draw from their own generator, never the rules stream.
const sample = Exploration.generate(7, 15);
seedRandom(4242);
const expected = [next(), next()];
seedRandom(4242);
generateLayout({ seed: 7, floor: 15, boss: false, propCount: 5, spots: sample });
check(next() === expected[0] && next() === expected[1], 'generateLayout consumed the rules random stream');

if (failures) {
  console.error(`${failures} layout check(s) failed`);
  process.exit(1);
}
console.log(`Layout tests passed: ${spotsTotal} spots on 6000 floors placed and reachable, nooks and vaults sealed until opened, ` +
  `${fallbacks} fell back to plain rooms, layouts deterministic and off the rules stream.`);
