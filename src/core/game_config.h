#pragma once
#include <algorithm>

namespace GameConfig {
constexpr int MaxFloor = 100;
constexpr int FloorsPerChapter = 10;
constexpr int MaxMaterials = 9999;
constexpr int MaxCounter = 100000000;
constexpr int MaxCoins = 1000000000; // Matches save validation, including legacy balances.
constexpr int MaxLevel = 100;
constexpr int MaxRestsPerChapter = 3; // Scarce supplies allows one.
constexpr int MaxEnhanceLevel = 5;
constexpr int EnhancePercent = 5; // Each level adds this share of the current value.
inline int floor(int value) { return std::clamp(value, 1, MaxFloor); }
inline int chapter(int value) { return (floor(value) - 1) / FloorsPerChapter; }
inline int refreshCost(int value, int refreshes) {
  return (40 + chapter(value) * 25) * (1 + std::clamp(refreshes, 0, 20));
}
inline int restCost(int value) { return 60 + chapter(value) * 40; }
// Repairing costs more per point for valuable gear, so only good items are worth keeping alive.
inline int repairCostPerPoint(int price, int tier) {
  return std::max(1, static_cast<int>(static_cast<long long>(std::max(0, price)) * (std::clamp(tier, 0, 3) + 2) / 80));
}
// Enhancing to level n costs the item's price times n: 1x, 2x, ... 5x.
inline int enhanceCost(int price, int nextLevel) {
  return static_cast<int>(std::min(static_cast<long long>(std::max(0, price)) * std::clamp(nextLevel, 1, MaxEnhanceLevel),
                                   static_cast<long long>(MaxCoins)));
}
inline int repeatRewardPercent(int attempts) {
  return attempts <= 1 ? 30 : (attempts <= 3 ? 10 : 0);
}
inline const char *challengeName(int challenge) {
  return challenge == 1 ? "补给匮乏" : (challenge == 2 ? "无复苏" : "标准");
}
inline int addCounter(int value, int increment) {
  return static_cast<int>(std::clamp(static_cast<long long>(value) + increment,
                                  0LL, static_cast<long long>(MaxCounter)));
}
}
