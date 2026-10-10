#include "core/progression.h"
#include "player/jobs.h"
#include "player/skills.h"
#include "storage/save.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool passed, const std::string &message) {
  if (!passed)
    throw std::runtime_error(message);
}

struct Fixture {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("rpg_save_tests_" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) + ".dat");
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
  std::string name() const { return path.string(); }
  std::string read() const {
    std::ifstream input(path);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
  }
  void write(const std::string &text) const {
    std::ofstream output(path);
    output << text;
    require(static_cast<bool>(output), "write fixture");
  }
};

std::string replaceLine(const std::string &text, const std::string &prefix,
                        const std::string &replacement) {
  std::istringstream input(text);
  std::string line, result;
  bool replaced = false;
  while (std::getline(input, line)) {
    if (line.compare(0, prefix.size(), prefix) == 0) {
      if (!replacement.empty()) result += replacement + "\n";
      replaced = true;
    } else {
      result += line + "\n";
    }
  }
  require(replaced, "fixture line exists: " + prefix);
  return result;
}

std::string reorderSkills(const std::string &text) {
  std::istringstream input(text);
  std::vector<std::string> skills;
  std::string line, result;
  while (std::getline(input, line)) {
    if (line.compare(0, 6, "SKILL ") == 0) skills.push_back(line);
    else result += line + "\n";
  }
  std::reverse(skills.begin(), skills.end());
  for (const auto &entry : skills) result += entry + "\n";
  return result;
}

void compareItem(const Item &expected, const Item &actual) {
  require(expected.getName() == actual.getName(), "item name");
  require(expected.getType() == actual.getType(), "item type");
  require(expected.getValue() == actual.getValue(), "item value");
  require(expected.getLevelRequirement() == actual.getLevelRequirement(), "item level");
  require(expected.getQuantity() == actual.getQuantity(), "item quantity");
  require(expected.getDurability() == actual.getDurability(), "item durability");
  require(expected.getPrice() == actual.getPrice(), "item price");
  require(expected.getJobRequirement() == actual.getJobRequirement(), "item class restriction");
  require(expected.getId() == actual.getId(), "stable template ID");
  require(expected.getTier() == actual.getTier(), "item tier");
  require(expected.getEffect() == actual.getEffect(), "item effect");
  require(expected.isLocked() == actual.isLocked(), "item lock");
}

