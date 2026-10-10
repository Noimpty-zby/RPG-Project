#include "skills.h"
#include "enemy.h"
#include "player.h"
#include "../progression.h"
#include <algorithm>
#include <iostream>
#include <limits>

using namespace std;

// ══════════════════════════════════════════════════════════════
//  基类实现
// ══════════════════════════════════════════════════════════════
skill::skill(int skillid, int levelrequired, int costmp, int cooldown,
             int damage, int level, std::string skillname)
    : skillid(skillid), levelrequired(levelrequired), level(level), costmp(costmp),
      cooldown(cooldown), currentcooldown(0), damage(damage),
      basecostmp(costmp), basedamage(damage), skillname(skillname) {}

int skill::getskillid() const { return skillid; }
int skill::getlevelrequired() const { return levelrequired; }
int skill::getunlockfloor() const {
  return Progression::skillUnlockFloor(skillid);
}
int skill::getcostmp() const { return costmp; }
int skill::getcooldown() const { return cooldown; }
int skill::getcurrentcooldown() const { return currentcooldown; }
int skill::getdamage() const { return damage; }
int skill::getScaledDamage(const player &hero) const {
  return getdamage() * getskilllevel() + hero.getattack() * 3 / 4;
}
int skill::getskilllevel() const { return level; }
std::string skill::getskillname() const { return skillname; }

void skill::setskillid(int v) { skillid = v; }
void skill::setlevelrequired(int v) { levelrequired = v; }
void skill::setcostmp(int v) { costmp = v; }
void skill::setcooldown(int v) { cooldown = v; }
void skill::setdamage(int v) { damage = v; }
void skill::setskillname(std::string v) { skillname = v; }
void skill::setskilllevelup(int v) { level = v; }

// 升级费用：当前等级的平方；旧档高等级保留但不再继续升级。
int skill::getlevelUpCost() const {
  const long long rank = min(getskilllevel(), 3784);
  return static_cast<int>(min(150LL * rank * rank,
                              static_cast<long long>(numeric_limits<int>::max())));
}

void skill::reducecooldown() {
  if (currentcooldown > 0)
    currentcooldown--;
}
void skill::resetcooldown() { currentcooldown = getcooldown(); }
void skill::resetcurrentcooltime() { currentcooldown = 0; }

void skill::restoreProgress(int rank, int remainingCooldown) {
  level = max(1, rank);
  damage = basedamage + (level - 1) * 15;
  // 毒云不耗MP；奥术爆发使用动态MP消耗，不设置固定费用。
  costmp = basecostmp == 0 ? 0 : basecostmp + (level - 1) * 3;
  currentcooldown = clamp(remainingCooldown, 0, cooldown);
}

bool skill::canUse(player &hero) const {
  return hero.getlevel() >= levelrequired &&
         hero.getHighestFloor() >= getunlockfloor() && hero.getmp() >= costmp &&
         currentcooldown == 0;
}

bool skill::canUpgrade(int playerlevel, int highestFloor) const {
  return highestFloor >= getunlockfloor() &&
         getskilllevel() < Progression::skillRankCap(highestFloor) &&
         playerlevel >= max(getlevelrequired(), getskilllevel() + 1);
}

bool skill::levelUpSkill(int playerlevel, int costcoins, int highestFloor) {
  if (canUpgrade(playerlevel, highestFloor) && costcoins >= getlevelUpCost()) {
    restoreProgress(getskilllevel() + 1, 0);
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
         << "【技能升级！】" << getskillname() << " → Lv." << getskilllevel()
         << "\n"
         << "  当前伤害：" << getdamage() << "  MP消耗：" << getcostmp() << "\n"
         << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
    return true;
  } else if (highestFloor < getunlockfloor()) {
    cout << "尚未解锁该技能，需要到达第 " << getunlockfloor() << " 层！" << endl;
  } else if (getskilllevel() >= Progression::skillRankCap(highestFloor)) {
    cout << "技能已达到当前阶段等级上限（"
         << Progression::skillRankCap(highestFloor) << "级）！" << endl;
  } else if (playerlevel < max(getlevelrequired(), getskilllevel() + 1)) {
    cout << "角色等级不足，无法升级该技能！" << endl;
  } else {
    cout << "金币不足！需要 " << getlevelUpCost() << " 金币。" << endl;
  }
  return false;
}

