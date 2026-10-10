#pragma once
#include "player.h"
#include <string>

class Warrior : public player {
public:
  explicit Warrior(std::string name);
};
class Mage : public player {
public:
  explicit Mage(std::string name);
};
class Assassin : public player {
  int dodgeRate = 15;

public:
  explicit Assassin(std::string name);
  void takedamage(int damage) override;
};
