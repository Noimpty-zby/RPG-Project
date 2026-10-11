// Port of src/core/run_state.h. Index 0 of floor arrays is unused.
// Routes: 0 undecided, 1 sabotage, 2 rescue, 3 supply.
export function newRunState() {
  return {
    seed: 1,
    challenge: 0, // 0 standard, 1 scarce supplies, 2 no revives
    legacyRules: false,
    lootTarget: 1, // Weapon=1 or Armor=2
    pity: 0,
    materials: 0,
    routes: new Array(10).fill(0),
    events: new Array(10).fill(0),
    campRested: new Array(10).fill(0),
    ordersClaimed: new Array(10).fill(0),
    shopRefreshes: new Array(10).fill(0),
    shopPurchased: new Array(10).fill(0),
    cleared: new Array(101).fill(0),
    attempts: new Array(101).fill(0),
    battles: 0,
    turns: 0,
    potionsUsed: 0,
    deaths: 0,
    damageTaken: 0,
    usefulDrops: 0,
    gearDrought: 0,
    longestGearDrought: 0,
    // Exploration (see exploration.js). explored: per floor, bits 0..15 mark
    // spots found and bits 16..18 merchant goods sold.
    explored: new Array(101).fill(0),
    keys: 0,
    ingredients: new Array(5).fill(0), // Fish, herb, ore, frost crystal, stardust.
    boon: 0, // Next encounter's blessing, 0 none.
    bane: 0, // Next encounter's curse, 0 none.
    questItems: new Array(10).fill(0), // Ally quest items found per chapter, 0..3.
    spotsExplored: 0,
    vaultsOpened: 0,
    puzzlesSolved: 0,
    mealsCooked: 0,
  };
}
