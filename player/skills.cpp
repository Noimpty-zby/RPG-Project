#include "skills.h"
#include "player.h"
#include "progression.h"
#include "game_config.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>

using namespace std;
namespace {
int percent(int value, int pct) {
  return static_cast<int>(clamp(static_cast<long long>(value) * pct / 100,
                               0LL, static_cast<long long>(GameConfig::MaxCounter)));
}
int shielded(int raw, const enemy &target) {
  if (!target.hasAffix(Affix::SHIELDING)) return raw;
  const int absorbed = min(8 + target.getlevel() * 6, raw / 2);
  return max(0, raw - absorbed);
}
int controlDuration(const player &hero, int turns) {
  return turns + min(1, hero.equipmentEffect(static_cast<int>(ItemEffect::ControlFocus)));
}
bool beginCast(skill &ability, player &hero, enemy &target) {
  if (!target.isalive() || !ability.canUse(hero)) return false;
  hero.setmp(hero.getmp() - ability.getActualCost(hero));
  ability.resetcooldown();
  return true;
}
int directDamage(const skill &ability, const player &hero) {
  const int base = ability.getScaledDamage(hero);
  const int branch = ability.getSpecialization();
  const int slot = ability.getskillid();
  const int resource = hero.getCombatResource();
  if (hero.getjob() == "Warrior") {
    if (slot == 1) return percent(base, branch == 1 ? 80 + 35 * resource :
                                           (branch == 2 ? 140 : 100 + 15 * resource));
    if (slot == 2) return percent(base, branch == 1 ? 65 : (branch == 2 ? 130 : 100));
    if (slot == 3) return max(5, base + hero.getlevel() * 3);
    if (slot == 4) {
      const int hpPercent = static_cast<int>(static_cast<long long>(hero.gethp()) * 100 / max(1, hero.getmaxhp()));
      return percent(base, hpPercent > 60 ? 100 : (hpPercent >= 30 ? 180 : 300));
    }
  } else if (hero.getjob() == "Mage") {
    if (slot == 1) return percent(base, branch == 1 ? 120 + 30 * resource :
                                           (branch == 2 ? 95 : 135 + 15 * resource));
    if (slot == 2) return percent(base, branch == 1 ? 70 : (branch == 2 ? 155 : 115));
    if (slot == 3) return base + hero.getlevel() * 2;
    if (slot == 4) return percent(ability.getActualCost(hero), 300 + 75 * (ability.getEffectiveRank() - 1));
    if (slot == 5) return percent(base, 150);
  } else if (hero.getjob() == "Assassin") {
    if (slot == 1) return percent(base, branch == 1 ? 80 : (branch == 2 ? 125 : 100));
    if (slot == 2) return percent(base, branch == 1 ? 120 + 40 * resource :
                                           (branch == 2 ? 120 : 160 + 15 * resource));
  }
  return base;
}
void strike(const skill &ability, enemy &target, int damage, bool pierce = false) {
  const int amount = pierce ? damage : shielded(damage, target);
  cout << "【" << ability.getskillname() << "】预计命中伤害 " << amount << "。" << endl;
  target.takedamage(amount);
}
}

skill::skill(int skillid, int levelrequired, int costmp, int cooldown,
             int damage, int level, string skillname)
    : skillid(skillid), levelrequired(levelrequired), level(max(1, level)), costmp(costmp),
      cooldown(cooldown), currentcooldown(0), damage(damage), basecostmp(costmp),
      basedamage(damage), skillname(std::move(skillname)) {}
