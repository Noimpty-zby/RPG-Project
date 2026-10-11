#include "core/game_config.h"
#include "player/jobs.h"
#include "player/skills.h"
#include <climits>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
void prepare(player &hero) {
  hero.loadStats(50, 0, 10000, 400, 1000, 100, 1000, 1000, 0, 3);
  hero.setHighestFloor(100);
}
enemy target() {
  enemy result("target", 100000, 100, 0, 0, 0, 50);
  result.setdodgeRate(0);
  return result;
}
void testGrowthAndTransactions() {
  Warrior hero("growth");
  hero.gaincoin(100);
  hero.gaincoin(-100);
  hero.costcoin(-100);
  hero.costcoin(101);
  require(hero.getcoin() == 100, "invalid coin transactions changed balance");
  hero.gaincoin(INT_MAX);
  require(hero.getcoin() == 1000000000, "coin overflow not saturated at save-compatible limit");
  hero.gainexp(-100);
  require(hero.getexp() == 0 && hero.getlevel() == 1, "negative experience accepted");
  hero.sethp(1); hero.setmp(0); hero.gainexp(100);
  require(hero.getlevel() == 2 && hero.gethp() > 1 && hero.gethp() < hero.getmaxhp(), "levelup should partially recover HP");
  require(hero.getmp() > 0 && hero.getmp() < hero.getmaxmp(), "levelup should partially recover MP");
  hero.gainexp(INT_MAX);
  require(hero.getlevel() == 100 && hero.getexp() <= 9999, "grinding bypassed character level cap");
  hero.levelup(); require(hero.getlevel() == 100, "direct levelup bypassed cap");
  auto *ability = hero.getSkill(1);
  ability->restoreProgress(1, 0); const int first = ability->getScaledDamage(hero);
  ability->restoreProgress(2, 0); const int second = ability->getScaledDamage(hero);
  ability->restoreProgress(3, 0); const int third = ability->getScaledDamage(hero);
  require(second - first == third - second && second > first, "skill ranks do not grow linearly");
  ability->restoreProgress(5, 0); const int capped = ability->getScaledDamage(hero);
  ability->restoreProgress(INT_MAX, INT_MAX);
  require(ability->getskilllevel() == INT_MAX && ability->getScaledDamage(hero) == capped &&
          ability->getcurrentcooldown() == ability->getcooldown(), "legacy rank/cooldown caused unsafe growth");
  Warrior legacy("legacy level");
  legacy.loadStats(10000, 0, 1000000000, 100, 1000, 100, 100, 100, 100, 1000);
  require(legacy.getlevel() == 10000 && legacy.getcoin() == 1000000000 && legacy.getfreerevives() == 1000,
          "supported legacy level, coins or revives were silently reduced");
  legacy.levelup();
  require(legacy.getlevel() == 10000, "legacy level continued growing beyond campaign cap");
}
void testSpecializationTransactions() {
  Warrior hero("choices");
  hero.loadStats(10, 0, 10000, 100, 200, 40, 100, 100, 0, 3);
  hero.setHighestFloor(9);
  require(!hero.chooseSpecialization(1, 1), "specialization opened before chapter gate");
  require(hero.getcoin() == 10000, "locked specialization consumed coins");
  hero.setHighestFloor(10);
  require(hero.chooseSpecialization(1, 1) && hero.getcoin() == 10000, "first choice not free");
  require(!hero.chooseSpecialization(0, 1) && !hero.chooseSpecialization(1, 3) &&
          !hero.chooseSpecialization(3, 1) && !hero.chooseSpecialization(2, 1), "invalid specialization accepted");
  require(!hero.chooseSpecialization(1, 1) && hero.getcoin() == 10000, "same choice charged coins");
  require(hero.chooseSpecialization(1, 2) && hero.getcoin() == 9850, "respec fee incorrect");
  hero.costcoin(hero.getcoin());
  require(!hero.chooseSpecialization(1, 1) && hero.getSkill(1)->getSpecialization() == 2, "unaffordable respec mutated branch");
  auto enemy = target(); hero.resetEncounterResources(); hero.useSkill(1, enemy);
  hero.gaincoin(1000);
  require(!hero.chooseSpecialization(1, 1), "respec accepted during encounter");
  hero.finishEncounterWear();
  const int remaining = hero.getSkill(1)->getcurrentcooldown();
  require(hero.chooseSpecialization(1, 1) && hero.getSkill(1)->getcurrentcooldown() == remaining,
          "camp respec was blocked or reset cooldown");
  require(!hero.getSkill(3)->setSpecialization(1), "later slot accepted unsupported specialization");
}
void testSpecializationEffects() {
  Warrior warrior("warrior"); prepare(warrior);
  auto first = target(); auto second = target();
  warrior.getSkill(1)->setSpecialization(1); warrior.addCombatResource(2);
  warrior.useSkill(1, first);
  require(warrior.getCombatResource() == 0, "counter did not consume resolve");
  warrior.getSkill(1)->resetcurrentcooltime(); warrior.getSkill(1)->setSpecialization(2);
  warrior.useSkill(1, second);
  require(first.gethp() < second.gethp(), "prepared counter did not exceed heavy strike");
  warrior.getSkill(2)->setSpecialization(1); warrior.useSkill(2, first);
  const int healed = warrior.gethp();
  warrior.getSkill(2)->resetcurrentcooltime(); warrior.getSkill(2)->setSpecialization(2);
  const int manaBefore = warrior.getmp(); const int cost = warrior.getSkill(2)->getActualCost(warrior);
  warrior.useSkill(2, second);
  require(warrior.gethp() == healed && warrior.getmp() == manaBefore - cost + 6, "warcry damage branch healed or lost refund");

  Mage mage("mage"); prepare(mage); first = target(); second = target();
  mage.getSkill(1)->setSpecialization(1); mage.addCombatResource(2); mage.useSkill(1, first);
  require(mage.getCombatResource() == 0, "focused fire did not consume focus");
  mage.getSkill(1)->resetcurrentcooltime(); mage.getSkill(1)->setSpecialization(2); mage.useSkill(1, second);
  require(mage.getCombatResource() == 1 && first.gethp() < second.gethp(), "fire branches lack resource/damage tradeoff");
  mage.getSkill(2)->setSpecialization(1); mage.useSkill(2, first);
  mage.getSkill(2)->resetcurrentcooltime(); mage.getSkill(2)->setSpecialization(2); mage.useSkill(2, second);
  require(first.getattack() == 60 && second.getattack() == 100, "ice damage branch retained control");

  Assassin assassin("assassin"); prepare(assassin); first = target(); second = target();
  assassin.getSkill(1)->setSpecialization(1); assassin.useSkill(1, first);
  require(assassin.getCombatResource() == 2, "opening specialization failed to build two openings");
  assassin.getSkill(1)->resetcurrentcooltime(); assassin.getSkill(1)->setSpecialization(2);
  assassin.consumeCombatResource(); assassin.useSkill(1, second);
  require(assassin.getCombatResource() == 0 && second.gethp() < first.gethp(), "piercing branch did not trade openings for damage");
  assassin.getSkill(2)->setSpecialization(1); assassin.addCombatResource(2); assassin.useSkill(2, first);
  require(assassin.getCombatResource() == 0, "ambush did not consume openings");
  assassin.getSkill(2)->resetcurrentcooltime(); assassin.getSkill(2)->setSpecialization(2); assassin.useSkill(2, second);
  require(second.getattack() == 85 && first.getattack() == 100, "disruption branch failed to weaken target");
}
void testRecoveryAndResources() {
  Warrior hero("recovery"); prepare(hero);
  hero.equipItem_T(Item("armor", 1, Armor, 500, 20, 20, 1));
  require(hero.getRecoveryBaseHp() == 1000, "armor inflated recovery base");
  auto enemy = target(); hero.useSkill(2, enemy);
  require(hero.gethp() == 550, "warcry scaled with armor HP");
  hero.setHealingPercent(50); hero.heal(200);
  require(hero.gethp() == 650 && hero.getSuppressedHealingTotal() == 100, "suppressed healing not recorded correctly");
  hero.sethp(hero.getmaxhp() - 10); hero.heal(100);
  require(hero.getSuppressedHealingTotal() == 100, "overhealing counted as suppressed healing");
  hero.resetEncounterResources(); hero.setmp(0); hero.restoreTurnResources();
  require(hero.getmp() == 2 && hero.getCombatResource() == 0, "warrior turn resource incorrect");
  hero.guard(false); require(hero.getCombatResource() == 1, "warrior guard did not generate resolve");
  Mage mage("mage"); prepare(mage); mage.setmp(0); mage.restoreTurnResources(); mage.attacktarget(enemy);
  require(mage.getmp() == 11 && mage.getCombatResource() == 1, "mage attack mana/focus incorrect");
  Assassin assassin("assassin"); prepare(assassin); assassin.setmp(0); assassin.restoreTurnResources(); assassin.attacktarget(enemy);
  require(assassin.getmp() == 7 && assassin.getCombatResource() == 1, "assassin attack resource incorrect");
}
void testInventoryAndWear() {
  Warrior hero("items"); prepare(hero);
  hero.addItem(Item("blade", 1, Weapon, 20, 2, 90, 2, WarriorOnly));
  hero.equipItem(1);
  require(hero.getSaveBackpack()[0].getQuantity() == 1 && hero.getSaveEquipments()[0].getQuantity() == 1,
          "stack equipment transfer lost quantities");
  hero.equipItem(1);
  require(hero.getSaveBackpack().size() == 1 && hero.getSaveEquipments().size() == 1,
          "swapping from first bag slot invalidated inventory index");
  hero.resetEncounterResources(); auto enemy = target();
  hero.attacktarget(enemy); hero.useSkill(1, enemy); hero.takedamage(1);
  require(hero.getSaveEquipments()[0].getDurability() == 2, "action-based durability wear remains");
  hero.finishEncounterWear(); hero.finishEncounterWear();
  require(hero.getSaveEquipments()[0].getDurability() == 1, "encounter wear applied zero or twice");
  hero.resetEncounterResources(); hero.finishEncounterWear();
  require(hero.getSaveEquipments().empty() && hero.getattack() == 100, "broken weapon left attack bonus");

  hero.addItem(Item("potion", 1, Potion, 20, 0, 30, 3));
  require(hero.toggleItemLock(1), "valid item could not be locked");
  require(hero.sellItem(1, 1) == 0 && hero.getSaveBackpack().size() == 2, "locked gear was sold");
  require(hero.salvageUnlocked() == 0 && hero.getSaveBackpack().size() == 2, "salvage deleted locked gear or potion");
  hero.toggleItemLock(1); require(hero.salvageUnlocked() == 1, "unlocked gear did not salvage");
  const int before = hero.getcoin();
  require(hero.sellItem(1, 0) == 0 && hero.sellItem(1, 4) == 0 && hero.getcoin() == before,
          "invalid sale quantity accepted");
  require(hero.sellItem(1, 2) == 20 && hero.getSaveBackpack()[0].getQuantity() == 1,
          "partial stack sale sold the wrong amount");
  require(hero.sellItem(1, 1) == 10 && hero.getcoin() == before + 30 && hero.getSaveBackpack().empty(),
          "stack sale proceeds ignored quantity");
  hero.equipItem_T(Item("crafted", 1, Weapon, 30, 20, 100, 1, WarriorOnly));
  hero.runState().materials = 10;
  require(!hero.modifyEquipment(1, 2) && hero.runState().materials == 10, "unsupported effect consumed materials");
  require(hero.modifyEquipment(1, 1) && hero.runState().materials == 7 && hero.equipmentEffect(1) == 1,
          "valid modification failed");
  require(!hero.modifyEquipment(1, 1) && hero.runState().materials == 7, "same effect charged materials twice");
  require(!hero.modifyEquipment(0, 1) && !hero.modifyEquipment(1, 99), "invalid modification accepted");
  hero.addItem(Item("locked plate", 1, Armor, 40, 20, 100, 1, WarriorOnly));
  hero.toggleItemLock(static_cast<int>(hero.getSaveBackpack().size()));
  hero.equipItem(static_cast<int>(hero.getSaveBackpack().size()));
  require(hero.getSaveEquipments().back().isLocked() && hero.modifyEquipment(2, 3) &&
          hero.runState().materials == 4, "a lock blocked modifying worn gear");
  hero.unequipItem(2);
  hero.runState().materials = 7;
  hero.addItem(Item("overflow salvage", 1, Weapon, 10, 20, 50, 1));
  hero.runState().materials = GameConfig::MaxMaterials;
  require(hero.salvageUnlocked() == 0 && !hero.getSaveBackpack().empty(), "full material capacity destroyed gear");

  hero.equipItem_T(Item("first armor", 1, Armor, 100, 20, 100, 1));
  hero.sethp(1050);
  hero.equipItem_T(Item("second armor", 1, Armor, 200, 20, 100, 1));
  require(hero.gethp() == 1050 && hero.getmaxhp() == 1200, "armor swap truncated HP at intermediate unequip");
  hero.equipItem_T(Item("smaller armor", 1, Armor, 20, 20, 100, 1));
  require(hero.gethp() == 1020 && hero.getmaxhp() == 1020, "armor downsizing failed to clamp HP");

  Warrior consumer("potions");
  consumer.addItem(Item("potion", 1, Potion, 20, 0, 10, 2));
  require(!consumer.useItem(1) && consumer.runState().potionsUsed == 0, "full HP use changed potion statistics");
  consumer.sethp(100);
  require(consumer.useItem(1) && consumer.runState().potionsUsed == 1, "out-of-combat potion use was not counted");
  consumer.setHealingPercent(1);
  require(!consumer.useItem(1) && consumer.runState().potionsUsed == 1, "zero-effective healing consumed potion");
  consumer.addItem(Item("negative weapon", 1, Weapon, -10, 5, 10, 1));
  consumer.equipItem_T(Item("negative armor", 1, Armor, -10, 5, 10, 1));
  require(consumer.getSaveBackpack().size() == 1 && consumer.getSaveEquipments().empty(),
          "invalid equipment entered inventory");
}
}
// The potion plan avoids overshooting, consumes nothing, and drinking follows it.
void testPotionPlan() {
  Warrior hero("potions");
  prepare(hero);
  hero.sethp(100);
  hero.addItem(Item("中药", 1, Potion, 300, 0, 1, 2, AnyJob));
  hero.addItem(Item("小药", 1, Potion, 50, 0, 1, 5, AnyJob));
  hero.addItem(Item("大药", 1, Potion, 500, 0, 1, 1, AnyJob));
  const auto plan = hero.planPotions(1000);
  require(plan.restored == 900 && plan.overflow == 0 && plan.uses.size() == 3 &&
              plan.uses[0] == std::make_pair(3, 1) && plan.uses[1] == std::make_pair(1, 1) &&
              plan.uses[2] == std::make_pair(2, 2),
          "the plan must take the largest potion that fits, then smaller ones");
  require(hero.gethp() == 100 && hero.getSaveBackpack()[0].getQuantity() == 2,
          "planning must not drink anything");
  require(hero.drinkPotionsTo(1000) == 900 && hero.gethp() == 1000 &&
              hero.getSaveBackpack().size() == 2 && hero.getSaveBackpack()[0].getQuantity() == 1 &&
              hero.getSaveBackpack()[1].getQuantity() == 3,
          "drinking must follow the plan exactly");
  hero.sethp(900);
  const auto over = hero.planPotions(1000);
  require(over.restored == 100 && over.overflow == 0 && over.uses.size() == 1 &&
              hero.getSaveBackpack()[over.uses[0].first - 1].getName() == "小药" && over.uses[0].second == 2,
          "small potions that fit beat a large one that overflows");
  hero.sethp(980);
  const auto spill = hero.planPotions(1000);
  require(spill.restored == 20 && spill.overflow == 30, "when every potion overflows, the smallest one is used");
  require(hero.planPotions(500).uses.empty(), "a target at or below current HP needs no potions");
}

