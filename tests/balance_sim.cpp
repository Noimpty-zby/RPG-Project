#include "combat.h"
#include "jobs.h"
#include "progression.h"
#include "skills.h"
#include "world.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

// Representative fresh encounters, not a simulated 1-100 campaign. Both
// policies receive identical equipment, enemy affixes, HP/MP and RNG seeds.
// Different actions can consume different numbers of RNG draws thereafter.
namespace {
constexpr int turnLimit = 80;
enum class Job { Warrior, Mage, Assassin };
constexpr std::array<Job, 3> jobs{Job::Warrior, Job::Mage, Job::Assassin};

const char *jobName(Job job) {
  if (job == Job::Warrior) return "Warrior";
  if (job == Job::Mage) return "Mage";
  return "Assassin";
}

JobReq jobRequirement(Job job) {
  if (job == Job::Warrior) return WarriorOnly;
  if (job == Job::Mage) return MageOnly;
  return AssassinOnly;
}

std::unique_ptr<player> newHero(Job job) {
  if (job == Job::Warrior) return std::make_unique<Warrior>("sim");
  if (job == Job::Mage) return std::make_unique<Mage>("sim");
  return std::make_unique<Assassin>("sim");
}

struct Fixture {
  enemy monster;
  Item weapon;
  Item armor;
  Item potion;
  unsigned int combatSeed;
};

Fixture generateFixture(Job job, int floor, unsigned int seed) {
  std::srand(seed);
  enemy monster = generateEnemy(floor);
  std::optional<Item> weapon, armor, potion;
  // Take the first eligible generated item of each type, not the best of N.
  // Gear is generated at the actual encounter floor using production tables.
  for (int attempts = 0; attempts < 10000 && (!weapon || !armor || !potion);
       ++attempts) {
    Item item = generateRandomItem(floor);
    if (!potion && item.getType() == Potion) potion = item;
    if (item.getJobRequirement() != jobRequirement(job)) continue;
    if (!weapon && item.getType() == Weapon) weapon = item;
    if (!armor && item.getType() == Armor) armor = item;
  }
  if (!weapon || !armor || !potion)
    throw std::runtime_error("Unable to generate an eligible fixture loadout");
  potion->setQuantity(3);
  return {monster, *weapon, *armor, *potion, seed ^ 0x9e3779b9U};
}

std::unique_ptr<player> prepareHero(Job job, int floor, const Fixture &fixture) {
  auto hero = newHero(job);
  const int level = std::max(1, floor * 7 / 10);
  while (hero->getlevel() < level) hero->levelup();
  hero->setHighestFloor(floor);
  for (int slot = 1; slot <= hero->getSkillCount(); ++slot) {
    skill *ability = hero->getSkill(slot);
    if (floor >= ability->getunlockfloor() &&
        level >= ability->getlevelrequired())
      ability->restoreProgress(Progression::skillRankCap(floor), 0);
  }
  hero->equipItem_T(fixture.weapon);
  hero->equipItem_T(fixture.armor);
  if (hero->getSaveEquipments().size() != 2)
    throw std::runtime_error("Fixture equipment was rejected");
  hero->addItem(fixture.potion);
  hero->sethp(hero->getmaxhp());
  hero->setmp(hero->getmaxmp());
  return hero;
}

std::array<int, 5> skillCycle(Job job) {
  // Useful class-specific openers, then rotate through all available skills.
  // Both policies use this exact cycle and skip unavailable skills for free.
  if (job == Job::Warrior) return {3, 2, 4, 5, 1};
  if (job == Job::Mage) return {2, 4, 5, 1, 3};
  return {5, 2, 3, 4, 1};
}

int actionId(BattleAction action) {
  if (action.type == ActionType::Skill) return action.index;
  if (action.type == ActionType::Guard) return -1;
  if (action.type == ActionType::Potion) return -2;
  return 0;
}

bool healsHp(Job job, BattleAction action) {
  if (action.type == ActionType::Potion) return true;
  if (action.type != ActionType::Skill) return false;
  if (job == Job::Warrior) return action.index == 2 || action.index == 5;
  return job == Job::Assassin && action.index == 4;
}

struct Policy {
  Job job;
  bool aware;
  int initialEnemyAttack;
  std::size_t cursor = 0;
  int overrides = 0;
  int recoveryPotions = 0;
  int emergencySuppressedHeals = 0;

