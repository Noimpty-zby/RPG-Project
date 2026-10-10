#include "../progression.h"
#include "enemy.h"
#include "item.h"
#include "jobs.h"
#include "skills.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

void prepare(player &hero, int floor = 100) {
  hero.loadStats(100, 0, 100000, 100, 1000, 100, 1000, 1000, 0, 3);
  hero.setHighestFloor(floor);
}

enemy targetWithHp(int hp) {
  enemy target("training target", hp, 100, 0, 0, 0, 100);
  target.setdogeRate(0);
  return target;
}

void testProgressionBoundaries() {
  const std::array<int, 10> floors{1, 19, 20, 39, 40, 64, 65, 84, 85, 100};
  const std::array<int, 10> caps{1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
  for (std::size_t i = 0; i < floors.size(); ++i)
    require(Progression::skillRankCap(floors[i]) == caps[i],
            "skill rank cap changed at wrong floor");
  const std::array<int, 5> unlocks{1, 10, 25, 45, 65};
  for (int slot = 1; slot <= 5; ++slot)
    require(Progression::skillUnlockFloor(slot) == unlocks[slot - 1],
            "wrong skill unlock floor");

  struct TierCase { int floor; std::array<int, 4> counts; };
  const TierCase cases[] = {
      {1, {80, 20, 0, 0}}, {19, {80, 20, 0, 0}},
      {20, {50, 40, 10, 0}}, {44, {50, 40, 10, 0}},
      {45, {15, 50, 35, 0}}, {69, {15, 50, 35, 0}},
      {70, {0, 45, 45, 10}}, {84, {0, 45, 45, 10}},
      {85, {0, 20, 50, 30}}, {100, {0, 20, 50, 30}}};
  for (const auto &test : cases) {
    std::array<int, 4> counts{};
    for (int roll = 0; roll < 100; ++roll) {
      int tier = Progression::equipmentTier(test.floor, roll);
      require(tier >= 0 && tier <= 3, "equipment tier out of range");
      ++counts[tier];
    }
    require(counts == test.counts, "equipment distribution changed at wrong floor");
  }
}

void testSkillGatesAndUpgrade() {
  Warrior hero("warrior");
  prepare(hero, 1);
  for (int slot = 2; slot <= 5; ++slot) {
    skill *s = hero.getSkill(slot);
    hero.setHighestFloor(s->getunlockfloor() - 1);
    require(!s->canUse(hero), "high character level bypassed floor gate");
    hero.setHighestFloor(s->getunlockfloor());
    require(s->canUse(hero), "skill did not unlock at its floor");
  }

  Warriorskill_1 first;
  require(!first.canUpgrade(100, 19), "rank 2 unlocked before floor 20");
  require(!first.canUpgrade(1, 20), "low character level bypassed rank gate");
  require(first.canUpgrade(2, 20), "rank 2 unavailable at floor 20");
  require(!first.levelUpSkill(100, 149, 20), "upgrade accepted insufficient coins");
  require(first.getskilllevel() == 1 && first.getcostmp() == 6,
          "failed upgrade mutated skill");
  require(first.levelUpSkill(100, 150, 20), "valid upgrade rejected");
  require(first.getskilllevel() == 2 && first.getdamage() == 45 &&
              first.getcostmp() == 9 && first.getlevelUpCost() == 600,
          "rank 2 damage, MP or next cost incorrect");
  require(!first.levelUpSkill(100, 100000, 39), "rank cap ignored");
  require(first.levelUpSkill(100, 600, 40), "rank 3 unavailable at floor 40");

  first.restoreProgress(5, 1);
  first.restoreProgress(5, 1);
  require(first.getdamage() == 90 && first.getcostmp() == 18 &&
              first.getcurrentcooldown() == 1,
          "repeated restore compounded skill growth or lost cooldown");
  first.restoreProgress(6, 0);
  require(first.getskilllevel() == 6 && !first.canUpgrade(100, 100),
          "legacy high rank was lost or can keep upgrading");
}

void testSpecialMageSkills() {
  Mage hero("mage");
  prepare(hero, 24);
  Mageskill_3 poison;
  poison.restoreProgress(5, 0);
  hero.setmp(0);
  auto target = targetWithHp(10000);
  require(poison.getcostmp() == 0 && !poison.useSkill(hero, target),
          "poison cost or floor gate incorrect");
  hero.setHighestFloor(25);
  require(poison.canUse(hero) && poison.useSkill(hero, target) && hero.getmp() == 50,
          "poison cannot replenish MP from zero after upgrading");

  Mageskill_4 arcane;
  arcane.restoreProgress(5, 0);
  hero.setHighestFloor(44);
  hero.setmp(200);
  require(!arcane.canUse(hero), "arcane bypassed floor gate");
  hero.setHighestFloor(45);
  hero.setmp(9);
  require(!arcane.canUse(hero), "arcane shown available without minimum MP");
  for (int rank : {5, 9}) {
    arcane.restoreProgress(rank, 0);
    hero.setmp(200);
    auto arcaneTarget = targetWithHp(5000);
    require(arcane.useSkill(hero, arcaneTarget), "arcane cast failed");
    require(hero.getmp() == 100 && arcaneTarget.gethp() == 4100,
            "arcane MP consumption or capped multiplier incorrect");
  }
}

void testClassDamageMultipliers() {
  Mage mage("mage");
  prepare(mage);
  struct MageCase { int slot; int percent; };
  for (const auto &test : {MageCase{1, 135}, MageCase{2, 115}, MageCase{5, 150}}) {
    auto target = targetWithHp(10000);
    skill *ability = mage.getSkill(test.slot);
    ability->restoreProgress(5, 0);
    const int expected = ability->getScaledDamage(mage) * test.percent / 100;
    require(mage.useSkill(test.slot, target) && target.gethp() == 10000 - expected,
            "mage elemental skill multiplier incorrect");
  }

  Assassin assassin("assassin");
  prepare(assassin);
  auto target = targetWithHp(10000);
  skill *ambush = assassin.getSkill(2);
  ambush->restoreProgress(5, 0);
  const int expected = ambush->getScaledDamage(assassin) * 2;
  require(assassin.useSkill(2, target) && target.gethp() == 10000 - expected,
          "ambush multiplier incorrect");
  skill *combo = assassin.getSkill(3);
  combo->restoreProgress(5, 0);
  const int singleHit = combo->getScaledDamage(assassin) * 6 / 10;
  std::srand(17);
  const int hits = 2 + std::rand() % 3;
  std::srand(17);
  target = targetWithHp(10000);
  require(assassin.useSkill(3, target) &&
              target.gethp() == 10000 - singleHit * hits,
          "combo did not use 60 percent damage per hit");
}

void testHealingAndTemporaryWeakening() {
  Warrior warrior("warrior");
  prepare(warrior);
  warrior.setHealingPercent(50);
  auto target = targetWithHp(10000);
  require(warrior.useSkill(2, target) && warrior.gethp() == 175,
          "war cry ignored healing suppression");
  require(warrior.useSkill(5, target) && warrior.gethp() == 180,
          "ultimate healing ignored suppression");

  require(warrior.useSkill(3, target) && target.getattack() == 65,
          "shield counter did not apply 35 percent weakening");
  warrior.getSkill(3)->resetcurrentcooltime();
  require(warrior.useSkill(3, target) && target.getattack() == 65,
          "repeated shield counter permanently stacked weakening");
  target.finishTurn(3);
  require(target.getattack() == 65, "weakening expired too soon");
  target.finishTurn(1);
  require(target.getattack() == 100, "weakening did not expire");

  Assassin assassin("assassin");
  prepare(assassin);
  assassin.setHealingPercent(50);
  auto dyingTarget = targetWithHp(50);
  require(assassin.useSkill(4, dyingTarget) && assassin.gethp() == 110,
          "lifesteal used overkill damage or ignored healing suppression");
  assassin.getSkill(4)->resetcurrentcooltime();
  auto dodgingTarget = targetWithHp(10000);
  dodgingTarget.setdogeRate(100);
  require(assassin.useSkill(4, dodgingTarget) && assassin.gethp() == 110,
          "lifesteal healed after a dodged attack");
}

void testDamageAndBossIdentity() {
  Warrior warrior("warrior");
  prepare(warrior);
  skill *strike = warrior.getSkill(1);
  const int before = strike->getScaledDamage(warrior);
  warrior.equipItem_T(Item("test sword", 1, Weapon, 100, 100, 0, 1));
  require(strike->getScaledDamage(warrior) == before + 75,
          "weapon attack does not increase skill damage");

  auto shielded = targetWithHp(10000);
  std::srand(1);
  for (int attempt = 0; attempt < 1000 && !shielded.hasAffix(Affix::SHIELDING);
       ++attempt)
    shielded.setAffix(100);
  require(shielded.hasAffix(Affix::SHIELDING), "could not create shield fixture");
  const int raw = strike->getScaledDamage(warrior);
  require(warrior.useSkill(1, shielded) &&
              10000 - shielded.gethp() >= raw - raw / 2,
          "shield absorbed more than half a skill hit");

  Assassin assassin("assassin");
  prepare(assassin);
  auto ordinary = targetWithHp(2000);
  ordinary.configureEncounter(100, false);
  require(assassin.useSkill(5, ordinary) && ordinary.gethp() == 950,
          "high HP ordinary enemy incorrectly received boss resistance");
  assassin.getSkill(5)->resetcurrentcooltime();
  auto boss = targetWithHp(500);
  boss.configureEncounter(100, true);
  require(assassin.useSkill(5, boss) && boss.gethp() == 325,
          "low HP boss lost its execution resistance");
}
} // namespace

int main() {
  std::ostringstream gameplayOutput;
  auto *previous = std::cout.rdbuf(gameplayOutput.rdbuf());
  try {
    testProgressionBoundaries();
    testSkillGatesAndUpgrade();
    testSpecialMageSkills();
    testClassDamageMultipliers();
    testHealingAndTemporaryWeakening();
    testDamageAndBossIdentity();
  } catch (const std::exception &error) {
    std::cout.rdbuf(previous);
    std::cerr << "skill progression tests failed: " << error.what() << '\n';
    return 1;
  }
  std::cout.rdbuf(previous);
  std::cout << "skill progression tests passed\n";
}
