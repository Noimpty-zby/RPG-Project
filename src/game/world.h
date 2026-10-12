#pragma once
#include "enemy/enemy.h"
#include "item/item.h"
#include <optional>

class player;

struct EnemyStats {
  int hp;
  int attack;
  int expReward;
  int coinReward;
  int level;
  bool boss;
};

// Enemy tuning knobs, in percent.
namespace EnemyTuning {
// The base curve every enemy scales from. Raised when floor exploration joined
// the game, so a player who explores meets about the earlier difficulty.
constexpr int BaseHpPercent = 110;
constexpr int BaseAttackPercent = 115;
// Ordinary enemies relative to the base curve that bosses also scale from.
constexpr int OrdinaryHpPercent = 160;
constexpr int OrdinaryAttackPercent = 90;
// Elites relative to ordinary enemies of the same floor.
constexpr int EliteHpPercent = 170;
constexpr int EliteAttackPercent = 115;
// Objective guardians relative to the base curve: durable, slightly weaker hits.
constexpr int SurviveHpPercent = 300;
constexpr int DisruptHpPercent = 400;
constexpr int ObjectiveAttackPercent = 80;
} // namespace EnemyTuning

Item generateRandomItem(int floor);
Item generateTargetedItem(int floor, JobReq job, ItemType type);
// First-clear elite reward: class gear of the hero's loot target in the floor's best quality band.
Item generateEliteReward(const player &hero, int floor);
std::optional<Item> generateLoot(player &hero, int floor, bool firstClear);
EnemyStats enemyStatsForFloor(int floor);
enemy generateEnemy(int floor, bool elite = false);
