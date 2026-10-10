#pragma once
#include "enemy.h"
#include <string>

class player;

// ══════════════════════════════════════════════
//  skill 基类
// ══════════════════════════════════════════════
class skill {
  int skillid;
  int levelrequired;
  int level;
  int costmp;
  int cooldown;
  int currentcooldown;
  int damage;
  int basecostmp;
  int basedamage;
  int specialization = 0;
  std::string skillname;

public:
  skill(int skillid, int levelrequired, int costmp, int cooldown, int damage,
        int level, std::string skillname);

  // ── Getters ──
  int getskillid() const;
  int getlevelrequired() const;
  int getunlockfloor() const;
  int getcostmp() const;
  int getcooldown() const;
  int getdamage() const;
  int getScaledDamage(const player &hero) const;
  int getActualCost(const player &hero) const;
  int getEffectiveRank() const;
  int getSpecialization() const { return specialization; }
  bool setSpecialization(int value);
  std::string getSpecializationName(const player &hero) const;
  std::string getPreview(const player &hero) const;
  int getlevelUpCost() const;
  int getskilllevel() const;
  int getcurrentcooldown() const;
  std::string getskillname() const;

  // ── Setters ──
  void setskillid(int skillid);
  void setlevelrequired(int levelrequired);
  void setcostmp(int costmp);
  void setcooldown(int cooldown);
  void setdamage(int damage);
  void setskillname(std::string skillname);
  void setskilllevelup(int level);

  // ── 冷却控制 ──
  void reducecooldown();
  void resetcooldown();
  void resetcurrentcooltime();
  void restoreProgress(int level, int cooldown);

  // ── 逻辑 ──
  virtual bool canUse(player &hero) const;
  bool canUpgrade(int playerLevel, int highestFloor) const;
  bool levelUpSkill(int playerLevel, int costcoins, int highestFloor);
  void learnSkill(int playerlevel);

  virtual ~skill() = default;
  virtual bool useSkill(player &hero, enemy &target) = 0;
};

class Warriorskill_1 : public skill {
public:
  Warriorskill_1();
  bool useSkill(player &hero, enemy &target) override;
};
class Warriorskill_2 : public skill {
public:
  Warriorskill_2();
  bool useSkill(player &hero, enemy &target) override;
};
class Warriorskill_3 : public skill {
public:
  Warriorskill_3();
  bool useSkill(player &hero, enemy &target) override;
};
class Warriorskill_4 : public skill {
public:
  Warriorskill_4();
  bool useSkill(player &hero, enemy &target) override;
};
class Warriorskill_5 : public skill {
public:
  Warriorskill_5();
  bool useSkill(player &hero, enemy &target) override;
};

class Mageskill_1 : public skill {
public:
  Mageskill_1();
  bool useSkill(player &hero, enemy &target) override;
};
class Mageskill_2 : public skill {
public:
  Mageskill_2();
  bool useSkill(player &hero, enemy &target) override;
};
class Mageskill_3 : public skill {
public:
  Mageskill_3();
  bool useSkill(player &hero, enemy &target) override;
};
class Mageskill_4 : public skill {
public:
  Mageskill_4();
  bool canUse(player &hero) const override;
  bool useSkill(player &hero, enemy &target) override;
};
class Mageskill_5 : public skill {
public:
  Mageskill_5();
  bool useSkill(player &hero, enemy &target) override;
};

class Assassinskill_1 : public skill {
public:
  Assassinskill_1();
  bool useSkill(player &hero, enemy &target) override;
};
class Assassinskill_2 : public skill {
public:
  Assassinskill_2();
  bool useSkill(player &hero, enemy &target) override;
};
class Assassinskill_3 : public skill {
public:
  Assassinskill_3();
  bool useSkill(player &hero, enemy &target) override;
};
class Assassinskill_4 : public skill {
public:
  Assassinskill_4();
  bool useSkill(player &hero, enemy &target) override;
};
class Assassinskill_5 : public skill {
public:
  Assassinskill_5();
  bool useSkill(player &hero, enemy &target) override;
};