  BattleAction choose(player &hero, const enemy &monster) {
    std::vector<BattleAction> candidates;
    int potionIndex = 0;
    const auto &backpack = hero.getSaveBackpack();
    for (std::size_t i = 0; i < backpack.size(); ++i) {
      if (backpack[i].getType() == Potion && backpack[i].getQuantity() > 0) {
        potionIndex = static_cast<int>(i + 1);
        break;
      }
    }
    if (potionIndex && hero.gethp() * 100 < hero.getmaxhp() * 35)
      candidates.push_back({ActionType::Potion, potionIndex});
    const auto cycle = skillCycle(job);
    bool hasReadySkill = false;
    int cheapestUnlocked = hero.getmaxmp();
    for (std::size_t offset = 0; offset < cycle.size(); ++offset) {
      int slot = cycle[(cursor + offset) % cycle.size()];
      skill *ability = hero.getSkill(slot);
      if (ability->getlevelrequired() <= hero.getlevel() &&
          ability->getunlockfloor() <= hero.getHighestFloor())
        cheapestUnlocked = std::min(cheapestUnlocked, ability->getcostmp());
      if (ability->canUse(hero)) {
        candidates.push_back({ActionType::Skill, slot});
        hasReadySkill = true;
      }
    }
    // A shared resource-recovery fallback: guard only when no skill is ready
    // and MP is below the cheapest unlocked skill's cost. Otherwise attack.
    if (!hasReadySkill && hero.getmp() < cheapestUnlocked)
      candidates.push_back({ActionType::Guard, 0});
    candidates.push_back({ActionType::Attack, 0});
    candidates.push_back({ActionType::Guard, 0});

    const auto intent = monster.getIntent();
    const int baselineAction = actionId(candidates.front());
    const int incoming = monster.getattack() * intent.damagePercent / 100;
    // This is a simple visible-risk heuristic, not a lookahead or optimal solver.
    const int poison = monster.hasAffix(Affix::POSIONOUS)
                           ? std::max(1, std::min(monster.getattack() / 5,
                                                  hero.getmaxhp() / 20))
                           : 0;
    const bool emergency = hero.gethp() <= incoming + poison;
    auto acceptable = [&](BattleAction action) {
      if (intent.kind == EnemyIntentKind::ReadPattern &&
          actionId(action) == intent.predictedAction) return false;
      if (intent.kind == EnemyIntentKind::Shatter &&
          action.type == ActionType::Guard) return false;
      // Do not defer a lifesaving heal just because its efficiency is halved.
      // Healing skills still deal damage: only save them for a recovery window
      // when a meaningful amount of HP is missing and the next hit is survivable.
      if (intent.kind == EnemyIntentKind::Wound && !emergency &&
          hero.gethp() * 100 < hero.getmaxhp() * 85 && healsHp(job, action))
        return false;
      return true;
    };
    if (aware) {
      if (intent.kind == EnemyIntentKind::Recover && potionIndex &&
          hero.gethp() * 100 < hero.getmaxhp() * 65)
        candidates.insert(candidates.begin(), {ActionType::Potion, potionIndex});

      // Reapply a class's weakening skill when its old effect has expired and
      // the next attack threatens at least half our current HP. Do not keep
      // refreshing an already-active debuff at the expense of other attacks.
      int weakeningSlot = job == Job::Warrior ? 3 : job == Job::Mage ? 2 : 0;
      if (weakeningSlot && incoming > 0 && incoming * 2 >= hero.gethp() &&
          monster.getattack() >= initialEnemyAttack &&
          hero.getSkill(weakeningSlot)->canUse(hero) &&
          acceptable({ActionType::Skill, weakeningSlot})) {
        const int reduction = job == Job::Warrior
                                  ? 35
                                  : monster.hasAffix(Affix::STRONG) ? 45 : 30;
        if (hero.gethp() > incoming * (100 - reduction) / 100)
          candidates.insert(candidates.begin(), {ActionType::Skill, weakeningSlot});
      }
    }
    std::size_t selected = 0;
    if (aware) {
      for (; selected < candidates.size(); ++selected) {
        if (acceptable(candidates[selected])) break;
      }
      if (selected == candidates.size())
        throw std::runtime_error("Intent filtering removed every legal action");
    }
    BattleAction chosen = candidates[selected];
    // A last-resort guard can survive a currently lethal attack and advance
    // cooldowns into the next turn. Preserve available healing/weakening actions.
    if (aware && incoming >= hero.gethp() &&
        hero.gethp() > incoming / 2 + poison &&
        !healsHp(job, chosen) &&
        !(chosen.type == ActionType::Skill &&
          ((job == Job::Warrior && chosen.index == 3) ||
           (job == Job::Mage && chosen.index == 2))) &&
        acceptable({ActionType::Guard, 0}))
      chosen = {ActionType::Guard, 0};
    if (aware && actionId(chosen) != baselineAction) ++overrides;
    if (chosen.type == ActionType::Potion && intent.kind == EnemyIntentKind::Recover)
      ++recoveryPotions;
    if (healsHp(job, chosen) && intent.kind == EnemyIntentKind::Wound && emergency)
      ++emergencySuppressedHeals;
    if (chosen.type == ActionType::Skill) {
      for (std::size_t i = 0; i < cycle.size(); ++i)
        if (cycle[i] == chosen.index) cursor = (i + 1) % cycle.size();
    }
    return chosen;
  }
};

struct Result {
  bool won;
  bool timedOut;
  int turns;
  int potions;
  int guards;
  int overrides;
  int recoveryPotions;
  int emergencySuppressedHeals;
  double hpPercent;
};

Result simulate(Job job, int floor, const Fixture &fixture, bool aware) {
  auto hero = prepareHero(job, floor, fixture);
  enemy monster = fixture.monster;
  std::srand(fixture.combatSeed);
  BattleSession battle(*hero, monster);
  Policy policy{job, aware, monster.getattack()};
  int potions = 0;
  int guards = 0;
  while (!battle.finished() && battle.turns() < turnLimit) {
    if (!battle.beginTurn()) break;
    BattleAction choice = policy.choose(*hero, monster);
    if (!battle.act(choice))
      throw std::runtime_error("Simulation selected an unavailable action");
    if (choice.type == ActionType::Potion) ++potions;
    if (choice.type == ActionType::Guard) ++guards;
  }
  // Mutual death (e.g. explosive affix) is a loss. No victory rewards or
  // between-encounter healing are applied in a representative single fight.
  bool won = hero->isalive() && !monster.isalive();
  return {won, !battle.finished(), battle.turns(), potions, guards,
          policy.overrides, policy.recoveryPotions, policy.emergencySuppressedHeals,
          100.0 * hero->gethp() / std::max(1, hero->getmaxhp())};
}

struct Totals {
  int runs = 0, wins = 0, timeouts = 0, maxTurns = 0;
  long long turns = 0, potions = 0, guards = 0, overrides = 0;
  long long recoveryPotions = 0, emergencySuppressedHeals = 0;
  double hpPercent = 0, winnerHpPercent = 0;
  void add(const Result &result) {
    ++runs;
    wins += result.won;
    timeouts += result.timedOut;
    maxTurns = std::max(maxTurns, result.turns);
    turns += result.turns;
    potions += result.potions;
    guards += result.guards;
    overrides += result.overrides;
    recoveryPotions += result.recoveryPotions;
    emergencySuppressedHeals += result.emergencySuppressedHeals;
    hpPercent += result.hpPercent;
    if (result.won) winnerHpPercent += result.hpPercent;
  }
};

class NullOutput : public std::streambuf {
  int overflow(int character) override { return traits_type::not_eof(character); }
};

class SilenceGameplay {
  NullOutput sink;
  std::streambuf *previous;
public:
  SilenceGameplay() : previous(std::cout.rdbuf(&sink)) {}
  ~SilenceGameplay() { std::cout.rdbuf(previous); }
};

void printRow(int milestone, int floor, const char *encounter, Job job,
              bool aware, const Totals &total) {
  std::cout << milestone << ',' << floor << ',' << encounter << ','
            << jobName(job) << ',' << (aware ? "intent_aware" : "fixed_cycle")
            << ',' << floor * 7 / 10 << ',' << Progression::skillRankCap(floor)
            << ',' << total.runs << ',' << total.wins << ','
            << total.runs - total.wins - total.timeouts << ',' << total.timeouts
            << ',' << 100.0 * total.wins / total.runs << ','
            << static_cast<double>(total.turns) / total.runs << ','
            << total.maxTurns << ','
            << static_cast<double>(total.potions) / total.runs << ','
            << total.hpPercent / total.runs << ',';
  if (total.wins) std::cout << total.winnerHpPercent / total.wins;
  else std::cout << "NA";
  std::cout << ',' << static_cast<double>(total.guards) / total.runs << ','
            << static_cast<double>(total.overrides) / total.runs << ','
            << static_cast<double>(total.recoveryPotions) / total.runs << ','
            << static_cast<double>(total.emergencySuppressedHeals) / total.runs << '\n';
}
} // namespace