int skill::getskillid() const { return skillid; }
int skill::getlevelrequired() const { return levelrequired; }
int skill::getunlockfloor() const { return Progression::skillUnlockFloor(skillid); }
int skill::getcostmp() const { return costmp; }
int skill::getcooldown() const { return cooldown; }
int skill::getcurrentcooldown() const { return currentcooldown; }
int skill::getdamage() const { return damage; }
int skill::getEffectiveRank() const { return clamp(level, 1, 5); }
int skill::getScaledDamage(const player &hero) const {
  const long long value = static_cast<long long>(damage) + static_cast<long long>(hero.getattack()) * 3 / 4;
  return static_cast<int>(clamp(value, 0LL, static_cast<long long>(GameConfig::MaxCounter)));
}
int skill::getActualCost(const player &hero) const {
  if (hero.getjob() == "Mage" && skillid == 4) return max(10, hero.getmp() / 2);
  int cost = costmp;
  if (cost == 0) return 0;
  if (specialization == 1 && skillid == 2 && hero.getjob() == "Warrior") cost += 4;
  if (specialization == 2 && skillid == 1) cost += hero.getjob() == "Mage" ? -4 : 3;
  if (specialization == 1 && skillid == 1 && hero.getjob() == "Mage") cost += 3;
  // Round costs up: a one-MP skill must never become free from equipment.
  const int efficiency = 100 - 10 * hero.equipmentEffect(static_cast<int>(ItemEffect::SkillEfficiency));
  return max(1, static_cast<int>((static_cast<long long>(max(1, cost)) * efficiency + 99) / 100));
}
int skill::getskilllevel() const { return level; }
string skill::getskillname() const { return skillname; }
void skill::setskillid(int value) { skillid = value; if (skillid < 1 || skillid > 2) specialization = 0; }
void skill::setlevelrequired(int value) { levelrequired = max(1, value); }
void skill::setcostmp(int value) { costmp = clamp(value, 0, GameConfig::MaxCounter); }
void skill::setcooldown(int value) { cooldown = clamp(value, 0, 100); currentcooldown = min(currentcooldown, cooldown); }
void skill::setdamage(int value) { damage = clamp(value, 0, GameConfig::MaxCounter); }
void skill::setskillname(string value) { skillname = std::move(value); }
void skill::setskilllevelup(int value) { restoreProgress(value, currentcooldown); }
bool skill::setSpecialization(int value) {
  if (value < 0 || value > 2 || (value != 0 && (skillid < 1 || skillid > 2))) return false;
  specialization = value;
  return true;
}
int skill::getlevelUpCost() const {
  const long long rank = clamp(level, 1, 3784);
  return static_cast<int>(min(150LL * rank * rank, static_cast<long long>(numeric_limits<int>::max())));
}
void skill::reducecooldown() { if (currentcooldown > 0) --currentcooldown; }
void skill::resetcooldown() { currentcooldown = getcooldown(); }
void skill::resetcurrentcooltime() { currentcooldown = 0; }
void skill::restoreProgress(int rank, int remainingCooldown) {
  level = max(1, rank);
  const int effective = getEffectiveRank();
  damage = basedamage + (effective - 1) * 15;
  costmp = basecostmp == 0 ? 0 : basecostmp + (effective - 1) * 3;
  currentcooldown = clamp(remainingCooldown, 0, cooldown);
}
bool skill::canUse(player &hero) const {
  return hero.isalive() && hero.getlevel() >= levelrequired && hero.getHighestFloor() >= getunlockfloor() &&
         hero.getmp() >= getActualCost(hero) && currentcooldown == 0;
}
bool skill::canUpgrade(int playerLevel, int highestFloor) const {
  return highestFloor >= getunlockfloor() && level < Progression::skillRankCap(highestFloor) &&
         playerLevel >= max(levelrequired, level + 1);
}
bool skill::levelUpSkill(int playerLevel, int coins, int highestFloor) {
  if (!canUpgrade(playerLevel, highestFloor) || coins < getlevelUpCost()) return false;
  // Upgrading cannot bypass an active cooldown.
  restoreProgress(level + 1, currentcooldown);
  cout << "【技能进阶】" << skillname << " → " << level << "阶。" << endl;
  return true;
}
void skill::learnSkill(int playerLevel) {
  cout << skillname << (playerLevel >= levelrequired ? "：达到等级要求。" : "：等级不足。") << endl;
}
string skill::getSpecializationName(const player &hero) const {
  if (specialization == 0) return "基础";
  const char *names[3][2][2] = {
    {{"反击", "重斩"}, {"坚守", "振奋"}},
    {{"蓄焰", "火花"}, {"深冻", "碎冰"}},
    {{"开隙", "穿甲"}, {"伏击", "扰乱"}}
  };
  const int job = hero.getjob() == "Warrior" ? 0 : (hero.getjob() == "Mage" ? 1 : 2);
  return names[job][clamp(skillid, 1, 2) - 1][specialization - 1];
}
string skill::getPreview(const player &hero) const {
  ostringstream out;
  const int amount = directDamage(*this, hero);
  if (hero.getjob() == "Assassin" && skillid == 3)
    out << "2—4次连击，每击 " << percent(getScaledDamage(hero), 55) << "；后续击穿盾";
  else if (hero.getjob() == "Assassin" && skillid == 5)
    out << "无视护盾：当前敌HP的普通35%/Boss15% + " << getdamage();
  else out << "当前预计伤害 " << amount << "（未计敌方减免/闪避）";
  if (hero.getjob() == "Warrior") {
    if (skillid == 1 && specialization != 2) out << "；消耗战意，最多2层";
    if (skillid == 2) {
      if (specialization == 2) out << "；不回血，命中恢复6MP";
      else out << "；基础生命恢复 " << percent(hero.getRecoveryBaseHp(), specialization == 1 ? 25 : 15);
    }
    if (skillid == 3) out << "；敌攻击-35%，持续 " << controlDuration(hero, 2) << " 次行动";
    if (skillid == 4) out << "；生命比例>60%/30—60%/<30%对应1/1.8/3倍";
    if (skillid == 5) out << "；恢复 " << 10 * getEffectiveRank() << " HP";
  } else if (hero.getjob() == "Mage") {
    if (skillid == 1 && specialization != 2) out << "；消耗专注，最多2层";
    if (skillid == 1 && specialization == 2) out << "；保留并增加1层专注";
    if (skillid == 2 && specialization != 2)
      out << "；敌攻击-" << (specialization == 1 ? "40%" : "30%（强力敌45%）")
          << "，持续 " << controlDuration(hero, specialization == 1 ? 3 : 2) << " 次行动；命中回4MP";
    if (skillid == 2 && specialization == 2) out << "；放弃减攻，命中回6MP";
    if (skillid == 3) out << "；无视护盾，恢复 " << 20 + (getEffectiveRank() - 1) * 3 << " MP";
    if (skillid == 4) out << "；实际消耗当前MP一半，至少10MP";
    if (skillid == 5) out << "；命中后敌生命<30%时追加其当前生命30%伤害";
  } else {
    if (skillid == 1) out << (specialization == 2 ? "；无视护盾" :
                            (specialization == 1 ? "；命中增加2层破绽" : "；命中增加1层破绽"));
    if (skillid == 2 && specialization != 2) out << "；消耗破绽，最多2层";
    if (skillid == 2 && specialization == 2) out << "；命中回8MP并减攻15%一次行动";
    if (skillid == 4) out << "；吸取实际伤害的 " << 40 + (getEffectiveRank() - 1) * 3 << "%（最多基础生命10%）";
  }
  return out.str();
}

