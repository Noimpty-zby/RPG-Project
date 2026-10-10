#include "combat.h"
#include "jobs.h"
#include "skills.h"
#include "world.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

static std::unique_ptr<Warrior> hero() {
  auto h = std::make_unique<Warrior>("test");
  h->loadStats(80, 0, 100000, 4000, 5000, 100, 1000, 1000, 0, 3);
  h->setHighestFloor(100);
  return h;
}

static enemy foe(int floor) {
  enemy e("test enemy", 100000, 100, 0, 1, 1, floor);
  e.setdogeRate(0);
  e.configureEncounter(floor, false);
  return e;
}

int main() {
  std::ostringstream quiet;
  auto old = std::cout.rdbuf(quiet.rdbuf());
  std::srand(42);
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(40);
    h.setmp(0); h.getSkill(1)->resetcooldown();
    BattleSession b(h, e);
    assert(b.beginTurn());
    int mp = h.getmp(), cd = h.getSkill(1)->getcurrentcooldown();
    for (int i = 0; i < 10; ++i) {
      assert(!b.act({ActionType::Skill, 99}));
      assert(!b.act({ActionType::Potion, 99}));
      assert(b.beginTurn());
      assert(h.getmp() == mp && h.getSkill(1)->getcurrentcooldown() == cd);
      assert(b.turns() == 0 && e.getIntent().kind == EnemyIntentKind::Attack);
    }
    assert(b.act({ActionType::Attack}));
    assert(b.turns() == 1 && e.getIntent().kind == EnemyIntentKind::Wound);
    assert(b.beginTurn());
    assert(h.getmp() == mp + 8 && h.getSkill(1)->getcurrentcooldown() == cd - 1);
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(41);
    BattleSession b(h, e);
    b.beginTurn(); int hp = h.gethp();
    assert(b.act({ActionType::Guard}));
    assert(h.gethp() == hp - 50);
    assert(e.getIntent().kind == EnemyIntentKind::Shatter);
    b.beginTurn(); hp = h.gethp();
    assert(b.act({ActionType::Guard}));
    assert(h.gethp() == hp - 80);
    b.beginTurn(); hp = h.gethp();
    b.act({ActionType::Attack});
    assert(h.gethp() == hp); // recovery provides an explicit safe action window
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(40);
    BattleSession b(h, e);
    b.beginTurn(); b.act({ActionType::Attack});
    b.beginTurn(); int hp = h.gethp();
    assert(b.act({ActionType::Skill, 2}));
    assert(h.gethp() == hp + 375 - 100); // 15% heal, suppressed by 50%
    hp = h.gethp(); h.heal(100);
    assert(h.gethp() == hp + 100); // no leaked suppression outside the action
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(40);
    h.addItem(Item("potion", 1, Potion, 200, 0, 1, 2, AnyJob));
    BattleSession b(h, e);
    b.beginTurn(); b.act({ActionType::Attack});
    b.beginTurn(); int hp = h.gethp();
    assert(b.act({ActionType::Potion, 1}));
    assert(h.gethp() == hp); // +100 healing, -100 attack; item consumes turn
    assert(h.getSaveBackpack()[0].getQuantity() == 1 && b.turns() == 2);
    b.beginTurn(); hp = h.gethp();
    assert(b.act({ActionType::Potion, 1}));
    assert(h.gethp() == hp + 200);
    assert(h.getSaveBackpack().empty());
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(40);
    { BattleSession b(h, e); b.beginTurn(); b.act({ActionType::Attack}); b.beginTurn(); }
    int hp = h.gethp(); h.heal(100);
    assert(h.gethp() == hp + 100); // interrupted battle cleanup
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(42);
    BattleSession b(h, e);
    // A stable attack/guard cycle becomes predictable; changing it wins a window.
    for (int i = 0; i < 4; ++i) {
      b.beginTurn(); b.act({i % 2 ? ActionType::Guard : ActionType::Attack});
    }
    assert(e.getIntent().kind == EnemyIntentKind::ReadPattern);
    assert(e.getIntent().predictedAction == 0);
    b.beginTurn(); int hp = h.gethp();
    assert(b.act({ActionType::Skill, 1}));
    assert(h.gethp() == hp - 75);
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; int hp = h.gethp();
    Item armor("armor", 1, Armor, 100, 100, 1, 1, WarriorOnly);
    h.equipItem_T(armor);
    assert(h.gethp() == hp && h.getmaxhp() == 5100);
    h.unequipItem(1); h.equipItem(1);
    assert(h.gethp() == hp && h.getmaxhp() == 5100);
    Warrior low("low"); low.gaincoin(10000);
    int coins = low.getcoin(); low.levelUpSkill(1);
    assert(low.getcoin() == coins && low.getSkill(1)->getskilllevel() == 1);
    low.equipItem_T(Item("late weapon", 99, Weapon, 500, 100, 1, 1, WarriorOnly));
    assert(low.getSaveEquipments().empty() && low.getSaveBackpack().size() == 1);
  }
  {
    auto ownedHero = hero(); auto &h = *ownedHero; auto e = foe(40);
    h.sethp(h.getmaxhp());
    h.addItem(Item("potion", 1, Potion, 200, 0, 1, 1, AnyJob));
    BattleSession b(h, e); b.beginTurn();
    assert(!b.act({ActionType::Potion, 1}));
    assert(b.turns() == 0 && h.getSaveBackpack()[0].getQuantity() == 1);
  }
  std::cout.rdbuf(old);
  std::cout << "combat tests passed\n";
}
