#pragma once
#include <algorithm>

namespace GameConfig {
constexpr int MaxFloor = 100;
constexpr int FloorsPerChapter = 10;
constexpr int MaxMaterials = 9999;
constexpr int MaxCounter = 100000000;
inline int floor(int value) { return std::clamp(value, 1, MaxFloor); }
inline int chapter(int value) { return (floor(value) - 1) / FloorsPerChapter; }
inline int refreshCost(int value, int refreshes) {
  return (40 + chapter(value) * 25) * (1 + std::clamp(refreshes, 0, 20));
}
inline int restCost(int value) { return 60 + chapter(value) * 40; }
inline int repeatRewardPercent(int attempts) {
  return attempts <= 1 ? 30 : (attempts <= 3 ? 10 : 0);
}
inline int addCounter(int value, int increment) {
  return static_cast<int>(std::clamp(static_cast<long long>(value) + increment,
                                  0LL, static_cast<long long>(MaxCounter)));
}
}
