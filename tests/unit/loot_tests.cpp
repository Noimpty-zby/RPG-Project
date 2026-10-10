#include "core/random.h"
#include "game/world.h"
#include "player/jobs.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool passed, const std::string &message) {
  if (!passed) throw std::runtime_error(message);
}

void itemIdentityAndEffects() {
  Item old("Old sword", 1, Weapon, 10, 20, 30, 1);
  require(old.getTier() == 0 && old.getEffect() == ItemEffect::None &&
          !old.isLocked() && !old.getId().empty(), "old constructor defaults");
  Item first("Sword", 1, Weapon, 10, 20, 30, 1, WarriorOnly, 2);
  Item second("Sword", 5, Weapon, 90, 80, 70, 2, WarriorOnly, 2);
  require(first.getId() == second.getId(), "template IDs ignore rolled values");
  const auto id = first.getId();
  require(first.setEffect(ItemEffect::ManaOnAttack), "weapon supports mana effect");
  first.setLocked(true);
  require(first.getId() == id && first.isLocked(), "modification preserves template identity");
  require(!first.setEffect(ItemEffect::GuardRecovery) &&
          first.getEffect() == ItemEffect::ManaOnAttack, "incompatible change preserves effect");
  require(Item::templateId("Sword", Weapon, MageOnly, 2) != id &&
          Item::templateId("Sword", Weapon, WarriorOnly, 3) != id &&
          Item::templateId("Sword", Armor, WarriorOnly, 2) != id,
          "job, tier and slot distinguish templates");
  Item armor("Armor", 1, Armor, 10, 20, 30, 1, MageOnly, 2);
  require(armor.setEffect(ItemEffect::GuardRecovery) &&
          armor.setEffect(ItemEffect::ControlFocus) &&
          !armor.setEffect(ItemEffect::ManaOnAttack), "armor effect compatibility");
  Item assassin("Dagger", 1, Weapon, 10, 20, 30, 1, AssassinOnly, 2);
  require(!assassin.setEffect(ItemEffect::ControlFocus) &&
          assassin.setEffect(ItemEffect::SkillEfficiency), "class effect compatibility");
  Item potion("Potion", 1, Potion, 50, 0, 20, 1);
  require(!potion.setEffect(ItemEffect::SkillEfficiency), "potions have no gear effect");
  potion.setQuantity(0);
  require(potion.getQuantity() == 0, "zero quantity supports consumed stack removal");
}

void targetedGeneration() {
  Random::seed(9471);
  std::array<bool, 4> observed{};
  for (int floor : {1, 19, 20, 44, 45, 69, 70, 84, 85, 100}) {
    for (JobReq job : {WarriorOnly, MageOnly, AssassinOnly}) {
      for (ItemType type : {Weapon, Armor}) {
        for (int draw = 0; draw < 160; ++draw) {
          const auto item = generateTargetedItem(floor, job, type);
          require(item.getJobRequirement() == job && item.getType() == type,
                  "targeted item keeps requested job and slot");
          const int tier = item.getTier();
          require(tier >= 0 && tier <= 3, "valid generated tier");
          observed[tier] = true;
          const int base = type == Weapon ? 8 + floor * 11 / 10 : 20 + floor * 3;
          const int center = base * (100 + tier * 15) / 100;
          const int variation = center * 3 / 100;
          require(item.getValue() >= center - variation &&
                  item.getValue() <= center + variation, "within-tier rolls stay within three percent");
          if (tier < 3) {
            const int nextCenter = base * (100 + (tier + 1) * 15) / 100;
            require(center + variation < nextCenter - nextCenter * 3 / 100,
                    "adjacent quality stat bands never overlap");
          }
          require(item.getLevelRequirement() >= 1 && item.getQuantity() == 1,
                  "generated gear has usable metadata");
          require(item.supportsEffect(item.getEffect()) &&
                  (tier >= 2 ? item.getEffect() != ItemEffect::None
                             : item.getEffect() == ItemEffect::None),
                  "high-quality gear has one compatible function");
          require(item.getId() == Item::templateId(item.getName(), type, job, tier),
                  "generation uses stable template identity");
          if (floor < 20) require(tier <= 1, "early loot quality gate");
          if (floor < 70) require(tier < 3, "epic quality gate");
          if (floor >= 70) require(tier >= 1, "late loot minimum quality");
        }
      }
    }
    const auto potion = generateTargetedItem(floor, AnyJob, Potion);
    require(potion.getType() == Potion && potion.getJobRequirement() == AnyJob &&
            potion.getLevelRequirement() == 1 && potion.getEffect() == ItemEffect::None,
            "targeted shop potions remain supported");
  }
  for (bool found : observed) require(found, "quality samples exercise every tier");
  bool rejected = false;
  try { (void)generateTargetedItem(1, AnyJob, Weapon); }
  catch (const std::invalid_argument &) { rejected = true; }
  require(rejected, "unsupported generic weapon target rejected");
}

