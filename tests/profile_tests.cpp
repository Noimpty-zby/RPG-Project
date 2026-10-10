#include "game_config.h"
#include "help.h"
#include "jobs.h"
#include "profile.h"
#include "skills.h"
#include "world.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
bool rejects(const std::string &text) {
  try {
    Profile::parse(text);
    return false;
  } catch (const std::runtime_error &) {
    return true;
  }
}
std::filesystem::path scratchDirectory() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  auto dir = std::filesystem::temp_directory_path() / ("aincrad-profile-" + std::to_string(stamp));
  std::filesystem::create_directories(dir);
  return dir;
}
std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

void testBestiaryMapping() {
  std::set<std::string> names;
  std::set<int> reached;
  for (int id = 0; id < Bestiary::EntryCount; ++id) {
    require(!Bestiary::entryName(id).empty() && !Bestiary::entryDescription(id).empty(),
            "every bestiary entry needs a name and description");
    names.insert(Bestiary::entryName(id));
  }
  require(static_cast<int>(names.size()) == Bestiary::EntryCount, "bestiary names must be unique");
  for (int floor = 1; floor <= GameConfig::MaxFloor; ++floor) {
    const int id = Bestiary::entryForFloor(floor);
    reached.insert(id);
    require(Bestiary::entryChapter(id) == GameConfig::chapter(floor), "entry chapter must match floor");
    require(Bestiary::entryIsBoss(id) == (floor % 10 == 0), "only floor 10s map to bosses");
    std::srand(static_cast<unsigned>(floor));
    const enemy monster = generateEnemy(floor);
    require(monster.getname().find(Bestiary::entryName(id)) != std::string::npos,
            "generated enemies must use their themed bestiary name");
    if (floor % 10 != 0) {
      // The entry description names the same signature tactic the enemy uses.
      const std::string role = monster.getEncounterTitle();
      require(Bestiary::entryDescription(id).rfind(role, 0) == 0,
              "bestiary entry must describe the enemy's signature tactic");
    }
  }
  require(static_cast<int>(reached.size()) == Bestiary::EntryCount, "every bestiary entry must be reachable");
  for (int id = 0; id < AffixCount; ++id)
    require(*affixName(static_cast<Affix>(id)) && *affixDescription(static_cast<Affix>(id)),
            "every affix needs a name and description");
}

void testRoundTripAndValidation() {
  Profile profile;
  enemy monster = generateEnemy(14);
  for (int i = 0; i < 1000 && !monster.hasAffix(Affix::HEALING); ++i) monster.setAffix(100);
  profile.recordEncounter(14, monster);
  profile.recordEncounter(14, monster);
  profile.recordVictory(14);
  profile.recordVictory(14);
  profile.recordVictory(14); // More victories than encounters are clamped.
  enemy boss = generateEnemy(100);
  boss.removeAffix(Affix::HEALING); // Keep the affix count deterministic.
  profile.recordEncounter(100, boss);
  RunRecord record;
  record.date = "2026-10-10";
  record.job = "Mage";
  record.challenge = 2;
  record.cleared = true;
  record.floor = 100;
  record.level = 62;
  record.battles = 140;
  record.turns = 900;
  record.deaths = 0;
  record.potions = 70;
  record.seed = 4000000000U;
  record.name = "pipe|name";
  profile.addRecord(record);
  Warrior hero("achiever");
  hero.runState().cleared[10] = 1;
  hero.runState().attempts[10] = 1;
  require(profile.checkAchievements(hero, nullptr).size() == 1 && profile.unlocked("first_chapter"),
          "clearing floor 10 must unlock exactly the first-chapter achievement");

  const Profile restored = Profile::parse(profile.serialize());
  require(restored.serialize() == profile.serialize(), "profile must round-trip exactly");
  const auto &entry = restored.entry(Bestiary::entryForFloor(14));
  require(entry.seen == 2 && entry.won == 2 && entry.firstFloor == 14, "bestiary counts lost");
  require(restored.affixSeen(Affix::HEALING) == 2, "affix counts lost");
  require(restored.discovered() == 2, "discovered count wrong");
  require(restored.records().size() == 1 && restored.records()[0].name == "pipe|name" &&
              restored.records()[0].seed == 4000000000U && restored.records()[0].cleared,
          "run record fields lost");
  require(restored.unlocked("first_chapter"), "achievements lost");
  require(Profile::parse("PROFILE 1\nACHIEVEMENT future_key\n").unlocked("future_key"),
          "unknown achievement keys must survive for newer builds");

  require(rejects("") && rejects("SEEN 1 1 0 1\n"), "missing header accepted");
  require(rejects("PROFILE 1\nSEEN 1 1 2 2\n"), "more wins than encounters accepted");
  require(rejects("PROFILE 1\nSEEN 1 1 0 2\nSEEN 1 1 0 2\n"), "duplicate entry accepted");
  require(rejects("PROFILE 1\nSEEN 6 1 0 2\n"), "first floor outside the entry's chapter accepted");
  require(rejects("PROFILE 1\nSEEN 50 1 0 2\n") && rejects("PROFILE 1\nAFFIX 7 1\n"), "out-of-range id accepted");
  require(rejects("PROFILE 1\nRECORD 2026-1-01|Mage|0|0|5|3|1|1|0|0|1|x\n"), "bad date accepted");
  require(rejects("PROFILE 1\nRECORD 2026-10-10|Knight|0|0|5|3|1|1|0|0|1|x\n"), "unknown job accepted");
  require(rejects("PROFILE 1\nRECORD 2026-10-10|Mage|0|0|5|3|1|1|0|0|1|\n"), "empty record name accepted");
  require(rejects("PROFILE 1\nMYSTERY 1\n"), "unknown field accepted");
  std::string many = "PROFILE 1\n";
  for (int i = 0; i <= Profile::MaxRecords; ++i) many += "RECORD 2026-10-10|Mage|0|0|5|3|1|1|0|0|1|x\n";
  require(rejects(many), "too many records accepted");
}

