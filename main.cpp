#include "campaign.h"
#include "game_config.h"
#include "help.h"
#include "jobs.h"
#include "profile.h"
#include "save.h"
#include "skills.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

namespace {
constexpr const char *ProfilePath = "profile.dat";

int readInt() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    long long value;
    if (input >> value) {
      input >> std::ws;
      if (input.eof() && value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max())
        return static_cast<int>(value);
    }
    std::cout << "请输入范围内的整数：";
  }
  std::cout << "\n输入结束，退出游戏。\n";
  std::exit(0);
}
// Unlocks newly earned achievements and persists the profile if any changed.
void checkAchievements(Profile &profile, const player &hero, const BattleOutcome *battle = nullptr) {
  const auto unlocked = profile.checkAchievements(hero, battle);
  for (const auto *achievement : unlocked)
    std::cout << "【成就解锁】" << achievement->title << "：" << achievement->description << '\n';
  if (!unlocked.empty()) profile.save();
}
void backpack(player &hero) {
  for (;;) {
    hero.printBackpack();
    std::cout << "[1] 使用药水 [2] 装备 [3] 卖出 [4] 锁定/解锁 [5] 批量拆解未锁定装备 [0] 返回\n";
    const int action = readInt();
    if (action == 0) return;
    if (action == 5) {
      std::cout << "拆解背包中所有未锁定装备（保留药水及已穿装备）？[1] 确认 [0] 返回\n";
      if (readInt() == 1) std::cout << "获得材料：" << hero.salvageUnlocked() << '\n';
      continue;
    }
    if (action < 1 || action > 4) continue;
    std::cout << "物品序号：";
    const int index = readInt();
    const auto &items = hero.getSaveBackpack();
    if (index < 1 || index > static_cast<int>(items.size())) { std::cout << "没有这个序号。\n"; continue; }
    if (action == 1 && !hero.useItem(index)) std::cout << "无法使用：只有药水可用，且生命未满时才会消耗。\n";
    if (action == 2) hero.equipItem(index);
    if (action == 3) {
      const Item &item = items[index - 1];
      if (item.isLocked()) { std::cout << "该物品已锁定，先解锁再卖出。\n"; continue; }
      int quantity = 1;
      if (item.getQuantity() > 1) {
        std::cout << "卖出数量（1-" << item.getQuantity() << "，单价 " << item.getPrice() / 3 << "，0取消）：";
        quantity = readInt();
        if (quantity == 0) continue;
        if (quantity < 1 || quantity > item.getQuantity()) { std::cout << "数量无效，未卖出。\n"; continue; }
      }
      const std::string name = item.getName();
      std::cout << "卖出 " << name << " ×" << quantity << "，获得 " << hero.sellItem(index, quantity) << " 金币。\n";
    }
    if (action == 4 && hero.toggleItemLock(index))
      std::cout << (items[index - 1].isLocked() ? "已锁定：不会被卖出或拆解。\n" : "已解锁。\n");
  }
}
void skills(player &hero) {
  for (;;) {
    hero.showSkills();
    std::cout << "[1] 升级 [2] 返回 [3] 选择专精（前两技能；10/30层开放）\n";
    const int action = readInt();
    if (action == 2) return;
    if (action != 1 && action != 3) continue;
    std::cout << "技能序号：";
    const int slot = readInt();
    if (action == 1) {
      if (!hero.levelUpSkill(slot)) std::cout << "未升级：检查金币、等级需求、楼层解锁和当前阶数上限。\n";
    } else {
      std::cout << "专精编号 [0] 基础 [1] 分支一 [2] 分支二；初选免费，重选费用随进度增加：";
      if (!hero.chooseSpecialization(slot, readInt())) std::cout << "未更改：检查解锁、金币和当前分支。\n";
    }
  }
}
void shop(player &hero, int floor) {
  const int c = GameConfig::chapter(floor);
  for (;;) {
    const auto stock = Campaign::shopStock(hero, floor);
    std::cout << "章节商店（商品跨楼层保留；金币 " << hero.getcoin() << "）\n";
    for (int i = 0; i < static_cast<int>(stock.size()); ++i) {
      if (hero.runState().shopPurchased[c] & (1 << i)) { std::cout << i + 1 << ". 已售出\n"; continue; }
      const Item &item = stock[i];
      std::cout << i + 1 << ". " << item.getName() << " 数值 " << item.getValue()
        << " 等级需求 " << item.getLevelRequirement() << " 价格 " << Campaign::purchasePrice(hero, floor, item)
        << " 品质 " << item.getTierName() << " 效果 " << item.getEffectName()
        << " " << item.getJobRequirementName() << '\n';
    }
    std::cout << "[1-5] 购买 [6] 刷新（" << GameConfig::refreshCost(floor, hero.runState().shopRefreshes[c])
      << "金币，章内最多20次） [0] 返回\n";
    const int action = readInt();
    if (action == 0) return;
    if (action == 6) {
      if (!Campaign::refreshShop(hero, floor)) std::cout << "未刷新：金币不足或达到章内上限。\n";
    } else if (!Campaign::purchase(hero, floor, action)) std::cout << "未购买：检查金币、序号和库存。\n";
  }
}
void modifyEquipment(player &hero) {
  hero.showEquippedItems();
  std::cout << "已装备序号（一个效果槽，改造消耗材料）：";
  const int index = readInt();
  const auto &equipped = hero.getSaveEquipments();
  if (index < 1 || index > static_cast<int>(equipped.size())) { std::cout << "没有这个序号。\n"; return; }
  std::cout << "本次改造消耗 " << 3 + equipped[index - 1].getTier() * 2 << " 材料；当前 "
    << hero.runState().materials << "。\n";
  for (int effect = 1; effect <= 4; ++effect)
    std::cout << "[" << effect << "] " << Item::effectDescription(static_cast<ItemEffect>(effect)) << '\n';
  std::cout << "选择效果：";
  if (!hero.modifyEquipment(index, readInt())) std::cout << "改造未执行：材料不足、效果相同或该部位不支持此效果。\n";
}
void camp(player &hero, int floor) {
  for (;;) {
    Campaign::printJournal(hero, floor);
    std::cout << "营地：[1] 休息（每章一次，" << GameConfig::restCost(floor)
      << "金币，恢复基础HP的" << (hero.runState().challenge == 1 ? 25 : 40)
      << "%与最大MP的50%） [2] 领取订单 [3] 定向装备目标 [4] 装备改造\n"
      << "[5] 商店 [6] 技能与专精 [7] 迁移传统存档 [0] 返回\n";
    const int action = readInt();
    if (action == 0) return;
    if (action == 1 && !Campaign::rest(hero, floor)) std::cout << "未休息：已使用、资源已满或金币不足。\n";
    if (action == 2 && !Campaign::claimOrder(hero, floor)) std::cout << "订单尚未达标或已领取。\n";
    if (action == 3) {
      std::cout << "选择 [1] 武器 [2] 防具；更换目标将清空保底计数：";
      const int target = readInt();
      if ((target == 1 || target == 2) && target != hero.runState().lootTarget) {
        hero.runState().lootTarget = target; hero.runState().pity = 0;
      }
    }
    if (action == 4) modifyEquipment(hero);
    if (action == 5) shop(hero, floor);
    if (action == 6) skills(hero);
    if (action == 7) {
      if (!hero.runState().legacyRules) { std::cout << "已经使用章节规则。\n"; continue; }
      std::cout << "迁移将把超额技能阶数转换为有限材料，并按当前进度规范角色成长。建议保留原存档备份。\n[1] 执行迁移 [0] 保留传统存档\n";
      if (readInt() == 1 && SaveSystem::migrateLegacy(hero)) std::cout << "迁移完成；保存后生效。\n";
    }
  }
}
void showObjective(const EncounterRules &rules) {
  if (rules.objective == EncounterObjective::Survive)
    std::cout << "目标：存活 " << rules.targetTurns << " 回合，或击败敌人。\n";
  else if (rules.objective == EncounterObjective::Disrupt)
    std::cout << "目标：普攻命中蓄力敌人完成两次打断，或击败敌人。\n";
  else if (rules.objective == EncounterObjective::Escape)
    std::cout << "目标：撑过准备时间后格挡撤离，或击败敌人。\n";
  else std::cout << "目标：击败敌人。\n";
  if (rules.bossSabotaged) std::cout << "供能设施已破坏，Boss护盾失效。\n";
}
// Read-only view during a fight; never advances the turn.
void inspect(const player &hero, const enemy &monster, const BattleSession &battle) {
  std::cout << "—— 敌人 ——\n" << monster.getname() << " | " << monster.getEncounterTitle()
    << " | HP " << monster.gethp() << '/' << monster.getmaxhp() << " | 攻击 " << monster.getattack()
    << " | 闪避 " << monster.getdodgeRate() << "%\n";
  if (monster.getAffixes().empty()) std::cout << "无词缀。\n";
  for (Affix affix : monster.getAffixes())
    std::cout << "[" << affixName(affix) << "] " << affixDescription(affix) << '\n';
  std::cout << "战术提示：" << monster.getTacticalHint() << '\n' << "—— 自身 ——\n";
  hero.printstats();
  hero.showEquippedItems();
  hero.showSkillsCompact();
  std::cout << "当前目标：" << battle.objectiveDescription() << '\n';
}
int potionCount(const player &hero) {
  int count = 0;
  for (const Item &item : hero.getSaveBackpack())
    if (item.getType() == Potion) count += item.getQuantity();
  return count;
}
bool fight(player &hero, int floor, Profile &profile) {
  auto monster = Campaign::makeEnemy(hero, floor);
  const auto rules = Campaign::rulesFor(hero, floor);
  std::cout << "遭遇：" << monster.getname() << "（第" << floor << "层） HP " << monster.gethp()
    << " 攻击 " << monster.getattack() << '\n';
  profile.recordEncounter(floor, monster);
  showObjective(rules);
  Campaign::seedStream(hero, floor, 1200 + static_cast<unsigned>(hero.runState().attempts[floor]));
  BattleSession battle(hero, monster, rules);
  monster.displayAffixes();
  std::cout << monster.getEncounterTitle() << "：" << monster.getTacticalHint() << '\n';
  while (battle.beginTurn()) {
    monster.displayIntent();
    std::cout << battle.objectiveDescription() << '\n';
    std::cout << "HP " << hero.gethp() << '/' << hero.getmaxhp() << " MP " << hero.getmp()
      << '/' << hero.getmaxmp() << " 敌HP " << monster.gethp() << " 药水 " << potionCount(hero) << '\n'
      << "[1] 普攻（命中可打断蓄力） [2] 技能 [3] 格挡 [4] 药水 [5] 查看敌我详情\n";
    const int action = readInt();
    BattleAction chosen{ActionType::Attack};
    if (action == 2) {
      hero.showSkillsCompact(); std::cout << "技能序号，0返回：";
      chosen = {ActionType::Skill, readInt()}; if (chosen.index == 0) continue;
    } else if (action == 3) chosen.type = ActionType::Guard;
    else if (action == 4) {
      hero.printBackpack(); std::cout << "药水序号，0返回：";
      chosen = {ActionType::Potion, readInt()}; if (chosen.index == 0) continue;
    } else if (action == 5) { inspect(hero, monster, battle); continue; }
    else if (action != 1) { std::cout << "无效行动。\n"; continue; }
    if (!battle.act(chosen)) std::cout << "行动未执行，没有推进回合。\n";
  }
  const auto &report = battle.report();
  const BattleOutcome outcome{floor, battle.won(), report.damageTaken, report.potionsUsed,
                              report.interrupts, hero.gethp(), hero.getmaxhp()};
  Campaign::printBattleReport(battle);
  const bool won = Campaign::settleEncounter(hero, monster, battle, floor);
  if (won) profile.recordVictory(floor);
  checkAchievements(profile, hero, &outcome);
  profile.save();
  std::cout << (won ? "目标完成！可前往下一层，或在营地调整。\n" : "本次挑战未完成。\n");
  return won;
}
void travel(player &hero, int &floor) {
  std::cout << "前往楼层（1-" << hero.getHighestFloor() << "，当前第" << floor
    << "层；回访已到达的楼层可以补练，0取消）：";
  const int target = readInt();
  if (target == 0) return;
  if (target < 1 || target > hero.getHighestFloor()) { std::cout << "只能前往已到达的楼层。\n"; return; }
  floor = target;
}
void archive(const Profile &profile) {
  for (;;) {
    std::cout << "远征档案：[1] 敌人图鉴（" << profile.discovered() << "/" << Bestiary::EntryCount
      << "） [2] 词缀图鉴 [3] 成就（" << profile.unlockedCount() << "/" << Profile::achievements().size()
      << "） [4] 远征记录 [0] 返回\n";
    const int action = readInt();
    if (action == 0) return;
    if (action == 1) {
      for (;;) {
        profile.printBestiaryOverview();
        std::cout << "查看章节（1-10，0返回）：";
        const int chapter = readInt();
        if (chapter == 0) break;
        if (chapter >= 1 && chapter <= 10) profile.printBestiaryChapter(chapter - 1);
      }
    }
    if (action == 2) profile.printAffixes();
    if (action == 3) profile.printAchievements();
    if (action == 4) profile.printRecords();
  }
}
void help() {
  for (;;) {
    std::cout << "帮助：";
    for (int topic = 1; topic <= Help::topicCount(); ++topic)
      std::cout << "[" << topic << "] " << Help::topicTitle(topic) << ' ';
    std::cout << "[0] 返回\n";
    const int topic = readInt();
    if (topic == 0) return;
    if (topic >= 1 && topic <= Help::topicCount())
      std::cout << "—— " << Help::topicTitle(topic) << " ——\n" << Help::topicText(topic) << '\n';
  }
}
void recordRun(Profile &profile, const player &hero, bool cleared) {
  const RunState &run = hero.runState();
  RunRecord record;
  record.date = todayDate();
  record.job = hero.getjob();
  record.challenge = run.challenge;
  record.cleared = cleared;
  record.floor = hero.getHighestFloor();
  record.level = hero.getlevel();
  record.battles = run.battles;
  record.turns = run.turns;
  record.deaths = run.deaths;
  record.potions = run.potionsUsed;
  record.seed = run.seed;
  record.name = hero.getname();
  profile.addRecord(record);
  profile.save();
  std::cout << "本次远征已记入档案（[11] 远征档案 → 远征记录）。\n";
}
void exitGame(const player &hero, int floor) {
  std::cout << "退出前保存？[1] 是 [2] 否\n";
  if (readInt() == 1) SaveSystem::saveGame(hero, floor);
  std::cout << "再见！\n";
}
bool recover(player &hero) {
  if (hero.isalive()) return true;
  if (hero.runState().challenge == 2) {
    std::cout << "无复苏挑战结束。\n";
    return false;
  }
  std::cout << "角色已倒下。[1] 免费复苏（剩余" << hero.getfreerevives()
    << "） [2] 花费三分之一金币复苏 [0] 结束\n";
  for (;;) {
    const int choice = readInt();
    if (choice == 0) return false;
    if ((choice == 1 || choice == 2) && Campaign::revive(hero, choice == 1)) return true;
    std::cout << "复苏未执行，选择其他方式或0结束。\n";
  }
}
}
int main() {
  std::cout << "=== Aincrad：百层远征 ===\n十个章节，三种路线；行动意图与目标将在战斗前显示。\n";
  std::string profileWarning;
  Profile profile = Profile::load(ProfilePath, &profileWarning);
  if (!profileWarning.empty()) std::cout << "[档案] " << profileWarning << '\n';
  std::unique_ptr<player> hero;
  int floor = 1;
  if (SaveSystem::hasSaveFile()) {
    std::cout << "[1] 读取存档 [2] 新游戏（保存时覆盖旧档）\n";
    int choice; do { choice = readInt(); } while (choice != 1 && choice != 2);
    if (choice == 1) hero = SaveSystem::loadGame(floor);
  }
  if (!hero) {
    std::cout << "角色名字：";
    std::string name;
    if (!std::getline(std::cin, name)) { std::cout << "输入结束。\n"; return 0; }
    if (name.empty()) name = "旅人";
    if (name.size() > 120) { std::cout << "名字过长，使用旅人。\n"; name = "旅人"; }
    std::cout << "职业 [1] 战士 [2] 法师 [3] 刺客：";
    int job; do { job = readInt(); } while (job < 1 || job > 3);
    if (job == 1) hero = std::make_unique<Warrior>(name);
    else if (job == 2) hero = std::make_unique<Mage>(name);
    else hero = std::make_unique<Assassin>(name);
    std::cout << "远征种子（1..2147483647，0随机；同种子同选择可复现）：";
    int seed; do { seed = readInt(); } while (seed < 0);
    std::cout << "挑战 [0] 标准 [1] 补给匮乏（恢复减少，物价提高） [2] 无复苏；两种挑战的敌人特殊意图更频繁：";
    int challenge; do { challenge = readInt(); } while (challenge < 0 || challenge > 2);
    Campaign::initialize(*hero, seed == 0 ? static_cast<unsigned>(std::time(nullptr)) : static_cast<unsigned>(seed), challenge);
    std::cout << "提示：主菜单 [12] 帮助 介绍意图、目标、路线与装备。\n";
  }
  // Only a death or a final clear that happens in this session ends the run
  // in the records; reloading an old save never logs the same run twice.
  bool diedThisSession = false;
  while (floor <= GameConfig::MaxFloor) {
    if (!recover(*hero)) {
      if (diedThisSession) recordRun(profile, *hero, false);
      return 0;
    }
    hero->setHighestFloor(floor);
    checkAchievements(profile, *hero);
    const int c = GameConfig::chapter(floor);
    if (hero->runState().routes[c] == 0) {
      std::cout << "进入" << Campaign::chapter(floor).name << "，选择本章路线（章内固定）：\n";
      for (int route = 1; route <= 3; ++route) std::cout << '[' << route << "] " << Campaign::routeDescription(route) << '\n';
      while (!Campaign::chooseRoute(*hero, floor, readInt())) std::cout << "请选择1..3。\n";
    }
    std::cout << "\n楼层 " << floor << " · " << Campaign::chapter(floor).name << " | HP " << hero->gethp()
      << '/' << hero->getmaxhp() << " | 金币 " << hero->getcoin() << " | "
      << (hero->runState().cleared[floor] ? "已首通" : "待完成") << '\n'
      << "[1] 挑战本层 [2] 背包 [3] 装备 [4] 退出 [5] 存档 [6] 状态 [7] 技能 [8] 营地 [9] 下一层\n"
      << "[10] 前往楼层 [11] 远征档案 [12] 帮助\n";
    const int action = readInt();
    if (action == 4) { exitGame(*hero, floor); return 0; }
    if (action == 5) { SaveSystem::saveGame(*hero, floor); continue; }
    if (action == 6) { hero->printstats(); Campaign::printJournal(*hero, floor); continue; }
    if (action == 7) { skills(*hero); continue; }
    if (action == 8) { camp(*hero, floor); continue; }
    if (action == 2) { backpack(*hero); continue; }
    if (action == 3) {
      hero->showEquippedItems(); std::cout << "卸下序号（0返回）：";
      const int index = readInt(); if (index != 0) hero->unequipItem(index); continue;
    }
    if (action == 9) {
      if (!hero->runState().cleared[floor]) std::cout << "先完成本层目标。\n"; else ++floor;
      continue;
    }
    if (action == 10) { travel(*hero, floor); continue; }
    if (action == 11) { archive(profile); continue; }
    if (action == 12) { help(); continue; }
    if (action != 1) { std::cout << "无效选择。\n"; continue; }
    if (Campaign::eventPending(*hero, floor)) {
      std::cout << Campaign::eventDescription(floor) << '\n';
      while (!Campaign::resolveEvent(*hero, floor, readInt()))
        std::cout << "选择无效或金币不足，请重选；选项2无需金币，低资源时仍可完成。\n";
    }
    const bool firstFinalClear = floor == GameConfig::MaxFloor && !hero->runState().cleared[floor];
    if (fight(*hero, floor, profile)) {
      if (firstFinalClear) recordRun(profile, *hero, true);
      continue;
    }
    if (hero->isalive()) { std::cout << "撤回营地，可重新准备。\n"; continue; }
    diedThisSession = true;
  }
  std::cout << "恭喜通关百层远征！\n";
  Campaign::printJournal(*hero, GameConfig::MaxFloor);
  exitGame(*hero, GameConfig::MaxFloor + 1);
  return 0;
}