int main(int argc, char **argv) {
  try {
    int seeds = argc > 1 ? std::stoi(argv[1]) : 100;
    if (argc > 2 || seeds < 1 || seeds > 10000)
      throw std::runtime_error("Usage: balance_sim [seed_count: 1..10000]");
    std::cerr
        << "Representative encounter simulation; NOT a 1-100 campaign clear test.\n"
        << "Ordinary floors 41/61/81/99 accompany boss milestones 40/60/80/100.\n"
        << "Each pair shares generated enemy, first matching class equipment, "
           "full HP/MP, 3 same-floor potions, battle RNG seed, and no revives.\n"
        << "Level=floor*0.7 rounded down; unlocked skills are fully funded to "
           "the current progression rank cap. All cooldowns begin ready.\n"
        << "Both policies cycle through available skills, use potions below "
           "35% HP, and guard if no skill is ready and MP is insufficient.\n"
        << "Intent-aware avoids predictions and guard against Shatter, permits "
           "emergency suppressed heals, drinks below 65% HP during Recover, "
           "and prioritizes expired weakening or a lifesaving guard against "
           "dangerous attacks. Fixed-cycle keeps its 35% potion threshold.\n"
        << "These are transparent heuristics, not optimal strategies; awareness "
           "is not assumed to improve every encounter or seed.\n"
        << "80-turn limit; timeouts reported separately; all-run averages include "
           "losses/timeouts; remaining HP is before victory rewards.\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "milestone_floor,encounter_floor,encounter,job,policy,hero_level,"
                 "rank_cap,runs,wins,losses,timeouts,win_pct,avg_turns,max_turns,"
                 "avg_potions,avg_remaining_hp_pct,winner_avg_hp_pct,avg_guards,"
                 "avg_intent_overrides,avg_recovery_potions,"
                 "avg_emergency_suppressed_heals\n";
    for (int milestone : {40, 60, 80, 100}) {
      for (bool boss : {false, true}) {
        const int floor = boss ? milestone : (milestone == 100 ? 99 : milestone + 1);
        for (Job job : jobs) {
          Totals fixed, aware;
          {
            SilenceGameplay silence;
            for (int sample = 0; sample < seeds; ++sample) {
              unsigned int seed = static_cast<unsigned int>(
                  1000003 * floor + 10007 * static_cast<int>(job) + sample);
              auto fixture = generateFixture(job, floor, seed);
              fixed.add(simulate(job, floor, fixture, false));
              aware.add(simulate(job, floor, fixture, true));
            }
          }
          printRow(milestone, floor, boss ? "boss" : "ordinary", job, false, fixed);
          printRow(milestone, floor, boss ? "boss" : "ordinary", job, true, aware);
        }
      }
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Balance simulation failed: " << error.what() << '\n';
    return 1;
  }
}
