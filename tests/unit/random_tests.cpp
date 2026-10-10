#include "core/game_config.h"
#include "core/random.h"
#include "game/campaign.h"
#include "player/jobs.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

// Golden values pin the generator and the whole content pipeline. CI runs this
// on Linux and Windows, so equal results prove a seed means the same expedition
// everywhere. If you change generation on purpose, update the constants below
// with the values printed by the failure message.
namespace {
void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}
std::string hex(std::uint64_t value) {
  std::ostringstream text;
  text << "0x" << std::hex << value;
  return text.str();
}

struct Fingerprint {
  std::uint64_t hash = 14695981039346656037ULL;
  void add(const std::string &text) {
    for (unsigned char byte : text) {
      hash ^= byte;
      hash *= 1099511628211ULL;
    }
    hash ^= 0xff; // Field separator.
    hash *= 1099511628211ULL;
  }
  void add(long long value) { add(std::to_string(value)); }
  void add(const Item &item) {
    add(item.getName());
    add(item.getValue());
    add(item.getDurability());
    add(item.getLevelRequirement());
    add(item.getTier());
    add(static_cast<int>(item.getEffect()));
  }
};

void testReferenceSequence() {
  // First outputs of the game's stream for seed 42.
  Random::seed(42);
  std::array<std::uint32_t, 4> values{};
  for (auto &value : values) value = Random::next();
  const std::array<std::uint32_t, 4> golden{0x713066eaU, 0x3c7a0d56U, 0xf424216aU, 0x25c89145U};
  for (std::size_t i = 0; i < values.size(); ++i)
    require(values[i] == golden[i], "PCG32 output " + std::to_string(i) + " is " + hex(values[i]));
  Random::seed(42);
  require(Random::next() == golden[0], "reseeding must restart the sequence");
}

void testBelow() {
  require(Random::below(0) == 0 && Random::below(1) == 0 && Random::below(-5) == 0,
          "degenerate bounds must return 0");
  Random::seed(7);
  std::array<int, 10> buckets{};
  for (int i = 0; i < 100000; ++i) {
    const int value = Random::below(10);
    require(value >= 0 && value < 10, "below() left its range");
    ++buckets[value];
  }
  for (int count : buckets) require(count > 9500 && count < 10500, "below(10) is visibly uneven");
}

std::uint64_t campaignFingerprint() {
  Fingerprint print;
  for (unsigned seed : {1u, 777u}) {
    for (int job = 0; job < 3; ++job) {
      std::unique_ptr<player> hero;
      if (job == 0) hero = std::make_unique<Warrior>("golden");
      else if (job == 1) hero = std::make_unique<Mage>("golden");
      else hero = std::make_unique<Assassin>("golden");
      Campaign::initialize(*hero, seed, 0);
      for (const Item &item : hero->getSaveEquipments()) print.add(item);
      for (const Item &item : hero->getSaveBackpack()) print.add(item);
      for (int floor = 1; floor <= GameConfig::MaxFloor; ++floor) {
        Campaign::chooseRoute(*hero, floor, floor / 10 % 3 + 1);
        const enemy monster = Campaign::makeEnemy(*hero, floor);
        print.add(monster.getname());
        print.add(monster.getmaxhp());
        print.add(monster.getattack());
        print.add(monster.getdodgeRate());
        for (Affix affix : monster.getAffixes()) print.add(static_cast<int>(affix));
        Campaign::seedStream(*hero, floor, 900);
        if (const auto loot = generateLoot(*hero, floor, true)) print.add(*loot);
        if (floor % 10 == 1)
          for (const Item &item : Campaign::shopStock(*hero, floor)) print.add(item);
      }
    }
  }
  return print.hash;
}

void testCampaignFingerprint() {
  const std::uint64_t actual = campaignFingerprint();
  require(actual == 0x7dfdeae748f15693ULL, "campaign fingerprint is " + hex(actual));
  require(campaignFingerprint() == actual, "generation must not depend on earlier calls");
}
} // namespace

int main() {
  std::ostringstream quiet;
  auto *previous = std::cout.rdbuf(quiet.rdbuf());
  try {
    testReferenceSequence();
    testBelow();
    testCampaignFingerprint();
    std::cout.rdbuf(previous);
    std::cout << "Random tests passed: PCG32 golden sequence, unbiased bounds, campaign fingerprint.\n";
  } catch (const std::exception &error) {
    std::cout.rdbuf(previous);
    std::cerr << "random tests failed: " << error.what() << '\n';
    return 1;
  }
}