void roundTrip(player &hero, Fixture &fixture) {
  hero.loadStats(50, 123, 4567, 200, 700, 180, 120, 160, 25, 2);
  hero.setHighestFloor(85);
  auto &run = hero.runState();
  run.seed = 4294967295U;
  run.challenge = 1;
  run.legacyRules = true;
  run.lootTarget = Armor;
  run.pity = 7;
  run.materials = 123;
  for (int i = 0; i < 8; ++i) {
    run.routes[i] = i % 3 + 1;
    run.events[i] = i % 2 + 1;
    run.campRested[i] = 1;
    run.ordersClaimed[i] = 1;
    run.shopRefreshes[i] = i;
    run.shopPurchased[i] = i + 1;
  }
  for (int i = 1; i <= 84; ++i) { run.cleared[i] = 1; run.attempts[i] = 2; }
  run.battles = 170;
  run.turns = 700;
  run.potionsUsed = 31;
  run.deaths = 2;
  run.damageTaken = 55000;
  run.usefulDrops = 17;
  run.gearDrought = 3;
  run.longestGearDrought = 7;
  hero.equipItem_T(Item("Old high level armor", 80, Armor, 100, 23, 90, 1,
                        AnyJob, 3, ItemEffect::GuardRecovery, true), true);
  hero.equipItem_T(Item("Weapon", 40, Weapon, 30, 19, 70, 1,
                        AnyJob, 2, ItemEffect::ManaOnAttack), true);
  hero.sethp(37); // Below base HP: old loader incorrectly added armor's HP.
  hero.addItem(Item("Potion", 1, Potion, 50, 0, 20, 3));
  hero.addItem(Item("Same name", 1, Weapon, 8, 20, 30, 1, WarriorOnly));
  hero.addItem(Item("Same name", 1, Weapon, 8, 20, 30, 1, MageOnly));
  require(hero.getSaveBackpack().size() == 3, "class restrictions must not merge");
  for (int slot = 1; slot <= hero.getSkillCount(); ++slot) {
    skill *entry = hero.getSkill(slot);
    entry->restoreProgress(slot + 2, slot % (entry->getcooldown() + 1));
    require(entry->setSpecialization(slot <= 2 ? slot : 0), "set specialization");
  }
  require(SaveSystem::saveGame(hero, 40, fixture.name()), "save succeeds");
  const auto original = fixture.read();
  require(original.find("VERSION 3\n") != std::string::npos, "save version");
  fixture.write(reorderSkills(original));
  int floor = -99;
  auto restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored != nullptr, "load succeeds");
  require(floor == 40 && restored->getHighestFloor() == 85, "floor history survives retreat");
  require(restored->getname() == hero.getname() && restored->getjob() == hero.getjob(), "identity");
  require(restored->gethp() == 37 && restored->getmaxhp() == 800, "armor does not heal on load");
  require(restored->getattack() == 210 && restored->getmp() == 120 &&
          restored->getmaxmp() == 160, "combat stats round trip");
  require(restored->getlevel() == 50 && restored->getexp() == 123 &&
          restored->getcoin() == 4567 && restored->getfreerevives() == 2 &&
          restored->getcritRate() == 25, "progress stats round trip");
  const auto &backpack = restored->getSaveBackpack();
  require(backpack.size() == hero.getSaveBackpack().size(), "backpack size");
  for (size_t i = 0; i < backpack.size(); ++i)
    compareItem(hero.getSaveBackpack()[i], backpack[i]);
  const auto &equipped = restored->getSaveEquipments();
  require(equipped.size() == hero.getSaveEquipments().size(), "equipment size");
  for (size_t i = 0; i < equipped.size(); ++i)
    compareItem(hero.getSaveEquipments()[i], equipped[i]);
  for (int slot = 1; slot <= hero.getSkillCount(); ++slot) {
    const skill *before = hero.getSkill(slot), *after = restored->getSkill(slot);
    require(before->getskillid() == after->getskillid(), "skill ID");
    require(before->getskilllevel() == after->getskilllevel(), "legacy rank above five preserved");
    require(before->getdamage() == after->getdamage(), "skill damage round trip");
    require(before->getcostmp() == after->getcostmp(), "skill MP round trip");
    require(before->getcurrentcooldown() == after->getcurrentcooldown(), "skill remaining cooldown");
    require(before->getSpecialization() == after->getSpecialization(), "skill specialization");
  }
  require(SaveSystem::saveGame(*restored, floor, fixture.name()), "resave succeeds");
  require(fixture.read() == original, "save-load-save is stable");
}

std::string oldFormat(const std::string &modern, int version) {
  const std::set<std::string> allowed{"NAME", "JOB", "LEVEL", "EXP", "COIN", "HP",
      "MAX_HP", "ATTACK", "MP", "MAX_MP", "CRIT_RATE", "FREE_REVIVES", "FLOOR",
      "HIGHEST_FLOOR", "BACKPACK_COUNT", "EQUIP_COUNT", "SKILL_COUNT", "ITEM", "EQUIP", "SKILL"};
  std::istringstream input(modern);
  std::string line, output = "VERSION " + std::to_string(version) + "\n";
  while (std::getline(input, line)) {
    const auto end = line.find(' ');
    const auto key = line.substr(0, end);
    if (!allowed.count(key) || (version == 1 && key == "HIGHEST_FLOOR")) continue;
    if (key == "SKILL" || key == "ITEM" || key == "EQUIP") {
      int fields = key == "SKILL" ? 3 : 8;
      std::size_t cut = end;
      for (int i = 0; i < fields; ++i) cut = line.find('|', cut + 1);
      require(cut != std::string::npos, "modern record has expected fields");
      line.resize(cut);
    }
    output += line + "\n";
  }
  return output;
}

