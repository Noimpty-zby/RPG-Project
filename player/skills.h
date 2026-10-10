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

// ══════════════════════════════════════════════
//  战士技能（5个）
//  主题：坚守与爆发，"越战越勇"
//  Lv.1  【猛击】      快速单体，主力输出，低CD
//  Lv.8  【战吼斩】    伤害+自身回血，攻防一体
//  Lv.18 【盾反】      不造成伤害，大幅削减敌方攻击（防御控制）
//  Lv.28 【血怒】      HP越低倍率越高，绝境爆发
//  Lv.40 【无双斩】    终极大招，极高伤害+长冷却
// ══════════════════════════════════════════════
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

// ══════════════════════════════════════════════
//  法师技能（5个）
//  主题：元素魔法体系，"MP即力量"
//  Lv.1  【火球术】    稳定火焰单体
//  Lv.8  【冰霜新星】  伤害+削减敌方攻击力（冻结减攻）
//  Lv.18 【毒云术】    不消耗MP反而回复MP（特殊机制），附带中毒伤害
//  Lv.28 【奥术爆发】  消耗当前一半MP，伤害与消耗量成正比（高风险高回报）
//  Lv.40 【流星陨落】  最高单体伤害，极长冷却
// ══════════════════════════════════════════════
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

// ══════════════════════════════════════════════
//  刺客技能（5个）
//  主题：暗影连击，"快、准、狠"
//  Lv.1  【穿刺】      低CD快速刺击
//  Lv.8  【影袭】      必定触发暴击（×2伤害）
//  Lv.18 【连环刺】    随机连击2~4次，单次伤害低，总伤害高
//  Lv.28 【吸血刃】    伤害+吸收造成伤害的40%为HP
//  Lv.40 【瞬杀】      无视护盾，直接造成敌方当前HP的50%伤害
// ══════════════════════════════════════════════
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
