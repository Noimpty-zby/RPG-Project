#include "enemy.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void expect(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

enemy makeEnemy(int floor, bool boss = false, int attack = 100) {
  enemy target("Test enemy", 500, attack, 0, 30, 20, floor);
  target.configureEncounter(floor, boss);
  return target;
}

void testEncounterBoundaries() {
  enemy legacy("Legacy", 5000, 100, 0, 30, 20, 80);
  expect(!legacy.isBoss(), "High HP must not imply boss status");
  for (int i = 0; i < 9; ++i) {
    expect(legacy.getIntent().kind == EnemyIntentKind::Attack,
           "An unconfigured enemy must preserve its ordinary attacks");
    legacy.finishTurn(0);
  }

  auto earlyBoss = makeEnemy(39, true);
  expect(earlyBoss.isBoss(), "Boss status must be explicit even with low HP");
  for (int i = 0; i < 9; ++i) {
    const auto intent = earlyBoss.getIntent();
    expect(intent.kind == EnemyIntentKind::Attack &&
               intent.damagePercent == 100 && intent.healingPercent == 100,
           "Tactics must not change encounters below floor 40");
    earlyBoss.finishTurn(0);
  }
  auto tactical = makeEnemy(40);
  expect(tactical.getattack() == 100 && tactical.gethp() == 500 &&
             tactical.getexp_reward() == 30 && tactical.getcoin_reward() == 20,
         "Configuring tactics must not inflate baseline stats or rewards");
}

void testThreeTurnRhythm() {
  auto wound = makeEnemy(40);
  expect(wound.getIntent().kind == EnemyIntentKind::Attack,
         "Cycle must begin with an ordinary attack");
  wound.finishTurn(0);
  auto intent = wound.getIntent();
  expect(intent.kind == EnemyIntentKind::Wound &&
             intent.healingPercent == 50 && intent.damagePercent == 100,
         "Wound must halve healing for only its telegraphed action");
  wound.finishTurn(1);
  intent = wound.getIntent();
  expect(intent.kind == EnemyIntentKind::Recover &&
             intent.damagePercent == 0 && intent.healingPercent == 100 &&
             !intent.piercesGuard,
         "Recovery must be a healing window without an enemy attack");
  wound.finishTurn(-2);
  expect(wound.getIntent().kind == EnemyIntentKind::Attack,
         "Recovery must return to the start of the cycle");

  auto shatter = makeEnemy(41);
  shatter.finishTurn(-1);
  intent = shatter.getIntent();
  expect(intent.kind == EnemyIntentKind::Shatter &&
             intent.damagePercent == 80 && intent.piercesGuard &&
             intent.healingPercent == 100,
         "Shatter must trade damage for bypassing guard");
}

void testReadPattern() {
  auto reader = makeEnemy(42);
  reader.finishTurn(1);
  auto intent = reader.getIntent();
  expect(intent.kind == EnemyIntentKind::ReadPattern &&
             intent.predictedAction == -99 && intent.damagePercent == 75,
         "An unseen transition must not fabricate a prediction");

  // At turn 8, action 1 has led to action 2 twice and action 3 once.
  reader.configureEncounter(42, false);
  for (int action : {1, 2, 1, 3, 1, 2, 1})
    reader.finishTurn(action);
  intent = reader.getIntent();
  expect(intent.kind == EnemyIntentKind::ReadPattern &&
             intent.predictedAction == 2 && intent.damagePercent == 75,
         "Reader must predict the most frequent successor of the last action");
  expect(reader.getIntent().predictedAction == 2,
         "Reading the intent must not mutate prediction history");

  reader.finishTurn(-99);
  reader.finishTurn(6);
  expect(reader.getIntent().kind == EnemyIntentKind::ReadPattern &&
             reader.getIntent().predictedAction == 2,
         "Failed or invalid actions must not advance the turn or history");

  // Cover every documented action ID, including guard and potion transitions.
  for (int action = -2; action <= 5; ++action) {
    reader.configureEncounter(42, false);
    for (int i = 0; i < 4; ++i)
      reader.finishTurn(action);
    expect(reader.getIntent().predictedAction == action,
           "All documented action IDs must be learnable");
    std::ostringstream display;
    auto *previous = std::cout.rdbuf(display.rdbuf());
    reader.displayIntent();
    std::cout.rdbuf(previous);
    const std::string expected =
        action == -2 ? "使用药水"
        : action == -1 ? "格挡"
        : action == 0  ? "普通攻击"
                       : "技能槽 " + std::to_string(action);
    expect(display.str().find(expected) != std::string::npos,
           "Prediction tell must name the corresponding player action");
  }
  reader.configureEncounter(42, false);
  reader.finishTurn(0);
  expect(reader.getIntent().predictedAction == -99,
         "Reconfiguring an encounter must clear learned transitions");
}

