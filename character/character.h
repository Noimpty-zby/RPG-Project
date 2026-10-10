#pragma once
#include <string>
class Character {
private:
  std::string name;

protected:
  int hp;
  int attack;
  int mp;
  int max_hp;
  int max_mp;

public:
  Character(std::string name, int hp, int attack, int mp);
  std::string getname() const;
  int gethp() const;
  int getattack() const;
  int getmp() const;
  void setmp(int mp);
  void sethp(int hp);
  virtual void takedamage(int damage);
  bool isalive() const;
  int getmaxhp() const;
  int getmaxmp() const;
  virtual ~Character() = default;
};
