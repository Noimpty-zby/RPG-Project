#include "world.h"
#include "progression.h"
#include <algorithm>
#include <cstdlib>
#include <string>

using namespace std;

// ══════════════════════════════════════════════════════════════
//  装备名称池（按职业 + 品质分级）
//  tier: 0=普通  1=精良  2=稀有  3=史诗
// ══════════════════════════════════════════════════════════════

// ── 战士专用武器（重型近战：剑/锤/斧/枪/弯刀）──
static const char *W_WPN[5][4] = {
    {"破旧铁剑", "精锻铁剑", "精钢巨剑", "神圣圣剑"},
    {"铁制战锤", "精钢战锤", "符文战锤", "暗金战锤"},
    {"缺口战斧", "精锻战斧", "暗铁战斧", "符文巨斧"},
    {"朽木长枪", "精钢战枪", "龙魂长枪", "神圣战枪"},
    {"生锈弯刀", "精钢弯刀", "血月弯刀", "神器弯刀"},
};
// ── 战士专用防具（重甲/盾/护腿/头盔）──
static const char *W_ARM[4][4] = {
    {"破旧链甲", "铁链甲", "精钢板甲", "秘银重甲"},
    {"木制圆盾", "铁皮盾牌", "精钢塔盾", "神盾"},
    {"皮革护腿", "铁制护腿", "精钢护腿", "龙鳞护腿"},
    {"旧铁头盔", "铁制头盔", "精钢战盔", "龙骨战盔"},
};
// ── 法师专用武器（法杖/魔杖/法典/念珠）──
static const char *M_WPN[4][4] = {
    {"朽木法杖", "水晶法杖", "龙纹法杖", "混沌魔杖"},
    {"学徒魔杖", "元素魔杖", "奥术魔杖", "虚空魔杖"},
    {"破旧法典", "元素法典", "古代法典", "禁忌法典"},
    {"玻璃念珠", "水晶念珠", "符文念珠", "星辰念珠"},
};
// ── 法师专用防具（法袍/法帽/腰带）──
static const char *M_ARM[3][4] = {
    {"粗布法袍", "学者长袍", "魔纹法衣", "星辰法衣"},
    {"布制帽子", "学者帽", "魔法高帽", "星辰法冠"},
    {"皮质腰带", "魔力腰带", "奥术腰带", "虚空腰带"},
};
// ── 刺客专用武器（短刀/弓/毒刃/飞镖）──
static const char *A_WPN[4][4] = {
    {"锈铁短刀", "寒铁匕首", "幽影刺刀", "影刃"},
    {"木弓", "角弓", "精灵长弓", "龙骨弓"},
    {"毒涂匕首", "蛇毒短刃", "幽毒利刃", "死亡之刃"},
    {"铁制飞镖", "毒针飞镖", "幻影飞镖", "神器飞镖"},
};
// ── 刺客专用防具（皮甲/披风/手套）──
static const char *A_ARM[3][4] = {
    {"破旧皮甲", "精皮甲", "影袭皮甲", "暗影战甲"},
    {"布制披风", "皮质披风", "暗影披风", "虚空披风"},
    {"皮手套", "利爪手套", "暗影手套", "死神手套"},
};
// ── 通用防具（护符/腰带）──
static const char *C_ARM[2][4] = {
    {"破旧护符", "精制护符", "圣光护符", "神器护符"},
    {"兽皮腰带", "皮腰带", "守护腰带", "圣战腰带"},
};
// ── 药水（通用）──
static const char *POTIONS[4] = {"小型治疗药水", "治疗药水", "优质治疗药水",
                                 "神圣圣水"};

