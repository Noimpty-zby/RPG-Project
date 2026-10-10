#include "core/game_config.h"
#include "game/campaign.h"
#include "player/jobs.h"
#include "player/skills.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <streambuf>
#include <stdexcept>
#include <string>

namespace {
class NullBuffer : public std::streambuf {
  int overflow(int c) override { return c; }
};
int potionCount(const player &hero) {
  int count = 0;
  for (const auto &item : hero.getSaveBackpack())
    if (item.getType() == Potion) count += item.getQuantity();
  return count;
}
int potionIndex(const player &hero) {
  const auto &items = hero.getSaveBackpack();
  for (std::size_t i = 0; i < items.size(); ++i)
    if (items[i].getType() == Potion) return static_cast<int>(i + 1);
  return 0;
}
void equipUpgrades(player &hero) {
  for (ItemType type : {Weapon, Armor}) {
    int best = -1, value = 0;
    for (const auto &eq : hero.getSaveEquipments()) if (eq.getType() == type) value = eq.getValue();
    const auto &items = hero.getSaveBackpack();
    for (std::size_t i = 0; i < items.size(); ++i) {
      const auto &item = items[i];
      if (item.getType() == type && item.getValue() > value && item.getDurability() > 0 &&
          item.getLevelRequirement() <= hero.getlevel() &&
          (item.getJobRequirement() == AnyJob || item.getJobRequirement() == Campaign::jobOf(hero))) {
        best = static_cast<int>(i + 1); value = item.getValue();
      }
    }
    if (best > 0) hero.equipItem(best);
  }
}
void prepare(player &hero, int floor, int build) {
  Campaign::claimOrder(hero, floor);
  equipUpgrades(hero);
  for (int slot = 1; slot <= hero.getSkillCount(); ++slot) {
    auto *ability = hero.getSkill(slot);
    if (ability->canUpgrade(hero.getlevel(), hero.getHighestFloor()) &&
        hero.getcoin() >= ability->getlevelUpCost() + 150) hero.levelUpSkill(slot);
  }
  for (int slot = 1; slot <= 2; ++slot)
    if (hero.getSkill(slot)->getSpecialization() == 0) hero.chooseSpecialization(slot, build);
  const auto stock = Campaign::shopStock(hero, floor);
  for (int slot = 1; slot <= 2; ++slot) {
    int value = 0;
    for (const auto &eq : hero.getSaveEquipments()) if (eq.getType() == stock[slot - 1].getType()) value = eq.getValue();
    if (stock[slot - 1].getLevelRequirement() <= hero.getlevel() && stock[slot - 1].getValue() > value)
      Campaign::purchase(hero, floor, slot);
  }
  equipUpgrades(hero);
  if (hero.gethp() * 100 < hero.getmaxhp() * 65) {
    if (!potionIndex(hero)) {
      Campaign::purchase(hero, floor, 3); Campaign::purchase(hero, floor, 4);
    }
    if (hero.gethp() * 100 < hero.getmaxhp() * 40) Campaign::rest(hero, floor);
    // A real player can buy another stock with earned gold. Both policies use
    // the same camp rules, including prices and the persisted refresh cap.
    if (!potionIndex(hero) && Campaign::refreshShop(hero, floor)) {
      Campaign::purchase(hero, floor, 3); Campaign::purchase(hero, floor, 4);
    }
    for (int use = 0; use < 3 && hero.gethp() * 100 < hero.getmaxhp() * 65 && potionIndex(hero); ++use)
      hero.useItem(potionIndex(hero));
  }
  // Before a boss, both policies spend spare coins on potions like a player
  // preparing for a known fight: buy stocked potions, refreshing if needed.
  if (floor % 10 == 0) {
    for (int refreshes = 0; potionCount(hero) < 4 && refreshes < 5; ++refreshes) {
      Campaign::purchase(hero, floor, 3); Campaign::purchase(hero, floor, 4);
      if (potionCount(hero) >= 4 || !Campaign::refreshShop(hero, floor)) break;
    }
  }
  // Materials change a single effect; they never add permanent attack.
  if (hero.runState().materials >= 10 && !hero.getSaveEquipments().empty())
    hero.modifyEquipment(1, static_cast<int>(ItemEffect::SkillEfficiency));
}
BattleAction choose(player &hero, enemy &monster, BattleSession &battle, bool aware, int &cursor) {
  const auto intent = monster.getIntent();
  const int potion = potionIndex(hero);
  const bool emergency = hero.gethp() * 100 < hero.getmaxhp() * 28;
  if (potion && (emergency || (aware && intent.healingPercent == 100 && hero.gethp() * 100 < hero.getmaxhp() * 55)))
    return {ActionType::Potion, potion};
  if (aware && intent.interruptible) return {ActionType::Attack};
  if (aware && battle.objective() == EncounterObjective::Escape && battle.turns() >= 2)
    return {ActionType::Guard};
  if (aware && intent.kind == EnemyIntentKind::HeavyStrike && hero.gethp() < monster.getattack() * 2)
    return {ActionType::Guard};
  const std::array<int, 5> sequence = hero.job() == Job::Warrior ? std::array<int,5>{3,2,4,5,1} :
    hero.job() == Job::Mage ? std::array<int,5>{2,4,5,1,3} : std::array<int,5>{5,2,3,4,1};
  for (int i = 0; i < 5; ++i) {
    const int slot = sequence[cursor++ % 5];
    if (hero.getSkill(slot)->canUse(hero) && (!aware || slot != intent.predictedAction))
      return {ActionType::Skill, slot};
  }
  return {ActionType::Attack};
}
struct Result { int floor, level, coins, turns, potions, deaths, upgrades, drought, timeouts, objectiveWins; };
// First attempt at each boss, per class: [job][chapter] = {wins, attempts}.
std::array<std::array<std::array<int, 2>, 10>, 3> bossFirstAttempts{};
Result run(int job, unsigned seed, int route, int challenge, bool aware) {
  std::unique_ptr<player> hero;
  if (job == 0) hero = std::make_unique<Warrior>("sim");
  else if (job == 1) hero = std::make_unique<Mage>("sim");
  else hero = std::make_unique<Assassin>("sim");
  Campaign::initialize(*hero, seed, challenge);
  int floor = 1, timeouts = 0, objectiveWins = 0;
  for (; floor <= 100; ++floor) {
    hero->setHighestFloor(floor);
    Campaign::chooseRoute(*hero, floor, route);
    if (Campaign::eventPending(*hero, floor))
      Campaign::resolveEvent(*hero, floor, hero->getcoin() >= GameConfig::restCost(floor) ? 1 : 2);
    bool won = false;
    for (int retry = 0; retry < 3 && !won; ++retry) {
      prepare(*hero, floor, seed % 2 + 1);
      auto monster = Campaign::makeEnemy(*hero, floor);
      Campaign::seedStream(*hero, floor, 1200 + static_cast<unsigned>(hero->runState().attempts[floor]));
      BattleSession battle(*hero, monster, Campaign::rulesFor(*hero, floor));
      int cursor = 0;
      while (battle.beginTurn()) {
        const auto action = choose(*hero, monster, battle, aware, cursor);
        if (!battle.act(action)) throw std::runtime_error("simulation submitted invalid action");
      }
      timeouts += battle.report().timeout;
      if (battle.turns() > battle.turnLimit()) throw std::runtime_error("encounter exceeded its turn bound");
      if (battle.won() && monster.isalive()) ++objectiveWins;
      if (challenge == 0 && retry == 0 && floor % 10 == 0) {
        auto &tally = bossFirstAttempts[job][floor / 10 - 1];
        tally[0] += battle.won();
        ++tally[1];
      }
      won = Campaign::settleEncounter(*hero, monster, battle, floor);
      if (!won && !hero->isalive()) {
        if (!Campaign::revive(*hero, hero->getfreerevives() > 0)) break;
      }
    }
    if (!won) break;
  }
  const auto &r = hero->runState();
  return {floor,hero->getlevel(),hero->getcoin(),r.turns,r.potionsUsed,r.deaths,r.usefulDrops,r.longestGearDrought,timeouts,objectiveWins};
}
}
// Usage: campaign_sim [--check] [--seeds N] [--bosses]
//   --check   enforce the regression gates below (exit 1 on failure)
//   --seeds N run seeds 1..N (default 8)
//   --bosses  print standard-mode first-attempt boss win rates instead of CSV
int main(int argc, char **argv) {
  bool check = false, bosses = false;
  unsigned seeds = 8;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--check") check = true;
    else if (arg == "--bosses") bosses = true;
    else if (arg == "--seeds" && i + 1 < argc) seeds = static_cast<unsigned>(std::max(1, std::atoi(argv[++i])));
    else { std::cerr << "usage: campaign_sim [--check] [--seeds N] [--bosses]\n"; return 2; }
  }
  NullBuffer quiet;
  auto *output = std::cout.rdbuf(&quiet);
  const char *names[] = {"Warrior","Mage","Assassin"};
  std::array<int, 3> bestFloor{};
  std::array<int, 3> standardObjectiveWins{};
  std::array<int, 3> standardClears{};
  int timeouts = 0, awareTimeouts = 0, invalid = 0, objectiveWins = 0;
  std::string csv = "job,seed,route,challenge,policy,ending_floor,level,coins,turns,potions,deaths,gear_upgrades,longest_drought,timeouts,objective_wins\n";
  for (int job = 0; job < 3; ++job) for (unsigned seed = 1; seed <= seeds; ++seed)
    for (int route = 1; route <= 3; ++route) for (int challenge = 0; challenge < 3; ++challenge)
      for (int aware = 0; aware < 2; ++aware) {
        const auto r = run(job, seed, route, challenge, aware != 0);
        if (challenge == 0 && aware) {
          bestFloor[job] = std::max(bestFloor[job], r.floor);
          standardObjectiveWins[job] += r.objectiveWins;
          standardClears[job] += r.floor == 101;
        }
        timeouts += r.timeouts;
        if (aware && challenge == 0) awareTimeouts += r.timeouts;
        objectiveWins += r.objectiveWins;
        invalid += r.coins < 0 || r.level < 1 || r.level > 100 || r.turns <= 0 || r.floor < 1 || r.floor > 101;
        csv += std::string(names[job]) + ',' + std::to_string(seed) + ',' + std::to_string(route) + ',' +
          std::to_string(challenge) + ',' + (aware ? "aware" : "fixed") + ',' + std::to_string(r.floor) + ',' +
          std::to_string(r.level) + ',' + std::to_string(r.coins) + ',' + std::to_string(r.turns) + ',' +
          std::to_string(r.potions) + ',' + std::to_string(r.deaths) + ',' + std::to_string(r.upgrades) + ',' +
          std::to_string(r.drought) + ',' + std::to_string(r.timeouts) + ',' + std::to_string(r.objectiveWins) + '\n';
      }
  std::cout.rdbuf(output);
  if (bosses) {
    std::cout << "Standard mode, first attempt at each boss (both policies), win %\njob     ";
    for (int chapter = 1; chapter <= 10; ++chapter) std::cout << std::setw(5) << chapter * 10;
    std::cout << '\n';
    for (int job = 0; job < 3; ++job) {
      std::cout << std::left << std::setw(8) << names[job] << std::right;
      for (const auto &tally : bossFirstAttempts[job])
        std::cout << std::setw(5) << (tally[1] ? tally[0] * 100 / tally[1] : -1);
      std::cout << '\n';
    }
  } else {
    std::cout << csv;
  }
  // Dodge and emergency healing can make even an aware policy miss a timed
  // objective. That is a recoverable loss, not an engine hang; the per-fight
  // bound above is mandatory. Every class must still complete non-kill goals
  // and have a reachable standard-mode campaign in these fixtures.
  // Reachability is not a claim about optimal human play or enjoyment.
  // Loose regression guard: with revives, the intent-aware policy should clear
  // standard mode in at least half of its runs for every class.
  const int runsPerClass = static_cast<int>(seeds) * 3;
  const int totalRuns = runsPerClass * 3 * 3 * 2;
  if (check && (invalid || objectiveWins == 0 ||
      *std::min_element(standardObjectiveWins.begin(), standardObjectiveWins.end()) == 0 ||
      *std::min_element(bestFloor.begin(), bestFloor.end()) < 101 ||
      *std::min_element(standardClears.begin(), standardClears.end()) * 2 < runsPerClass)) {
    std::cerr << "Campaign check failed: invalid=" << invalid << " timeouts=" << timeouts << " aware_timeouts=" << awareTimeouts
      << " standard-aware clears=" << standardClears[0] << ',' << standardClears[1] << ',' << standardClears[2]
      << " of " << runsPerClass << '\n';
    return 1;
  }
  if (check) std::cerr << "Campaign checks passed: " << totalRuns << " runs; objective timeouts=" << timeouts
    << " (standard aware=" << awareTimeouts << "); standard-aware clears="
    << standardClears[0] << ',' << standardClears[1] << ',' << standardClears[2] << " of " << runsPerClass << '\n';
}
