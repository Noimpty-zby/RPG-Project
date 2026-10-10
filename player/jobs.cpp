#include "jobs.h"
#include "player.h"
#include "skills.h"
#include <cstdlib>
#include <iostream>
#include <memory>

using namespace std;

// ══════════════════════════════════════════════
//  战士 —— 坚甲重盾，高血量，稳定输出
//  初始：HP 200 / ATK 18 / MP 15 / 暴击 8%
//  每级：HP+20  ATK+4  MP+3
// ══════════════════════════════════════════════
Warrior::Warrior(string name) : player(name, 200, 18, 15, 1, 0, 0, 8) {
  learnSkill(std::make_unique<Warriorskill_1>());
  learnSkill(std::make_unique<Warriorskill_2>());
  learnSkill(std::make_unique<Warriorskill_3>());
  learnSkill(std::make_unique<Warriorskill_4>());
  learnSkill(std::make_unique<Warriorskill_5>());
}
void Warrior::levelup() {
  level++;
  attack += 4;
  max_hp += 20;
  hp = max_hp; // 升级回满血（类似大多数RPG）
  max_mp += 3;
  mp = max_mp;
  cout << getname() << " 升到了 " << level << " 级！HP/MP已回满！" << endl;
}
string Warrior::getjob() const { return "Warrior"; }

// ══════════════════════════════════════════════
//  法师 —— 法力充沛，技能爆发强，肉身脆弱
//  初始：HP 120 / ATK 12 / MP 40 / 暴击 12%
//  每级：HP+12  ATK+3  MP+8
// ══════════════════════════════════════════════
Mage::Mage(string name) : player(name, 120, 12, 40, 1, 0, 0, 12) {
  learnSkill(std::make_unique<Mageskill_1>());
  learnSkill(std::make_unique<Mageskill_2>());
  learnSkill(std::make_unique<Mageskill_3>());
  learnSkill(std::make_unique<Mageskill_4>());
  learnSkill(std::make_unique<Mageskill_5>());
}
void Mage::levelup() {
  level++;
  attack += 3;
  max_hp += 12;
  hp = max_hp;
  max_mp += 8;
  mp = max_mp;
  cout << getname() << " 升到了 " << level << " 级！HP/MP已回满！" << endl;
}
string Mage::getjob() const { return "Mage"; }

// ══════════════════════════════════════════════
//  刺客 —— 高爆发，高暴击，有闪避，脆
//  初始：HP 150 / ATK 15 / MP 25 / 暴击 25%
//  每级：HP+15  ATK+5  MP+4
//  被动：25% 闪避（原20%，略微下调平衡）
// ══════════════════════════════════════════════
Assassin::Assassin(string name) : player(name, 150, 15, 25, 1, 0, 0, 25) {
  learnSkill(std::make_unique<Assassinskill_1>());
  learnSkill(std::make_unique<Assassinskill_2>());
  learnSkill(std::make_unique<Assassinskill_3>());
  learnSkill(std::make_unique<Assassinskill_4>());
  learnSkill(std::make_unique<Assassinskill_5>());
}
void Assassin::levelup() {
  level++;
  attack += 5;
  max_hp += 15;
  hp = max_hp;
  max_mp += 4;
  mp = max_mp;
  cout << getname() << " 升到了 " << level << " 级！HP/MP已回满！" << endl;
}
string Assassin::getjob() const { return "Assassin"; }
void Assassin::takedamage(int damage) {
  int dodgeChance = rand() % 100;
  if (dodgeChance < dodgeRate) { // dodgeRate = 25（定义在 jobs.h）
    cout << "【被动·鬼步】完美闪避！" << endl;
    return;
  }
  player::takedamage(damage);
}