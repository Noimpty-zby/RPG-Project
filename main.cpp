#include "campaign.h"
#include "game_config.h"
#include "jobs.h"
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
    if (action == 1) hero.useItem(index);
    if (action == 2) hero.equipItem(index);
    if (action == 3) hero.dropItem(index);
    if (action == 4 && !hero.toggleItemLock(index)) std::cout << "无法锁定该物品。\n";
  }
}
void skills(player &hero) {
  for (;;) {
    hero.showSkills(hero);
    std::cout << "[1] 升级 [2] 返回 [3] 选择专精（前两技能；10/30层开放）\n";
    const int action = readInt();
    if (action == 2) return;
    if (action != 1 && action != 3) continue;
    std::cout << "技能序号：";
    const int slot = readInt();
    if (action == 1) hero.levelUpSkill(slot);
    else {
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
      const int price = Campaign::purchasePrice(hero, floor, stock[i]);
      std::cout << i + 1 << ". " << stock[i].getName() << " 数值 " << stock[i].getValue()
        << " 等级需求 " << stock[i].getLevelRequirement() << " 价格 " << price
        << " 品质 " << stock[i].getTierName() << " 效果 " << stock[i].getEffectName()
        << " " << stock[i].getJobRequirementName() << '\n';
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
    if (action == 4) {
      hero.showEquippedItems();
      std::cout << "已装备序号（一个效果槽，改造消耗材料）：";
      const int index = readInt();
      const auto &equipped = hero.getSaveEquipments();
      if (index > 0 && index <= static_cast<int>(equipped.size()))
        std::cout << "本次改造消耗 " << 3 + equipped[index - 1].getTier() * 2 << " 材料；当前 " << hero.runState().materials << "。\n";
      std::cout << "效果 [1] 普攻回蓝 [2] 格挡回蓝 [3] 技能节能 [4] 控制强化：";
      if (!hero.modifyEquipment(index, readInt())) std::cout << "改造未执行，检查材料、序号和效果。\n";
    }
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
bool fight(player &hero, int floor) {
  auto monster = Campaign::makeEnemy(hero, floor);
  const auto rules = Campaign::rulesFor(hero, floor);
  std::cout << "遭遇：" << monster.getname() << " HP " << monster.gethp() << " 攻击 " << monster.getattack() << '\n';
  showObjective(rules);
  Campaign::seedStream(hero, floor, 1200 + static_cast<unsigned>(hero.runState().attempts[floor]));
  BattleSession battle(hero, monster, rules);
  monster.displayAffixes();
  std::cout << monster.getEncounterTitle() << "：" << monster.getTacticalHint() << '\n';
  while (battle.beginTurn()) {
    monster.displayIntent();
    std::cout << battle.objectiveDescription() << '\n';
    std::cout << "HP " << hero.gethp() << '/' << hero.getmaxhp() << " MP " << hero.getmp()
      << '/' << hero.getmaxmp() << " 敌HP " << monster.gethp() << '\n'
      << "[1] 普攻（命中可打断蓄力） [2] 技能 [3] 格挡 [4] 药水\n";
    const int action = readInt();
    BattleAction chosen{ActionType::Attack};
    if (action == 2) {
      hero.showSkills(hero); std::cout << "技能序号，0返回：";
      chosen = {ActionType::Skill, readInt()}; if (chosen.index == 0) continue;
    } else if (action == 3) chosen.type = ActionType::Guard;
    else if (action == 4) {
      hero.printBackpack(); std::cout << "药水序号，0返回：";
      chosen = {ActionType::Potion, readInt()}; if (chosen.index == 0) continue;
    } else if (action != 1) { std::cout << "无效行动。\n"; continue; }
    if (!battle.act(chosen)) std::cout << "行动未执行，没有推进回合。\n";
  }
  Campaign::printBattleReport(battle);
  const bool won = Campaign::settleEncounter(hero, monster, battle, floor);
  std::cout << (won ? "目标完成！可前往下一层，或在营地调整。\n" : "本次挑战未完成。\n");
  return won;
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
  }
  if (!recover(*hero)) return 0;
  while (floor <= 100) {
    if (!recover(*hero)) return 0;
    hero->setHighestFloor(floor);
    const int c = GameConfig::chapter(floor);
    if (hero->runState().routes[c] == 0) {
      std::cout << "进入" << Campaign::chapter(floor).name << "，选择本章路线（章内固定）：\n";
      for (int route = 1; route <= 3; ++route) std::cout << '[' << route << "] " << Campaign::routeDescription(route) << '\n';
      while (!Campaign::chooseRoute(*hero, floor, readInt())) std::cout << "请选择1..3。\n";
    }
    std::cout << "\n楼层 " << floor << " · " << Campaign::chapter(floor).name << " | HP " << hero->gethp()
      << '/' << hero->getmaxhp() << " | 金币 " << hero->getcoin() << " | "
      << (hero->runState().cleared[floor] ? "已首通" : "待完成") << '\n'
      << "[1] 挑战本层 [2] 背包 [3] 装备 [4] 退出 [5] 存档 [6] 状态 [7] 技能 [8] 营地 [9] 下一层\n";
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
    if (action != 1) { std::cout << "无效选择。\n"; continue; }
    if (Campaign::eventPending(*hero, floor)) {
      std::cout << Campaign::eventDescription(floor) << '\n';
      while (!Campaign::resolveEvent(*hero, floor, readInt()))
        std::cout << "选择无效或金币不足，请重选；选项2无需金币，低资源时仍可完成。\n";
    }
    if (fight(*hero, floor)) continue;
    if (hero->isalive()) { std::cout << "撤回营地，可重新准备。\n"; continue; }
  }
  std::cout << "恭喜通关百层远征！\n";
  Campaign::printJournal(*hero, 100); exitGame(*hero, 101);
  return 0;
}