void pityAndRepeats(player &hero, JobReq job) {
  auto &run = hero.runState();
  run.usefulDrops = 7;
  run.gearDrought = 3;
  run.longestGearDrought = 5;
  for (ItemType target : {Weapon, Armor}) {
    const int threshold = target == Weapon ? 6 : 8;
    run.lootTarget = target;
    run.pity = threshold - 1;
    auto item = generateLoot(hero, 100, true);
    require(item && item->getType() == target && item->getJobRequirement() == job &&
            item->getLevelRequirement() <= hero.getlevel() && run.pity == 0,
            "first-clear pity guarantees an immediately usable target");
    run.pity = threshold - 1;
    run.attempts[50] = 2;
    for (int draw = 0; draw < 100; ++draw) (void)generateLoot(hero, 50, false);
    require(run.pity == threshold - 1, "repeat rolls neither advance nor reset first-clear pity");
    for (int floor : {-10, 0, 101, 10000}) {
      require(!generateLoot(hero, floor, true) && run.pity == threshold - 1,
              "invalid reward floors do not mint items or advance pity");
    }
    run.attempts[50] = 5;
    for (int draw = 0; draw < 100; ++draw)
      require(!generateLoot(hero, 50, false), "fourth and later repeats yield no random loot");
    run.pity = 0;
    int drought = 0;
    for (int floor = 1; floor <= 100; ++floor) {
      const auto dropped = generateLoot(hero, floor, true);
      const bool useful = dropped && dropped->getType() == target &&
          (dropped->getJobRequirement() == AnyJob || dropped->getJobRequirement() == job) &&
          dropped->getLevelRequirement() <= hero.getlevel();
      drought = useful ? 0 : drought + 1;
      require(drought < threshold && run.pity == drought,
              "first-clear drought remains bounded across floors");
    }
  }
  require(run.usefulDrops == 7 && run.gearDrought == 3 && run.longestGearDrought == 5,
          "campaign exclusively owns useful drop statistics");
}
} // namespace

int main() {
  std::ostringstream log;
  auto *previous = std::cout.rdbuf(log.rdbuf());
  std::string failure;
  try {
    itemIdentityAndEffects();
    targetedGeneration();
    Warrior warrior("Loot warrior");
    Mage mage("Loot mage");
    Assassin assassin("Loot assassin");
    pityAndRepeats(warrior, WarriorOnly);
    pityAndRepeats(mage, MageOnly);
    pityAndRepeats(assassin, AssassinOnly);
  } catch (const std::exception &error) { failure = error.what(); }
  std::cout.rdbuf(previous);
  if (!failure.empty()) {
    std::cerr << "Loot tests FAILED: " << failure << '\n' << log.str();
    return 1;
  }
  std::cout << "Loot tests passed: stable identities, quality bands, effects, targeting, pity and repeats.\n";
}