void testRepair() {
  Warrior hero("smith"); prepare(hero);
  Item epic("epic blade", 1, Weapon, 50, 40, 400, 1, WarriorOnly, 3);
  epic.setDurability(10); // 30 of 40 durability missing.
  hero.equipItem_T(epic);
  const int perPoint = GameConfig::repairCostPerPoint(400, 3);
  require(perPoint == 400 * 5 / 80 && hero.repairCost(1) == 30 * perPoint, "repair cost formula");
  require(GameConfig::repairCostPerPoint(400, 0) < perPoint, "common gear must be cheaper to repair");
  require(hero.repairCost(0) == 0 && hero.repairCost(2) == 0 && !hero.repairEquipment(2),
          "invalid repair slot accepted");
  const int before = hero.getcoin();
  require(hero.repairEquipment(1) && hero.getcoin() == before - 30 * perPoint &&
              hero.getSaveEquipments()[0].getDurability() == 40,
          "repair must restore durability to the cap and charge once");
  require(hero.repairCost(1) == 0 && !hero.repairEquipment(1) && hero.getcoin() == before - 30 * perPoint,
          "repairing full durability must not charge");
  hero.resetEncounterResources(); hero.finishEncounterWear();
  hero.costcoin(hero.getcoin());
  require(hero.repairCost(1) == perPoint && !hero.repairEquipment(1) &&
              hero.getSaveEquipments()[0].getDurability() == 39,
          "repair without enough coins must change nothing");
}
void testEnhancement() {
  Warrior hero("enchanter"); prepare(hero);
  hero.equipItem_T(Item("blade", 1, Weapon, 100, 30, 200, 1, WarriorOnly, 1));
  hero.equipItem_T(Item("plate", 1, Armor, 200, 30, 300, 1, WarriorOnly));
  const int attack = hero.getattack(), maxHp = hero.getmaxhp();
  for (int level = 1; level <= GameConfig::MaxEnhanceLevel; ++level) {
    const int value = hero.getSaveEquipments()[0].getValue();
    const int coins = hero.getcoin();
    require(hero.enhanceCost(1) == 200 * level, "enhancement cost must scale with the next level");
    require(hero.enhanceEquipment(1) && hero.getcoin() == coins - 200 * level &&
                hero.getSaveEquipments()[0].getValue() == value + value * GameConfig::EnhancePercent / 100 &&
                hero.getSaveEquipments()[0].getEnhanceLevel() == level,
            "enhancement must raise value by the configured percent and record the level");
  }
  const Item &blade = hero.getSaveEquipments()[0];
  require(hero.getattack() == attack + blade.getValue() - 100, "enhancement gain must reach attack exactly once");
  require(blade.getDisplayName() == "blade +5" && blade.getName() == "blade", "display name shows the level");
  require(hero.enhanceCost(1) == 0 && !hero.enhanceEquipment(1), "enhancement must stop at the cap");
  require(hero.enhanceEquipment(2) && hero.getmaxhp() == maxHp + 10, "armor enhancement must raise max HP");
  hero.unequipItem(1);
  require(hero.getattack() == attack - 100 && hero.getSaveBackpack().back().getEnhanceLevel() == 5,
          "unequipping must remove the enhanced value and keep the level on the item");
  hero.costcoin(hero.getcoin());
  require(!hero.enhanceEquipment(1) && hero.getSaveEquipments()[0].getEnhanceLevel() == 1,
          "enhancing without coins must change nothing");
}
int main() {
  std::ostringstream log; auto *original = std::cout.rdbuf(log.rdbuf());
  try {
    testGrowthAndTransactions(); testSpecializationTransactions(); testSpecializationEffects();
    testRecoveryAndResources(); testInventoryAndWear(); testPotionPlan(); testRepair(); testEnhancement();
  } catch (const std::exception &error) {
    std::cout.rdbuf(original); std::cerr << "build tests failed: " << error.what() << '\n' << log.str(); return 1;
  }
  std::cout.rdbuf(original); std::cout << "build tests passed\n";
}