void legacySaves(Fixture &fixture) {
  Warrior hero("Legacy hero");
  hero.addItem(Item("Legacy item", 1, Potion, 50, 0, 20, 2));
  hero.getSkill(1)->restoreProgress(12, 1);
  hero.runState().legacyRules = true;
  require(SaveSystem::saveGame(hero, 76, fixture.name()), "legacy source save");
  const auto modern = fixture.read();
  auto legacy = oldFormat(modern, 1);
  legacy = replaceLine(legacy, "ITEM ", "ITEM Legacy item|1|3|50|0|20|2");
  fixture.write(legacy);
  int floor = 0;
  auto restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 76 && restored->getHighestFloor() == 76, "v1 floor migration");
  require(restored->runState().legacyRules && restored->runState().cleared[75],
          "legacy flag and passed floors retained");
  require(restored->getSkill(1)->getskilllevel() == 12, "v1 high skill rank");
  require(restored->getSkill(1)->getdamage() == hero.getSkill(1)->getdamage(), "v1 correct upgrade damage");
  require(restored->getSaveBackpack()[0].getJobRequirement() == AnyJob, "legacy item class default");
  fixture.write(replaceLine(legacy, "FLOOR ", "FLOOR 101"));
  restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 101 && restored->getHighestFloor() == 100 &&
          restored->runState().cleared[100] && restored->runState().attempts[100] == 1,
          "completed v1 tower");
  fixture.write(oldFormat(modern, 2));
  restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && restored->runState().legacyRules, "v2 retains legacy rules");
  require(SaveSystem::saveGame(*restored, floor, fixture.name()), "resave legacy as v3");
  restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && restored->runState().legacyRules &&
          restored->getSkill(1)->getskilllevel() == 12, "v3 does not silently migrate");
  const int coin = restored->getcoin();
  const auto itemId = restored->getSaveBackpack()[0].getId();
  require(SaveSystem::migrateLegacy(*restored), "explicit migration succeeds");
  require(!restored->runState().legacyRules && restored->getcoin() == coin &&
          restored->getSaveBackpack()[0].getId() == itemId &&
          restored->runState().cleared[75] && restored->getHighestFloor() == 76,
          "migration preserves campaign, coins and items");
  require(restored->getSkill(1)->getskilllevel() == Progression::skillRankCap(76),
          "migration normalizes rank");
  const int materials = restored->runState().materials;
  require(materials > 0 && materials <= 30 && !SaveSystem::migrateLegacy(*restored) &&
          restored->runState().materials == materials, "migration compensation once and bounded");
  require(SaveSystem::saveGame(*restored, floor, fixture.name()) &&
          SaveSystem::loadGame(floor, fixture.name()), "migrated v3 reloads");
}

void legacyRetreat(Fixture &fixture) {
  Warrior hero("Retreated legacy hero");
  hero.setHighestFloor(85);
  hero.runState().legacyRules = true;
  require(SaveSystem::saveGame(hero, 40, fixture.name()), "save legacy retreat source");
  const auto version2 = oldFormat(fixture.read(), 2);
  fixture.write(version2);
  int floor = 0;
  auto restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 40 && restored->getHighestFloor() == 85,
          "v2 current and historical floors survive retreat");
  for (int passed = 1; passed < 85; ++passed)
    require(restored->runState().cleared[passed] == 1 &&
            restored->runState().attempts[passed] == 1,
            "v2 retreat cannot reopen first-clear rewards on earlier floors");
  require(restored->runState().cleared[85] == 0 &&
          restored->runState().attempts[85] == 0,
          "highest reached floor is not assumed defeated");
  require(SaveSystem::saveGame(*restored, floor, fixture.name()), "persist inferred v2 progress as v3");
  restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 40 && restored->runState().cleared[84] &&
          !restored->runState().cleared[85], "v3 retains inferred legacy first-clear ledger");
  fixture.write(replaceLine(replaceLine(version2, "FLOOR ", "FLOOR 101"),
                            "HIGHEST_FLOOR ", "HIGHEST_FLOOR 100"));
  restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 101 && restored->runState().cleared[100] &&
          restored->runState().attempts[100] == 1, "completed v2 tower retains final clear");
}