void skill::learnSkill(int playerlevel) {
  if (playerlevel >= getlevelrequired())
    cout << "? 习得技能：【" << getskillname() << "】" << endl;
  else
    cout << "需要 " << getlevelrequired() << " 级才能学习【" << getskillname()
         << "】" << endl;
}

// ──────────────────────────────────────────────────────────────
//  内部辅助：护盾伤害穿透计算
// ──────────────────────────────────────────────────────────────
static int calcShieldedDamage(int rawDmg, const enemy &target) {
  if (target.hasAffix(Affix::SHIELDING)) {
    int shield = min(8 + target.getlevel() * 6, rawDmg / 2);
    cout << "  〖护盾〗吸收 " << shield << " 点伤害！";
    int dmg = max(0, rawDmg - shield);
    if (dmg > 0)
      cout << "穿透造成 " << dmg << " 点！" << endl;
    else
      cout << "攻击被完全吸收！" << endl;
    return dmg;
  }
  return rawDmg;
}

// ══════════════════════════════════════════════════════════════
//
//  ██╗    ██╗ █████╗ ██████╗ ██████╗ ██╗ ██████╗ ██████╗
//  ██║    ██║██╔══██╗██╔══██╗██╔══██╗██║██╔═══██╗██╔══██╗
//  ██║ █╗ ██║███████║██████╔╝██████╔╝██║██║   ██║██████╔╝
//  ██║███╗██║██╔══██║██╔══██╗██╔══██╗██║██║   ██║██╔══██╗
//  ╚███╔███╔╝██║  ██║██║  ██║██║  ██║██║╚██████╔╝██║  ██║
//   ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═╝
//
//  战士技能 × 5
//  核心机制：以伤换血、绝境爆发、防御控场
// ══════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────
//  Lv.1  【猛击】
//  直接高伤害，低冷却（2回合），战斗主力
//  skill(id, levelReq, costMP, cooldown, baseDmg, skillLv, name)
// ──────────────────────────────────────────────────────────────
Warriorskill_1::Warriorskill_1() : skill(1, 1, 6, 2, 30, 1, "猛击") {}
bool Warriorskill_1::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero);
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【猛击】" << hero.getname() << " 全力挥剑，造成 " << dmg
       << " 点伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.8  【战吼斩】
