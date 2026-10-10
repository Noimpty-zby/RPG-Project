#pragma once
#include "enemy/enemy.h"
#include <array>
#include <set>
#include <string>
#include <vector>

class player;

// Facts about one finished encounter, captured before settlement healing.
struct BattleOutcome {
  int floor = 1;
  bool won = false;
  int damageTaken = 0;
  int potionsUsed = 0;
  int interrupts = 0;
  int hpAfter = 0;
  int maxHp = 1;
};

// One finished expedition (a clear, or a death that ended the run).
struct RunRecord {
  std::string date; // YYYY-MM-DD, local time
  std::string job;  // JobProfile::saveKey
  int challenge = 0;
  bool cleared = false;
  int floor = 1;
  int level = 1;
  int battles = 0;
  int turns = 0;
  int deaths = 0;
  int potions = 0;
  unsigned seed = 1;
  std::string name;
};

struct AchievementInfo {
  const char *key; // Stable profile identifier.
  const char *title;
  const char *description;
};

// Cross-run progress kept beside the save: bestiary, achievements and the
// history of finished expeditions. Starting a new game never resets it.
class Profile {
public:
  struct BestiaryEntry {
    int seen = 0;
    int won = 0;
    int firstFloor = 0;
  };
  static constexpr int Version = 1;
  static constexpr int MaxRecords = 20;

  static const std::vector<AchievementInfo> &achievements();

  // A missing file gives an empty profile. A malformed file is renamed to
  // "<path>.broken" so the fresh profile never overwrites it. A file from a
  // newer game version is left untouched and this session will not write it.
  static Profile load(const std::string &path, std::string *warning = nullptr);
  // Throws std::runtime_error on malformed input.
  static Profile parse(const std::string &text);
  std::string serialize() const;
  bool save() const;
  bool writable() const { return canWrite; }

  void recordEncounter(int floor, const enemy &monster);
  void recordVictory(int floor);
  void addRecord(RunRecord record);
  // Unlocks every achievement whose condition now holds and returns the new
  // ones. battle may be null when checking outside an encounter.
  std::vector<const AchievementInfo *> checkAchievements(const player &hero,
                                                         const BattleOutcome *battle);

  const BestiaryEntry &entry(int id) const;
  int discovered() const;
  int affixSeen(Affix affix) const;
  int affixesDiscovered() const;
  bool unlocked(const std::string &key) const { return unlockedKeys.count(key) != 0; }
  int unlockedCount() const;
  const std::vector<RunRecord> &records() const { return runRecords; }

  void printBestiaryOverview() const;
  void printBestiaryChapter(int chapter) const;
  void printAffixes() const;
  void printAchievements() const;
  void printRecords() const;

private:
  std::string path;
  bool canWrite = true;
  std::array<BestiaryEntry, Bestiary::EntryCount> bestiary{};
  std::array<int, AffixCount> affixes{};
  std::set<std::string> unlockedKeys;
  int chapterOneJobs = 0; // Bit per Job that has beaten the floor 10 boss.
  std::vector<RunRecord> runRecords;
};

std::string todayDate();
