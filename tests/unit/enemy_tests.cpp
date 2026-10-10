#include "core/random.h"
#include "enemy/enemy.h"
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
enemy foe(int floor, bool boss = false) {
  enemy e("fixture", 1000, 100, 0, 30, 20, floor);
  e.configureEncounter(floor, boss);
  e.setdodgeRate(0);
  return e;
}

void testProfilesAndBoundaries() {
  enemy legacy("legacy", 5000, 100, 0, 1, 1, 100);
  require(!legacy.isBoss(), "HP must not imply boss status");
  for (int i = 0; i < 10; ++i) {
    require(legacy.getIntent().kind == EnemyIntentKind::Attack,
            "Unconfigured enemies must remain compatible");
    legacy.finishTurn(0);
  }
  std::set<std::string> profiles;
  for (int floor = 10; floor <= 100; floor += 10) {
    auto boss = foe(floor, true);
    profiles.insert(boss.getEncounterTitle());
    require(!boss.getTacticalHint().empty() && boss.isBoss(),
            "Each chapter boss needs a visible tactical objective");
    require(boss.getattack() == 100 && boss.getmaxhp() == 1000,
            "Boss configuration must not inflate base stats");
  }
  require(profiles.size() == 10, "All ten chapter boss profiles must be named");
  auto outside = foe(1000, true);
  require(outside.getEncounterTitle() == foe(100, true).getEncounterTitle(),
          "Chapter lookup must clamp upper boundaries");
  auto low = foe(-4, true);
  require(low.getIntent().kind == EnemyIntentKind::Attack,
          "Negative floors must not index a profile out of range");
  for (int floor : {1, 2, 3, 4}) {
    auto normal = foe(floor);
    require(normal.getIntent().kind == EnemyIntentKind::Attack,
            "Ordinary foes must allow an opening action");
    normal.finishTurn(0);
    require(normal.getIntent().kind != EnemyIntentKind::Attack,
            "Even early ordinary enemies need a visible small tactical problem");
  }
}

void testChargeAndEarnedRecovery() {
  auto e = foe(10, true);
  auto intent = e.getIntent();
  require(intent.kind == EnemyIntentKind::Charge && intent.interruptible &&
              intent.damagePercent == 0,
          "Charge must be visible and interruptible before the heavy strike");
  e.finishTurn(-1);
  require(e.getIntent().kind == EnemyIntentKind::HeavyStrike &&
              e.getIntent().damagePercent == IntentTuning::BossHeavyStrikePercent &&
              !e.getIntent().piercesGuard,
          "Ignoring charge must lead to a blockable heavy strike");
  require(IntentTuning::HeavyStrikePercent > 200 && IntentTuning::BossHeavyStrikePercent > 200,
          "An unanswered charge must out-damage two plain attacks");
  e.finishTurn(-1);
  require(e.getIntent().kind != EnemyIntentKind::Recover,
          "A landed heavy strike must not hand the player a free recovery turn");

  e.configureEncounter(10, true);
  require(e.interruptCharge(), "Charge should accept an interruption");
  e.finishTurn(0, false, EnemyIntentKind::Charge);
  require(e.getIntent().kind == EnemyIntentKind::Recover,
          "A successful interruption must cancel the next heavy strike");
  e.finishTurn(1);
  require(!e.interruptCharge(), "Ordinary attacks are not interruptible");
  require(e.getIntent().kind == EnemyIntentKind::Attack,
          "Interrupted enemy must not jump back into heavy strike");

  // Pressured enemies (scarce supplies, elites) shake off an interrupt at once.
  auto pressured = foe(10, true);
  pressured.setTacticalPressure(1);
  require(pressured.interruptCharge(), "Pressured charge should still be interruptible");
  pressured.finishTurn(0, false, EnemyIntentKind::Charge);
  require(pressured.getIntent().kind != EnemyIntentKind::Recover &&
              pressured.getIntent().kind != EnemyIntentKind::HeavyStrike,
          "A pressured interrupt cancels the strike but grants no recovery turn");

  // Crossing a health threshold during an ordinary hit cannot retroactively
  // turn that already-telegraphed attack into a charge.
  auto threshold = foe(1);
  const auto cached = threshold.getIntent();
  threshold.sethp(300);
  threshold.finishTurn(0, false, cached.kind);
  require(threshold.getIntent().kind == EnemyIntentKind::Charge,
          "A new low-HP charge must still receive its own warning turn");
}

