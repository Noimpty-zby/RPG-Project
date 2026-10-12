#include "core/random.h"
#include "game/campaign.h"
#include "game/combat.h"
#include "player/jobs.h"
#include "player/skills.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
// Damage of an intent against the fixtures' 100-attack enemy, plain or guarded.
int hit(int percent) { return std::max(1, 100 * percent / 100); }
int guarded(int damage) { return damage / 2 + damage % 2; }
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
std::unique_ptr<Warrior> hero(int hp = 800, int attack = 50) {
  auto h = std::make_unique<Warrior>("test");
  h->loadStats(80, 0, 100, hp, 1000, attack, 200, 200, 0, 3);
  h->setHighestFloor(100);
  return h;
}
enemy foe(int floor = 1, bool boss = false, int attack = 100) {
  enemy e("fixture", 100000, attack, 0, 500, 500, floor);
  e.configureEncounter(floor, boss);
  e.setdodgeRate(0);
  return e;
}
void turn(BattleSession &battle, BattleAction action) {
  require(battle.beginTurn(), "Expected a live action window");
  require(battle.act(action), "Expected an available action");
}
void givePotion(player &h, int quantity = 3) {
  h.addItem(Item("fixture potion", 1, Potion, 200, 0, 1, quantity, AnyJob));
}
void giveAffix(enemy &e, Affix affix) {
  for (int i = 0; i < 1000 && !e.hasAffix(affix); ++i) e.setAffix(100);
  require(e.hasAffix(affix), "Affix fixture generation failed");
}

void testInvalidActionsAndResources() {
  auto h = hero(); auto e = foe();
  h->setmp(0);
  h->getSkill(1)->resetcooldown();
  BattleSession b(*h, e);
  require(!b.claimSettlement(), "Live encounters cannot claim rewards");
  require(!b.act({ActionType::Attack}), "An action requires beginTurn");
  require(b.beginTurn(), "Initial beginTurn failed");
  const int mp = h->getmp(), hp = h->gethp();
  const int cooldown = h->getSkill(1)->getcurrentcooldown();
  require(mp == 2, "Passive Warrior recovery must be two MP, not eight");
  for (int i = 0; i < 10; ++i) {
    require(!b.act({ActionType::Skill, 99}) && !b.act({ActionType::Potion, 99}) &&
                !b.act({static_cast<ActionType>(99), 0}),
            "Invalid actions must be rejected");
    require(b.beginTurn(), "A failed choice must retain the action window");
    require(b.turns() == 0 && h->getmp() == mp && h->gethp() == hp &&
                h->getSkill(1)->getcurrentcooldown() == cooldown,
            "Retries must not farm MP, cooldowns or advance time");
  }
  require(b.act({ActionType::Attack}), "Basic attack failed");
  require(b.turns() == 1 && h->getmp() == mp + 3,
          "A successful basic attack should earn class resources");
  require(b.beginTurn() && h->getmp() == mp + 5,
          "Only the next actual turn should restore passive MP again");
  require(h->getcoin() == 100 && h->getexp() == 0,
          "The battle engine must not award currency or experience");

  auto full = hero(1000); auto fullEnemy = foe(); givePotion(*full, 1);
  BattleSession fullBattle(*full, fullEnemy);
  fullBattle.beginTurn();
  require(!fullBattle.act({ActionType::Potion, 1}) && fullBattle.turns() == 0 &&
              full->getSaveBackpack()[0].getQuantity() == 1,
          "A full-HP potion attempt must not consume an item or a turn");
}

