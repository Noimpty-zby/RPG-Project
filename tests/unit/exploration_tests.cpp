#include "core/game_config.h"
#include "game/campaign.h"
#include "game/exploration.h"
#include "player/jobs.h"
#include "storage/profile.h"
#include "storage/save.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace Exploration;

void require(bool passed, const std::string &message) {
  if (!passed) throw std::runtime_error(message);
}

std::unique_ptr<player> campaignHero(unsigned seed, int highest = 100) {
  auto hero = std::make_unique<Warrior>("explorer");
  Campaign::initialize(*hero, seed, 0);
  hero->setHighestFloor(highest);
  return hero;
}
int indexOf(const player &hero, int floor, SpotKind kind) {
  for (const auto &view : survey(hero, floor))
    if (view.spot.kind == kind) return view.index;
  return -1;
}
// The first floor in [from, to] holding a spot of this kind, or 0.
int floorWith(const player &hero, SpotKind kind, int from = 1, int to = 100) {
  for (int floor = from; floor <= to; ++floor)
    if (indexOf(hero, floor, kind) >= 0) return floor;
  return 0;
}
bool sameSpots(const std::vector<Spot> &a, const std::vector<Spot> &b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].kind != b[i].kind || a[i].variant != b[i].variant || a[i].zone != b[i].zone) return false;
  return true;
}

void testGeneration() {
  for (unsigned seed : {1u, 777u, 12345u}) {
    for (int chapter = 0; chapter < 10; ++chapter) {
      std::map<SpotKind, int> counts;
      std::vector<int> vaultKeys;
      for (int local = 1; local <= 10; ++local) {
        const int floor = chapter * 10 + local;
        const auto spots = generate(seed, floor);
        require(sameSpots(spots, generate(seed, floor)), "generation must be stable");
        require(spots.size() >= 3 && static_cast<int>(spots.size()) <= MaxSpots, "spot count bounds");
        const auto page = std::find_if(spots.begin(), spots.end(), [](const Spot &s) { return s.kind == SpotKind::Lore; });
        require(page != spots.end() && page->variant == floor && page->zone == Zone::Hidden,
                "every floor holds its hidden journal page");
        const bool fork = isForkFloor(seed, floor);
        require(std::all_of(page + 1, spots.end(), [](const Spot &s) {
                  return s.kind == SpotKind::FogSource || s.kind == SpotKind::Vantage || s.kind == SpotKind::Shortcut ||
                         s.zone == Zone::Branch;
                }) && (page + 1 == spots.end()) == !fork,
                "the journal page ends the spots a floor had before forks; only fork floors add more");
        std::map<SpotKind, int> here;
        for (const auto &spot : spots) ++here[spot.kind];
        if (local == 10) {
          require(spots.size() == 3 && here[SpotKind::Cache] == 1 && here[SpotKind::Curio] == 1,
                  "boss floors hold one cache, one curio and the page");
          continue;
        }
        require(here[SpotKind::Cache] >= 1 && here[SpotKind::Curio] >= 1 && here[SpotKind::Gather] <= (fork ? 3 : 2) &&
                here[SpotKind::AffixSource] <= 1, "ordinary floors hold a cache and a curio");
        if (local == 1) require(here[SpotKind::AffixSource] == 0, "no affix source on a chapter's first floor");
        for (const auto &[kind, count] : here) counts[kind] += count;
        for (const auto &spot : spots)
          if (spot.kind == SpotKind::Vault) vaultKeys.push_back(spot.variant);
      }
      require(counts[SpotKind::Key] == 2 && counts[SpotKind::Vault] == 2 && counts[SpotKind::Merchant] == 2 &&
              counts[SpotKind::QuestItem] == 3 && counts[SpotKind::Puzzle] == 1,
              "every chapter holds 2 keys, 2 vaults, 2 merchants, 3 quest items and a puzzle");
      require(vaultKeys == std::vector<int>({1, 2}), "the one-key vault comes first");
      const auto plan = chapterPlan(seed, chapter);
      require(plan.vaultFloors[0] < plan.vaultFloors[1] && plan.questFloors[0] < plan.questFloors[1] &&
              plan.questFloors[1] < plan.questFloors[2], "plans list distinct floors in order");
    }
  }
  require(generate(1, 0).empty() && generate(1, 101).empty(), "invalid floors hold nothing");
  require(!sameSpots(generate(1, 5), generate(2, 5)) || !sameSpots(generate(1, 6), generate(2, 6)),
          "seeds change the layout");
}

