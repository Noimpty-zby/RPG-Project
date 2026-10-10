#include "player/jobs.h"
#include "core/random.h"
#include "player/skills.h"
#include <iostream>
#include <memory>

Warrior::Warrior(std::string name) : player(std::move(name), Job::Warrior) {
  learnSkill(std::make_unique<Warriorskill_1>());
  learnSkill(std::make_unique<Warriorskill_2>());
  learnSkill(std::make_unique<Warriorskill_3>());
  learnSkill(std::make_unique<Warriorskill_4>());
  learnSkill(std::make_unique<Warriorskill_5>());
}
Mage::Mage(std::string name) : player(std::move(name), Job::Mage) {
  learnSkill(std::make_unique<Mageskill_1>());
  learnSkill(std::make_unique<Mageskill_2>());
  learnSkill(std::make_unique<Mageskill_3>());
  learnSkill(std::make_unique<Mageskill_4>());
  learnSkill(std::make_unique<Mageskill_5>());
}
Assassin::Assassin(std::string name) : player(std::move(name), Job::Assassin) {
  learnSkill(std::make_unique<Assassinskill_1>());
  learnSkill(std::make_unique<Assassinskill_2>());
  learnSkill(std::make_unique<Assassinskill_3>());
  learnSkill(std::make_unique<Assassinskill_4>());
  learnSkill(std::make_unique<Assassinskill_5>());
}
void Assassin::takedamage(int damage) {
  if (damage <= 0) return;
  if (Random::below(100) < dodgeRate) {
    std::cout << "【鬼步】闪避！" << std::endl;
    return;
  }
  player::takedamage(damage);
}