void testChargeCounterAndRecovery() {
  auto h = hero(); auto e = foe(10, true);
  BattleSession b(*h, e);
  require(e.getIntent().kind == EnemyIntentKind::Charge,
          "Chapter one should show its charge before striking");
  const int hp = h->gethp();
  turn(b, {ActionType::Attack});
  require(h->gethp() == hp && b.report().interrupts == 1 &&
              e.getIntent().kind == EnemyIntentKind::Recover,
          "A basic hit must interrupt charge and earn a safe recovery turn");
  turn(b, {ActionType::Guard});
  require(h->gethp() == hp && e.getIntent().kind != EnemyIntentKind::Recover,
          "Recovery must last exactly one earned window");

  auto guardHero = hero(); auto charging = foe(10, true);
  BattleSession guardBattle(*guardHero, charging);
  turn(guardBattle, {ActionType::Guard});
  require(charging.getIntent().kind == EnemyIntentKind::HeavyStrike,
          "An unbroken charge must become the advertised heavy strike");
  turn(guardBattle, {ActionType::Guard});
  const int heavy = guarded(hit(IntentTuning::BossHeavyStrikePercent));
  require(guardHero->gethp() == 800 - heavy && guardBattle.report().damageTaken == heavy,
          "Basic guard must halve the heavy strike");

  // A dodge costs the hit's damage and MP, never the correct read.
  auto missHero = hero(); auto dodging = foe(10, true); dodging.setdodgeRate(100);
  missHero->setmp(0);
  BattleSession missBattle(*missHero, dodging);
  require(missBattle.beginTurn(), "Expected a live action window");
  const int dodgingHp = dodging.gethp(), missMp = missHero->getmp();
  std::ostringstream shown;
  auto *previous = std::cout.rdbuf(shown.rdbuf());
  require(missBattle.act({ActionType::Attack}), "Expected an available action");
  std::cout.rdbuf(previous);
  require(missBattle.report().interrupts == 1 &&
              dodging.getIntent().kind == EnemyIntentKind::Recover,
          "A dodged plain attack must still interrupt the charge");
  require(dodging.gethp() == dodgingHp && missHero->getmp() == missMp &&
              shown.str().find("普攻虽被闪避，仍打断了蓄力") != std::string::npos,
          "A dodged interrupt deals no damage, earns no MP and says why it still worked");
  auto plainHero = hero(); auto plainDodger = foe(1, false); plainDodger.setdodgeRate(100);
  BattleSession plainBattle(*plainHero, plainDodger);
  require(!plainDodger.getIntent().interruptible, "Expected a non-charge opening intent");
  turn(plainBattle, {ActionType::Attack});
  require(plainBattle.report().interrupts == 0,
          "Only a charge can be interrupted, dodged or not");
}

// Every text about an interrupt must match what the enemy then does: ordinary
// fights and bosses rest, scarce-supply and elite fights do not.
void testInterruptTextsMatchRecovery() {
  struct Case { int floor; bool boss; int difficulty; bool elite; };
  const Case cases[] = {{4, false, 0, false}, {4, false, 1, true}, {4, false, 1, false},
                        {4, false, 2, true},  {10, true, 0, false}, {10, true, 1, false}};
  for (const Case &c : cases) {
    auto h = hero(); auto e = foe(c.floor, c.boss, 0);
    EncounterRules rules;
    rules.difficulty = c.difficulty;
    rules.elite = c.elite;
    BattleSession b(*h, e, rules);
    for (int i = 0; i < 30 && !e.getIntent().interruptible; ++i) turn(b, {ActionType::Guard});
    require(e.getIntent().interruptible, "Interrupt text fixture must reach a charge");
    const std::string hint = e.getTacticalHint();
    std::ostringstream shown;
    auto *previous = std::cout.rdbuf(shown.rdbuf());
    e.displayIntent();
    turn(b, {ActionType::Attack});
    std::cout.rdbuf(previous);
    const bool rests = e.getIntent().kind == EnemyIntentKind::Recover;
    require(b.report().interrupts == 1 && rests == (c.difficulty == 0),
            "Only unpressured enemies rest after an interrupt");
    const std::string text = shown.str();
    require(rests == (text.find("敌人随后休整一回合") != std::string::npos) &&
                rests == (text.find("下回合敌人休整") != std::string::npos) &&
                rests != (text.find("不休整") != std::string::npos),
            "Charge intent and interrupt log must promise rest exactly when it follows");
    require(c.boss || rests != (hint.find("不休整") != std::string::npos),
            "The charge guard's tactical hint must match the interrupt rule");
  }
}

