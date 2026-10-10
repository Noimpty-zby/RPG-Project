#pragma once
#include "player.h"
#include <string>

using namespace std;
class Warrior : public player {
public:
  Warrior(std::string name);
  virtual void levelup() override;
  virtual std::string getjob() const override;
};
class Mage : public player {
public:
  Mage(std::string name);
  virtual void levelup() override;
  virtual std::string getjob() const override;
};
class Assassin : public player {
  int dodgeRate = 15;

public:
  Assassin(std::string name);
  virtual void levelup() override;
  virtual std::string getjob() const override;
  virtual void takedamage(int damage) override;
};