std::vector<int> forkFloorsOf(unsigned seed, int chapter) {
  std::vector<int> floors;
  for (int floor = chapter * 10 + 1; floor <= chapter * 10 + 10; ++floor)
    if (isForkFloor(seed, floor)) floors.push_back(floor);
  return floors;
}
// A fork floor of this seed whose branches include the given one, or 0.
int forkWith(unsigned seed, Branch branch) {
  for (int floor = 1; floor <= 100; ++floor) {
    const auto ways = branches(seed, floor);
    if (std::find(ways.begin(), ways.end(), branch) != ways.end()) return floor;
  }
  return 0;
}
int branchSpot(const player &hero, int floor) {
  for (const auto &view : survey(hero, floor))
    if (view.spot.zone == Zone::Branch) return view.index;
  return -1;
}
const BranchView &branchView(const Fork &fork, Branch kind) {
  for (const auto &view : fork.branches)
    if (view.kind == kind) return view;
  throw std::runtime_error("branch missing");
}

void testForkPlacement() {
  for (unsigned seed : {1u, 777u, 12345u}) {
    for (int chapter = 0; chapter < 10; ++chapter) {
      const auto plan = chapterPlan(seed, chapter);
      const auto floors = forkFloorsOf(seed, chapter);
      require(floors == std::vector<int>(plan.forkFloors.begin(), plan.forkFloors.end()), "the plan lists the fork floors");
      require(floors.size() == ForkFloorsPerChapter, "three fork floors per chapter");
      for (int floor : floors) {
        const int local = (floor - 1) % 10 + 1;
        require(local >= 2 && local <= 9 && !Campaign::isEliteFloor(floor) && floor != plan.puzzleFloor &&
                floor != plan.vaultFloors[0] && floor != plan.vaultFloors[1],
                "forks skip first, elite, boss, puzzle and vault floors");
        const auto ways = branches(seed, floor);
        require(ways.size() >= 2 && std::find(ways.begin(), ways.end(), Branch::Fog) != ways.end() &&
                std::is_sorted(ways.begin(), ways.end()), "every fork offers the fog branch, near to key");
        const auto spots = generate(seed, floor);
        int fogSpots = 0;
        for (const auto &spot : spots) {
          fogSpots += spot.zone == Zone::Branch && spot.branch == Branch::Fog;
          require((spot.zone == Zone::Branch) == (spot.branch != Branch::None), "only branch spots name a branch");
          require(!(spot.kind == SpotKind::Cache && spot.zone == Zone::Middle) &&
                  !(spot.kind == SpotKind::Gather && spot.zone != Zone::Branch), "the fog branch carries the supplies");
        }
        require(fogSpots >= 1 && std::count_if(spots.begin(), spots.end(), [](const Spot &s) {
                  return (s.kind == SpotKind::Cache && s.variant == 1) || s.kind == SpotKind::FogSource ||
                         s.kind == SpotKind::Vantage;
                }) == 3, "a fork floor holds a medicine chest past the fog, a fog source and high ground");
      }
    }
  }
  require(branches(1, 10).empty() && branches(1, 0).empty() && !isForkFloor(1, 101), "no forks off fork floors");
}

