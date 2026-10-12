#pragma once
#include "enemy/enemy.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

class player;

// Things to find on a floor besides its guardian. The rules decide what a
// floor holds and what each choice does; frontends decide where to draw it.
//
// A floor's spots are derived from the run seed and the floor alone, so they
// never change between visits, menus or reloads. Each choice is resolved from
// its own seed stream too: retrying after a reload gives the same result.
namespace Exploration {
enum class SpotKind {
  Cache = 1,       // Supply cache: free, small random reward.
  Curio = 2,       // Chapter curio: gamble by hand or pay for the good result.
  Gather = 3,      // Fish, herbs, ore...: ingredients for camp cooking.
  AffixSource = 4, // Strips one dangerous affix from this floor's guardian.
  Key = 5,         // Vault key; keys carry over between chapters.
  Vault = 6,       // Locked room: variant 1 needs one key, variant 2 needs two.
  Merchant = 7,    // Wandering merchant with three goods.
  QuestItem = 8,   // One of the chapter ally's three lost items.
  Puzzle = 9,      // The chapter's ordering puzzle; see puzzle()/solvePuzzle().
  Lore = 10        // A hidden page of the expedition journal (cross-run collection).
};
// Where a map should put a spot. Hidden spots sit in alcoves or behind cracked
// walls; vaults are locked side rooms.
enum class Zone { Middle = 0, Guardian = 1, Hidden = 2, Vault = 3 };

constexpr int MaxSpots = 16;     // RunState::explored bits 0..15, one per spot.
constexpr int MerchantBit = 16;  // Bits 16..18: merchant goods sold on this floor.
constexpr int MerchantGoods = 3;
constexpr int IngredientCount = 5;
constexpr int MaxKeys = 99;
constexpr int MaxIngredients = 99;
constexpr int LorePages = 100;   // One per floor.
constexpr int QuestItemsPerChapter = 3;
constexpr int BoonCount = 4;     // RunState::boon 1..4; see boonDescription().
constexpr int BaneCount = 2;     // RunState::bane 1..2.
constexpr int RecipeCount = 6;

struct Spot {
  SpotKind kind = SpotKind::Cache;
  int variant = 0; // Curio type 0..2, gather node 0..1, vault keys 1..2, quest item 0..2.
  Zone zone = Zone::Middle;
};

// One chapter's fixed layout of keys, vaults, merchants, quest items and puzzle,
// as floor numbers (local floors 1..9; bosses never hold them).
struct ChapterPlan {
  std::array<int, 2> keyFloors{};
  std::array<int, 2> vaultFloors{}; // [0] needs one key, [1] needs two.
  std::array<int, 2> merchantFloors{};
  std::array<int, 3> questFloors{};
  int puzzleFloor = 0;
};

// Pure generation from the run seed; usable without a hero (save validation).
ChapterPlan chapterPlan(std::uint32_t seed, int chapter);
std::vector<Spot> generate(std::uint32_t seed, int floor);
// Whether finding everything on a floor includes this spot (merchants do not).
bool countsTowardCompletion(SpotKind kind);

struct SpotView {
  int index = 0; // 0-based; pass to options()/interact().
  Spot spot;
  bool done = false; // Explored; a merchant is done after its first sale.
  std::string name;
  std::string description;
};
std::vector<SpotView> survey(const player &hero, int floor);
// Spots that count toward completion and are not explored yet.
int remaining(const player &hero, int floor);

struct Option {
  int id = 0;           // Pass to interact().
  std::string label;
  std::string detail;   // Exact costs, odds and results.
  bool available = false;
  std::string reason;   // Why it is unavailable.
};
// Choices for a spot; empty for an invalid, unreachable or finished spot.
std::vector<Option> options(const player &hero, int floor, int index);

struct Result {
  bool ok = false;        // false: nothing changed.
  std::string text;       // What happened (also printed).
  int lorePage = 0;       // 1..100 when a lore page was read: record it in the profile.
};
// Applies a choice. A floor above the highest reached, a dead hero, a finished
// spot or an unaffordable choice changes nothing.
Result interact(player &hero, int floor, int index, int option);

struct Good {
  int kind = 0; // 0 blind box, 1 key, 2 charm, 3 ingredient pack.
  std::string name;
  std::string description;
  int price = 0;
  bool sold = false;
};
// The merchant's goods on this floor; empty when the floor has no merchant.
std::vector<Good> merchantGoods(const player &hero, int floor);

// The chapter puzzle on this floor. Elements are numbered from 0; an answer
// lists every element once, first to last. Exactly one order fits the clues.
struct Puzzle {
  std::string title;
  std::string prompt;
  std::vector<std::string> elements;
  std::vector<std::string> clues;
};
bool hasPuzzle(const player &hero, int floor);
Puzzle puzzle(const player &hero, int floor);
// A wrong order costs 5% of current HP (never lethal) and may be retried.
Result solvePuzzle(player &hero, int floor, const std::vector<int> &order);
// The unique answer; for tests and simulations.
std::vector<int> puzzleSolution(std::uint32_t seed, int floor);

// Camp cooking. Recipes use two ingredients; a meal heals or restores MP at
// once and may set the boon for the next encounter (replacing the old one).
struct Recipe {
  int id = 0;
  std::string name;
  std::string description;
  std::array<int, IngredientCount> needs{}; // Empty for the stew: any two different.
};
const std::vector<Recipe> &recipes();
const char *ingredientName(int ingredient);
bool canCook(const player &hero, int recipe);
Result cook(player &hero, int recipe);

// The next encounter's modifiers, consumed when that encounter is settled.
std::string boonDescription(int boon);
std::string baneDescription(int bane);
// Called by Campaign::makeEnemy: affix sources destroyed on this floor, the
// pending boon and bane, and the ally's ambush once the chapter's three quest
// items are found. Notes describe each change for previews.
void applyToEncounter(const player &hero, int floor, enemy &monster,
                      std::vector<std::string> *notes = nullptr);
// Called by Campaign::settleEncounter for every settled encounter.
void finishEncounter(player &hero);

// Expedition journal pages, one per floor.
std::string loreTitle(int page);
std::string loreText(int page);
// A one-line chapter overview for the journal.
std::string chapterSummary(const player &hero, int floor);
} // namespace Exploration
