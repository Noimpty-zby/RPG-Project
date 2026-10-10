#include "jobs.h"
#include "save.h"
#include "skills.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
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
}

void roundTrip(player &hero, Fixture &fixture) {
  hero.loadStats(50, 123, 4567, 200, 700, 180, 120, 160, 25, 2);
  hero.setHighestFloor(85);
  hero.equipItem_T(Item("Old high level armor", 80, Armor, 100, 23, 90, 1), true);
  hero.equipItem_T(Item("Weapon", 40, Weapon, 30, 19, 70, 1), true);
  hero.sethp(37); // Below base HP: old loader incorrectly added armor's HP.
  hero.addItem(Item("Potion", 1, Potion, 50, 0, 20, 3));
  hero.addItem(Item("Same name", 1, Weapon, 8, 20, 30, 1, WarriorOnly));
  hero.addItem(Item("Same name", 1, Weapon, 8, 20, 30, 1, MageOnly));
  require(hero.getSaveBackpack().size() == 3, "class restrictions must not merge");
  for (int slot = 1; slot <= hero.getSkillCount(); ++slot) {
    skill *entry = hero.getSkill(slot);
    entry->restoreProgress(slot + 2, slot % (entry->getcooldown() + 1));
  }
  require(SaveSystem::saveGame(hero, 40, fixture.name()), "save succeeds");
  const auto original = fixture.read();
  require(original.find("VERSION 2\n") != std::string::npos, "save version");
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
  }
  require(SaveSystem::saveGame(*restored, floor, fixture.name()), "resave succeeds");
  require(fixture.read() == original, "save-load-save is stable");
}

void legacySaves(Fixture &fixture) {
  Warrior hero("Legacy hero");
  hero.addItem(Item("Legacy item", 1, Potion, 50, 0, 20, 2));
  hero.getSkill(1)->restoreProgress(12, 1);
  require(SaveSystem::saveGame(hero, 76, fixture.name()), "legacy source save");
  auto legacy = replaceLine(fixture.read(), "VERSION ", "VERSION 1");
  legacy = replaceLine(legacy, "HIGHEST_FLOOR ", "");
  legacy = replaceLine(legacy, "ITEM ", "ITEM Legacy item|1|3|50|0|20|2");
  fixture.write(legacy);
  int floor = 0;
  auto restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 76 && restored->getHighestFloor() == 76, "v1 floor migration");
  require(restored->getSkill(1)->getskilllevel() == 12, "v1 high skill rank");
  require(restored->getSkill(1)->getdamage() == hero.getSkill(1)->getdamage(), "v1 correct upgrade damage");
  require(restored->getSaveBackpack()[0].getJobRequirement() == AnyJob, "legacy item class default");
  fixture.write(replaceLine(legacy, "FLOOR ", "FLOOR 101"));
  restored = SaveSystem::loadGame(floor, fixture.name());
  require(restored && floor == 101 && restored->getHighestFloor() == 100, "completed v1 tower");
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
                           replaceLine(valid, "FLOOR ", "FLOOR 40")}) {
    fixture.write(text);
    int floor = 42;
    require(!SaveSystem::loadGame(floor, fixture.name()) && floor == 42,
            "reject duplicates, missing fields and inconsistent floor history");
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
    corruptSaves(fixture);
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
