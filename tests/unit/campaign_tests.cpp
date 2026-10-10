#include "core/game_config.h"
#include "core/progression.h"
#include "game/campaign.h"
#include "player/jobs.h"
#include "storage/save.h"
#include <cassert>
#include <array>
#include <cstdio>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
void prepareEventHero(Warrior &hero, int coins = 10000, int hp = 400,
                      int mp = 20, int maxMp = 80) {
  hero.loadStats(50, 0, coins, hp, 1000, 100, mp, maxMp, 0, 3);
  hero.setHighestFloor(100);
  hero.runState().seed = 713;
}
bool sameEnemy(const enemy &a, const enemy &b) {
  if (a.gethp() != b.gethp() || a.getattack() != b.getattack() ||
      a.getdodgeRate() != b.getdodgeRate() || a.getexp_reward() != b.getexp_reward() ||
      a.getcoin_reward() != b.getcoin_reward()) return false;
  for (Affix affix : {Affix::STRONG, Affix::TOUGH, Affix::QUICK, Affix::EXPLOSIVE,
                     Affix::POISONOUS, Affix::HEALING, Affix::SHIELDING})
    if (a.hasAffix(affix) != b.hasAffix(affix)) return false;
  return true;
}
void testRotatingObjectives() {
  for (int route = 1; route <= 3; ++route) {
    Warrior hero("goals");
    std::set<std::set<int>> layouts;
    for (int c = 0; c < 10; ++c) {
      hero.runState().routes[c] = route;
      std::set<int> positions;
      std::set<EncounterObjective> goals;
      for (int local = 1; local <= 10; ++local) {
        const auto rules = Campaign::rulesFor(hero, c * 10 + local);
        if (local == 10) require(rules.objective == EncounterObjective::Defeat,
                                 "Side objectives must never replace any chapter boss");
        if (rules.objective != EncounterObjective::Defeat) {
          positions.insert(local);
          goals.insert(rules.objective);
        }
      }
      require(positions.size() == 3, "Every chapter must have exactly three distinct side-goal floors");
      if (route == 3) require(goals.size() == 3, "Supply route must rotate survival, disruption and escape");
      layouts.insert(positions);
    }
    require(layouts.size() >= 3, "Later chapters must vary objective locations");
    require(Campaign::rulesFor(hero, 3).objective == EncounterObjective::Survive &&
                Campaign::rulesFor(hero, 6).objective == EncounterObjective::Disrupt &&
                Campaign::rulesFor(hero, 8).objective == (route == 1 ? EncounterObjective::Disrupt :
                  route == 2 ? EncounterObjective::Survive : EncounterObjective::Escape),
            "Chapter one must retain its original tutorial objectives");
  }
}
void testEventEntryAndResourceBoundaries() {
  for (int c = 0; c < 10; ++c) {
    const int floor = c * 10 + 5;
    for (int choice = 1; choice <= 2; ++choice) {
      Warrior hero("event"); prepareEventHero(hero);
      require(!Campaign::resolveEvent(hero, floor - 1, choice) &&
                  !Campaign::resolveEvent(hero, floor, 0), "Early and invalid events must be rejected");
      require(Campaign::resolveEvent(hero, floor, choice), "All chapter event choices must be executable");
      const std::array<int, 4> before{hero.gethp(), hero.getmp(), hero.getcoin(), hero.runState().materials};
      require(!Campaign::resolveEvent(hero, floor + 1, 3 - choice) && !Campaign::eventPending(hero, floor),
              "Completing either choice must prevent event reentry throughout the chapter");
      require(before == std::array<int, 4>{hero.gethp(), hero.getmp(), hero.getcoin(), hero.runState().materials},
              "Event reentry must not pay twice or grant duplicate rewards");
    }
    Warrior poor("poor"); prepareEventHero(poor, GameConfig::restCost(floor) - 1);
    const int coins = poor.getcoin();
    require(!Campaign::resolveEvent(poor, floor, 1) && poor.getcoin() == coins &&
                poor.runState().events[c] == 0, "Unaffordable events must not consume money or mark completion");
    require(Campaign::resolveEvent(poor, floor, 2), "An unaffordable paid choice must leave a free alternative");
    Warrior fragile("fragile"); prepareEventHero(fragile, 0, 1, 0, 0);
    fragile.runState().materials = GameConfig::MaxMaterials;
    require(Campaign::resolveEvent(fragile, floor, 2) && fragile.gethp() == 1 &&
                fragile.getmp() == 0 && fragile.runState().materials == GameConfig::MaxMaterials,
            "One HP, zero MP, no money and full materials must not soft-lock the mandatory event");
    Warrior dead("dead"); prepareEventHero(dead, 10000, 0);
    require(!Campaign::resolveEvent(dead, floor, 1) && !Campaign::resolveEvent(dead, floor, 2) &&
                dead.runState().events[c] == 0, "Events must not revive dead heroes");
  }
  Warrior supplied("supplied"); prepareEventHero(supplied);
  require(Campaign::resolveEvent(supplied, 5, 2) && supplied.gethp() == 340 &&
              supplied.runState().materials == 3, "Supply event must trade current HP for three materials");
  for (int mp : {0, 19, 20, 21}) {
    Warrior helper("helper"); prepareEventHero(helper, 0, 400, mp, 80);
    helper.equipItem_T(Item("recovery armor", 1, Armor, 500, 20, 100, 1));
    require(Campaign::resolveEvent(helper, 15, 2), "Purification assistance must always have a fallback");
    require(helper.getmp() == (mp >= 20 ? mp - 20 : mp) &&
                helper.gethp() == (mp >= 20 ? 500 : 380) && helper.runState().materials == 1,
            "Purification assistance must use max-MP cost and base-HP recovery, or only fallback HP loss");
  }
  Warrior oneMana("one mana"); prepareEventHero(oneMana, 0, 1000, 1, 1);
  require(Campaign::resolveEvent(oneMana, 15, 2) && oneMana.getmp() == 0 && oneMana.gethp() == 1000,
          "MP assistance must cost at least one MP and cap healing at full HP");
}
void testEventEnemyEffectsAndPrices() {
  Item sample("price fixture", 1, Potion, 10, 0, 101, 1);
  for (int c = 0; c < 10; ++c) {
    Warrior hero("prices"); prepareEventHero(hero);
    const int floor = c * 10 + 5;
    const auto ordinary = Campaign::makeEnemy(hero, floor);
    const auto boss = Campaign::makeEnemy(hero, (c + 1) * 10);
    require(Campaign::resolveEvent(hero, floor, 1), "Paid event fixture failed");
    const auto aided = Campaign::makeEnemy(hero, (c + 1) * 10);
    require(aided.getattack() == (c % 3 == 0 ? std::max(1, boss.getattack() * 90 / 100) : boss.getattack()),
            "Only aid chapters may lower boss attack");
    require(Campaign::makeEnemy(hero, floor).getattack() == ordinary.getattack(),
            "Paid chapter events must not change ordinary enemy attack");
    for (int challenge : {0, 1, 2}) {
      hero.runState().challenge = challenge;
      const int discounted = c % 3 == 0 ? 90 : 101;
      require(Campaign::purchasePrice(hero, floor, sample) == (challenge == 1 ? discounted * 125 / 100 : discounted),
              "Only aid grants a discount, with challenge pricing applied identically");
    }
    hero.runState().challenge = 0;
    const auto stock = Campaign::shopStock(hero, floor);
    const int before = hero.getcoin(), price = Campaign::purchasePrice(hero, floor, stock[2]);
    require(Campaign::purchase(hero, floor, 3) && hero.getcoin() == before - price,
            "Actual purchase must charge exactly the shared display price");
    require(sameEnemy(aided, Campaign::makeEnemy(hero, (c + 1) * 10)),
            "Shop access must not perturb deterministic event-modified enemies");
  }
  Warrior purifier("purifier"); prepareEventHero(purifier);
  bool sawPoison = false;
  for (unsigned seed = 1; seed <= 100; ++seed) {
    purifier.runState().seed = seed;
    purifier.runState().events[1] = 0;
    const auto original = Campaign::makeEnemy(purifier, 16);
    purifier.runState().events[1] = 1;
    const auto clean = Campaign::makeEnemy(purifier, 16);
    sawPoison |= original.hasAffix(Affix::POISONOUS);
    require(!clean.hasAffix(Affix::POISONOUS) && clean.gethp() == original.gethp() &&
                clean.getattack() == original.getattack() && clean.getdodgeRate() == original.getdodgeRate(),
            "Purification must remove poison without changing the enemy roll");
  }
  require(sawPoison, "Purification test must include a naturally poisoned enemy");
  Warrior intel("intel"); prepareEventHero(intel);
  const auto normal = Campaign::makeEnemy(intel, 26), boss = Campaign::makeEnemy(intel, 30);
  const auto neighbor = Campaign::makeEnemy(intel, 20);
  require(Campaign::resolveEvent(intel, 25, 1), "Intel event fixture failed");
  require(Campaign::makeEnemy(intel, 26).getdodgeRate() == std::max(0, normal.getdodgeRate() - 10) &&
              Campaign::makeEnemy(intel, 30).getdodgeRate() == std::max(0, boss.getdodgeRate() - 10) &&
              sameEnemy(neighbor, Campaign::makeEnemy(intel, 20)),
          "Intel must reduce ordinary and boss dodge only within its own chapter");
  Warrior bounty("bounty"); prepareEventHero(bounty, 0);
  const auto beforeBoss = Campaign::makeEnemy(bounty, 30), beforeNormal = Campaign::makeEnemy(bounty, 26);
  require(Campaign::resolveEvent(bounty, 25, 2) && bounty.getcoin() == GameConfig::restCost(25) * 2 &&
              Campaign::makeEnemy(bounty, 30).getattack() == std::max(1, beforeBoss.getattack() * 110 / 100) &&
              sameEnemy(beforeNormal, Campaign::makeEnemy(bounty, 26)),
          "Bounty must pay once while raising only that chapter boss attack");
  bounty.runState().challenge = 1;
  Item huge("huge price", 1, Potion, 1, 0, std::numeric_limits<int>::max(), 1);
  require(Campaign::purchasePrice(bounty, 25, huge) == std::numeric_limits<int>::max(),
          "Shared price calculation must not overflow on large prices");
}
void testEventsPersistWithoutNewFields() {
  for (int choice : {1, 2}) {
    Warrior hero("persisted events"); prepareEventHero(hero);
    for (int c = 0; c < 10; ++c)
      require(Campaign::resolveEvent(hero, c * 10 + 5, choice), "Event persistence fixture failed");
    const std::string path = "chapter-events-test.dat";
    require(SaveSystem::saveGame(hero, 95, path), "Event state must remain save-compatible");
    int floor = 1;
    auto restored = SaveSystem::loadGame(floor, path);
    require(restored && floor == 95 && restored->runState().events == hero.runState().events,
            "Existing event integers must restore every chapter outcome");
    const int coins = restored->getcoin(), materials = restored->runState().materials;
    for (int c = 0; c < 10; ++c) {
      require(!Campaign::resolveEvent(*restored, c * 10 + 5, choice),
              "Reloading must not allow repeat event payment or rewards");
      require(sameEnemy(Campaign::makeEnemy(hero, (c + 1) * 10), Campaign::makeEnemy(*restored, (c + 1) * 10)),
              "Reloading must preserve event-modified deterministic boss rolls");
    }
    require(restored->getcoin() == coins && restored->runState().materials == materials,
            "Persisted event reentry must leave resources unchanged");
    std::remove(path.c_str()); std::remove((path + ".bak").c_str());
  }
}
void testLivingTimeoutIsNotADeath() {
  Warrior hero("timeout"); prepareEventHero(hero);
  enemy blocker("blocker", 1000000, 0, 0, 100, 100, 1);
  blocker.configureEncounter(1, false); blocker.setdodgeRate(0);
  BattleSession battle(hero, blocker, {EncounterObjective::Escape});
  while (battle.beginTurn()) require(battle.act({ActionType::Attack}), "Timeout fixture must submit valid actions");
  const int coins = hero.getcoin(), exp = hero.getexp(), materials = hero.runState().materials;
  const auto items = hero.getSaveBackpack().size();
  require(battle.report().timeout && hero.isalive() && !battle.won(), "Timeout fixture must survive without meeting its goal");
  require(!Campaign::settleEncounter(hero, blocker, battle, 1) && hero.runState().deaths == 0 &&
              hero.runState().battles == 1 && hero.runState().attempts[1] == 1 &&
              hero.runState().turns == 15 && hero.runState().cleared[1] == 0,
          "A living timeout must record an attempt without counting a death or clear");
  require(hero.getcoin() == coins && hero.getexp() == exp && hero.runState().materials == materials &&
              hero.getSaveBackpack().size() == items, "A living timeout must not grant encounter rewards");
  require(!Campaign::settleEncounter(hero, blocker, battle, 1) && hero.runState().attempts[1] == 1,
          "A timed-out encounter must still settle at most once");
  hero.sethp(1);
  enemy lethal("lethal", 1000000, 100, 0, 0, 0, 1);
  lethal.configureEncounter(1, false); lethal.setdodgeRate(0);
  BattleSession loss(hero, lethal);
  require(loss.beginTurn() && loss.act({ActionType::Attack}) && !hero.isalive(), "Death fixture failed");
  require(!Campaign::settleEncounter(hero, lethal, loss, 1) && hero.runState().deaths == 1 &&
              hero.runState().attempts[1] == 2, "Actual death must still increment death statistics once");
}
} // namespace

