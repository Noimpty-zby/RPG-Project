#include "core/game_config.h"
#include "game/campaign.h"
#include "game/exploration.h"
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
// Coins a run spends on camp services, for the pacing report.
struct Spend { long long enhance = 0, repair = 0, rest = 0, merchant = 0; };
// Wear the best item; carry an enhancement over when the inherited item beats
// it and the fee leaves three quarters of the purse, as for enhancing.
void equipUpgrades(player &hero, Spend &spend) {
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
    int heir = -1;
    for (std::size_t i = 0; i < items.size(); ++i) {
      const int index = static_cast<int>(i + 1), cost = hero.inheritCost(index);
      if (items[i].getType() == type && cost > 0 && static_cast<long long>(cost) * 4 <= hero.getcoin() &&
          hero.inheritValue(index) > value) {
        heir = index; value = hero.inheritValue(index);
      }
    }
    if (heir > 0) {
      spend.enhance += hero.inheritCost(heir);
      hero.inheritEnhancement(heir);
    } else if (best > 0) {
      hero.equipItem(best);
    }
  }
}
bool exploring = true;
// Both policies explore every floor on arrival like a careful player: take
// everything free, solve the puzzle, open vaults when keys allow, pay for a
// curio's sure result only when it is cheap, and skip gambles at low health.
void explore(player &hero, int floor, Spend &spend) {
  using Exploration::SpotKind;
  if (!exploring) return;
  auto &run = hero.runState();
  for (const auto &view : Exploration::survey(hero, floor)) {
    const int index = view.index;
    const bool healthy = hero.gethp() * 100 >= hero.getmaxhp() * 60;
    switch (view.spot.kind) {
    case SpotKind::Puzzle:
      if (!view.done) Exploration::solvePuzzle(hero, floor, Exploration::puzzleSolution(run.seed, floor));
      break;
    case SpotKind::Merchant: {
      const auto goods = Exploration::merchantGoods(hero, floor);
      for (std::size_t i = 0; i < goods.size(); ++i) {
        const auto &good = goods[i];
        const int coins = hero.getcoin();
        const bool wanted = good.kind == 1 ? run.keys < 2 && coins >= good.price * 4
                                           : coins >= good.price * 8;
        if (!good.sold && wanted && Exploration::interact(hero, floor, index, static_cast<int>(i) + 1).ok)
          spend.merchant += coins - hero.getcoin();
      }
      break;
    }
    case SpotKind::Curio: {
      if (view.done) break;
      const auto list = Exploration::options(hero, floor, index);
      const int variant = view.spot.variant;
      int choice = 1;
      if (list.size() == 2 && list[1].available &&
          ((variant == 0 && run.materials >= 6) ||
           (variant == 1 && hero.getcoin() >= GameConfig::restCost(floor) * 5) ||
           (variant == 2 && hero.getmp() * 100 >= hero.getmaxmp() * 80)))
        choice = 2;
      if (choice == 2 || healthy) Exploration::interact(hero, floor, index, choice);
      break;
    }
    case SpotKind::AffixSource:
      if (!view.done) Exploration::interact(hero, floor, index, run.materials >= 4 ? 2 : (healthy ? 1 : 0));
      break;
    case SpotKind::Vault:
      if (!view.done && run.keys >= view.spot.variant) Exploration::interact(hero, floor, index, 1);
      break;
    default:
      if (!view.done) Exploration::interact(hero, floor, index, 1);
      break;
    }
  }
}
// Cook a blessing before an elite or boss, or when ingredients pile up.
void cook(player &hero, int floor) {
  if (!exploring) return;
  auto &run = hero.runState();
  int total = 0;
  for (int count : run.ingredients) total += count;
  const bool major = floor % 10 == 0 || Campaign::isEliteFloor(floor);
  if (run.boon == 0 && (major || total >= 8))
    for (int recipe : {5, 1, 3, 2, 4, 6})
      if (Exploration::cook(hero, recipe).ok) break;
}
void prepare(player &hero, int floor, int build, Spend &spend) {
  Campaign::claimOrder(hero, floor);
  cook(hero, floor);
  equipUpgrades(hero, spend);
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
  equipUpgrades(hero, spend);
  if (hero.gethp() * 100 < hero.getmaxhp() * 65) {
    if (!potionIndex(hero)) {
      Campaign::purchase(hero, floor, 3); Campaign::purchase(hero, floor, 4);
    }
    if (hero.gethp() * 100 < hero.getmaxhp() * 40) {
      const int coins = hero.getcoin();
      if (Campaign::rest(hero, floor)) spend.rest += coins - hero.getcoin();
    }
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
  // Invest spare coins in worn gear, keeping most of the purse in reserve.
  for (int slot = 1; slot <= static_cast<int>(hero.getSaveEquipments().size()); ++slot)
    for (int cost = hero.enhanceCost(slot); cost > 0 && cost * 4 <= hero.getcoin(); cost = hero.enhanceCost(slot)) {
      if (!hero.enhanceEquipment(slot)) break;
      spend.enhance += cost;
    }
  // Keep worn gear alive when it is nearly broken and the repair is affordable.
  for (int slot = 1; slot <= static_cast<int>(hero.getSaveEquipments().size()); ++slot) {
    const Item &item = hero.getSaveEquipments()[slot - 1];
    const int cost = hero.repairCost(slot);
    if (item.getDurability() * 4 <= item.getMaxDurability() && cost > 0 && cost * 3 <= hero.getcoin()) {
      hero.repairEquipment(slot);
      spend.repair += cost;
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
  // Guard a telegraphed heavy strike that would take 30% or more of current HP,
  // unless a plain attack could finish the enemy first.
  if (aware && intent.kind == EnemyIntentKind::HeavyStrike &&
      monster.getattack() * intent.damagePercent / 100 * 10 >= hero.gethp() * 3 &&
      monster.gethp() > hero.getattack())
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
// Fight to the end with the policy; returns whether the hero won.
template <class Settle>
bool playOut(player &hero, enemy &monster, BattleSession &battle, bool aware, Settle settle) {
  int cursor = 0;
  while (battle.beginTurn()) {
    const auto action = choose(hero, monster, battle, aware, cursor);
    if (!battle.act(action)) throw std::runtime_error("simulation submitted invalid action");
  }
  if (battle.turns() > battle.turnLimit()) throw std::runtime_error("encounter exceeded its turn bound");
  return settle();
}
struct Result { int floor, level, coins, turns, potions, deaths, upgrades, drought, timeouts, objectiveWins, explored; };
// Standard-mode pacing per class, for --report.
struct Pacing {
  long long ordinaryFights = 0, ordinaryTurns = 0, ordinaryDamagePercent = 0;
  long long eliteAttempts = 0, eliteWins = 0, eliteTurns = 0;
  long long enhanceCoins = 0, repairCoins = 0, restCoins = 0, endCoins = 0, runs = 0;
  long long explored = 0, vaults = 0, puzzles = 0, meals = 0, merchantCoins = 0, keysLeft = 0;
  long long patrolFights = 0, patrolWins = 0;
  std::array<long long, 4> branches{}; // Taken per Exploration::Branch.
  std::array<std::array<int, 2>, 10> bossFirstAttempts{}; // {wins, attempts} per chapter
  // Bounties of chapters whose boss was cleared: {completed, offered} per kind and policy.
  std::array<std::array<std::array<int, 2>, Campaign::BountyKinds>, 2> bounties{};
};
std::array<Pacing, 3> pacing{};
// Both policies take a fork like a careful player who has just prepared at
// camp: fight the patrol with two thirds of their health, spend a key on the stash when hurt or
// rich in keys, else walk the fog (whose source explore() shuts first). A lost
// patrol fight is not retried. Returns false when the hero fell for good.
bool takeFork(player &hero, int floor, bool aware, Pacing &stats, bool standard) {
  using Exploration::Branch;
  const auto fork = Exploration::fork(hero, floor);
  if (fork.branches.empty() || fork.taken != Branch::None) return true;
  const auto offered = [&](Branch kind) {
    return std::any_of(fork.branches.begin(), fork.branches.end(),
                       [&](const Exploration::BranchView &b) { return b.kind == kind && b.available; });
  };
  const bool fresh = hero.gethp() * 100 >= hero.getmaxhp() * 65;
  if (offered(Branch::Patrol) && fresh) {
    auto monster = Campaign::makePatrol(hero, floor);
    Campaign::seedStream(hero, floor, Campaign::PatrolBattleStream);
    BattleSession battle(hero, monster, Campaign::patrolRules(hero, floor));
    const bool won = playOut(hero, monster, battle, aware,
                             [&] { return Campaign::settlePatrol(hero, monster, battle, floor); });
    if (standard) {
      ++stats.patrolFights;
      stats.patrolWins += won;
    }
    if (!hero.isalive() && !Campaign::revive(hero, hero.getfreerevives() > 0)) return false;
  }
  if (Exploration::branchTaken(hero, floor) == Branch::None) {
    const int keys = hero.runState().keys;
    const bool hurt = hero.gethp() * 100 < hero.getmaxhp() * 60;
    if (offered(Branch::Locked) && keys >= 1 && (hurt || keys >= 3)) Exploration::takeBranch(hero, floor, Branch::Locked);
    else Exploration::takeBranch(hero, floor, Branch::Fog);
  }
  if (standard) ++stats.branches[static_cast<int>(Exploration::branchTaken(hero, floor))];
  return true;
}
Result run(int job, unsigned seed, int route, int challenge, bool aware) {
  std::unique_ptr<player> hero;
  if (job == 0) hero = std::make_unique<Warrior>("sim");
  else if (job == 1) hero = std::make_unique<Mage>("sim");
  else hero = std::make_unique<Assassin>("sim");
  Campaign::initialize(*hero, seed, challenge);
  int floor = 1, timeouts = 0, objectiveWins = 0;
  Spend spend;
  Pacing &stats = pacing[job];
  for (; floor <= 100; ++floor) {
    hero->setHighestFloor(floor);
    Campaign::chooseRoute(*hero, floor, route);
    if (Campaign::eventPending(*hero, floor))
      Campaign::resolveEvent(*hero, floor, hero->getcoin() >= GameConfig::restCost(floor) ? 1 : 2);
    explore(*hero, floor, spend);
    if (!Exploration::fork(*hero, floor).branches.empty()) {
      prepare(*hero, floor, seed % 2 + 1, spend);
      if (!takeFork(*hero, floor, aware, stats, challenge == 0)) break;
      explore(*hero, floor, spend); // The branch and the guardian's side of the fork.
    }
    if (!Exploration::guardianReachable(*hero, floor)) throw std::runtime_error("fork left the guardian unreachable");
    bool won = false;
    for (int retry = 0; retry < 3 && !won; ++retry) {
      prepare(*hero, floor, seed % 2 + 1, spend);
      auto monster = Campaign::makeEnemy(*hero, floor);
      Campaign::seedStream(*hero, floor, 1200 + static_cast<unsigned>(hero->runState().attempts[floor]));
      const auto rules = Campaign::rulesFor(*hero, floor);
      const int startMaxHp = hero->getmaxhp();
      BattleSession battle(*hero, monster, rules);
      playOut(*hero, monster, battle, aware, [] { return true; });
      timeouts += battle.report().timeout;
      if (battle.won() && monster.isalive()) ++objectiveWins;
      if (challenge == 0 && floor % 10 == 0 && retry == 0) {
        auto &tally = stats.bossFirstAttempts[floor / 10 - 1];
        tally[0] += battle.won();
        ++tally[1];
      } else if (challenge == 0 && rules.elite && retry == 0) {
        ++stats.eliteAttempts;
        stats.eliteWins += battle.won();
        stats.eliteTurns += battle.turns();
      } else if (challenge == 0 && floor % 10 != 0 && !rules.elite &&
                 rules.objective == EncounterObjective::Defeat) {
        ++stats.ordinaryFights;
        stats.ordinaryTurns += battle.turns();
        stats.ordinaryDamagePercent += battle.report().damageTaken * 100LL / std::max(1, startMaxHp);
      }
      won = Campaign::settleEncounter(*hero, monster, battle, floor);
      if (!won && !hero->isalive()) {
        if (!Campaign::revive(*hero, hero->getfreerevives() > 0)) break;
      }
    }
    if (!won) break;
  }
  if (challenge == 0) {
    for (int c = 0; c < 10; ++c) {
      if (!hero->runState().cleared[(c + 1) * 10]) continue;
      const auto kinds = Campaign::chapterBounties(*hero, c);
      for (int slot = 0; slot < 2; ++slot) {
        auto &tally = stats.bounties[aware][kinds[slot]];
        tally[0] += hero->runState().bounties[c * 2 + slot] >= Campaign::bounty(kinds[slot]).target;
        ++tally[1];
      }
    }
    stats.enhanceCoins += spend.enhance;
    stats.repairCoins += spend.repair;
    stats.restCoins += spend.rest;
    stats.endCoins += hero->getcoin();
    ++stats.runs;
    const auto &r = hero->runState();
    stats.explored += r.spotsExplored;
    stats.vaults += r.vaultsOpened;
    stats.puzzles += r.puzzlesSolved;
    stats.meals += r.mealsCooked;
    stats.merchantCoins += spend.merchant;
    stats.keysLeft += r.keys;
  }
  const auto &r = hero->runState();
  return {floor,hero->getlevel(),hero->getcoin(),r.turns,r.potionsUsed,r.deaths,r.usefulDrops,r.longestGearDrought,timeouts,objectiveWins,r.spotsExplored};
}
}
// Usage: campaign_sim [--check] [--seeds N] [--report] [--no-explore]
//   --check   enforce the regression gates below (exit 1 on failure)
//   --seeds N run seeds 1..N (default 24)
//   --report  print standard-mode pacing (ordinary/elite fights, camp spending,
//             first-attempt boss win rates) instead of CSV
//   --no-explore  skip floor exploration and cooking (for comparisons)
int main(int argc, char **argv) {
  bool check = false, report = false;
  unsigned seeds = 24;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--check") check = true;
    else if (arg == "--report") report = true;
    else if (arg == "--seeds" && i + 1 < argc) seeds = static_cast<unsigned>(std::max(1, std::atoi(argv[++i])));
    else if (arg == "--no-explore") exploring = false;
    else { std::cerr << "usage: campaign_sim [--check] [--seeds N] [--report] [--no-explore]\n"; return 2; }
  }
  NullBuffer quiet;
  auto *output = std::cout.rdbuf(&quiet);
  const char *names[] = {"Warrior","Mage","Assassin"};
  std::array<int, 3> bestFloor{};
  std::array<int, 3> standardObjectiveWins{};
  std::array<int, 3> standardClears{};
  int timeouts = 0, awareTimeouts = 0, invalid = 0, objectiveWins = 0;
  std::string csv = "job,seed,route,challenge,policy,ending_floor,level,coins,turns,potions,deaths,gear_upgrades,longest_drought,timeouts,objective_wins,explored\n";
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
          std::to_string(r.drought) + ',' + std::to_string(r.timeouts) + ',' + std::to_string(r.objectiveWins) + ',' +
          std::to_string(r.explored) + '\n';
      }
  std::cout.rdbuf(output);
  if (report) {
    auto average = [](long long total, long long count, int scale = 1) {
      return count ? static_cast<double>(total) * scale / static_cast<double>(count) : 0.0;
    };
    std::cout << std::fixed << std::setprecision(1)
              << "Standard mode pacing (both policies; elite/boss = first attempts)\n"
              << "job      ordinary-turns ordinary-dmg% elite-win% elite-turns enhance-coins repair-coins rest-coins end-coins\n";
    for (int job = 0; job < 3; ++job) {
      const Pacing &p = pacing[job];
      std::cout << std::left << std::setw(8) << names[job] << std::right
                << std::setw(15) << average(p.ordinaryTurns, p.ordinaryFights)
                << std::setw(14) << average(p.ordinaryDamagePercent, p.ordinaryFights)
                << std::setw(11) << average(p.eliteWins, p.eliteAttempts, 100)
                << std::setw(12) << average(p.eliteTurns, p.eliteAttempts)
                << std::setw(14) << std::setprecision(0) << average(p.enhanceCoins, p.runs)
                << std::setw(13) << average(p.repairCoins, p.runs)
                << std::setw(11) << average(p.restCoins, p.runs)
                << std::setw(10) << average(p.endCoins, p.runs) << std::setprecision(1) << '\n';
    }
    std::cout << "\nStandard mode exploration per run\n"
              << "job      spots  vaults puzzles  meals merchant-coins keys-left patrol-win% patrol fog  key\n";
    for (int job = 0; job < 3; ++job) {
      const Pacing &p = pacing[job];
      std::cout << std::left << std::setw(8) << names[job] << std::right
                << std::setw(6) << average(p.explored, p.runs) << std::setw(8) << average(p.vaults, p.runs)
                << std::setw(8) << average(p.puzzles, p.runs) << std::setw(7) << average(p.meals, p.runs)
                << std::setw(15) << std::setprecision(0) << average(p.merchantCoins, p.runs)
                << std::setprecision(1) << std::setw(10) << average(p.keysLeft, p.runs)
                << std::setw(12) << average(p.patrolWins, p.patrolFights, 100)
                << std::setw(7) << average(p.branches[1], p.runs) << std::setw(5) << average(p.branches[2], p.runs)
                << std::setw(5) << average(p.branches[3], p.runs) << '\n';
    }
    std::cout << "\nFirst attempt at each boss, win %\njob     ";
    for (int chapter = 1; chapter <= 10; ++chapter) std::cout << std::setw(5) << chapter * 10;
    std::cout << '\n';
    for (int job = 0; job < 3; ++job) {
      std::cout << std::left << std::setw(8) << names[job] << std::right;
      for (const auto &tally : pacing[job].bossFirstAttempts)
        std::cout << std::setw(5) << (tally[1] ? tally[0] * 100 / tally[1] : -1);
      std::cout << '\n';
    }
    std::cout << "\nChapter bounties completed without aiming for them, % (fixed / aware)\n";
    for (int kind = 0; kind < Campaign::BountyKinds; ++kind) {
      std::array<int, 2> done{}, offered{};
      for (const auto &p : pacing)
        for (int aware = 0; aware < 2; ++aware) {
          done[aware] += p.bounties[aware][kind][0];
          offered[aware] += p.bounties[aware][kind][1];
        }
      for (int aware = 0; aware < 2; ++aware)
        std::cout << std::setw(6) << (offered[aware] ? done[aware] * 100 / offered[aware] : -1);
      std::cout << "  " << Campaign::bountyGoal(kind) << " (" << offered[0] + offered[1] << " offered)\n";
    }
  } else {
    std::cout << csv;
  }
  // Emergency healing and survival guards can make even an aware policy miss a timed
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