//  中等伤害，斩击后回复自身 15% 最大HP
//  攻防一体，适合血量告急时使用
// ──────────────────────────────────────────────────────────────
Warriorskill_2::Warriorskill_2() : skill(2, 8, 14, 3, 55, 1, "战吼斩") {}
bool Warriorskill_2::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero);
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【战吼斩】" << hero.getname() << " 怒吼冲锋，造成 " << dmg
       << " 点伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  // 特效：回复15%最大HP
  int healAmt = max(8, hero.getmaxhp() * 15 / 100);
  const int hpBefore = hero.gethp();
  hero.heal(healAmt);
  cout << "  〖战吼激励〗恢复了 " << hero.gethp() - hpBefore << " 点HP！"
       << "（当前：" << hero.gethp() << "/" << hero.getmaxhp() << "）" << endl;
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.18 【盾反】
//  盾击反震，将敌方攻击力削减 35%，持续2次敌方行动
//  适合对付高攻击力怪物，尤其是 STRONG 词缀敌人
// ──────────────────────────────────────────────────────────────
Warriorskill_3::Warriorskill_3() : skill(3, 18, 18, 4, 0, 1, "盾反") {}
bool Warriorskill_3::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  target.weakenAttack(35, 2);
  cout << "? 【盾反】" << hero.getname() << " 以盾格挡，震慑敌人！" << endl;
  cout << "  〖破招〗" << target.getname()
       << " 的攻击力降低35%，持续2次敌方行动！" << endl;
  // 顺带对敌人造成少量反震伤害（盾击）
  int shockDmg = max(5, getScaledDamage(hero) + hero.getlevel() * 3);
  cout << "  盾击造成 " << shockDmg << " 点反震伤害！" << endl;
  target.takedamage(shockDmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.28 【血怒】
//  HP越低，伤害倍率越高（绝境反杀核心技）
//  HP > 60%：1.0× 基础伤害
//  HP 30~60%：1.8× 基础伤害
//  HP < 30%： 3.0× 基础伤害（触发血怒！）
// ──────────────────────────────────────────────────────────────
Warriorskill_4::Warriorskill_4() : skill(4, 28, 22, 4, 80, 1, "血怒") {}
bool Warriorskill_4::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int hpPercent = hero.gethp() * 100 / max(1, hero.getmaxhp());
  double multiplier;
  if (hpPercent > 60) {
    multiplier = 1.0;
    cout << "? 【血怒】战意燃烧！";
  } else if (hpPercent >= 30) {
    multiplier = 1.8;
    cout << "? 【血怒·激发】怒火中烧！";
  } else {
    multiplier = 3.0;
    cout << "? 【血怒·狂暴】濒死之际，战意冲天！";
  }
  int raw = static_cast<int>(getScaledDamage(hero) * multiplier);
  int dmg = calcShieldedDamage(raw, target);
  cout << "造成 " << dmg << " 点伤害！（倍率 ×" << multiplier << "）" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.40 【无双斩】
//  极高基础伤害，长冷却（6回合）
//  额外：斩击后恢复技能等级×10点HP
// ──────────────────────────────────────────────────────────────
Warriorskill_5::Warriorskill_5() : skill(5, 40, 35, 6, 180, 1, "无双斩") {}
bool Warriorskill_5::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero);
  int dmg = calcShieldedDamage(raw, target);
  cout << "?? 【无双斩】天地为之一震！对 " << target.getname() << " 造成 "
       << dmg << " 点毁灭性伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  const int hpBefore = hero.gethp();
  hero.heal(10 * getskilllevel());
  cout << "  〖无双之势〗战意更盛，恢复 " << hero.gethp() - hpBefore
       << " 点HP！" << endl;
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ══════════════════════════════════════════════════════════════
//
//  ███╗   ███╗ █████╗  ██████╗ ███████╗
//  ████╗ ████║██╔══██╗██╔════╝ ██╔════╝
//  ██╔████╔██║███████║██║  ███╗█████╗
//  ██║╚██╔╝██║██╔══██║██║   ██║██╔══╝
//  ██║ ╚═╝ ██║██║  ██║╚██████╔╝███████╗
//  ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝
//
//  法师技能 × 5
//  核心机制：元素体系、MP管理、高爆发
// ══════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────
//  Lv.1  【火球术】
//  稳定火焰单体，低冷却，法师主力输出技
// ──────────────────────────────────────────────────────────────
Mageskill_1::Mageskill_1() : skill(1, 1, 10, 2, 40, 1, "火球术") {}
bool Mageskill_1::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero) * 135 / 100;
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【火球术】烈焰轰击 " << target.getname() << "，造成 " << dmg
       << " 点火焰伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.8  【冰霜新星】
