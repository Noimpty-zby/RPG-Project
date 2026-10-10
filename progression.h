#pragma once
#include <algorithm>

namespace Progression {

// 用已到达的最高楼层解锁成长，回到低层不会失去技能。
inline int skillRankCap(int highestFloor) {
  if (highestFloor >= 85) return 5;
  if (highestFloor >= 65) return 4;
  if (highestFloor >= 40) return 3;
  if (highestFloor >= 20) return 2;
  return 1;
}

inline int skillUnlockFloor(int slot) {
  constexpr int floors[] = {1, 3, 6, 10, 15};
  return floors[std::clamp(slot, 1, 5) - 1];
}

// 品质：0=普通，1=精良，2=稀有，3=史诗；高层保留品质差异。
inline int equipmentTier(int floor, int roll0to99) {
  const int roll = std::clamp(roll0to99, 0, 99);
  if (floor < 20) return roll < 80 ? 0 : 1;
  if (floor < 45) return roll < 50 ? 0 : (roll < 90 ? 1 : 2);
  if (floor < 70) return roll < 15 ? 0 : (roll < 65 ? 1 : 2);
  if (floor < 85) return roll < 45 ? 1 : (roll < 90 ? 2 : 3);
  return roll < 20 ? 1 : (roll < 70 ? 2 : 3);
}

} // namespace Progression