void testForkBranches() {
  const unsigned seed = 2024;
  const int floor = forkWith(seed, Branch::Locked);
  require(floor > 0, "a key fork exists");
  auto hero = campaignHero(seed);
  require(!guardianReachable(*hero, floor) && branchTaken(*hero, floor) == Branch::None, "the fork comes before the guardian");
  const int supply = branchSpot(*hero, floor);
  const auto view = survey(*hero, floor)[supply];
  require(!view.blocked.empty() && !interact(*hero, floor, supply, 1).ok, "supplies past the fog wait for the branch");
  const auto blocked = options(*hero, floor, supply);
  require(!blocked.empty() && std::none_of(blocked.begin(), blocked.end(), [](const Option &o) { return o.available; }),
          "blocked spots list their choices as unavailable");
  for (const auto &spot : survey(*hero, floor))
    if (spot.spot.zone == Zone::Guardian) require(!spot.blocked.empty(), "the guardian's side waits for a branch");
  require(!takeBranch(*hero, floor, Branch::Patrol).ok, "the patrol branch needs a fight");
  hero->runState().keys = 0;
  require(!branchView(fork(*hero, floor), Branch::Locked).available && !takeBranch(*hero, floor, Branch::Locked).ok,
          "the key branch needs a key");
  // High ground shows the stash before choosing.
  const int vantage = indexOf(*hero, floor, SpotKind::Vantage);
  require(branchView(fork(*hero, floor), Branch::Locked).reward.find("内容不明") != std::string::npos, "the stash starts unknown");
  require(interact(*hero, floor, vantage, 1).ok &&
          branchView(fork(*hero, floor), Branch::Locked).reward.find("内容不明") == std::string::npos, "high ground shows the stash");
  hero->runState().keys = 2;
  const int coins = hero->getcoin(), materials = hero->runState().materials, hp = hero->gethp();
  const auto result = takeBranch(*hero, floor, Branch::Locked);
  require(result.ok && hero->runState().keys == 1 && hero->gethp() == hp &&
          (hero->getcoin() > coins || hero->runState().materials > materials || hero->getSaveBackpack().size() > 1),
          "the key branch pays its stash without hurting");
  require(branchTaken(*hero, floor) == Branch::Locked && guardianReachable(*hero, floor), "taking a branch opens the guardian");
  const auto after = fork(*hero, floor);
  require(std::none_of(after.branches.begin(), after.branches.end(), [](const BranchView &b) { return b.available; }) &&
          !takeBranch(*hero, floor, Branch::Fog).ok, "one branch per floor");
  const bool shortcut = indexOf(*hero, floor, SpotKind::Shortcut) >= 0;
  require(!interact(*hero, floor, supply, 1).ok, "the fog branch closed with the key branch taken");
  if (shortcut) {
    require(interact(*hero, floor, indexOf(*hero, floor, SpotKind::Shortcut), 1).ok && interact(*hero, floor, supply, 1).ok,
            "a shortcut opens the fog branch's supplies");
  } else {
    int expected = 0;
    for (const auto &spot : survey(*hero, floor))
      expected += !spot.done && countsTowardCompletion(spot.spot.kind) && spot.spot.zone != Zone::Branch;
    require(remaining(*hero, floor) == expected, "closed branches leave the count of spots to find");
  }

  // The fog costs health until its source is shut.
  const int fogFloor = forkWith(seed, Branch::Fog);
  for (bool shut : {false, true}) {
    auto walker = campaignHero(seed);
    if (shut) require(interact(*walker, fogFloor, indexOf(*walker, fogFloor, SpotKind::FogSource), 1).ok, "shut the source");
    const int before = walker->gethp();
    require(takeBranch(*walker, fogFloor, Branch::Fog).ok && interact(*walker, fogFloor, branchSpot(*walker, fogFloor), 1).ok,
            "walking the fog opens its supplies");
    require(shut ? walker->gethp() == before : walker->gethp() == before - before * 20 / 100, "the fog's toll");
  }
  auto low = campaignHero(seed);
  low->sethp(1);
  require(takeBranch(*low, fogFloor, Branch::Fog).ok && low->gethp() == 1, "the fog never kills");
  require(guardianReachable(*campaignHero(seed), 10), "floors without a fork stay open");
}

void testPatrols() {
  const unsigned seed = 31;
  const int floor = forkWith(seed, Branch::Patrol);
  require(floor > 0, "a patrol fork exists");
  auto hero = campaignHero(seed);
  const enemy a = Campaign::makePatrol(*hero, floor), b = Campaign::makePatrol(*hero, floor);
  require(a.getname() == b.getname() && a.getmaxhp() == b.getmaxhp() && a.getAffixes() == b.getAffixes(),
          "patrols are fixed by seed and floor");
  require(a.getname().rfind("巡逻·", 0) == 0 && a.getAffixes().size() >= 2, "patrols are named and carry two affixes");
  hero->runState().boon = 1;
  hero->runState().bane = 2;
  const enemy blessed = Campaign::makePatrol(*hero, floor);
  require(blessed.getattack() == a.getattack() && blessed.getmaxhp() == a.getmaxhp(), "boons and banes are kept for the guardian");

  // A lost fight changes no branch; a win takes the near branch once.
  hero->sethp(1);
  enemy lethal("lethal", 1000000, 100, 0, 0, 0, 1);
  lethal.configureEncounter(floor, false); lethal.setdodgeRate(0);
  BattleSession loss(*hero, lethal);
  require(loss.beginTurn() && loss.act({ActionType::Attack}) && !hero->isalive(), "death fixture");
  require(!Campaign::settlePatrol(*hero, lethal, loss, floor) && hero->runState().deaths == 1 &&
          branchTaken(*hero, floor) == Branch::None && hero->runState().boon == 1, "a lost patrol keeps the fork open");
  hero->revive();
  enemy weak("weak", 1, 0, 0, 40, 30, 1);
  weak.configureEncounter(floor, false); weak.setdodgeRate(0);
  BattleSession win(*hero, weak);
  require(win.beginTurn() && win.act({ActionType::Attack}) && win.won(), "win fixture");
  const int coins = hero->getcoin(), materials = hero->runState().materials;
  require(Campaign::settlePatrol(*hero, weak, win, floor) && branchTaken(*hero, floor) == Branch::Patrol &&
          hero->getcoin() == coins + 30 && hero->runState().materials == materials + Campaign::PatrolMaterials &&
          hero->runState().attempts[floor] == 0 && hero->runState().boon == 1 && guardianReachable(*hero, floor),
          "a won patrol pays once, opens the guardian and keeps the boon");
  enemy second("weak", 1, 0, 0, 40, 30, 1);
  second.configureEncounter(floor, false); second.setdodgeRate(0);
  BattleSession again(*hero, second);
  require(again.beginTurn() && again.act({ActionType::Attack}) && !Campaign::settlePatrol(*hero, second, again, floor) &&
          hero->getcoin() == coins + 30, "a taken fork pays no second patrol");
}

