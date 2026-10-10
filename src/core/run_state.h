#pragma once
#include <array>
#include <cstdint>

// Persistent campaign state. Index 0 of floor arrays is unused.
// Chapters are indexed 0..9; routes: 0 undecided, 1 sabotage, 2 rescue, 3 supply.
struct RunState {
  std::uint32_t seed = 1;
  int challenge = 0; // 0 standard, 1 scarce supplies, 2 no revives
  bool legacyRules = false;
  int lootTarget = 1; // Weapon=1 or Armor=2
  int pity = 0;
  int materials = 0;
  std::array<int, 10> routes{};
  std::array<int, 10> events{}; // 0 pending, 1/2 resolved choice
  std::array<int, 10> campRested{};
  std::array<int, 10> ordersClaimed{};
  std::array<int, 10> shopRefreshes{};
  std::array<int, 10> shopPurchased{}; // 5-bit stock mask
  std::array<int, 101> cleared{};
  std::array<int, 101> attempts{};
  int battles = 0;
  int turns = 0;
  int potionsUsed = 0;
  int deaths = 0;
  int damageTaken = 0;
  int usefulDrops = 0;
  int gearDrought = 0;
  int longestGearDrought = 0;
};