void testSuppressionAndShatterCounters() {
  auto h = hero(500); auto priest = foe(20, true); givePotion(*h);
  BattleSession b(*h, priest);
  turn(b, {ActionType::Potion, 1});
  require(h->gethp() == 500 + 100 - hit(IntentTuning::WoundPercent) &&
              b.report().suppressedHealing == 100 &&
              b.report().potionsUsed == 1 && b.turns() == 1,
          "Suppression must halve actual healing and potion use must cost a turn");
  const int afterPotion = h->gethp();
  h->heal(100);
  require(h->gethp() == afterPotion + 100, "Suppression must not leak outside its action");

  auto basic = hero(); auto secondPriest = foe(20, true);
  BattleSession basicBattle(*basic, secondPriest);
  turn(basicBattle, {ActionType::Guard});
  require(basic->gethp() == 800 - guarded(hit(IntentTuning::WoundPercent)) &&
              basicBattle.report().suppressedHealing == 0,
          "Ordinary guard must offer a basic response to suppression");

  auto guardHero = hero(); auto breaker = foe(30, true); guardHero->setmp(0);
  BattleSession guardBattle(*guardHero, breaker);
  turn(guardBattle, {ActionType::Guard});
  require(guardHero->gethp() == 800 - hit(IntentTuning::ShatterPercent) && guardHero->getmp() == 8 &&
              guardBattle.report().guards == 1,
          "Shatter bypasses guard reduction but must retain guard resource gains");

  auto controlHero = hero(); auto controlled = foe(30, true);
  controlled.weakenAttack(50, 2);
  BattleSession controlBattle(*controlHero, controlled);
  turn(controlBattle, {ActionType::Attack});
  require(controlHero->gethp() == 800 - std::max(1, 50 * IntentTuning::ShatterPercent / 100),
          "Temporary weakening must still mitigate shatter damage");
}

enemy predictableEnemy() {
  auto reader = foe(40, true);
  for (int i = 0; i < 30 && reader.getIntent().kind != EnemyIntentKind::ReadPattern; ++i)
    reader.finishTurn(0);
  require(reader.getIntent().kind == EnemyIntentKind::ReadPattern &&
              reader.getIntent().predictedAction == 0,
          "Reader fixture did not learn repeated attacks");
  return reader;
}

void testReadCounterAndReport() {
  auto h = hero(); auto reader = predictableEnemy();
  BattleSession b(*h, reader);
  turn(b, {ActionType::Guard});
  require(h->gethp() == 800 - guarded(hit(IntentTuning::ReadMissPercent)) &&
              b.report().readPunishments == 0,
          "Changing a predicted attack to basic guard must avoid the punishment");
  auto repeated = hero(); auto readerAgain = predictableEnemy();
  BattleSession punished(*repeated, readerAgain);
  turn(punished, {ActionType::Attack});
  require(repeated->gethp() == 800 - hit(IntentTuning::ReadHitPercent) &&
              punished.report().readPunishments == 1 &&
              punished.report().damageTaken == hit(IntentTuning::ReadHitPercent),
          "Matching a displayed prediction must be counted and resolved");
}

void testObjectivesAndSettlement() {
  auto h = hero(); auto e = foe(1, false, 0);
  BattleSession survive(*h, e, {EncounterObjective::Survive, 2});
  turn(survive, {ActionType::Guard});
  require(!survive.finished(), "Survival must count actual completed turns");
  turn(survive, {ActionType::Guard});
  require(survive.finished() && survive.won() && e.isalive(),
          "Survival should succeed without requiring an enemy kill");
  require(survive.claimSettlement() && !survive.claimSettlement(),
          "A completed encounter can be settled only once");
  require(!survive.beginTurn() && !survive.act({ActionType::Attack}),
          "A won objective must not accept extra actions");

  auto disruptHero = hero(); auto disruptEnemy = foe(2, false, 0);
  BattleSession disrupt(*disruptHero, disruptEnemy, {EncounterObjective::Disrupt});
  while (!disrupt.finished())
    turn(disrupt, {disruptEnemy.getIntent().interruptible ? ActionType::Attack
                                                        : ActionType::Guard});
  require(disrupt.won() && disrupt.report().interrupts == 2 && disruptEnemy.isalive(),
          "Disrupt must complete after two actual basic-attack interruptions");

  auto escaping = hero(); auto pursuer = foe(1, false, 0);
  BattleSession escape(*escaping, pursuer, {EncounterObjective::Escape});
  turn(escape, {ActionType::Guard});
  turn(escape, {ActionType::Guard});
  require(!escape.finished(), "Escape must not complete before three turns");
  turn(escape, {ActionType::Attack});
  require(!escape.finished(), "Escape must require the deliberate guard action");
  turn(escape, {ActionType::Guard});
  require(escape.won() && escape.turns() == 4,
          "Guard after the third turn should complete an escape");

  for (auto objective : {EncounterObjective::Defeat, EncounterObjective::Survive,
                         EncounterObjective::Disrupt, EncounterObjective::Escape}) {
    auto killer = hero(); auto weak = foe(1, false, 0); weak.resethp(1);
    BattleSession kill(*killer, weak, {objective});
    turn(kill, {ActionType::Attack});
    require(kill.won(), "A kill should satisfy each supported encounter objective");
  }
}