void testConditionalReactions() {
  auto priest = foe(20, true);
  require(priest.getIntent().kind == EnemyIntentKind::Wound,
          "Priest should teach suppression in its opening");
  priest.finishTurn(1);
  priest.finishTurn(1);
  priest.finishTurn(1);
  auto noHeal = priest;
  priest.finishTurn(-2, true);
  noHeal.finishTurn(0, false);
  require(priest.getIntent().kind == EnemyIntentKind::Wound &&
              noHeal.getIntent().kind == EnemyIntentKind::Attack,
          "Suppression must react to healing, not a universal three-turn cycle");

  auto breaker = foe(30, true);
  breaker.finishTurn(1);
  breaker.finishTurn(1);
  breaker.finishTurn(1);
  breaker.finishTurn(-1);
  require(breaker.getIntent().kind == EnemyIntentKind::Shatter &&
              breaker.getIntent().piercesGuard &&
              breaker.getIntent().damagePercent == IntentTuning::ShatterPercent &&
              IntentTuning::ShatterPercent > 100,
          "Shield breaker must respond to defensive habits and out-hit a plain attack");

  auto reader = foe(40, true);
  bool sawRead = false;
  for (int i = 0; i < 20; ++i) {
    const auto intent = reader.getIntent();
    if (intent.kind == EnemyIntentKind::ReadPattern) {
      require(intent.predictedAction == 0 || intent.predictedAction == -1,
              "Reader must predict a learned successor, not invent an action");
      sawRead = true;
      break;
    }
    reader.finishTurn(i % 2 ? -1 : 0);
  }
  require(sawRead, "Repeated attack/guard patterns should eventually be read");
  const auto before = reader.getIntent();
  reader.finishTurn(-99);
  reader.finishTurn(6);
  require(reader.getIntent().kind == before.kind &&
              reader.getIntent().predictedAction == before.predictedAction,
          "Invalid actions must not advance intent or learning");
  reader.configureEncounter(40, true);
  require(reader.getIntent().predictedAction == -99,
          "New encounters must clear learned transitions");
}

void testPressureAndSabotage() {
  auto easy = foe(20, true), hard = foe(20, true);
  hard.setTacticalPressure(2);
  int easySpecials = 0, hardSpecials = 0;
  for (int i = 0; i < 20; ++i) {
    easySpecials += easy.getIntent().kind == EnemyIntentKind::Wound;
    hardSpecials += hard.getIntent().kind == EnemyIntentKind::Wound;
    easy.finishTurn(1);
    hard.finishTurn(1);
  }
  require(hardSpecials > easySpecials && hard.getattack() == easy.getattack(),
          "Difficulty must add tactical pressure without stat inflation");

  auto disrupted = foe(1);
  disrupted.setObjectivePressure(true);
  require(disrupted.getIntent().kind == EnemyIntentKind::Charge,
          "Disrupt objectives must provide an actual interruption opportunity");

  auto boss = foe(60, true);
  Random::seed(1);
  for (int i = 0; i < 1000 && !boss.hasAffix(Affix::SHIELDING); ++i)
    boss.setAffix(100);
  require(boss.hasAffix(Affix::SHIELDING), "Shield fixture generation failed");
  boss.applySabotage();
  require(!boss.hasAffix(Affix::SHIELDING), "Sabotage must remove the shield source");
  boss.finishTurn(1);
  require(boss.getIntent().kind == EnemyIntentKind::HeavyStrike &&
              boss.getIntent().damagePercent == IntentTuning::SabotagedBossHeavyStrikePercent &&
              IntentTuning::SabotagedBossHeavyStrikePercent < IntentTuning::BossHeavyStrikePercent,
          "Sabotage must also weaken the boss core, even beyond removing a shield");
}

