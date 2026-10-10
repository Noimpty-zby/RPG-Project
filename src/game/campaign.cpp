#include "game/campaign.h"
#include "core/game_config.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr Campaign::Chapter chapters[] = {
  {"荒野前哨", "切断警钟，护送迷路斥候", "斥候"},
  {"幽林回廊", "打断树心仪式，救回采药师", "采药师"},
  {"沉没书库", "夺回禁书，掩护学者撤离", "学者"},
  {"赤砂矿井", "拆除引爆装置，救出矿工", "矿工"},
  {"寒霜关隘", "摧毁冰晶，运送御寒物资", "守关人"},
  {"熔炉要塞", "切断供能炉，救出锻造师", "锻造师"},
  {"镜影剧场", "破坏镜阵，保护失忆旅人", "旅人"},
  {"星蚀观测站", "打断观星仪式，抢救星图", "观测者"},
  {"王庭废墟", "拆除王座封印，护送继承人", "继承人"},
  {"天顶之门", "切断核心回路，带领同伴突围", "远征队"}
};
bool validFloor(int floor) { return floor >= 1 && floor <= 100; }
int localFloor(int floor) { return (floor - 1) % 10 + 1; }
int eventType(int floor) { return GameConfig::chapter(floor) % 3; }
// Side-goal slot (0..2) for a local floor, or -1. Chapter one keeps floors 3/6/8;
// later chapters rotate by two. Modulo nine never puts a side goal on a boss.
int objectiveSlot(int chapter, int local) {
  const int positions[] = {3, 6, 8};
  for (int slot = 0; slot < 3; ++slot)
    if (local == (positions[slot] - 1 + chapter * 2) % 9 + 1) return slot;
  return -1;
}
void loseHealthSafely(player &hero, int percent) {
  const int loss = std::max(1, static_cast<int>(static_cast<long long>(hero.gethp()) * percent / 100));
  hero.sethp(std::max(1, hero.gethp() - loss));
}
const char *objectiveName(EncounterObjective objective) {
  switch (objective) {
  case EncounterObjective::Survive: return "生存";
  case EncounterObjective::Disrupt: return "打断";
  case EncounterObjective::Escape: return "撤离";
  default: return "击败";
  }
}
const char *eventOutcome(int type, int choice) {
  if (choice == 0) return "事件尚未处理";
  if (type == 0) return choice == 1 ? "援助已兑现：本章商品九折，Boss攻击-10%" : "物资已取回，材料领取完成";
  if (type == 1) return choice == 1 ? "净化已生效：本章敌人移除剧毒" : "协助已完成，材料领取完成";
  return choice == 1 ? "情报已生效：本章敌人闪避-10个百分点，最低0" : "悬赏已领取：本章Boss攻击+10%";
}
bool isUsable(const player &hero, const Item &item) {
  return item.getLevelRequirement() <= hero.getlevel() && hero.canWear(item);
}
bool isUpgrade(const player &hero, const Item &item) {
  if (item.getType() == Potion || !isUsable(hero, item)) return false;
  for (const auto &equipped : hero.getSaveEquipments())
    if (equipped.getType() == item.getType())
      return item.getValue() > equipped.getValue();
  return true;
}
void giveItem(player &hero, const Item &item) {
  auto &run = hero.runState();
  if (isUpgrade(hero, item)) {
    run.usefulDrops = GameConfig::addCounter(run.usefulDrops, 1);
    run.gearDrought = 0;
  }
  hero.addItem(item);
  std::cout << "获得：" << item.getDisplayName() << "（数值 " << item.getValue() << "）\n";
}
}