void testDeathTimeoutAndBoundaryRules() {
  auto dying = hero(20); auto lethal = foe();
  BattleSession survival(*dying, lethal, {EncounterObjective::Survive, 1});
  turn(survival, {ActionType::Guard});
  require(survival.finished() && !survival.won(),
          "Death on the survival target turn must take priority over success");

  auto doomed = hero(1); auto explosive = foe(1, false, 100); explosive.resethp(1);
  giveAffix(explosive, Affix::EXPLOSIVE);
  explosive.removeAffix(Affix::POISONOUS);
  explosive.setdodgeRate(0);
  BattleSession mutual(*doomed, explosive);
  turn(mutual, {ActionType::Attack});
  require(mutual.finished() && !mutual.won() && !explosive.isalive(),
          "Death from a defeated enemy's explosion must remain a loss");

  auto poisoned = hero(1); auto poisoner = foe();
  giveAffix(poisoner, Affix::POISONOUS);
  BattleSession poison(*poisoned, poisoner, {EncounterObjective::Survive, 1});
  require(!poison.beginTurn() && poison.finished() && !poison.won() &&
              poison.turns() == 0 && poison.report().poisonDamage == 1 &&
              poison.report().damageTaken == 1 && poison.claimSettlement() &&
              !poison.claimSettlement(),
          "Poison death before acting must not count an effective turn");

  auto stuck = hero(); auto harmless = foe(1, false, 0);
  BattleSession timeout(*stuck, harmless, {EncounterObjective::Escape});
  while (!timeout.finished()) turn(timeout, {ActionType::Attack});
  require(timeout.report().timeout && !timeout.won() && timeout.turns() == 15 &&
              timeout.claimSettlement() && !timeout.claimSettlement(),
          "Unfulfilled objectives must time out and settle failure once");

  auto shortHero = hero(); auto shortEnemy = foe(1, false, 0);
  BattleSession shortGoal(*shortHero, shortEnemy, {EncounterObjective::Survive, -99, -5});
  turn(shortGoal, {ActionType::Guard});
  require(shortGoal.won(), "Invalid survival target must clamp to one turn");
  auto longHero = hero(); auto longEnemy = foe(1, false, 0);
  BattleSession longGoal(*longHero, longEnemy, {EncounterObjective::Survive, 999, 999});
  require(longGoal.turnLimit() == 35, "Oversized survival target must stay bounded");
}

void testInitiallyDeadEncounterCannotSettle() {
  for (int state = 0; state < 3; ++state) {
    auto h = hero(state == 1 || state == 2 ? 0 : 800);
    auto e = foe();
    if (state == 0 || state == 2) e.sethp(0);
    h->equipItem_T(Item("unworn blade", 1, Weapon, 10, 5, 10, 1, WarriorOnly));
    h->addCombatResource(2);
    BattleSession invalid(*h, e);
    require(invalid.finished() && !invalid.won() && !invalid.claimSettlement() &&
                !invalid.beginTurn() && !invalid.act({ActionType::Attack}),
            "Initially dead participants must not create a claimable encounter");
    require(h->getCombatResource() == 2,
            "Invalid encounter entry must not reset existing class resources");
    h->finishEncounterWear();
    require(h->getSaveEquipments()[0].getDurability() == 5,
            "Invalid encounter entry must not schedule equipment wear");
    h->sethp(800);
    e.sethp(100);
    require(invalid.finished() && !invalid.won() && !invalid.claimSettlement(),
            "Reviving participants must not reactivate an invalid session");
  }
}

