#include "character.h"
#include <algorithm>
#include <iostream>

using namespace std;
Character::Character(string name, int hp, int attack, int mp) {
  this->name = name;
  this->hp = hp;
  this->attack = attack;
  this->mp = mp;
  this->max_hp = hp;
  this->max_mp = mp;
}
string Character::getname() const { return name; }
int Character::gethp() const { return hp; }
int Character::getattack() const { return attack; }
int Character::getmp() const { return mp; }
void Character::takedamage(int damage) {
  damage = max(0, damage);
  hp -= damage;
  if (hp < 0)
    hp = 0;
  cout << getname() << "takes " << damage << " damage, remaining HP:" << hp
       << endl;
}
void Character::attacktarget(Character &target) {
  cout << getname() << "attacks " << target.getname() << "!" << endl;
  target.takedamage(attack);
}
bool Character::isalive() const { return hp > 0; }
int Character::getmaxhp() const { return max_hp; }
int Character::getmaxmp() const { return max_mp; }
void Character::setmp(int mp) {
  if (mp > max_mp) {
    this->mp = max_mp;
  } else if (mp < 0) {
    this->mp = 0;
  } else {
    this->mp = mp;
  }
}
void Character::sethp(int hp) {
  if (hp > max_hp) {
    this->hp = max_hp;
  } else if (hp < 0) {
    this->hp = 0;
  } else {
    this->hp = hp;
  }
}