void testEliteEncounters() {
  Warrior hero("elite");
  prepareEventHero(hero);
  int elites = 0;
  for (int floor = 1; floor <= GameConfig::MaxFloor; ++floor) {
    const auto rules = Campaign::rulesFor(hero, floor);
    if (!rules.elite) continue;
    ++elites;
    require(Campaign::isEliteFloor(floor) && Campaign::eliteFloor(GameConfig::chapter(floor)) == floor,
            "elite helpers must agree");
    require(floor % 10 != 0 && rules.objective == EncounterObjective::Defeat,
            "elites must never replace a boss or a side goal");
    require(rules.difficulty == 1, "elites must raise intent pressure by one step");
  }
  require(elites == 10, "every chapter needs exactly one elite floor");
  for (int c = 0; c < 10; ++c) {
    const int floor = Campaign::eliteFloor(c);
    hero.runState().attempts[floor] = 0;
    const enemy elite = Campaign::makeEnemy(hero, floor);
    require(elite.getAffixes().size() >= 2 && elite.getname().rfind("精英·", 0) == 0,
            "elites carry at least two affixes and a visible title");
    std::srand(static_cast<unsigned>(floor));
    const enemy plain = generateEnemy(floor);
    std::srand(static_cast<unsigned>(floor));
    const enemy strong = generateEnemy(floor, true);
    require(strong.getmaxhp() > plain.getmaxhp() && strong.getattack() >= plain.getattack() &&
                strong.getexp_reward() > plain.getexp_reward(),
            "elites must be tougher and more rewarding than the same floor's enemy");
  }
  require(generateEnemy(10, true).getname().find("精英") == std::string::npos,
          "boss floors never generate elites");

  // Objective guardians are scaled from the base curve, undoing the ordinary tuning.
  hero.runState().routes[0] = 3;
  require(Campaign::rulesFor(hero, 6).objective == EncounterObjective::Disrupt, "fixture floor is a disrupt goal");
  Campaign::seedStream(hero, 6, 10 + static_cast<unsigned>(hero.runState().attempts[6]));
  const enemy base = generateEnemy(6);
  const enemy guardian = Campaign::makeEnemy(hero, 6);
  require(guardian.getmaxhp() == base.getmaxhp() * EnemyTuning::DisruptHpPercent / EnemyTuning::OrdinaryHpPercent &&
              guardian.getattack() == std::max(1, base.getattack() * EnemyTuning::ObjectiveAttackPercent /
                                                       EnemyTuning::OrdinaryAttackPercent),
          "guardian stats must be relative to the base curve");

  // First clear of an elite pays a best-band piece of the loot target once.
  Warrior rewarded("reward");
  Campaign::initialize(rewarded, 9, 0);
  rewarded.runState().lootTarget = Armor;
  rewarded.runState().pity = 4;
  const int floor = Campaign::eliteFloor(0);
  for (int f = 1; f < floor; ++f) { rewarded.runState().cleared[f] = 1; rewarded.runState().attempts[f] = 1; }
  rewarded.setHighestFloor(floor);
  auto settle = [&](int &materials, std::size_t &items) {
    enemy target("elite fixture", 1, 0, 0, 10, 10, floor);
    target.configureEncounter(floor, false);
    target.setdodgeRate(0);
    BattleSession battle(rewarded, target, Campaign::rulesFor(rewarded, floor));
    require(battle.beginTurn() && battle.act({ActionType::Attack}), "fixture fight");
    materials = rewarded.runState().materials;
    items = rewarded.getSaveBackpack().size();
    require(Campaign::settleEncounter(rewarded, target, battle, floor), "fixture victory");
  };
  int materials = 0;
  std::size_t items = 0;
  settle(materials, items);
  require(rewarded.runState().materials >= materials + 2 && rewarded.runState().pity == 0,
          "first elite clear grants materials and resets pity");
  bool rewardFound = false;
  for (const Item &item : rewarded.getSaveBackpack())
    rewardFound |= item.getType() == Armor && item.getJobRequirement() == WarriorOnly &&
                   item.getTier() == Progression::equipmentTier(floor, 99) &&
                   item.getLevelRequirement() <= rewarded.getlevel();
  require(rewardFound, "elite reward must be wearable class armor in the best quality band");
  settle(materials, items);
  require(rewarded.runState().materials == materials, "repeat elite clears pay no elite reward");
}
int main() {
  std::ostringstream quiet;
  auto *original = std::cout.rdbuf(quiet.rdbuf());
  testRotatingObjectives();
  testEventEntryAndResourceBoundaries();
  testEventEnemyEffectsAndPrices();
  testEventsPersistWithoutNewFields();
  testLivingTimeoutIsNotADeath();
  testEliteEncounters();
  Warrior hero("campaign");
  Campaign::initialize(hero, 777, 0);
  bool rejected = false;
  try { Campaign::initialize(hero, 778, 0); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected && hero.runState().seed == 777);
  assert(hero.runState().seed == 777 && hero.getSaveEquipments().size() == 2);
  assert(!Campaign::chooseRoute(hero, 0, 1));
  assert(!Campaign::chooseRoute(hero, 101, 1));
  assert(!Campaign::chooseRoute(hero, 1, 4));
  assert(Campaign::chooseRoute(hero, 1, 1));
  assert(!Campaign::chooseRoute(hero, 1, 2));
  auto first = Campaign::makeEnemy(hero, 10);
  Campaign::shopStock(hero, 1);
  auto second = Campaign::makeEnemy(hero, 10);
  assert(first.gethp() == second.gethp() && first.getattack() == second.getattack());
  auto stock = Campaign::shopStock(hero, 1);
  auto otherFloorStock = Campaign::shopStock(hero, 9);
  hero.runState().lootTarget = Armor;
  const auto changedTargetStock = Campaign::shopStock(hero, 1);
  for (int i = 0; i < 5; ++i) {
    assert(stock[i].getName() == otherFloorStock[i].getName());
    assert(stock[i].getValue() == otherFloorStock[i].getValue());
    assert(stock[i].getId() == changedTargetStock[i].getId());
  }
  hero.gaincoin(50000);
  int coins = hero.getcoin();
  assert(!Campaign::purchase(hero, 1, 0) && !Campaign::purchase(hero, 101, 1));
  assert(hero.getcoin() == coins);
  assert(Campaign::purchase(hero, 1, 3));
  coins = hero.getcoin();
  assert(!Campaign::purchase(hero, 9, 3) && hero.getcoin() == coins);
  assert(Campaign::refreshShop(hero, 1));
  assert(hero.runState().shopRefreshes[0] == 1 && hero.runState().shopPurchased[0] == 0);
  assert(GameConfig::refreshCost(1, 1) > GameConfig::refreshCost(1, 0));
  hero.runState().shopRefreshes[0] = 20;
  coins = hero.getcoin();
  assert(!Campaign::refreshShop(hero, 1) && hero.getcoin() == coins);
  assert(!Campaign::eventPending(hero, 4));
  assert(Campaign::eventPending(hero, 5));
  assert(!Campaign::resolveEvent(hero, 5, 3));
  assert(Campaign::resolveEvent(hero, 5, 1));
  coins = hero.getcoin();
  assert(!Campaign::resolveEvent(hero, 5, 2) && hero.getcoin() == coins);
  assert(Campaign::makeEnemy(hero, 10).getattack() <= first.getattack());
  assert(!Campaign::rest(hero, 1)); // Full resources must not charge.
  for (int rest = 1; rest <= GameConfig::MaxRestsPerChapter; ++rest) {
    hero.sethp(1);
    coins = hero.getcoin();
    assert(Campaign::restPrice(hero, 1) == GameConfig::restCost(1) * rest);
    assert(Campaign::rest(hero, 1) && hero.getcoin() == coins - GameConfig::restCost(1) * rest);
  }
  hero.sethp(1);
  coins = hero.getcoin();
  assert(Campaign::restsRemaining(hero, 1) == 0);
  assert(!Campaign::rest(hero, 1) && hero.getcoin() == coins);
  assert(Campaign::restsRemaining(hero, 11) == GameConfig::MaxRestsPerChapter);
  hero.runState().challenge = 1; // Scarce supplies: one rest per chapter.
  assert(Campaign::restsRemaining(hero, 11) == 1 && Campaign::rest(hero, 11) &&
         Campaign::restsRemaining(hero, 11) == 0 && !Campaign::rest(hero, 11));
  hero.runState().challenge = 0;
  assert(!Campaign::claimOrder(hero, 1));
  for (int f = 1; f <= 5; ++f) { hero.runState().cleared[f] = 1; hero.runState().attempts[f] = 1; }
  assert(Campaign::claimOrder(hero, 5));
  const auto count = hero.getSaveBackpack().size();
  assert(!Campaign::claimOrder(hero, 5) && hero.getSaveBackpack().size() == count);
  hero.runState().cleared[7] = 1;
  hero.runState().attempts[7] = 1;
  assert(Campaign::rulesFor(hero, 10).bossSabotaged);
  assert(Campaign::rulesFor(hero, 3).objective == EncounterObjective::Survive);
  assert(Campaign::rulesFor(hero, 6).objective == EncounterObjective::Disrupt);

  Warrior rewards("rewards");
  Campaign::initialize(rewards, 1, 0);
  Campaign::chooseRoute(rewards, 1, 3);
  enemy weak("test", 1, 0, 0, 100, 100, 3);
  weak.configureEncounter(3, false); weak.setdodgeRate(0);
  BattleSession battle(rewards, weak);
  assert(!Campaign::settleEncounter(rewards, weak, battle, 3));
  assert(battle.beginTurn()); assert(battle.act({ActionType::Attack}));
  assert(Campaign::settleEncounter(rewards, weak, battle, 3));
  assert(rewards.runState().cleared[3] == 1 && rewards.runState().battles == 1);
  const int materials = rewards.runState().materials, gold = rewards.getcoin();
  assert(!Campaign::settleEncounter(rewards, weak, battle, 3));
  assert(rewards.runState().materials == materials && rewards.getcoin() == gold);
  assert(rewards.runState().battles == 1);
  for (int i = 0; i < 5; ++i) {
    enemy again("repeat", 1, 0, 0, 100, 100, 3);
    again.configureEncounter(3, false); again.setdodgeRate(0);
    BattleSession repeat(rewards, again);
    assert(repeat.beginTurn() && repeat.act({ActionType::Attack}));
    const int before = rewards.getcoin();
    assert(Campaign::settleEncounter(rewards, again, repeat, 3));
    assert(rewards.runState().materials == materials);
    if (i >= 3) assert(before == rewards.getcoin());
  }
  const std::string path = "campaign-test.dat";
  hero.setHighestFloor(7);
  assert(SaveSystem::saveGame(hero, 5, path));
  int restoredFloor = 1;
  auto loaded = SaveSystem::loadGame(restoredFloor, path);
  assert(loaded && restoredFloor == 5);
  assert(loaded->runState().routes == hero.runState().routes);
  assert(loaded->runState().events == hero.runState().events);
  assert(loaded->runState().shopPurchased == hero.runState().shopPurchased);
  assert(!Campaign::claimOrder(*loaded, 5));
  std::remove(path.c_str());
  std::remove((path + ".bak").c_str());
  Warrior fragile("fragile");
  fragile.sethp(1);
  assert(Campaign::resolveEvent(fragile, 5, 2) && fragile.gethp() == 1);
  fragile.sethp(0); fragile.runState().challenge = 2;
  assert(!Campaign::revive(fragile, true));
  fragile.runState().challenge = 0;
  assert(Campaign::revive(fragile, true) && fragile.isalive());
  assert(!Campaign::revive(fragile, true));
  std::cout.rdbuf(original);
  std::cout << "Campaign tests passed: routes, goals, rewards, economy, persistence, replay safety.\n";
}
