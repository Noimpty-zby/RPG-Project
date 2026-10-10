#include "game/world.h"
#include "core/game_config.h"
#include "core/progression.h"
#include "player/player.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>

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

namespace {
Item makeItem(int floor, JobReq job, ItemType type, int tier, int maxLevel = 10000) {
  floor = GameConfig::floor(floor);
  if (type == Potion)
    return Item(POTIONS[tier], 1, Potion, 30 + (tier + 1) * 15 + floor * 3,
                0, (tier + 1) * 20 + floor * 5, 1, AnyJob, tier);
  if ((type != Weapon && type != Armor) || job < AnyJob || job > AssassinOnly ||
      (type == Weapon && job == AnyJob))
    throw invalid_argument("目标装备类型或职业无效");

  const char *name;
  if (type == Weapon) {
    if (job == WarriorOnly) name = W_WPN[rand() % 5][tier];
    else if (job == MageOnly) name = M_WPN[rand() % 4][tier];
    else name = A_WPN[rand() % 4][tier];
  } else {
    if (job == WarriorOnly) name = W_ARM[rand() % 4][tier];
    else if (job == MageOnly) name = M_ARM[rand() % 3][tier];
    else if (job == AssassinOnly) name = A_ARM[rand() % 3][tier];
    else name = C_ARM[rand() % 2][tier];
  }
  // 15% between quality bands, at most +/-3% inside one band.
  const int base = type == Weapon ? 8 + floor * 11 / 10 : 20 + floor * 3;
  const int center = base * (100 + tier * 15) / 100;
  const int variation = center * 3 / 100;
  const int value = center + rand() % (variation * 2 + 1) - variation;
  const int durability = 10 + floor / 2 + tier * 10 + rand() % 6;
  const int price = (tier + 1) * 35 + floor * 10;
  const int required = min(max(1, maxLevel), max(1, floor / 2 - 2 + tier));
  Item item(name, required, type, value, durability, price, 1, job, tier);
  if (tier >= 2) {
    vector<ItemEffect> choices{ItemEffect::SkillEfficiency};
    choices.push_back(type == Weapon ? ItemEffect::ManaOnAttack : ItemEffect::GuardRecovery);
    if (job == WarriorOnly || job == MageOnly)
      choices.push_back(ItemEffect::ControlFocus);
    item.setEffect(choices[rand() % choices.size()]);
  }
  return item;
}

bool usableTarget(const Item &item, const player &hero, ItemType target) {
  return item.getType() == target && item.getLevelRequirement() <= hero.getlevel() &&
         hero.canWear(item);
}

} // namespace

Item generateEliteReward(const player &hero, int floor) {
  floor = GameConfig::floor(floor);
  const ItemType type = hero.runState().lootTarget == Armor ? Armor : Weapon;
  // The best quality band available on this floor, wearable at the hero's level.
  return makeItem(floor, equipmentClass(hero.job()), type, Progression::equipmentTier(floor, 99),
                  hero.getlevel());
}

Item generateTargetedItem(int floor, JobReq job, ItemType type) {
  floor = GameConfig::floor(floor);
  const int tier = Progression::equipmentTier(floor, rand() % 100);
  return makeItem(floor, job, type, tier);
}

Item generateRandomItem(int floor) {
  floor = GameConfig::floor(floor);
  const int tier = Progression::equipmentTier(floor, rand() % 100);
  const int roll = rand() % 100;
  const ItemType type = roll < 30 ? Weapon : (roll < 65 ? Armor : Potion);
  const int jr = rand() % 100;
  const JobReq job = type == Weapon
      ? (jr < 35 ? WarriorOnly : (jr < 65 ? MageOnly : AssassinOnly))
      : (jr < 35 ? WarriorOnly : (jr < 60 ? MageOnly : (jr < 85 ? AssassinOnly : AnyJob)));
  return makeItem(floor, job, type, tier);
}

optional<Item> generateLoot(player &hero, int floor, bool firstClear) {
  if (floor < 1 || floor > GameConfig::MaxFloor) return nullopt;
  auto &run = hero.runState();
  const ItemType target = run.lootTarget == Armor ? Armor : Weapon;
  const int threshold = target == Weapon ? 6 : 8;
  optional<Item> result;
  if (firstClear)
    run.pity = min(threshold, run.pity + 1);
  if (firstClear && run.pity >= threshold) {
    const int tier = Progression::equipmentTier(floor, rand() % 100);
    result = makeItem(floor, equipmentClass(hero.job()), target, tier, hero.getlevel());
  } else {
    const int chance = firstClear ? 60
        : 60 * GameConfig::repeatRewardPercent(max(1, run.attempts[floor] - 1)) / 100;
    if (rand() % 100 < chance) {
      result = generateRandomItem(floor);
      // Most equipment belongs to the current class. Potions remain common.
      if (result->getType() != Potion && rand() % 100 < 75)
        result = makeItem(floor, equipmentClass(hero.job()), result->getType(), result->getTier(),
                          hero.getlevel());
    }
  }
  if (result && firstClear && usableTarget(*result, hero, target)) run.pity = 0;
  return result;
}

EnemyStats enemyStatsForFloor(int value) {
  const int floor = GameConfig::floor(value);
  const int baseHp = 40 + floor * 8, baseAttack = 4 + floor * 3 / 2;
  const int exp = 30 + floor * 25, coins = 8 + floor * 10;
  // Bosses keep their own curve. Ordinary enemies are tougher but hit softer,
  // so fights last long enough for their intents to matter.
  if (floor % 10 == 0)
    return {baseHp * 5 / 2, baseAttack * 9 / 5, exp * 4, coins * 5, floor + 3, true};
  return {baseHp * EnemyTuning::OrdinaryHpPercent / 100,
          max(1, baseAttack * EnemyTuning::OrdinaryAttackPercent / 100),
          exp, coins, floor, false};
}

enemy generateEnemy(int value, bool elite) {
  const int floor = GameConfig::floor(value);
  const auto stats = enemyStatsForFloor(floor);
  elite = elite && !stats.boss;
  const string name = stats.boss ? "★ " + enemyNameForFloor(floor) + " ★"
                                 : (elite ? "精英·" : "") + enemyNameForFloor(floor);
  enemy monster(name, stats.hp, stats.attack, 0, stats.expReward, stats.coinReward, stats.level);
  monster.setAffix(floor);
  if (elite) {
    monster.ensureAffixes(2);
    monster.resethp(monster.gethp() * EnemyTuning::EliteHpPercent / 100);
    monster.resetattack(monster.getattack() * EnemyTuning::EliteAttackPercent / 100);
    monster.resetexp_reward(monster.getexp_reward() * 2);
    monster.resetcoin_reward(monster.getcoin_reward() * 3 / 2);
  }
  if (stats.boss) monster.resetattack(monster.getattack() * bossAttackPercent(floor) / 100);
  if (monster.hasAffix(Affix::STRONG)) monster.resetattack(monster.getattack() * 120 / 100);
  if (monster.hasAffix(Affix::TOUGH)) monster.resethp(monster.gethp() * 120 / 100);
  if (monster.hasAffix(Affix::QUICK))
    monster.setdodgeRate(min(monster.getdodgeRate() + monster.getlevel() / 2, 25));
  monster.configureEncounter(floor, stats.boss);
  return monster;
}