void testCleanupAndSabotage() {
  auto h = hero(500); auto e = foe(20, true);
  { BattleSession b(*h, e); b.beginTurn(); }
  h->heal(100);
  require(h->gethp() == 600, "Abandoned action windows must clear suppression");
  auto saboteur = hero(); auto core = foe(60, true);
  giveAffix(core, Affix::SHIELDING);
  BattleSession sabotage(*saboteur, core,
                         {EncounterObjective::Defeat, 4, 0, true});
  require(!core.hasAffix(Affix::SHIELDING),
          "Encounter rules must apply campaign sabotage to the shield source");
  turn(sabotage, {ActionType::Guard});
  require(core.getIntent().damagePercent == IntentTuning::SabotagedBossHeavyStrikePercent,
          "Campaign sabotage must weaken the core heavy strike");
}

void testHealingAffixAndInterrupts() {
  // Healers mend while charging; a basic attack interrupts it.
  auto ignoring = hero(); auto healer = foe(10, true, 0);
  giveAffix(healer, Affix::HEALING);
  healer.removeAffix(Affix::POISONOUS);
  healer.resethp(1000); healer.sethp(500);
  BattleSession ignored(*ignoring, healer);
  turn(ignored, {ActionType::Guard});
  require(healer.gethp() == 500 + 1000 * IntentTuning::HealingAffixPercent / 100,
          "An uninterrupted charge must heal a healing enemy");
  auto breaker = hero(); auto interrupted = foe(10, true, 0);
  giveAffix(interrupted, Affix::HEALING);
  interrupted.removeAffix(Affix::POISONOUS);
  interrupted.resethp(1000); interrupted.sethp(500);
  BattleSession broken(*breaker, interrupted);
  turn(broken, {ActionType::Attack});
  require(broken.report().interrupts == 1 && interrupted.gethp() < 500,
          "Interrupting the charge must deny the heal");
}

// A skill the enemy dodges entirely refunds half its MP; a landed one does not,
// and the cooldown runs either way.
void testDodgedSkillRefund() {
  auto dodger = hero(); auto evasive = foe(1, false, 0);
  evasive.setdodgeRate(100);
  dodger->getSkill(1)->resetcurrentcooltime();
  BattleSession dodged(*dodger, evasive);
  require(dodged.beginTurn(), "Expected a live action window");
  const int cost = dodger->getSkill(1)->getActualCost(*dodger), mp = dodger->getmp();
  std::ostringstream shown;
  auto *previous = std::cout.rdbuf(shown.rdbuf());
  require(dodged.act({ActionType::Skill, 1}), "Expected the skill to be cast");
  std::cout.rdbuf(previous);
  require(dodger->getmp() == mp - cost + cost / 2 && dodger->getSkill(1)->getcurrentcooldown() > 0 &&
              shown.str().find("【落空返还】") != std::string::npos,
          "A fully dodged skill refunds half its MP and still cools down");
  auto striker = hero(); auto target = foe(1, false, 0);
  striker->getSkill(1)->resetcurrentcooltime();
  BattleSession landed(*striker, target);
  require(landed.beginTurn(), "Expected a live action window");
  const int landedMp = striker->getmp();
  require(landed.act({ActionType::Skill, 1}) &&
              striker->getmp() == landedMp - striker->getSkill(1)->getActualCost(*striker),
          "A landed skill refunds nothing");
}