void testInteractionRules() {
  auto hero = campaignHero(4242, 1);
  const int keyFloor = floorWith(*hero, SpotKind::Key, 2, 9);
  require(keyFloor > 1, "a key above floor one exists in chapter one");
  const int key = indexOf(*hero, keyFloor, SpotKind::Key);
  require(options(*hero, keyFloor, key).empty() && !interact(*hero, keyFloor, key, 1).ok,
          "floors above the highest reached cannot be explored");
  hero->setHighestFloor(100);
  require(!interact(*hero, keyFloor, -1, 1).ok && !interact(*hero, keyFloor, 99, 1).ok &&
          !interact(*hero, keyFloor, key, 7).ok, "invalid spots and options change nothing");
  const int before = remaining(*hero, keyFloor);
  require(interact(*hero, keyFloor, key, 1).ok && hero->runState().keys == 1 &&
          hero->runState().spotsExplored == 1 && remaining(*hero, keyFloor) == before - 1,
          "picking up a key");
  require(!interact(*hero, keyFloor, key, 1).ok && hero->runState().keys == 1, "each spot is explored once");
  require(options(*hero, keyFloor, key).empty() && survey(*hero, keyFloor)[key].done, "finished spots offer nothing");
  hero->sethp(0);
  require(!interact(*hero, keyFloor, indexOf(*hero, keyFloor, SpotKind::Lore), 1).ok, "the dead cannot explore");
}

void testDeterministicOutcomes() {
  for (int round = 0; round < 2; ++round) {
    auto a = campaignHero(99), b = campaignHero(99);
    for (int floor = 1; floor <= 30; ++floor)
      for (auto *hero : {a.get(), b.get()})
        for (const auto &view : survey(*hero, floor))
          if (!view.done && view.spot.kind != SpotKind::Puzzle) interact(*hero, floor, view.index, 1);
    require(a->getcoin() == b->getcoin() && a->gethp() == b->gethp() && a->getmp() == b->getmp() &&
            a->runState().materials == b->runState().materials && a->runState().keys == b->runState().keys &&
            a->runState().ingredients == b->runState().ingredients && a->runState().explored == b->runState().explored &&
            a->getSaveBackpack().size() == b->getSaveBackpack().size(), "outcomes depend only on seed and choices");
  }
}

void testCurioCosts() {
  auto hero = campaignHero(31337);
  for (int floor = 1; floor <= 100; ++floor)
    for (const auto &view : survey(*hero, floor)) {
      if (view.spot.kind != SpotKind::Curio || view.done) continue;
      const auto list = options(*hero, floor, view.index);
      require(list.size() == 2 && list[0].available, "curios offer a gamble and a sure choice");
      if (view.spot.variant == 0) {
        hero->runState().materials = 1;
        require(!options(*hero, floor, view.index)[1].available, "prying needs two materials");
        require(!interact(*hero, floor, view.index, 2).ok && hero->runState().materials == 1, "unaffordable choice refused");
        hero->runState().materials = 2;
        const int coins = hero->getcoin();
        require(interact(*hero, floor, view.index, 2).ok && hero->runState().materials == 0 &&
                hero->getcoin() > coins, "prying spends materials for coins and a potion");
        return;
      }
    }
  throw std::runtime_error("no container curio found");
}