// ──────────────────────────────────────────────────────────────
//  generateRandomItem：带职业限制 + 品质分级
// ──────────────────────────────────────────────────────────────
Item generateRandomItem(int floor) {
  // 阶段性开放品质；后半程仍保留掉落选择。
  int r = rand() % 100;
  int t = Progression::equipmentTier(floor, r);

  // ── 类型：30%武器 / 35%防具 / 35%药水 ──
  int roll = rand() % 100;
  bool isWeapon = (roll < 30);
  bool isArmor = (roll < 65 && !isWeapon);
  // else: 药水

  if (!isWeapon && !isArmor) {
    int potVal = 30 + (t + 1) * 15 + floor * 3;
    int price = (t + 1) * 20 + floor * 5;
    return Item(POTIONS[t], 1, Potion, potVal, 0, price, 1, AnyJob);
  }

  // ── 数值 ──
  // 武器攻击加成：参考同层怪物攻击的30~50%，让装备有明显提升
  int baseAtk = (t + 1) * 5 + floor * 2;
  int atkVal = baseAtk + rand() % max(1, baseAtk / 3);
  // 防具HP加成：比武器高，能扛1~2刀
  int baseHp = (t + 1) * 10 + floor * 4;
  int hpVal = baseHp + rand() % max(1, baseHp / 3);
  // 耐久/价格/等级需求
  int dur = (t + 1) * 10 + floor / 2 + rand() % 6;
  int price = (t + 1) * 35 + floor * 10;
  int levelReq = max(1, floor / 2 - 2 + t);

  // ── 职业归属 ──
  // 武器：战士35% / 法师30% / 刺客35%
  // 防具：战士35% / 法师25% / 刺客25% / 通用15%
  int jr = rand() % 100;
  JobReq job;
  if (isWeapon) {
    job = (jr < 35) ? WarriorOnly : (jr < 65) ? MageOnly : AssassinOnly;
  } else {
    job = (jr < 35)   ? WarriorOnly
          : (jr < 60) ? MageOnly
          : (jr < 85) ? AssassinOnly
                      : AnyJob;
  }

  if (isWeapon) {
    const char *name;
    if (job == WarriorOnly)
      name = W_WPN[rand() % 5][t];
    else if (job == MageOnly)
      name = M_WPN[rand() % 4][t];
    else
      name = A_WPN[rand() % 4][t];
    return Item(name, levelReq, Weapon, atkVal, dur, price, 1, job);
  } else {
    const char *name;
    if (job == WarriorOnly)
      name = W_ARM[rand() % 4][t];
    else if (job == MageOnly)
      name = M_ARM[rand() % 3][t];
    else if (job == AssassinOnly)
      name = A_ARM[rand() % 3][t];
    else
      name = C_ARM[rand() % 2][t];
    return Item(name, levelReq, Armor, hpVal, dur, price, 1, job);
  }
}

// ─────────────────────────────────────────────
//  怪物生成
// ─────────────────────────────────────────────
enemy generateEnemy(int floor) {
  // ── 成长曲线（参考 Slay the Spire）──
  // HP：前期慢，中后期加速，但不至于无解
  int hp = 35 + floor * 8 + (floor * floor) / 8;
  // 攻击：约等于对应职业HP的6~10%，有压力但不秒杀
  int attack = 4 + floor * 2 + floor / 4;
  // 经验：每层刷2~3只怪能稳定升级
  int exp_reward = 30 + floor * 25;
  // 金币：前期宽裕，中期可买装备，后期刷Boss发财
  int coin_reward = 8 + floor * 12;
  int level = floor;
  string name = "Monster of Floor " + to_string(floor);

  // ── Boss关卡（每10层）──
  if (floor % 10 == 0) {
    hp = hp * 3;                   // Boss 3倍血量
    attack = attack * 2;           // Boss 2倍攻击
    exp_reward = exp_reward * 4;   // Boss 4倍经验
    coin_reward = coin_reward * 5; // Boss 5倍金币
    level += 3;
    name = "★ BOSS of Floor " + to_string(floor) + " ★";
  }

  enemy monster(name, hp, attack, 0, exp_reward, coin_reward, level);
  monster.setAffix(floor);

  // 词缀加成更明显
  if (monster.hasAffix(Affix::STRONG))
    monster.resetattack(monster.getattack() * 120 / 100);
  if (monster.hasAffix(Affix::TOUGH))
    monster.resethp(monster.gethp() * 120 / 100);
  if (monster.hasAffix(Affix::QUICK))
    monster.setdogeRate(
        min(monster.getdogeRate() + monster.getlevel() / 2, 25));

  monster.configureEncounter(floor, floor % 10 == 0);

  return monster;
}