Warriorskill_1::Warriorskill_1() : skill(1, 1, 6, 2, 30, 1, "猛击") {}
bool Warriorskill_1::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int damage = directDamage(*this, hero);
  if (getSpecialization() != 2) hero.consumeCombatResource();
  strike(*this, target, damage);
  return true;
}
Warriorskill_2::Warriorskill_2() : skill(2, 2, 14, 3, 55, 1, "战吼斩") {}
bool Warriorskill_2::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int before = target.gethp();
  strike(*this, target, directDamage(*this, hero));
  if (getSpecialization() == 2) {
    if (target.gethp() < before) hero.setmp(hero.getmp() + 6);
  } else hero.heal(percent(hero.getRecoveryBaseHp(), getSpecialization() == 1 ? 25 : 15));
  return true;
}
Warriorskill_3::Warriorskill_3() : skill(3, 4, 18, 4, 0, 1, "盾反") {}
bool Warriorskill_3::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  target.weakenAttack(35, controlDuration(hero, 2));
  strike(*this, target, directDamage(*this, hero), true);
  return true;
}
Warriorskill_4::Warriorskill_4() : skill(4, 6, 22, 4, 80, 1, "血怒") {}
bool Warriorskill_4::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  strike(*this, target, directDamage(*this, hero));
  return true;
}
Warriorskill_5::Warriorskill_5() : skill(5, 9, 35, 6, 180, 1, "无双斩") {}
bool Warriorskill_5::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  strike(*this, target, directDamage(*this, hero));
  hero.heal(10 * getEffectiveRank());
  return true;
}
Mageskill_1::Mageskill_1() : skill(1, 1, 10, 2, 40, 1, "火球术") {}
bool Mageskill_1::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int damage = directDamage(*this, hero);
  if (getSpecialization() == 2) hero.addCombatResource(1);
  else hero.consumeCombatResource();
  strike(*this, target, damage);
  return true;
}
Mageskill_2::Mageskill_2() : skill(2, 2, 18, 3, 70, 1, "冰霜新星") {}
bool Mageskill_2::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int before = target.gethp();
  strike(*this, target, directDamage(*this, hero));
  if (getSpecialization() != 2) {
    const int freeze = getSpecialization() == 1 ? 40 : (target.hasAffix(Affix::STRONG) ? 45 : 30);
    target.weakenAttack(freeze, controlDuration(hero, getSpecialization() == 1 ? 3 : 2));
  }
  if (target.gethp() < before) hero.setmp(hero.getmp() + (getSpecialization() == 2 ? 6 : 4));
  return true;
}
Mageskill_3::Mageskill_3() : skill(3, 4, 0, 3, 20, 1, "毒云术") {}
bool Mageskill_3::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  strike(*this, target, directDamage(*this, hero), true);
  hero.setmp(hero.getmp() + 20 + (getEffectiveRank() - 1) * 3);
  return true;
}
Mageskill_4::Mageskill_4() : skill(4, 6, 0, 5, 0, 1, "奥术爆发") {}
bool Mageskill_4::canUse(player &hero) const { return skill::canUse(hero); }
bool Mageskill_4::useSkill(player &hero, enemy &target) {
  const int damage = directDamage(*this, hero); // Dynamic cost must be captured before payment.
  if (!beginCast(*this, hero, target)) return false;
  strike(*this, target, damage);
  return true;
}
Mageskill_5::Mageskill_5() : skill(5, 9, 50, 6, 200, 1, "流星陨落") {}
bool Mageskill_5::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int hpBefore = target.gethp();
  strike(*this, target, directDamage(*this, hero));
  if (target.isalive() && target.gethp() < hpBefore &&
      static_cast<long long>(target.gethp()) * 100 < static_cast<long long>(target.getmaxhp()) * 30)
    target.takedamage(percent(target.gethp(), 30));
  return true;
}
Assassinskill_1::Assassinskill_1() : skill(1, 1, 8, 2, 35, 1, "穿刺") {}
bool Assassinskill_1::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int before = target.gethp();
  strike(*this, target, directDamage(*this, hero), getSpecialization() == 2);
  if (getSpecialization() != 2 && target.gethp() < before)
    hero.addCombatResource(getSpecialization() == 1 ? 2 : 1);
  return true;
}
Assassinskill_2::Assassinskill_2() : skill(2, 2, 16, 3, 60, 1, "影袭") {}
bool Assassinskill_2::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int damage = directDamage(*this, hero);
  if (getSpecialization() != 2) hero.consumeCombatResource();
  const int before = target.gethp();
  // Shadow attacks retain partial piercing; no guaranteed critical multiplier is applied twice.
  const int absorbed = target.hasAffix(Affix::SHIELDING) ? min(4 + target.getlevel() * 3, damage / 4) : 0;
  strike(*this, target, damage - absorbed, true);
  if (getSpecialization() == 2 && target.gethp() < before) {
    hero.setmp(hero.getmp() + 8);
    target.weakenAttack(15, 1);
  }
  return true;
}
Assassinskill_3::Assassinskill_3() : skill(3, 4, 24, 4, 45, 1, "连环刺") {}
bool Assassinskill_3::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int hits = 2 + rand() % 3;
  const int each = percent(getScaledDamage(hero), 55);
  for (int hit = 0; hit < hits && target.isalive(); ++hit) strike(*this, target, each, hit != 0);
  return true;
}
Assassinskill_4::Assassinskill_4() : skill(4, 6, 28, 4, 90, 1, "吸血刃") {}
bool Assassinskill_4::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int before = target.gethp();
  strike(*this, target, directDamage(*this, hero));
  const int actual = max(0, before - target.gethp());
  const int drain = min(percent(actual, 40 + (getEffectiveRank() - 1) * 3), percent(hero.getRecoveryBaseHp(), 10));
  hero.heal(drain);
  return true;
}
Assassinskill_5::Assassinskill_5() : skill(5, 9, 40, 6, 50, 1, "瞬杀") {}
bool Assassinskill_5::useSkill(player &hero, enemy &target) {
  if (!beginCast(*this, hero, target)) return false;
  const int amount = percent(target.gethp(), target.isBoss() ? 15 : 35) + getdamage();
  strike(*this, target, amount, true);
  return true;
}