void testAffixSource() {
  auto hero = campaignHero(2024);
  int floor = 0;
  for (int candidate = 2; candidate < 100 && !floor; ++candidate)
    if (indexOf(*hero, candidate, SpotKind::AffixSource) >= 0 &&
        !Campaign::makeEnemy(*hero, candidate).getAffixes().empty() && candidate % 10 != 9 &&
        !isForkFloor(2024, candidate)) // Past a fork the source waits for a branch.
      floor = candidate;
  require(floor > 0, "a floor with an affix source and an affixed guardian");
  const int source = indexOf(*hero, floor, SpotKind::AffixSource);
  const enemy before = Campaign::makeEnemy(*hero, floor);
  hero->runState().materials = 2;
  require(interact(*hero, floor, source, 2).ok && hero->runState().materials == 0, "careful removal costs materials");
  std::vector<std::string> notes;
  const enemy after = Campaign::makeEnemy(*hero, floor, &notes);
  const bool stripped = after.getAffixes().size() + 1 == before.getAffixes().size();
  require(stripped || after.getattack() == std::max(1, before.getattack() * 90 / 100),
          "a destroyed source strips an affix or weakens the guardian");
  require(notes.size() == 1, "the effect is described for previews");
  const enemy neighbor = Campaign::makeEnemy(*hero, floor + 1);
  auto fresh = campaignHero(2024);
  const enemy untouched = Campaign::makeEnemy(*fresh, floor + 1);
  require(neighbor.getattack() == untouched.getattack() && neighbor.getAffixes() == untouched.getAffixes() &&
          neighbor.getmaxhp() == untouched.getmaxhp(), "sources only affect their own floor");
}

void settleWin(player &hero, int floor) {
  auto monster = Campaign::makeEnemy(hero, floor);
  BattleSession battle(hero, monster, Campaign::rulesFor(hero, floor));
  monster.resethp(0);
  require(Campaign::settleEncounter(hero, monster, battle, floor), "settled win");
}

void testBoonsAndBanes() {
  auto hero = campaignHero(55);
  const enemy plain = Campaign::makeEnemy(*hero, 4);
  hero->runState().boon = 1;
  require(Campaign::makeEnemy(*hero, 4).getattack() == std::max(1, plain.getattack() * 92 / 100), "boon: attack -8%");
  hero->runState().boon = 2;
  require(Campaign::makeEnemy(*hero, 4).getmaxhp() == plain.getmaxhp() * 92 / 100, "boon: hp -8%");
  hero->runState().boon = 3;
  require(Campaign::makeEnemy(*hero, 4).getdodgeRate() == std::max(0, plain.getdodgeRate() - 8), "boon: dodge");
  hero->runState().boon = 0;
  hero->runState().bane = 1;
  require(Campaign::makeEnemy(*hero, 4).getattack() == plain.getattack() * 112 / 100, "bane: attack +12%");
  hero->runState().bane = 2;
  require(Campaign::makeEnemy(*hero, 4).getmaxhp() == plain.getmaxhp() * 110 / 100, "bane: hp +10%");
  hero->runState().boon = 1;
  settleWin(*hero, 4);
  require(hero->runState().boon == 0 && hero->runState().bane == 0, "settling an encounter consumes both");
}

void testVaultsAndMerchants() {
  auto hero = campaignHero(8080);
  const auto plan = chapterPlan(8080, 2);
  const int small = indexOf(*hero, plan.vaultFloors[0], SpotKind::Vault);
  const int large = indexOf(*hero, plan.vaultFloors[1], SpotKind::Vault);
  require(!interact(*hero, plan.vaultFloors[0], small, 1).ok, "vaults need keys");
  hero->runState().keys = 3;
  require(interact(*hero, plan.vaultFloors[0], small, 1).ok && hero->runState().keys == 2 &&
          hero->runState().vaultsOpened == 1, "the small vault takes one key");
  const auto backpack = hero->getSaveBackpack().size();
  require(interact(*hero, plan.vaultFloors[1], large, 1).ok && hero->runState().keys == 0 &&
          hero->runState().vaultsOpened == 2 && hero->getSaveBackpack().size() == backpack + 1,
          "the large vault takes two keys and holds gear");
  const int floor = plan.merchantFloors[0];
  const int merchant = indexOf(*hero, floor, SpotKind::Merchant);
  auto goods = merchantGoods(*hero, floor);
  require(goods.size() == 3, "three goods");
  require(goods[0].kind != goods[1].kind && goods[1].kind != goods[2].kind && goods[0].kind != goods[2].kind,
          "distinct goods");
  require(merchantGoods(*hero, plan.puzzleFloor).empty() == (plan.puzzleFloor != plan.merchantFloors[0] &&
                                                             plan.puzzleFloor != plan.merchantFloors[1]),
          "goods only where a merchant stands");
  require(!interact(*hero, floor, merchant, 1).ok, "goods need coins");
  hero->gaincoin(100000);
  const int remainingBefore = remaining(*hero, floor);
  for (int slot = 1; slot <= 3; ++slot) require(interact(*hero, floor, merchant, slot).ok, "buy every good");
  for (int slot = 1; slot <= 3; ++slot) require(!interact(*hero, floor, merchant, slot).ok, "goods sell once");
  require(remaining(*hero, floor) == remainingBefore, "merchants do not count toward completion");
  require((hero->runState().explored[floor] >> MerchantBit) == 7 && survey(*hero, floor)[merchant].done,
          "sales are recorded");
}

