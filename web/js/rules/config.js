// Port of src/core/game_config.h and src/core/progression.h.
// Integer helpers reproduce C++ int semantics (truncating division).

export const clamp = (value, low, high) => Math.min(Math.max(value, low), high);
export const idiv = (a, b) => Math.trunc(a / b);

export const MaxFloor = 100;
export const FloorsPerChapter = 10;
export const MaxMaterials = 9999;
export const MaxCounter = 100000000;
export const MaxCoins = 1000000000;
export const MaxLevel = 100;
export const MaxRestsPerChapter = 3;
export const MaxEnhanceLevel = 5;
export const EnhancePercent = 5;
export const IntMax = 2147483647;

export const floorOf = (value) => clamp(value, 1, MaxFloor);
export const chapterOf = (value) => idiv(floorOf(value) - 1, FloorsPerChapter);
export const refreshCost = (floor, refreshes) =>
  (40 + chapterOf(floor) * 25) * (1 + clamp(refreshes, 0, 20));
export const restCost = (floor) => 60 + chapterOf(floor) * 40;
export const repairCostPerPoint = (price, tier) =>
  Math.max(1, idiv(Math.max(0, price) * (clamp(tier, 0, 3) + 2), 80));
export const enhanceCost = (price, nextLevel) =>
  Math.min(Math.max(0, price) * clamp(nextLevel, 1, MaxEnhanceLevel), MaxCoins);
export const repeatRewardPercent = (attempts) => (attempts <= 1 ? 30 : attempts <= 3 ? 10 : 0);
export const challengeName = (challenge) =>
  challenge === 1 ? '补给匮乏' : challenge === 2 ? '无复苏' : '标准';
export const addCounter = (value, increment) => clamp(value + increment, 0, MaxCounter);

// Progression: growth unlocks follow the highest floor reached.
export function skillRankCap(highestFloor) {
  if (highestFloor >= 85) return 5;
  if (highestFloor >= 65) return 4;
  if (highestFloor >= 40) return 3;
  if (highestFloor >= 20) return 2;
  return 1;
}
// The highest floor that raises the cap to this rank (1 for rank 1).
export function skillRankFloor(rank) {
  return [1, 20, 40, 65, 85][clamp(rank, 1, 5) - 1];
}
export function skillUnlockFloor(slot) {
  return [1, 3, 6, 10, 15][clamp(slot, 1, 5) - 1];
}
// Quality: 0 common, 1 fine, 2 rare, 3 epic.
export function equipmentTier(floor, roll0to99) {
  const roll = clamp(roll0to99, 0, 99);
  if (floor < 20) return roll < 80 ? 0 : 1;
  if (floor < 45) return roll < 50 ? 0 : roll < 90 ? 1 : 2;
  if (floor < 70) return roll < 15 ? 0 : roll < 65 ? 1 : 2;
  if (floor < 85) return roll < 45 ? 1 : roll < 90 ? 2 : 3;
  return roll < 20 ? 1 : roll < 70 ? 2 : 3;
}