void corruptSaves(Fixture &fixture) {
  Warrior hero("Validation hero");
  hero.addItem(Item("Potion", 1, Potion, 50, 0, 20, 2));
  require(SaveSystem::saveGame(hero, 1, fixture.name()), "validation source save");
  const auto valid = fixture.read();
  const std::vector<std::pair<std::string, std::string>> invalidFields = {
      {"VERSION ", "VERSION 999"}, {"COIN ", "COIN -2"},
      {"HP ", "HP 123oops"}, {"HP ", "HP 999999999999999999999999"},
      {"MAX_HP ", "MAX_HP 0"}, {"MP ", "MP 9999"}, {"LEVEL ", "LEVEL 0"},
      {"FLOOR ", "FLOOR 0"}, {"FLOOR ", "FLOOR 102"},
      {"HIGHEST_FLOOR ", "HIGHEST_FLOOR 0"}, {"HIGHEST_FLOOR ", ""},
      {"JOB ", "JOB Unknown"}, {"SKILL_COUNT ", "SKILL_COUNT 4"},
      {"SKILL 1|", "SKILL 1|-1|0"}, {"SKILL 1|", "SKILL 1|1001|0"},
      {"SKILL 1|", "SKILL 1|2|999"}, {"SKILL 1|", "SKILL 1|2|-1"},
      {"SKILL 1|", "SKILL 999|2|0"}, {"SKILL 1|", "SKILL 2|2|0"},
      {"SKILL 1|", "SKILL broken"}, {"SKILL 1|", "SKILL 1|1|0|"},
      {"BACKPACK_COUNT ", "BACKPACK_COUNT 2"},
      {"ITEM ", "ITEM Potion|1|9|50|0|20|2"},
      {"ITEM ", "ITEM Potion|1|3|50|0|20|-2"},
      {"ITEM ", "ITEM Potion|1|3|50|0|20|2|99"},
      {"ITEM ", "ITEM Potion|1|3|50|0|20|2|0|"},
      {"ITEM ", "ITEM Potion|1|3|broken|0|20|2"}};
  for (const auto &[prefix, value] : invalidFields) {
    fixture.write(replaceLine(valid, prefix, value));
    int floor = 42;
    require(!SaveSystem::loadGame(floor, fixture.name()), "reject corrupt: " + value);
    require(floor == 42, "failed load must preserve caller floor");
  }
  for (const auto &text : {valid + "COIN 10\n", std::string("VERSION 2\n"),
                           replaceLine(valid, "FLOOR ", "FLOOR 40"),
                           valid + "MYSTERY 1\n", valid + "PITY 1\n"}) {
    fixture.write(text);
    int floor = 42;
    require(!SaveSystem::loadGame(floor, fixture.name()) && floor == 42,
            "reject duplicates, missing fields and inconsistent floor history");
  }
  const std::vector<std::pair<std::string, std::string>> invalidRun = {
      {"RUN_SEED ", "RUN_SEED 4294967296"}, {"RUN_SEED ", "RUN_SEED -1"},
      {"CHALLENGE ", "CHALLENGE 3"}, {"LEGACY_RULES ", "LEGACY_RULES 2"},
      {"LOOT_TARGET ", "LOOT_TARGET 3"}, {"PITY ", "PITY 9"},
      {"MATERIALS ", "MATERIALS 10000"}, {"BATTLES ", "BATTLES -1"},
      {"GEAR_DROUGHT ", "GEAR_DROUGHT 1"}, {"ROUTES ", "ROUTES 0|0"},
      {"ROUTES ", "ROUTES 4|0|0|0|0|0|0|0|0|0"},
      {"CAMP_RESTED ", "CAMP_RESTED 2|0|0|0|0|0|0|0|0|0"},
      {"SHOP_PURCHASED ", "SHOP_PURCHASED 32|0|0|0|0|0|0|0|0|0"},
      {"SHOP_REFRESHES ", "SHOP_REFRESHES 21|0|0|0|0|0|0|0|0|0"},
      {"ATTEMPTS ", "ATTEMPTS 0|0"}, {"CLEARED ", "CLEARED 0|1"},
      {"SKILL 1|", "SKILL 1|1|0|1"}, {"SKILL 3|", "SKILL 3|1|0|1"},
      {"SKILL 1|", "SKILL 1|2|0|0"}, {"MATERIALS ", ""}};
  for (const auto &[prefix, replacement] : invalidRun) {
    fixture.write(replaceLine(valid, prefix, replacement));
    int floor = 73;
    require(!SaveSystem::loadGame(floor, fixture.name()) && floor == 73,
            "reject run/specialization inconsistency: " + replacement);
  }
  const auto id = Item::templateId("Potion", Potion, AnyJob, 0);
  for (const auto &line : std::vector<std::string>{"ITEM Potion|1|3|50|0|20|2|0|0|1|0|" + id,
                           "ITEM Potion|1|3|50|0|20|2|0|0|0|2|" + id,
                           "ITEM Potion|1|3|50|0|20|2|0|0|0|0|wrong-id"}) {
    fixture.write(replaceLine(valid, "ITEM ", line));
    int floor = 42;
    require(!SaveSystem::loadGame(floor, fixture.name()) && floor == 42,
            "reject effect, lock or ID corruption");
  }
  const auto armorId = Item::templateId("Armor", Armor, AnyJob, 0);
  const auto equipped = replaceLine(valid, "EQUIP_COUNT ", "EQUIP_COUNT 1") +
      "EQUIP Armor|1|2|50|10|20|1|0|0|0|0|" + armorId + "\n";
  fixture.write(equipped);
  int floor = 42;
  require(SaveSystem::loadGame(floor, fixture.name()) != nullptr,
          "valid equipment restoration fixture");
  for (const auto &text : {
      replaceLine(equipped, "EQUIP ", "EQUIP Armor|1|2|50|0|20|1|0|0|0|0|" + armorId),
      replaceLine(equipped, "EQUIP ", "EQUIP Armor|1|2|50|10|20|2|0|0|0|0|" + armorId),
      replaceLine(replaceLine(equipped, "MAX_HP ", "MAX_HP 50"), "HP ", "HP 50")}) {
    fixture.write(text);
    floor = 42;
    require(!SaveSystem::loadGame(floor, fixture.name()) && floor == 42,
            "invalid equipped durability, quantity or base stats rejected without item loss");
  }
}

