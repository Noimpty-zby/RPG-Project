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

Item generateRandomItem(int floor);
Item generateTargetedItem(int floor, JobReq job, ItemType type);
std::optional<Item> generateLoot(player &hero, int floor, bool firstClear);
EnemyStats enemyStatsForFloor(int floor);
enemy generateEnemy(int floor);