namespace Campaign {
const Chapter &chapter(int floor) { return chapters[GameConfig::chapter(floor)]; }
JobReq jobOf(const player &hero) { return equipmentClass(hero.job()); }
void seedStream(const player &hero, int floor, unsigned stream) {
  const unsigned f = static_cast<unsigned>(GameConfig::floor(floor));
  unsigned value = hero.runState().seed ^ (f * 0x9e3779b9U) ^ (stream * 0x85ebca6bU);
  value ^= value >> 16; value *= 0x7feb352dU; value ^= value >> 15;
  std::srand(value);
}
void initialize(player &hero, unsigned seed, int challenge) {
  if (hero.getlevel() != 1 || hero.getcoin() != 0 || hero.getHighestFloor() != 1 ||
      !hero.getSaveBackpack().empty() || !hero.getSaveEquipments().empty() ||
      hero.runState().battles != 0 || hero.runState().legacyRules ||
      std::any_of(hero.runState().cleared.begin(), hero.runState().cleared.end(), [](int n) { return n != 0; }))
    throw std::invalid_argument("campaign initialization requires a fresh character");
  hero.runState() = RunState{};
  hero.runState().seed = seed == 0 ? 1 : seed;
  hero.runState().challenge = std::clamp(challenge, 0, 2);
  hero.gaincoin(120);
  seedStream(hero, 1, 1);
  giveItem(hero, generateTargetedItem(1, jobOf(hero), Weapon));
  hero.equipItem(static_cast<int>(hero.getSaveBackpack().size()));
  giveItem(hero, generateTargetedItem(1, jobOf(hero), Armor));
  hero.equipItem(static_cast<int>(hero.getSaveBackpack().size()));
  hero.sethp(hero.getmaxhp());
  Item supplies = generateTargetedItem(1, AnyJob, Potion);
  supplies.setQuantity(challenge == 1 ? 2 : 4);
  hero.addItem(supplies);
}
std::string routeDescription(int route) {
  if (route == 1) return "破坏线：完成本章第2、7层，Boss失去护盾并降低核心压力；订单需完成5层；路线遭遇以打断为目标";
  if (route == 2) return "救援线：订单提前至完成3层；路线遭遇以生存为目标";
  if (route == 3) return "补给线：首通第3、7层额外药水；Boss保留全部机制";
  return "尚未选择路线";
}
bool chooseRoute(player &hero, int floor, int route) {
  if (!validFloor(floor) || route < 1 || route > 3) return false;
  auto &chosen = hero.runState().routes[GameConfig::chapter(floor)];
  if (chosen != 0) return false;
  chosen = route;
  return true;
}
int objectivesCompleted(const player &hero, int floor) {
  const int start = GameConfig::chapter(floor) * 10 + 1;
  int count = 0;
  for (int i = start; i < start + 9; ++i) count += hero.runState().cleared[i] != 0;
  return count;
}
int eliteFloor(int chapter) {
  chapter = std::clamp(chapter, 0, 9);
  // Late in the chapter for a build-up before the boss, skipping side goals.
  for (int local : {7, 6, 8, 5, 9, 4})
    if (objectiveSlot(chapter, local) < 0) return chapter * 10 + local;
  return chapter * 10 + 4;
}
bool isEliteFloor(int floor) {
  return validFloor(floor) && eliteFloor(GameConfig::chapter(floor)) == floor;
}
EncounterRules rulesFor(const player &hero, int floor) {
  EncounterRules rules;
  if (!validFloor(floor)) return rules;
  const auto &run = hero.runState();
  const int c = GameConfig::chapter(floor), local = localFloor(floor);
  rules.elite = isEliteFloor(floor);
  // Challenges and elites both raise how often special intents appear.
  rules.difficulty = (run.challenge == 0 ? 0 : 1) + (rules.elite ? 1 : 0);
  rules.bossSabotaged = local == 10 && run.routes[c] == 1 &&
                       run.cleared[c * 10 + 2] && run.cleared[c * 10 + 7];
  const EncounterObjective goals[] = {EncounterObjective::Survive, EncounterObjective::Disrupt,
    run.routes[c] == 1 ? EncounterObjective::Disrupt :
      (run.routes[c] == 2 ? EncounterObjective::Survive : EncounterObjective::Escape)};
  const int slot = objectiveSlot(c, local);
  if (slot >= 0) rules.objective = goals[(slot + c) % 3];
  rules.targetTurns = 3 + c / 4;
  return rules;
}
enemy makeEnemy(const player &hero, int floor) {
  if (!validFloor(floor)) throw std::out_of_range("floor must be 1..100");
  seedStream(hero, floor, 10 + static_cast<unsigned>(hero.runState().attempts[floor]));
  const auto rules = rulesFor(hero, floor);
  auto monster = generateEnemy(floor, rules.elite);
  // These are objective encounters: a durable pursuer/ritual guardian can be
  // bypassed by the advertised goal, so damage racing is no longer automatic.
  // Percentages are relative to the base curve, so undo the ordinary scaling.
  if (rules.objective != EncounterObjective::Defeat) {
    const int hpPercent = rules.objective == EncounterObjective::Disrupt
        ? EnemyTuning::DisruptHpPercent : EnemyTuning::SurviveHpPercent;
    monster.resethp(monster.getmaxhp() * hpPercent / EnemyTuning::OrdinaryHpPercent);
    monster.resetattack(std::max(1, monster.getattack() * EnemyTuning::ObjectiveAttackPercent /
                                        EnemyTuning::OrdinaryAttackPercent));
  }
  const int event = hero.runState().events[GameConfig::chapter(floor)];
  const int type = eventType(floor);
  if (event == 1 && type == 1) monster.removeAffix(Affix::POISONOUS);
  if (event == 1 && type == 2) monster.setdodgeRate(std::max(0, monster.getdodgeRate() - 10));
  if (floor % 10 == 0) {
    if (event == 1 && type == 0) monster.resetattack(std::max(1, monster.getattack() * 90 / 100));
    if (event == 2 && type == 2) monster.resetattack(std::max(1, monster.getattack() * 110 / 100));
  }
  return monster;
}
bool eventPending(const player &hero, int floor) {
  return validFloor(floor) && localFloor(floor) >= 5 &&
    hero.runState().events[GameConfig::chapter(floor)] == 0;
}
std::string eventDescription(int floor) {
  const std::string opening = std::string("遇到") + chapter(floor).ally + "：[1] 支付 " +
    std::to_string(GameConfig::restCost(floor));
  switch (eventType(floor)) {
  case 0:
    return opening + " 金币援助（本章商品九折，Boss攻击降低10%）  [2] 冒险取回物资（损失当前HP的15%，不会致死；获得3材料，材料上限9999）";
  case 1:
    return opening + " 金币净化（本章所有敌人移除剧毒词缀，无商品折扣）  [2] 协助净化（消耗最大MP的25%，至少1MP；恢复基础HP的10%并获得1材料。MP不足时改为损失当前HP的5%，不会致死；不回血，仅获得1材料。材料上限9999）";
  default:
    return opening + " 金币购买情报（本章所有敌人闪避降低10个百分点，最低0；无商品折扣）  [2] 接受悬赏（立即获得 " +
      std::to_string(GameConfig::restCost(floor) * 2) + " 金币；本章Boss攻击提高10%）";
  }
}
bool resolveEvent(player &hero, int floor, int choice) {
  if (!eventPending(hero, floor) || (choice != 1 && choice != 2) || !hero.isalive()) return false;
  const int cost = GameConfig::restCost(floor);
  if (choice == 1 && hero.getcoin() < cost) return false;
  const int type = eventType(floor);
  if (choice == 1) hero.costcoin(cost);
  else if (type == 0) {
    loseHealthSafely(hero, 15);
    const int before = hero.runState().materials;
    hero.runState().materials = std::min(GameConfig::MaxMaterials, hero.runState().materials + 3);
    std::cout << "获得材料 " << hero.runState().materials - before << "（上限9999）。\n";
  } else if (type == 1) {
    const int manaCost = std::max(1, hero.getmaxmp() / 4);
    if (hero.getmp() >= manaCost) {
      hero.setmp(hero.getmp() - manaCost);
      hero.heal(std::max(1, hero.getRecoveryBaseHp() / 10));
      std::cout << "协助净化消耗 " << manaCost << " MP。\n";
    } else {
      loseHealthSafely(hero, 5);
      std::cout << "MP不足，改为搬运物资：损失当前HP的5%（不会致死），不回血。\n";
    }
    const int before = hero.runState().materials;
    hero.runState().materials = std::min(GameConfig::MaxMaterials, hero.runState().materials + 1);
    std::cout << "获得材料 " << hero.runState().materials - before << "（上限9999）。\n";
  } else {
    hero.gaincoin(cost * 2);
  }
  hero.runState().events[GameConfig::chapter(floor)] = choice;
  std::cout << eventOutcome(type, choice) << "。\n";
  return true;
}
int restsRemaining(const player &hero, int floor) {
  if (!validFloor(floor)) return 0;
  const int limit = hero.runState().challenge == 1 ? 1 : GameConfig::MaxRestsPerChapter;
  return std::max(0, limit - hero.runState().campRested[GameConfig::chapter(floor)]);
}
int restPrice(const player &hero, int floor) {
  // Each extra rest in the same chapter costs one more base price: 1x, 2x, 3x.
  return GameConfig::restCost(floor) * (1 + hero.runState().campRested[GameConfig::chapter(floor)]);
}
bool rest(player &hero, int floor) {
  if (!validFloor(floor) || !hero.isalive() || restsRemaining(hero, floor) == 0) return false;
  auto &run = hero.runState();
  const int cost = restPrice(hero, floor);
  if (hero.getcoin() < cost || (hero.gethp() == hero.getmaxhp() && hero.getmp() == hero.getmaxmp()))
    return false;
  hero.costcoin(cost);
  hero.heal(hero.getRecoveryBaseHp() * (run.challenge == 1 ? 25 : 40) / 100);
  hero.setmp(hero.getmp() + hero.getmaxmp() / 2);
  ++run.campRested[GameConfig::chapter(floor)];
  return true;
}
bool claimOrder(player &hero, int floor) {
  if (!validFloor(floor)) return false;
  auto &run = hero.runState();
  const int c = GameConfig::chapter(floor);
  if (run.routes[c] == 0 || run.ordersClaimed[c] ||
      objectivesCompleted(hero, floor) < (run.routes[c] == 2 ? 3 : 5)) return false;
  seedStream(hero, floor, 300 + static_cast<unsigned>(run.lootTarget));
  const ItemType type = run.lootTarget == 2 ? Armor : Weapon;
  giveItem(hero, generateTargetedItem(floor, jobOf(hero), type));
  run.ordersClaimed[c] = 1;
  return true;
}
std::vector<Item> shopStock(const player &hero, int floor) {
  if (!validFloor(floor)) return {};
  const int c = GameConfig::chapter(floor);
  seedStream(hero, c * 10 + 1, 500 + static_cast<unsigned>(hero.runState().shopRefreshes[c]));
  std::vector<Item> stock;
  // Stock is chapter-scoped: opening another menu/floor never rerolls it.
  const int stockFloor = c * 10 + 5;
  stock.push_back(generateTargetedItem(stockFloor, jobOf(hero), Weapon));
  stock.push_back(generateTargetedItem(stockFloor, jobOf(hero), Armor));
  stock.push_back(generateTargetedItem(stockFloor, AnyJob, Potion));
  stock.push_back(generateTargetedItem(stockFloor, AnyJob, Potion));
  stock.push_back(generateTargetedItem(stockFloor, jobOf(hero),
                       (hero.runState().seed + static_cast<unsigned>(c)) % 2 ? Armor : Weapon));
  return stock;
}
int purchasePrice(const player &hero, int floor, const Item &item) {
  if (!validFloor(floor)) return 0;
  long long price = std::max(0, item.getPrice());
  if (eventType(floor) == 0 && hero.runState().events[GameConfig::chapter(floor)] == 1)
    price = std::max(1LL, price * 90 / 100);
  if (hero.runState().challenge == 1) price = price * 125 / 100;
  return static_cast<int>(std::min(price, static_cast<long long>(std::numeric_limits<int>::max())));
}
bool purchase(player &hero, int floor, int slot) {
  if (!validFloor(floor) || slot < 1 || slot > 5) return false;
  auto &run = hero.runState();
  const int c = GameConfig::chapter(floor), bit = 1 << (slot - 1);
  if (run.shopPurchased[c] & bit) return false;
  const auto stock = shopStock(hero, floor);
  const auto &item = stock[slot - 1];
  const int price = purchasePrice(hero, floor, item);
  if (hero.getcoin() < price) return false;
  hero.costcoin(price);
  giveItem(hero, item);
  run.shopPurchased[c] |= bit;
  return true;
}
bool refreshShop(player &hero, int floor) {
  if (!validFloor(floor)) return false;
  auto &run = hero.runState();
  const int c = GameConfig::chapter(floor);
  const int cost = GameConfig::refreshCost(floor, run.shopRefreshes[c]);
  if (run.shopRefreshes[c] >= 20 || hero.getcoin() < cost) return false;
  hero.costcoin(cost);
  ++run.shopRefreshes[c];
  run.shopPurchased[c] = 0;
  return true;
}
bool settleEncounter(player &hero, enemy &monster, BattleSession &battle, int floor) {
  if (!validFloor(floor) || !battle.claimSettlement()) return false;
  auto &run = hero.runState();
  const auto &report = battle.report();
  run.battles = GameConfig::addCounter(run.battles, 1);
  run.turns = GameConfig::addCounter(run.turns, battle.turns());
  run.damageTaken = GameConfig::addCounter(run.damageTaken, report.damageTaken);
  run.attempts[floor] = GameConfig::addCounter(run.attempts[floor], 1);
  hero.finishEncounterWear();
  if (!battle.won()) {
    if (!hero.isalive()) run.deaths = GameConfig::addCounter(run.deaths, 1);
    return false;
  }
  const bool first = run.cleared[floor] == 0;
  const int percent = first ? 100 : GameConfig::repeatRewardPercent(run.attempts[floor] - 1);
  hero.gainexp(monster.getexp_reward() * percent / 100);
  hero.gaincoin(monster.getcoin_reward() * percent / 100);
  if (first) {
    run.gearDrought = GameConfig::addCounter(run.gearDrought, 1);
    hero.heal(std::max(1, hero.getRecoveryBaseHp() * (run.challenge == 1 ? 2 : 4) / 100));
  }
  seedStream(hero, floor, 900 + static_cast<unsigned>(run.attempts[floor]));
  auto dropped = generateLoot(hero, floor, first);
  if (dropped) giveItem(hero, *dropped);
  const int c = GameConfig::chapter(floor), local = localFloor(floor);
  if (first && (local == 3 || local == 7)) {
    run.materials = std::min(GameConfig::MaxMaterials, run.materials + 2);
    if (run.routes[c] == 3) giveItem(hero, generateTargetedItem(floor, AnyJob, Potion));
  }
  if (first && isEliteFloor(floor)) {
    run.materials = std::min(GameConfig::MaxMaterials, run.materials + 2);
    std::cout << "【精英奖励】高品质装备与 2 材料：\n";
    giveItem(hero, generateEliteReward(hero, floor));
    run.pity = 0; // The reward is always a wearable piece of the loot target.
  }
  if (first && local == 10) {
    run.materials = std::min(GameConfig::MaxMaterials, run.materials + 3);
    std::cout << "章节完成：" << chapter(floor).name << "；专精与改造可在营地重新准备。\n";
  }
  run.cleared[floor] = 1;
  run.longestGearDrought = std::max(run.longestGearDrought, run.gearDrought);
  return true;
}
bool revive(player &hero, bool useFree) {
  auto &run = hero.runState();
  if (run.challenge == 2 || hero.isalive()) return false;
  if (useFree) {
    if (hero.getfreerevives() <= 0) return false;
    hero.decreaserevives();
  } else {
    if (hero.getcoin() <= 0) return false;
    hero.costcoin(std::max(1, hero.getcoin() / 3));
  }
  hero.revive();
  hero.setmp(hero.getmaxmp());
  std::cout << "回到本层营地。路线与首通记录保留，可调整打法后重试。\n";
  return true;
}
void printBattleReport(const BattleSession &battle) {
  const auto &r = battle.report();
  std::cout << "战斗复盘：" << battle.turns() << "回合，承伤 " << r.damageTaken
    << "，其中剧毒 " << r.poisonDamage << "；抑疗损失 " << r.suppressedHealing
    << "；被识破 " << r.readPunishments << " 次；打断 " << r.interrupts
    << " 次；药水 " << r.potionsUsed << "。\n";
  if (r.readPunishments) std::cout << "调整建议：留意预测行动，改变已被识破的出招。\n";
  if (r.suppressedHealing) std::cout << "调整建议：优先在无重伤的回合恢复，危险时仍可应急治疗。\n";
  if (r.poisonDamage) std::cout << "调整建议：剧毒持续消耗，避免为了等冷却而无限拖延。\n";
}
void printJournal(const player &hero, int floor) {
  const auto &run = hero.runState();
  const int c = GameConfig::chapter(floor);
  std::cout << "第" << c + 1 << "章：" << chapter(floor).name << " — " << chapter(floor).mission
    << "\n" << routeDescription(run.routes[c]) << "\n章节进度 " << objectivesCompleted(hero, floor)
    << "/9；订单 " << (run.ordersClaimed[c] ? "已领取" : "未领取")
    << "；材料 " << run.materials << "；装备目标 " << (run.lootTarget == 1 ? "武器" : "防具")
    << "；保底进度 " << run.pity << "\n种子 " << run.seed << "；挑战 " << GameConfig::challengeName(run.challenge)
    << "；规则 " << (run.legacyRules ? "传统存档（可选择迁移）" : "章节规则")
    << "\n累计战斗 " << run.battles << "，回合 " << run.turns << "，阵亡 " << run.deaths
    << "，药水 " << run.potionsUsed << "，装备提升 " << run.usefulDrops
    << "，最长装备空窗 " << run.longestGearDrought << " 层。\n";
  std::cout << "本章遭遇目标：";
  for (int local = 1; local <= 9; ++local) {
    const int targetFloor = c * 10 + local;
    const auto rules = rulesFor(hero, targetFloor);
    if (rules.objective != EncounterObjective::Defeat)
      std::cout << " 第" << targetFloor << "层" << objectiveName(rules.objective);
    if (rules.elite) std::cout << " 第" << targetFloor << "层精英";
  }
  std::cout << "；第" << (c + 1) * 10 << "层击败Boss。\n"
    << eventOutcome(eventType(floor), run.events[c]) << "。\n";
  if (c > 0 && run.events[c - 1] == 1)
    std::cout << "来自" << chapter(floor - 10).ally << "的信：上一章的合作已经记入远征日志，效果限于当地。\n";
}
}