void testLateBossRotation() {
  auto boss = makeEnemy(60, true);
  const EnemyIntentKind specials[] = {
      EnemyIntentKind::Wound, EnemyIntentKind::Shatter,
      EnemyIntentKind::ReadPattern, EnemyIntentKind::Wound};
  for (auto expected : specials) {
    expect(boss.getIntent().kind == EnemyIntentKind::Attack,
           "Mixed boss cycles must retain the ordinary opening");
    boss.finishTurn(0);
    expect(boss.getIntent().kind == expected,
           "Floor 60+ bosses must rotate their three specialties");
    boss.finishTurn(1);
    expect(boss.getIntent().kind == EnemyIntentKind::Recover,
           "Mixed bosses must retain the recovery window");
    boss.finishTurn(0);
  }

  auto ordinary = makeEnemy(60, false);
  ordinary.finishTurn(0);
  expect(ordinary.getIntent().kind == EnemyIntentKind::ReadPattern,
         "High-floor non-boss enemies must retain their single specialty");
  auto earlierBoss = makeEnemy(59, true);
  earlierBoss.finishTurn(0);
  expect(earlierBoss.getIntent().kind == EnemyIntentKind::Shatter,
         "Bosses below floor 60 must retain their single specialty");
}

void testTemporaryWeakening() {
  auto target = makeEnemy(40);
  target.weakenAttack(35, 2);
  expect(target.getattack() == 65, "Weakening must use the base attack");
  target.weakenAttack(35, 2);
  expect(target.getattack() == 65, "Repeated weakening must not compound");
  target.finishTurn(0);
  expect(target.getattack() == 65,
         "Two-turn weakening must remain after the first resolved turn");
  target.finishTurn(1);
  expect(target.getattack() == 100,
         "Expired weakening must restore the exact original baseline");

  target.weakenAttack(100, 2);
  expect(target.getattack() == 40, "Weakening must be capped at 60 percent");
  target.weakenAttack(10, 3);
  expect(target.getattack() == 40,
         "A weaker refresh must not overwrite an active stronger debuff");
  target.finishTurn(0);
  target.finishTurn(0);
  expect(target.getattack() == 40, "A refresh must retain the longer duration");
  target.finishTurn(0);
  expect(target.getattack() == 100, "Refreshed weakening must still expire");

  target.weakenAttack(50, 2);
  target.resetattack(240);
  expect(target.getattack() == 240,
         "Setting a new baseline must clear temporary weakening");
  target.finishTurn(0);
  expect(target.getattack() == 240,
         "An old debuff must not restore a stale baseline");
  target.weakenAttack(50, 2);
  expect(target.getattack() == 120, "New weakening must use the updated baseline");
  target.configureEncounter(60, true);
  expect(target.getattack() == 240,
         "Encounter reset must remove weakening and preserve its baseline");
  target.weakenAttack(-1, 2);
  target.weakenAttack(40, 0);
  expect(target.getattack() == 240, "Invalid weakening must be a no-op");
  auto harmless = makeEnemy(40, false, 0);
  harmless.weakenAttack(50, 2);
  expect(harmless.getattack() == 0, "Weakening must not create attack damage");
}
} // namespace

int main() {
  try {
    testEncounterBoundaries();
    testThreeTurnRhythm();
    testReadPattern();
    testLateBossRotation();
    testTemporaryWeakening();
    std::cout << "Enemy tactic tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Enemy tactic test failed: " << error.what() << '\n';
    return 1;
  }
}