void testQuestItems() {
  auto hero = campaignHero(777);
  const enemy boss = Campaign::makeEnemy(*hero, 50);
  const auto plan = chapterPlan(777, 4);
  for (int i = 0; i < 3; ++i) {
    const int floor = plan.questFloors[i];
    require(interact(*hero, floor, indexOf(*hero, floor, SpotKind::QuestItem), 1).ok, "collect quest item");
  }
  require(hero->runState().questItems[4] == 3, "three quest items");
  std::vector<std::string> notes;
  const enemy ambushed = Campaign::makeEnemy(*hero, 50, &notes);
  require(ambushed.getmaxhp() == boss.getmaxhp() * 92 / 100 && notes.size() == 1, "the ally ambushes the boss");
  require(Campaign::makeEnemy(*hero, 40).getmaxhp() == Campaign::makeEnemy(*campaignHero(777), 40).getmaxhp(),
          "the ambush only reaches this chapter's boss");
}

void testPuzzles() {
  for (unsigned seed : {1u, 2u, 3u, 4u}) {
    auto hero = campaignHero(seed);
    for (int chapter = 0; chapter < 10; ++chapter) {
      const int floor = chapterPlan(seed, chapter).puzzleFloor;
      require(hasPuzzle(*hero, floor) && !hasPuzzle(*hero, chapter * 10 + 10), "puzzle floors");
      const auto info = puzzle(*hero, floor);
      const auto answer = puzzleSolution(seed, floor);
      require(info.elements.size() == answer.size() && answer.size() == static_cast<std::size_t>(3 + (chapter >= 3) + (chapter >= 6)),
              "puzzle size grows with chapters");
      require(!info.clues.empty() && info.clues.size() <= 12 && !info.title.empty(), "puzzles have clues");
      require(!interact(*hero, floor, indexOf(*hero, floor, SpotKind::Puzzle), 1).ok, "puzzles are answered as orders");
      require(!solvePuzzle(*hero, floor, {0}).ok, "partial orders are ignored");
      auto wrong = answer;
      std::swap(wrong[0], wrong[1]);
      hero->sethp(hero->getmaxhp());
      const int hp = hero->gethp();
      const auto failed = solvePuzzle(*hero, floor, wrong);
      require(failed.ok && hero->gethp() < hp && hero->gethp() >= 1 && !survey(*hero, floor)[indexOf(*hero, floor, SpotKind::Puzzle)].done,
              "a wrong order costs health and keeps the puzzle");
      hero->sethp(1);
      solvePuzzle(*hero, floor, wrong);
      require(hero->gethp() == 1, "a wrong order is never lethal");
      const int keys = hero->runState().keys;
      require(solvePuzzle(*hero, floor, answer).ok && hero->runState().keys == keys + 1 &&
              hero->runState().puzzlesSolved == chapter + 1, "the right order solves it");
      require(!solvePuzzle(*hero, floor, answer).ok, "solved puzzles stay solved");
    }
  }
}

void testCooking() {
  auto hero = campaignHero(5);
  for (int recipe = 0; recipe <= RecipeCount + 1; ++recipe)
    require(!canCook(*hero, recipe) && !cook(*hero, recipe).ok, "no ingredients, no meal");
  auto &run = hero->runState();
  run.ingredients = {2, 0, 0, 0, 0};
  hero->sethp(hero->getmaxhp() / 2);
  hero->setmp(0);
  const int hp = hero->gethp();
  require(cook(*hero, 1).ok && run.ingredients[0] == 0 && run.boon == 1 && hero->gethp() == hp &&
          hero->getmp() == 0 && run.mealsCooked == 1, "grilled fish only blesses");
  run.ingredients = {1, 3, 2, 0, 0};
  require(cook(*hero, 3).ok && run.boon == 1, "ore bread blesses like fish");
  require(run.ingredients == std::array<int, 5>{1, 3, 0, 0, 0}, "ingredients consumed");
  require(cook(*hero, 6).ok && run.boon == 2 && run.ingredients == std::array<int, 5>{0, 2, 0, 0, 0},
          "stew uses the two most plentiful kinds and replaces the boon");
  require(!canCook(*hero, 6), "stew needs two different kinds");
}

