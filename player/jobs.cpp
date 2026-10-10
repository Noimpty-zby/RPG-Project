#include "jobs.h"
#include "skills.h"
#include "game_config.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>

Warrior::Warrior(std::string name) : player(name, 200, 18, 24, 1, 0, 0, 8) {
  learnSkill(std::make_unique<Warriorskill_1>());
  learnSkill(std::make_unique<Warriorskill_2>());
  learnSkill(std::make_unique<Warriorskill_3>());
  learnSkill(std::make_unique<Warriorskill_4>());
  learnSkill(std::make_unique<Warriorskill_5>());
}
void Warrior::levelup() {
  if (level >= 100) return;
  ++level;
  attack = GameConfig::addCounter(attack, 3);
  max_hp = GameConfig::addCounter(max_hp, 20);
  max_mp = GameConfig::addCounter(max_mp, 3);
  sethp(hp + std::max(1, getRecoveryBaseHp() * 12 / 100));
  setmp(mp + std::max(1, max_mp / 5));
  std::cout << getname() << " 升到 " << level << " 级，恢复基础生命12%及最大魔力20%。" << std::endl;
}
std::string Warrior::getjob() const { return "Warrior"; }
Mage::Mage(std::string name) : player(name, 120, 12, 40, 1, 0, 0, 12) {
  learnSkill(std::make_unique<Mageskill_1>());
  learnSkill(std::make_unique<Mageskill_2>());
  learnSkill(std::make_unique<Mageskill_3>());
  learnSkill(std::make_unique<Mageskill_4>());
  learnSkill(std::make_unique<Mageskill_5>());
}
void Mage::levelup() {
  if (level >= 100) return;
  ++level;
  attack = GameConfig::addCounter(attack, 2);
  max_hp = GameConfig::addCounter(max_hp, 12);
  max_mp = GameConfig::addCounter(max_mp, 8);
  sethp(hp + std::max(1, getRecoveryBaseHp() * 12 / 100));
  setmp(mp + std::max(1, max_mp / 5));
  std::cout << getname() << " 升到 " << level << " 级，恢复基础生命12%及最大魔力20%。" << std::endl;
}
std::string Mage::getjob() const { return "Mage"; }
Assassin::Assassin(std::string name) : player(name, 150, 15, 25, 1, 0, 0, 20) {
  learnSkill(std::make_unique<Assassinskill_1>());
  learnSkill(std::make_unique<Assassinskill_2>());
  learnSkill(std::make_unique<Assassinskill_3>());
  learnSkill(std::make_unique<Assassinskill_4>());
  learnSkill(std::make_unique<Assassinskill_5>());
}
void Assassin::levelup() {
  if (level >= 100) return;
  ++level;
  attack = GameConfig::addCounter(attack, 3);
  max_hp = GameConfig::addCounter(max_hp, 15);
  max_mp = GameConfig::addCounter(max_mp, 4);
  sethp(hp + std::max(1, getRecoveryBaseHp() * 12 / 100));
  setmp(mp + std::max(1, max_mp / 5));
  std::cout << getname() << " 升到 " << level << " 级，恢复基础生命12%及最大魔力20%。" << std::endl;
}
std::string Assassin::getjob() const { return "Assassin"; }
void Assassin::takedamage(int damage) {
  if (damage <= 0) return;
  if (std::rand() % 100 < dodgeRate) {
    std::cout << "【鬼步】闪避！" << std::endl;
    return;
  }
  player::takedamage(damage);
}