//  伤害 + 冻结效果：减攻30%，对 STRONG 词缀减攻45%，持续2次敌方行动
// ──────────────────────────────────────────────────────────────
Mageskill_2::Mageskill_2() : skill(2, 8, 18, 3, 70, 1, "冰霜新星") {}
bool Mageskill_2::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero) * 115 / 100;
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【冰霜新星】冰晶爆裂！对 " << target.getname() << " 造成 " << dmg
       << " 点冰霜伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  int freeze = 30;
  if (target.hasAffix(Affix::STRONG)) {
    freeze = 45;
    cout << "  〖深度冰封〗强力词缀被压制！" << endl;
  }
  target.weakenAttack(freeze, 2);
  cout << "  〖冰封〗" << target.getname() << " 攻击力降低 " << freeze
       << "%，持续2次敌方行动！" << endl;
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.18 【毒云术】
//  特殊机制：不消耗MP，反而回复30点MP（补充法力）
//  同时对敌人造成毒素伤害（基于技能等级×20，无视护盾）
//  适合MP不足时使用，是法师的"续航"技能
// ──────────────────────────────────────────────────────────────
Mageskill_3::Mageskill_3() : skill(3, 18, 0, 3, 20, 1, "毒云术") {}
bool Mageskill_3::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  // 毒素伤害：无视护盾（直接 takedamage）
  int poisonDmg = getScaledDamage(hero) + hero.getlevel() * 2;
  cout << "? 【毒云术】漫天毒雾弥漫！对 " << target.getname() << " 造成 "
       << poisonDmg << " 点毒素伤害！（无视护盾）" << endl;
  target.takedamage(poisonDmg);
  // 特效：回复30MP（每级额外+5）
  int mpRestore = 30 + (getskilllevel() - 1) * 5;
  hero.setmp(min(hero.getmaxmp(), hero.getmp() + mpRestore));
  cout << "  〖毒素蔓延〗法力涌动，恢复了 " << mpRestore << " 点MP！"
       << "（当前：" << hero.getmp() << "/" << hero.getmaxmp() << "）" << endl;
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.28 【奥术爆发】
//  消耗当前MP的一半，伤害 = 消耗的MP × 3（高风险高回报）
//  MP越多，爆发越高；MP不足50时效果较弱
// ──────────────────────────────────────────────────────────────
Mageskill_4::Mageskill_4() : skill(4, 28, 0, 5, 0, 1, "奥术爆发") {}
bool Mageskill_4::canUse(player &hero) const {
  return skill::canUse(hero) && hero.getmp() >= 10;
}
bool Mageskill_4::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  // 消耗当前MP的一半
  int mpCost = max(10, hero.getmp() / 2);
  if (hero.getmp() < mpCost) {
    cout << "MP不足，无法释放【奥术爆发】！" << endl;
    return false;
  }
  // 每级增加1.5倍MP转伤倍率，最高9倍，旧档高等级也不继续放大。
  const double multiplier = min(9.0, 3.0 + (getskilllevel() - 1) * 1.5);
  int raw = static_cast<int>(mpCost * multiplier);
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【奥术爆发】消耗 " << mpCost << " 点MP，"
       << "释放出 " << dmg << " 点奥术伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  hero.setmp(hero.getmp() - mpCost);
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.40 【流星陨落】
//  法师终极技：极高单体伤害，6回合冷却
//  额外：若敌方HP低于30%，额外造成30%当前HP的伤害（补刀判定）
// ──────────────────────────────────────────────────────────────
Mageskill_5::Mageskill_5() : skill(5, 40, 50, 6, 200, 1, "流星陨落") {}
bool Mageskill_5::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero) * 150 / 100;
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【流星陨落】苍穹裂开，巨石天降！对 " << target.getname()
       << " 造成 " << dmg << " 点天体伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  // 补刀判定：敌方HP < 30%时追加伤害
  if (target.isalive()) {
    int hpPercent = target.gethp() * 100 / max(1, target.getmaxhp());
    if (hpPercent < 30) {
      int execDmg = target.gethp() * 30 / 100;
      cout << "  〖天降制裁〗敌人奄奄一息！追加 " << execDmg
           << " 点陨石碎片伤害！" << endl;
      target.takedamage(execDmg);
    }
  }
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ══════════════════════════════════════════════════════════════
//
//  █████╗ ███████╗███████╗ █████╗ ███████╗███████╗██╗███╗   ██╗
//  ██╔══██╗██╔════╝██╔════╝██╔══██╗██╔════╝██╔════╝██║████╗  ██║
//  ███████║███████╗███████╗███████║███████╗███████╗██║██╔██╗ ██║
//  ██╔══██║╚════██║╚════██║██╔══██║╚════██║╚════██║██║██║╚██╗██║
//  ██║  ██║███████║███████║██║  ██║███████║███████║██║██║ ╚████║
//  ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚═╝╚═╝  ╚═══╝
//
//  刺客技能 × 5
//  核心机制：暗影连击、必暴、吸血、瞬杀
// ══════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────
//  Lv.1  【穿刺】
//  低CD（2回合），快速刺击，刺客主力输出
// ──────────────────────────────────────────────────────────────
Assassinskill_1::Assassinskill_1() : skill(1, 1, 8, 2, 35, 1, "穿刺") {}
bool Assassinskill_1::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero);
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【穿刺】暗影一刺！对 " << target.getname() << " 造成 " << dmg
       << " 点伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.8  【影袭】