void testRecordTrimming() {
  Profile profile;
  for (int i = 0; i < Profile::MaxRecords + 5; ++i) {
    RunRecord record;
    record.date = "2026-10-10";
    record.job = "Warrior";
    record.floor = i + 1;
    record.name = "run" + std::to_string(i);
    profile.addRecord(record);
  }
  require(static_cast<int>(profile.records().size()) == Profile::MaxRecords &&
              profile.records().front().name == "run5" &&
              profile.records().back().name == "run" + std::to_string(Profile::MaxRecords + 4),
          "records must keep only the newest entries");
}

void testAchievementConditions() {
  Profile profile;
  Assassin hero("boss");
  BattleOutcome flawless{20, true, 0, 0, 3, 10, 200};
  auto unlocked = profile.checkAchievements(hero, &flawless);
  require(profile.unlocked("flawless_boss") && profile.unlocked("interrupt_master") &&
              profile.unlocked("last_stand") && !profile.unlocked("no_potion_boss"),
          "boss outcome achievements evaluated incorrectly");
  require(profile.checkAchievements(hero, &flawless).empty(), "achievements must unlock only once");
  BattleOutcome lostBoss{60, false, 0, 0, 0, 0, 200};
  Profile other;
  other.checkAchievements(hero, &lostBoss);
  require(!other.unlocked("flawless_boss"), "a lost fight must not count as a flawless boss");
  BattleOutcome ordinary{61, true, 0, 0, 0, 200, 200};
  other.checkAchievements(hero, &ordinary);
  require(!other.unlocked("flawless_boss") && !other.unlocked("no_potion_boss"),
          "ordinary floors must not count as boss victories");
  BattleOutcome deepBoss{50, true, 30, 0, 0, 150, 200};
  other.checkAchievements(hero, &deepBoss);
  require(other.unlocked("no_potion_boss") && !other.unlocked("last_stand"), "deep boss conditions wrong");

  // Three classes beating the floor 10 boss is tracked across runs.
  Profile classes;
  Warrior warrior("w"); Mage mage("m"); Assassin assassin("a");
  for (player *who : std::initializer_list<player *>{&warrior, &mage}) {
    who->runState().cleared[10] = 1;
    who->runState().attempts[10] = 1;
    classes.checkAchievements(*who, nullptr);
  }
  require(!classes.unlocked("all_jobs"), "two classes unlocked the three-class achievement");
  const Profile resumed = Profile::parse(classes.serialize());
  Profile next = resumed;
  assassin.runState().cleared[10] = 1;
  assassin.runState().attempts[10] = 1;
  next.checkAchievements(assassin, nullptr);
  require(next.unlocked("all_jobs"), "class progress must persist across profile saves");

  Profile routes;
  Mage traveller("r");
  for (int chapter = 0; chapter < 3; ++chapter) {
    traveller.runState().routes[chapter] = chapter + 1;
    traveller.runState().cleared[chapter * 10 + 10] = 1;
    traveller.runState().attempts[chapter * 10 + 10] = 1;
  }
  routes.checkAchievements(traveller, nullptr);
  require(routes.unlocked("all_routes"), "three completed routes must unlock all_routes");

  Profile iron;
  Warrior ironman("i");
  ironman.runState().challenge = 2;
  ironman.runState().cleared[30] = 1;
  ironman.runState().attempts[30] = 1;
  iron.checkAchievements(ironman, nullptr);
  require(iron.unlocked("iron_trial") && !iron.unlocked("iron_summit"), "no-revive achievements wrong");

  Profile build;
  Warrior specialist("s");
  specialist.setHighestFloor(30);
  while (specialist.getlevel() < 10) specialist.levelup();
  require(specialist.chooseSpecialization(1, 1) && specialist.chooseSpecialization(2, 2),
          "fixture specialization failed");
  specialist.gaincoin(20000);
  specialist.equipItem_T(Item("epic blade", 1, Weapon, 10, 10, 10, 1, WarriorOnly, 3));
  build.checkAchievements(specialist, nullptr);
  require(build.unlocked("specialist") && build.unlocked("rich") && build.unlocked("epic_gear"),
          "build achievements wrong");
}

