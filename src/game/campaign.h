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
// The floor's guardian, including exploration effects (affix sources, the
// next encounter's boon and bane, the ally's ambush). notes, when given,
// receives one line per exploration effect for previews.
enemy makeEnemy(const player &hero, int floor, std::vector<std::string> *notes = nullptr);
bool eventPending(const player &hero, int floor);
std::string eventDescription(int floor);
bool resolveEvent(player &hero, int floor, int choice);
// Camp rests: up to three per chapter at 1x/2x/3x price; one in scarce supplies.
int restsRemaining(const player &hero, int floor);
int restPrice(const player &hero, int floor);
bool rest(player &hero, int floor);
// What the hero carries into the chapter's next hard fight: the first elite or
// boss floor from here that is not yet cleared (0 if none), a summary line and,
// when that fight is on this floor and the hero lacks half MP or a ready
// skill, a warning. Cooldowns only tick during battle turns.
struct Readiness {
  int majorFloor = 0;
  std::string summary;
  std::string warning;
};
Readiness readiness(const player &hero, int floor);
bool claimOrder(player &hero, int floor);
std::vector<Item> shopStock(const player &hero, int floor);
int purchasePrice(const player &hero, int floor, const Item &item);
bool purchase(player &hero, int floor, int slot);
bool refreshShop(player &hero, int floor);
bool settleEncounter(player &hero, enemy &monster, BattleSession &battle, int floor);
// Chapter bounties: two optional goals per chapter, chosen from the run seed
// (no rules randomness is consumed) and counted over every won fight on that
// chapter's floors. Reaching a goal pays BountyMaterials and a potion once.
constexpr int BountyKinds = 6;
constexpr int BountyMaterials = 3;
struct Bounty {
  const char *name;
  const char *before; // goal text around the target number
  const char *after;
  int target;
};
const Bounty &bounty(int kind);
std::string bountyGoal(int kind);
std::array<int, 2> chapterBounties(const player &hero, int chapter);
bool revive(player &hero, bool useFree);
// Adds an item to the backpack, counting gear upgrades like any other reward.
void grantItem(player &hero, const Item &item);
void printJournal(const player &hero, int floor);
void printBattleReport(const BattleSession &battle);
}