void testLoreAndProfile() {
  auto hero = campaignHero(64);
  const int lore = indexOf(*hero, 37, SpotKind::Lore);
  const auto result = interact(*hero, 37, lore, 1);
  require(result.ok && result.lorePage == 37 && result.text.find(loreTitle(37)) != std::string::npos,
          "reading a page reports it");
  for (int page = 1; page <= 100; ++page)
    require(!loreTitle(page).empty() && !loreText(page).empty(), "every page has text");
  Profile profile = Profile::parse("PROFILE 1\nJOBS_CH1 0\n");
  require(profile.loreCount() == 0, "version 1 profiles load");
  require(profile.recordLore(37) && !profile.recordLore(37) && !profile.recordLore(0) && !profile.recordLore(101),
          "pages are recorded once");
  const Profile copy = Profile::parse(profile.serialize());
  require(copy.loreFound(37) && copy.loreCount() == 1, "pages round trip");
  for (int page = 1; page <= 10; ++page) profile.recordLore(page);
  auto &run = hero->runState();
  run.spotsExplored = 300;
  run.vaultsOpened = 10;
  run.puzzlesSolved = 10;
  run.mealsCooked = 20;
  profile.checkAchievements(*hero, nullptr);
  for (const char *key : {"explorer", "vault_hunter", "puzzle_master", "gourmet", "lore_keeper"})
    require(profile.unlocked(key), std::string("achievement ") + key);
  require(!profile.unlocked("lore_complete"), "the full journal needs every page");
  for (const std::string &bad : {std::string("LORE 01\n"), "LORE " + std::string(100, '2') + "\n"}) {
    bool rejected = false;
    try { Profile::parse("PROFILE 2\n" + bad); } catch (const std::exception &) { rejected = true; }
    require(rejected, "malformed journal rejected");
  }
}