// Poison alone is no sign of stalling: the advice needs a Defeat fight where
// the player guarded while nothing called for it.
void testPoisonAdviceFollowsBehavior() {
  const auto advice = [](const BattleSession &b) {
    std::ostringstream shown;
    auto *previous = std::cout.rdbuf(shown.rdbuf());
    Campaign::printBattleReport(b);
    std::cout.rdbuf(previous);
    return shown.str().find("格挡等待") != std::string::npos;
  };
  const auto waiting = [](EnemyIntentKind kind) {
    return kind == EnemyIntentKind::Attack || kind == EnemyIntentKind::Charge ||
           kind == EnemyIntentKind::Recover;
  };

  auto survivor = hero(); auto toxic = foe(1, false, 0);
  giveAffix(toxic, Affix::POISONOUS);
  BattleSession survive(*survivor, toxic, {EncounterObjective::Survive, 5});
  while (!survive.finished()) turn(survive, {ActionType::Guard});
  require(survive.won() && survive.report().poisonDamage > 0 && !advice(survive),
          "Waiting out a survival objective must not be called stalling");

  auto attacker = hero(); auto toxicBoss = foe(10, true, 0);
  giveAffix(toxicBoss, Affix::POISONOUS);
  BattleSession assault(*attacker, toxicBoss);
  for (int i = 0; i < 6; ++i) turn(assault, {ActionType::Attack});
  require(assault.report().poisonDamage > 0 && assault.report().waitingGuards == 0 &&
              !advice(assault),
          "Attacking every turn must not be called stalling");

  auto staller = hero(); auto toxicFoe = foe(1, false, 0);
  giveAffix(toxicFoe, Affix::POISONOUS);
  BattleSession stall(*staller, toxicFoe);
  for (int i = 0; i < 30 && stall.report().waitingGuards < 2; ++i)
    turn(stall, {waiting(toxicFoe.getIntent().kind) ? ActionType::Guard : ActionType::Attack});
  require(stall.report().waitingGuards == 2 && advice(stall),
          "Guarding while nothing threatens in a poisoned Defeat fight is stalling");
}

void testAssassinCannotDodgeUnavoidableDamage() {
  // Poison ticks and death blasts bypass the assassin's passive dodge.
  auto assassin = std::make_unique<Assassin>("dodger");
  assassin->loadStats(80, 0, 100, 5000, 5000, 50, 200, 200, 0, 3);
  assassin->setHighestFloor(100);
  auto poisoner = foe(1, false, 0);
  giveAffix(poisoner, Affix::POISONOUS);
  BattleSession poison(*assassin, poisoner);
  for (int i = 0; i < 60; ++i) turn(poison, {ActionType::Guard});
  require(poison.report().poisonDamage == 60 && poison.report().damageTaken == 60,
          "Assassin dodge must not avoid poison ticks");

  int blasts = 0;
  for (int i = 0; i < 40; ++i) {
    auto target = std::make_unique<Assassin>("blast");
    target->loadStats(80, 0, 100, 5000, 5000, 50, 200, 200, 0, 3);
    target->setHighestFloor(100);
    auto bomber = foe(1, false, 100); bomber.resethp(1);
    giveAffix(bomber, Affix::EXPLOSIVE);
    bomber.removeAffix(Affix::POISONOUS);
    BattleSession blast(*target, bomber);
    turn(blast, {ActionType::Attack});
    blasts += blast.report().damageTaken > 0 && blast.report().poisonDamage == 0;
  }
  require(blasts == 40, "Assassin dodge must not avoid death blasts, which are not poison");
}
} // namespace

int main() {
  std::ostringstream quiet;
  auto *previous = std::cout.rdbuf(quiet.rdbuf());
  try {
    Random::seed(42);
    testInvalidActionsAndResources();
    testChargeCounterAndRecovery();
    testInterruptTextsMatchRecovery();
    testSuppressionAndShatterCounters();
    testReadCounterAndReport();
    testObjectivesAndSettlement();
    testDeathTimeoutAndBoundaryRules();
    testInitiallyDeadEncounterCannotSettle();
    testCleanupAndSabotage();
    testAssassinCannotDodgeUnavoidableDamage();
    testHealingAffixAndInterrupts();
    testPoisonAdviceFollowsBehavior();
    testDodgedSkillRefund();
    std::cout.rdbuf(previous);
    std::cout << "combat objective tests passed\n";
  } catch (const std::exception &error) {
    std::cout.rdbuf(previous);
    std::cerr << "combat objective tests failed: " << error.what() << '\n';
    return 1;
  }
}