void extremeLegacyValues(Fixture &fixture) {
  Warrior hero("Legacy limits");
  hero.loadStats(10000, 1000000000, 1000000000, 400, 800, 900000,
                 250, 500, 100, 1000);
  hero.getSkill(1)->restoreProgress(1000, 1);
  hero.setHighestFloor(100);
  hero.runState().legacyRules = true;
  require(SaveSystem::saveGame(hero, 100, fixture.name()), "save legacy maximum values");
  const auto source = fixture.read();
  for (int version : {1, 2}) {
    fixture.write(oldFormat(source, version));
    int floor = 0;
    auto restored = SaveSystem::loadGame(floor, fixture.name());
    require(restored && restored->getlevel() == 10000 &&
            restored->getexp() == 1000000000 && restored->getcoin() == 1000000000 &&
            restored->getfreerevives() == 1000 && restored->getSkill(1)->getskilllevel() == 1000,
            "legacy limits survive loading without silent clipping");
    require(SaveSystem::saveGame(*restored, floor, fixture.name()), "save legacy limits as v3");
    restored = SaveSystem::loadGame(floor, fixture.name());
    require(restored && restored->getlevel() == 10000 && restored->getcoin() == 1000000000 &&
            restored->getfreerevives() == 1000, "v3 preserves explicit legacy limits");
    require(SaveSystem::migrateLegacy(*restored) && restored->getlevel() == 75 &&
            restored->getSkill(1)->getskilllevel() == 5 &&
            restored->getfreerevives() == 3 && restored->getcoin() == 1000000000 &&
            restored->runState().materials == 30, "only explicit migration normalizes old growth");
    require(SaveSystem::saveGame(*restored, floor, fixture.name()) &&
            SaveSystem::loadGame(floor, fixture.name()), "normalized legacy limits remain loadable");
  }
}

void atomicSave(Fixture &fixture) {
  Warrior hero("Atomic hero");
  require(SaveSystem::saveGame(hero, 1, fixture.name()), "initial atomic save");
  const auto original = fixture.read();
  hero.runState().pity = 99;
  require(!SaveSystem::saveGame(hero, 1, fixture.name()) && fixture.read() == original,
          "invalid state must preserve original save");
  hero.runState().pity = 0;
  hero.gaincoin(50);
  require(SaveSystem::saveGame(hero, 1, fixture.name()) && fixture.read() != original,
          "replace an existing save");
  int floor = 0;
  auto loaded = SaveSystem::loadGame(floor, fixture.name());
  require(loaded && loaded->getcoin() == 50, "replaced save is complete");
  const auto directory = fixture.path.string() + ".directory";
  std::filesystem::create_directory(directory);
  const bool refused = !SaveSystem::saveGame(hero, 1, directory) &&
                       std::filesystem::is_directory(directory);
  std::filesystem::remove(directory);
  require(refused, "directory save target must be preserved");
  const auto prefix = fixture.path.filename().string();
  for (const auto &entry : std::filesystem::directory_iterator(fixture.path.parent_path())) {
    const auto name = entry.path().filename().string();
    require(name.find(prefix + ".tmp") != 0 && name.find(prefix + ".bak") != 0,
            "successful save left staging files behind");
  }
}
} // namespace

int main() {
  std::ostringstream gameLog;
  auto *originalOut = std::cout.rdbuf(gameLog.rdbuf());
  auto *originalError = std::cerr.rdbuf(gameLog.rdbuf());
  std::string failure;
  try {
    Fixture fixture;
    Warrior warrior("Warrior save");
    Mage mage("Mage save");
    Assassin assassin("Assassin save");
    roundTrip(warrior, fixture);
    roundTrip(mage, fixture);
    roundTrip(assassin, fixture);
    legacySaves(fixture);
    legacyRetreat(fixture);
    extremeLegacyValues(fixture);
    corruptSaves(fixture);
    atomicSave(fixture);
  } catch (const std::exception &error) {
    failure = error.what();
  }
  std::cout.rdbuf(originalOut);
  std::cerr.rdbuf(originalError);
  if (!failure.empty()) {
    std::cerr << "Save tests FAILED: " << failure << "\n" << gameLog.str();
    return 1;
  }
  std::cout << "Save tests passed: all classes, legacy migration, skill order, HP, malformed saves.\n";
  return 0;
}