struct SaveFile {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("rpg_explore_tests_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".dat");
  ~SaveFile() { std::error_code error; std::filesystem::remove(path, error); }
  std::string read() const { std::ifstream in(path); std::ostringstream text; text << in.rdbuf(); return text.str(); }
  void write(const std::string &text) const { std::ofstream(path) << text; }
};
std::string withLine(const std::string &text, const std::string &key, const std::string &line) {
  std::istringstream input(text);
  std::string row, output;
  bool replaced = false;
  while (std::getline(input, row)) {
    if (row.rfind(key + " ", 0) == 0) { row = line; replaced = true; }
    output += row + "\n";
  }
  require(replaced, "line exists: " + key);
  return output;
}
std::string field(const std::string &text, const std::string &key) {
  const auto at = text.find("\n" + key + " ");
  require(at != std::string::npos, "field exists: " + key);
  const auto start = at + key.size() + 2;
  return text.substr(start, text.find('\n', start) - start);
}

void testSaves() {
  auto hero = campaignHero(1234, 20);
  for (int floor = 1; floor <= 20; ++floor) {
    takeBranch(*hero, floor, Branch::Fog);
    for (const auto &view : survey(*hero, floor))
      if (!view.done && view.spot.kind != SpotKind::Puzzle) interact(*hero, floor, view.index, 1);
  }
  const int merchantFloor = chapterPlan(1234, 0).merchantFloors[0];
  hero->gaincoin(5000);
  interact(*hero, merchantFloor, indexOf(*hero, merchantFloor, SpotKind::Merchant), 2);
  hero->runState().ingredients[3] = 7;
  hero->runState().boon = 4;
  hero->runState().bane = 2;
  SaveFile file;
  require(SaveSystem::saveGame(*hero, 20, file.path.string()), "save explored run");
  const auto saved = file.read();
  int floor = 0;
  auto loaded = SaveSystem::loadGame(floor, file.path.string());
  require(loaded != nullptr, "load explored run");
  const auto &a = hero->runState(), &b = loaded->runState();
  require(a.explored == b.explored && a.keys == b.keys && a.ingredients == b.ingredients && a.boon == b.boon &&
          a.bane == b.bane && a.questItems == b.questItems && a.spotsExplored == b.spotsExplored &&
          a.vaultsOpened == b.vaultsOpened && a.puzzlesSolved == b.puzzlesSolved && a.mealsCooked == b.mealsCooked,
          "exploration round trips");
  require(SaveSystem::saveGame(*loaded, floor, file.path.string()) && file.read() == saved, "stable resave");
  const int count = std::stoi(field(saved, "SPOTS_EXPLORED"));
  const auto explored = field(saved, "EXPLORED");
  std::vector<std::string> masks;
  std::stringstream split(explored);
  for (std::string mask; std::getline(split, mask, '|');) masks.push_back(mask);
  auto withMask = [&](int at, const std::string &value) {
    auto copy = masks;
    copy[at] = value;
    std::string joined;
    for (std::size_t i = 0; i < copy.size(); ++i) joined += (i ? "|" : "") + copy[i];
    return withLine(saved, "EXPLORED", "EXPLORED " + joined);
  };
  int plainFloor = 1;
  while (plainFloor == merchantFloor || indexOf(*hero, plainFloor, SpotKind::Merchant) >= 0 ||
         isForkFloor(1234, plainFloor)) ++plainFloor;
  const int forkFloor = chapterPlan(1234, 0).forkFloors[0];
  require(branchTaken(*loaded, forkFloor) == Branch::Fog, "the branch taken round trips");
  int noKey = 0; // A fork floor without the key branch.
  for (int f = 1; f <= 20 && !noKey; ++f) {
    const auto ways = branches(1234, f);
    if (!ways.empty() && std::find(ways.begin(), ways.end(), Branch::Locked) == ways.end()) noKey = f;
  }
  const auto branchMask = [&](int at, Branch branch) {
    return withMask(at, std::to_string((std::stoi(masks[at]) & ((1 << BranchBit) - 1)) | (static_cast<int>(branch) << BranchBit)));
  };
  std::vector<std::pair<std::string, std::string>> broken = {
      {"spot bits beyond the floor", withMask(1, std::to_string(1 << 15))},
      {"records above the highest floor", withMask(21, "1")},
      {"goods without a merchant", withMask(plainFloor, std::to_string(std::stoi(masks[plainFloor]) | (1 << MerchantBit)))},
      {"counter mismatch", withLine(saved, "SPOTS_EXPLORED", "SPOTS_EXPLORED " + std::to_string(count + 1))},
      {"vault mismatch", withLine(saved, "VAULTS_OPENED", "VAULTS_OPENED 9")},
      {"quest mismatch", withLine(saved, "QUEST_ITEMS", "QUEST_ITEMS 0|0|0|0|0|0|0|0|0|0")},
      {"too many keys", withLine(saved, "KEYS", "KEYS 100")},
      {"unknown boon", withLine(saved, "BOON", "BOON 5")},
      {"unknown bane", withLine(saved, "BANE", "BANE 3")},
      {"ingredient overflow", withLine(saved, "INGREDIENTS", "INGREDIENTS 0|0|0|100|0")},
      {"short array", withLine(saved, "INGREDIENTS", "INGREDIENTS 0|0|0")},
      {"a branch off a fork floor", branchMask(plainFloor, Branch::Fog)},
      {"an unknown branch", withMask(forkFloor, std::to_string(std::stoi(masks[forkFloor]) | (1 << (BranchBit + 2))))},
  };
  if (noKey) broken.push_back({"a branch the fork lacks", branchMask(noKey, Branch::Locked)});
  for (const auto &[what, text] : broken) {
    file.write(text);
    floor = 42;
    require(!SaveSystem::loadGame(floor, file.path.string()) && floor == 42, "reject " + what);
  }
}

void testPlayWithoutExplorationUnchanged() {
  auto a = campaignHero(321), b = campaignHero(321);
  for (int floor = 1; floor <= 9; ++floor)
    for (const auto &view : survey(*b, floor))
      if (view.spot.kind == SpotKind::Gather || view.spot.kind == SpotKind::Lore) interact(*b, floor, view.index, 1);
  for (int floor = 1; floor <= 100; ++floor) {
    const enemy x = Campaign::makeEnemy(*a, floor), y = Campaign::makeEnemy(*b, floor);
    require(x.getattack() == y.getattack() && x.getmaxhp() == y.getmaxhp() && x.getAffixes() == y.getAffixes(),
            "spots without combat effects leave guardians alone");
  }
}
} // namespace

int main() {
  std::ostringstream gameLog;
  auto *originalOut = std::cout.rdbuf(gameLog.rdbuf());
  auto *originalError = std::cerr.rdbuf(gameLog.rdbuf());
  std::string failure;
  try {
    testGeneration();
    testForkPlacement();
    testForkBranches();
    testPatrols();
    testInteractionRules();
    testDeterministicOutcomes();
    testCurioCosts();
    testAffixSource();
    testBoonsAndBanes();
    testVaultsAndMerchants();
    testQuestItems();
    testPuzzles();
    testCooking();
    testLoreAndProfile();
    testSaves();
    testPlayWithoutExplorationUnchanged();
  } catch (const std::exception &error) {
    failure = error.what();
  }
  std::cout.rdbuf(originalOut);
  std::cerr.rdbuf(originalError);
  if (!failure.empty()) {
    std::cerr << "Exploration tests FAILED: " << failure << "\n";
    return 1;
  }
  std::cout << "Exploration tests passed: generation, forks, patrols, quotas, interactions, determinism, affix sources, "
               "boons, vaults, merchants, quest items, puzzles, cooking, journal, achievements, saves.\n";
  return 0;
}