//  必定暴击（×2倍伤害），绕过护盾的50%
//  代表刺客从暗影中突然发动的奇袭
// ──────────────────────────────────────────────────────────────
Assassinskill_2::Assassinskill_2() : skill(2, 8, 16, 3, 60, 1, "影袭") {}
bool Assassinskill_2::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero);
  // 必定暴击 ×2，但护盾只抵挡50%（暗影穿透）
  int critDmg = raw * 2;
  int dmg;
  if (target.hasAffix(Affix::SHIELDING)) {
    int shield = min((8 + target.getlevel() * 6) / 2, critDmg / 2);
    dmg = max(0, critDmg - shield);
    cout << "  〖暗影穿透〗护盾效果减半！（吸收 " << shield << " 点）" << endl;
  } else {
    dmg = critDmg;
  }
  cout << "? 【影袭·必定暴击 ×2】从暗影突刺！对 " << target.getname()
       << " 造成 " << dmg << " 点暴击伤害！" << endl;
  if (dmg > 0)
    target.takedamage(dmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.18 【连环刺】
//  随机连击 2~4 次，每次造成基础伤害×0.6
//  护盾只抵挡第一击，后续穿透（打破护盾再连击）
//  总伤害：1.2× ~ 2.4× 基础，有随机爽感
// ──────────────────────────────────────────────────────────────
Assassinskill_3::Assassinskill_3() : skill(3, 18, 24, 4, 45, 1, "连环刺") {}
bool Assassinskill_3::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int hits = 2 + rand() % 3; // 2~4 次
  cout << "?? 【连环刺】疾风连刺 " << hits << " 次！" << endl;
  int totalDmg = 0;
  int singleBase = getScaledDamage(hero) * 6 / 10;
  for (int i = 1; i <= hits; i++) {
    int dmg;
    if (i == 1) {
      dmg = calcShieldedDamage(singleBase, target); // 只有第一击受护盾影响
    } else {
      dmg = singleBase; // 后续击穿透护盾
    }
    cout << "  第 " << i << " 击：" << dmg << " 点" << endl;
    if (dmg > 0) {
      target.takedamage(dmg);
      totalDmg += dmg;
    }
    if (!target.isalive()) {
      cout << "  目标已被击倒！" << endl;
      break;
    }
  }
  cout << "  总计造成 " << totalDmg << " 点伤害！" << endl;
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.28 【吸血刃】
//  造成伤害 + 吸收造成伤害的 40% 为HP（技能等级提高吸血比例）
//  Lv.1: 40%  Lv.5: 60%（+5%/级）
// ──────────────────────────────────────────────────────────────
Assassinskill_4::Assassinskill_4() : skill(4, 28, 28, 4, 90, 1, "吸血刃") {}
bool Assassinskill_4::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  int raw = getScaledDamage(hero);
  int dmg = calcShieldedDamage(raw, target);
  cout << "? 【吸血刃】血色之刃劈下！对 " << target.getname() << " 造成 "
       << dmg << " 点伤害！" << endl;
  if (dmg > 0) {
    const int targetHpBefore = target.gethp();
    target.takedamage(dmg);
    // 吸血：40% + (等级-1)×5%
    int drainPct = 40 + (getskilllevel() - 1) * 5;
    int actualDamage = max(0, targetHpBefore - target.gethp());
    int drain = actualDamage * drainPct / 100;
    const int hpBefore = hero.gethp();
    hero.heal(drain);
    cout << "  〖生命汲取〗吸收了 " << hero.gethp() - hpBefore << " 点HP！"
         << "（吸血率 " << drainPct << "%，当前HP：" << hero.gethp() << "/"
         << hero.getmaxhp() << "）" << endl;
  }
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  Lv.40 【瞬杀】
//  终极技：无视护盾，直接造成敌方当前HP的 50% 伤害
//  对Boss效果减半（造成 25% 当前HP），避免无脑秒Boss
//  冷却6回合，无法升级伤害（百分比固定），但技能等级提升附加伤害
// ──────────────────────────────────────────────────────────────
Assassinskill_5::Assassinskill_5() : skill(5, 40, 40, 6, 50, 1, "瞬杀") {}
bool Assassinskill_5::useSkill(player &hero, enemy &target) {
  if (!canUse(hero))
    return false;
  bool isBoss = target.isBoss();
  int pct = isBoss ? 25 : 50;
  int execDmg = target.gethp() * pct / 100;
  // 附加固定伤害（随技能等级提升）
  int bonusDmg = getdamage() * getskilllevel();
  int totalDmg = execDmg + bonusDmg;
  cout << "? 【瞬杀】暗影将你笼罩……致命一击！" << endl;
  cout << "  无视护盾，对 " << target.getname() << " 造成当前HP的 " << pct
       << "% + " << bonusDmg << " 点附加 = 共 " << totalDmg << " 点伤害！"
       << endl;
  if (isBoss)
    cout << "  （Boss状态：瞬杀效果减半）" << endl;
  target.takedamage(totalDmg);
  hero.setmp(hero.getmp() - getcostmp());
  resetcooldown();
  return true;
}