void testLoadSafety() {
  const auto dir = scratchDirectory();
  const auto path = (dir / "profile.dat").string();
  std::string warning;
  Profile fresh = Profile::load(path, &warning);
  require(warning.empty() && fresh.writable() && fresh.discovered() == 0, "missing profile must start empty");
  fresh.recordEncounter(3, generateEnemy(3));
  require(fresh.save() && Profile::load(path).discovered() == 1, "profile did not persist");

  { std::ofstream(path, std::ios::binary) << "PROFILE 1\nSEEN 999 1 0 1\n"; }
  Profile recovered = Profile::load(path, &warning);
  require(!warning.empty() && recovered.writable() && recovered.discovered() == 0 &&
              std::filesystem::exists(path + ".broken") && !std::filesystem::exists(path),
          "a corrupt profile must be moved aside, not overwritten");

  const std::string future = "PROFILE 99\nWHATEVER\n";
  { std::ofstream(path, std::ios::binary) << future; }
  warning.clear();
  Profile newer = Profile::load(path, &warning);
  require(!warning.empty() && !newer.writable() && !newer.save() && readFile(path) == future,
          "a newer profile must be left untouched");
  std::filesystem::remove_all(dir);
}

void testHelp() {
  require(Help::topicCount() >= 6, "help needs its topics");
  for (int topic = 1; topic <= Help::topicCount(); ++topic)
    require(*Help::topicTitle(topic) && !Help::topicText(topic).empty(), "empty help topic");
  require(Help::topicText(0).empty() && Help::topicText(Help::topicCount() + 1).empty(),
          "out-of-range help topics must be empty");
  bool affixPage = false;
  for (int topic = 1; topic <= Help::topicCount(); ++topic) {
    const std::string text = Help::topicText(topic);
    bool all = true;
    for (int id = 0; id < AffixCount; ++id)
      all &= text.find(affixName(static_cast<Affix>(id))) != std::string::npos;
    affixPage |= all;
  }
  require(affixPage, "help must describe every affix");
}
} // namespace

int main() {
  std::ostringstream quiet;
  auto *previous = std::cout.rdbuf(quiet.rdbuf());
  try {
    testBestiaryMapping();
    testRoundTripAndValidation();
    testRecordTrimming();
    testAchievementConditions();
    testLoadSafety();
    testHelp();
    std::cout.rdbuf(previous);
    std::cout << "Profile tests passed: bestiary mapping, round-trip, validation, achievements, records, load safety, help.\n";
  } catch (const std::exception &error) {
    std::cout.rdbuf(previous);
    std::cerr << "profile tests failed: " << error.what() << '\n';
    return 1;
  }
}