void testTemporaryWeakening() {
  auto e = foe(1);
  e.weakenAttack(35, 2);
  e.weakenAttack(35, 2);
  require(e.getattack() == 65, "Weakening must not compound");
  e.finishTurn(1);
  require(e.getattack() == 65, "Weakening expired before its second enemy action");
  e.finishTurn(1);
  require(e.getattack() == 100, "Weakening must restore its baseline");
  e.weakenAttack(100, 2);
  require(e.getattack() == 40, "Weakening must cap at sixty percent");
  e.resetattack(240);
  e.finishTurn(1);
  require(e.getattack() == 240, "Resetting attack must clear stale weakening");
  e.weakenAttack(-1, 2);
  e.weakenAttack(40, 0);
  require(e.getattack() == 240, "Invalid weakening must do nothing");
  e.resetattack(std::numeric_limits<int>::max());
  e.weakenAttack(60, 1);
  require(e.getattack() == static_cast<int>(static_cast<long long>(std::numeric_limits<int>::max()) * 40 / 100),
          "Large attack baselines must not overflow while weakened");
  e.finishTurn(0);
  require(e.getattack() == std::numeric_limits<int>::max(),
          "A large attack baseline must restore exactly");
}

void testNumericAndAffixBoundaries() {
  auto e = foe(1);
  e.sethp(500);
  e.heal(-1);
  require(e.gethp() == 500, "Negative healing must not damage an enemy");
  e.heal(std::numeric_limits<int>::max());
  require(e.gethp() == e.getmaxhp(), "Large healing must saturate without overflowing");
  e.setdodgeRate(-100);
  require(e.getdodgeRate() == 0, "Negative dodge chance must clamp to zero");
  e.setdodgeRate(std::numeric_limits<int>::max());
  require(e.getdodgeRate() == 100, "Dodge chance must not exceed one hundred");
  e.resethp(-10);
  require(e.gethp() == 0 && e.getmaxhp() == 0 && !e.isalive(),
          "Negative reset HP must remain a valid dead state");
  e.heal(std::numeric_limits<int>::max());
  require(!e.isalive(), "Healing must not revive a defeated enemy");

  auto affixed = foe(100);
  Random::seed(37);
  for (int roll = 0; roll < 1000; ++roll) affixed.setAffix(100);
  for (Affix affix : {Affix::STRONG, Affix::TOUGH, Affix::QUICK,
                     Affix::EXPLOSIVE, Affix::POISONOUS, Affix::HEALING,
                     Affix::SHIELDING})
    require(affixed.hasAffix(affix), "Repeated affix fixture should reach all seven unique affixes");
  require(affixed.getexp_reward() == 30 + 7 * 100 * 3 &&
              affixed.getcoin_reward() == 20 + 7 * 100 * 2,
          "Repeated affix rolls must reward each unique affix only once");
  std::ostringstream visible;
  auto *previous = std::cout.rdbuf(visible.rdbuf());
  affixed.displayAffixes();
  std::cout.rdbuf(previous);
  for (const char *label : {"[强力]", "[坚韧]", "[迅捷]", "[爆炸]", "[剧毒]", "[治疗]", "[护盾]"}) {
    const auto first = visible.str().find(label);
    require(first != std::string::npos && visible.str().find(label, first + 1) == std::string::npos,
            "Affix display must not repeat a duplicate effect");
  }
  enemy large("large", 100, 100, 0, std::numeric_limits<int>::max() - 1,
              std::numeric_limits<int>::max() - 1, std::numeric_limits<int>::max());
  for (int roll = 0; roll < 100; ++roll) large.setAffix(100);
  require(large.getexp_reward() == std::numeric_limits<int>::max() &&
              large.getcoin_reward() == std::numeric_limits<int>::max(),
          "Affix reward arithmetic must saturate at the integer limit");
}
} // namespace

int main() {
  try {
    testProfilesAndBoundaries();
    testChargeAndEarnedRecovery();
    testConditionalReactions();
    testPressureAndSabotage();
    testTemporaryWeakening();
    testNumericAndAffixBoundaries();
    std::cout << "enemy tactics tests passed\n";
  } catch (const std::exception &error) {
    std::cerr << "enemy tactics tests failed: " << error.what() << '\n';
    return 1;
  }
}
