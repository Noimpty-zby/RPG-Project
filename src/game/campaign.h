#pragma once
#include "game/combat.h"
#include "game/world.h"
#include <string>
#include <vector>

namespace Campaign {
struct Chapter {
  const char *name;
  const char *mission;
  const char *ally;
};
const Chapter &chapter(int floor);
JobReq jobOf(const player &hero);
void initialize(player &hero, unsigned seed, int challenge);
void seedStream(const player &hero, int floor, unsigned stream);
bool chooseRoute(player &hero, int floor, int route);
std::string routeDescription(int route);
int objectivesCompleted(const player &hero, int floor);
// One elite floor per chapter, never an objective or boss floor.
int eliteFloor(int chapter);
bool isEliteFloor(int floor);
EncounterRules rulesFor(const player &hero, int floor);
enemy makeEnemy(const player &hero, int floor);
bool eventPending(const player &hero, int floor);
std::string eventDescription(int floor);
bool resolveEvent(player &hero, int floor, int choice);
// Camp rests: up to three per chapter at 1x/2x/3x price; one in scarce supplies.
int restsRemaining(const player &hero, int floor);
int restPrice(const player &hero, int floor);
bool rest(player &hero, int floor);
bool claimOrder(player &hero, int floor);
std::vector<Item> shopStock(const player &hero, int floor);
int purchasePrice(const player &hero, int floor, const Item &item);
bool purchase(player &hero, int floor, int slot);
bool refreshShop(player &hero, int floor);
bool settleEncounter(player &hero, enemy &monster, BattleSession &battle, int floor);
bool revive(player &hero, bool useFree);
void printJournal(const player &hero, int floor);
void printBattleReport(const BattleSession &battle);
}
