#include "character.h"
#include <algorithm>
#include <iostream>

using namespace std;
Character::Character(string name, int hp, int attack, int mp)
    : name(std::move(name)), hp(hp), attack(attack), mp(mp), max_hp(hp), max_mp(mp) {}
string Character::getname() const { return name; }
int Character::gethp() const { return hp; }
int Character::getattack() const { return attack; }
int Character::getmp() const { return mp; }
void Character::takedamage(int damage) {
  damage = max(0, damage);
  hp = max(0, hp - damage);
  cout << getname() << " 受到 " << damage << " 伤害（HP " << hp << "/" << max_hp
       << "）。" << endl;
}
bool Character::isalive() const { return hp > 0; }
int Character::getmaxhp() const { return max_hp; }
int Character::getmaxmp() const { return max_mp; }
void Character::setmp(int value) { mp = clamp(value, 0, max_mp); }
void Character::sethp(int value) { hp = clamp(value, 0, max_hp); }
