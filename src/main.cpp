#include "core/game_config.h"
#include "game/campaign.h"
#include "game/exploration.h"
#include "player/jobs.h"
#include "player/skills.h"
#include "storage/profile.h"
#include "storage/save.h"
#include "ui/help.h"
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
    std::cout << "[1] 使用药水 [2] 装备 [3] 卖出 [4] 锁定/解锁 [5] 批量拆解未锁定装备 [6] 喝药补到指定生命 [0] 返回\n";
    const int action = readInt();
    if (action == 0) return;
    if (action == 6) {
      std::cout << "补到最大生命的百分之几（1-100，0取消）：";
      const int percent = readInt();
      if (percent < 1 || percent > 100) continue;
      const int target = static_cast<int>(static_cast<long long>(hero.getmaxhp()) * percent / 100);
      const auto plan = hero.planPotions(target);
      if (plan.uses.empty()) {
        std::cout << (hero.gethp() >= target ? "生命已达到目标。\n" : "没有可用的药水。\n");
        continue;
      }
      std::cout << "将使用：";
      for (std::size_t i = 0; i < plan.uses.size(); ++i)
        std::cout << (i ? "、" : "") << hero.getSaveBackpack()[plan.uses[i].first - 1].getName() << "×" << plan.uses[i].second;
      std::cout << "；恢复 " << plan.restored << "，溢出 " << plan.overflow << "，之后 HP "
        << hero.gethp() + plan.restored << '/' << hero.getmaxhp() << "。[1] 确认 [0] 取消\n";
      if (readInt() != 1) continue;
      const int restored = hero.drinkPotionsTo(target);
      std::cout << "共恢复 " << restored << " 生命，当前 HP " << hero.gethp() << '/' << hero.getmaxhp() << "。\n";
      continue;
    }
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
void enhanceEquipment(player &hero) {
  for (;;) {
    const auto &equipped = hero.getSaveEquipments();
    if (equipped.empty()) { std::cout << "没有装备任何物品！\n"; return; }
    std::cout << "强化已穿戴装备（金币 " << hero.getcoin() << "；每级数值 +" << GameConfig::EnhancePercent
      << "%，最高 +" << GameConfig::MaxEnhanceLevel << "；换下后强化保留在装备上）\n";
    for (int i = 0; i < static_cast<int>(equipped.size()); ++i) {
      const Item &item = equipped[i];
      std::cout << i + 1 << ". " << item.getDisplayName() << " | " << (item.getType() == Weapon ? "攻击 " : "生命 ")
        << item.getValue();
      if (const int cost = hero.enhanceCost(i + 1))
        std::cout << " → " << item.getValue() + std::max(1, item.getValue() * GameConfig::EnhancePercent / 100)
          << " | 费用 " << cost << '\n';
      else std::cout << " | 已满级\n";
    }
    std::cout << "强化序号（0返回）：";
    const int index = readInt();
    if (index == 0) return;
    if (!hero.enhanceEquipment(index)) std::cout << "未强化：检查序号、金币和强化上限。\n";
  }
}
void repairEquipment(player &hero) {
  for (;;) {
    const auto &equipped = hero.getSaveEquipments();
    if (equipped.empty()) { std::cout << "没有装备任何物品！\n"; return; }
    std::cout << "修理已穿戴装备（金币 " << hero.getcoin() << "；修到耐久上限，品质越高越贵）\n";
    for (int i = 0; i < static_cast<int>(equipped.size()); ++i) {
      std::cout << i + 1 << ". " << equipped[i].getDisplayName() << " | 耐久 " << equipped[i].getDurability()
        << "/" << equipped[i].getMaxDurability() << " | ";
      if (const int cost = hero.repairCost(i + 1)) std::cout << "费用 " << cost << '\n';
      else std::cout << "无需修理\n";
    }
    std::cout << "修理序号（0返回）：";
    const int index = readInt();
    if (index == 0) return;
    if (!hero.repairEquipment(index)) std::cout << "未修理：耐久已满、序号无效或金币不足。\n";
  }
}
void cooking(player &hero) {
  for (;;) {
    const auto &run = hero.runState();
    std::cout << "料理（食材：";
    for (int i = 0; i < Exploration::IngredientCount; ++i)
      std::cout << (i ? " " : "") << Exploration::ingredientName(i) << run.ingredients[i];
    std::cout << "；当前祝福：" << Exploration::boonDescription(run.boon) << "，新祝福会替换旧的）\n";
    for (const auto &recipe : Exploration::recipes()) {
      std::string needs;
      for (int i = 0; i < Exploration::IngredientCount; ++i)
        if (recipe.needs[i]) needs += std::string(needs.empty() ? "" : "、") + Exploration::ingredientName(i) + "×" + std::to_string(recipe.needs[i]);
      std::cout << '[' << recipe.id << "] " << recipe.name << (needs.empty() ? "" : "（" + needs + "）") << "："
        << recipe.description << (Exploration::canCook(hero, recipe.id) ? "" : "（食材不足）") << '\n';
    }
    std::cout << "[0] 返回\n";
    const int choice = readInt();
    if (choice == 0) return;
    if (!Exploration::cook(hero, choice).ok) std::cout << "食材不足或选择无效。\n";
  }
}
void camp(player &hero, int floor) {
  for (;;) {
    Campaign::printJournal(hero, floor);
    const auto ready = Campaign::readiness(hero, floor);
    std::cout << ready.summary << '\n';
    if (!ready.warning.empty()) std::cout << ready.warning << '\n';
    std::cout << "营地：[1] 休息（本章剩余 " << Campaign::restsRemaining(hero, floor) << " 次，本次 "
      << Campaign::restPrice(hero, floor) << "金币，恢复基础HP的" << (hero.runState().challenge == 1 ? 25 : 40)
      << "%与最大MP的50%） [2] 领取订单 [3] 定向装备目标 [4] 装备改造\n"
      << "[5] 商店 [6] 技能与专精 [7] 迁移传统存档 [8] 强化装备 [9] 修理装备 [10] 料理 [0] 返回\n";
    const int action = readInt();
    if (action == 0) return;
    if (action == 1 && !Campaign::rest(hero, floor)) {
      if (Campaign::restsRemaining(hero, floor) == 0) std::cout << "未休息：本章休息次数已用完。\n";
      else if (hero.gethp() == hero.getmaxhp() && hero.getmp() == hero.getmaxmp())
        std::cout << "未休息：生命和魔力都是满的。\n";
      else std::cout << "未休息：金币不足（需要 " << Campaign::restPrice(hero, floor) << "）。\n";
    }
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
    if (action == 8) enhanceEquipment(hero);
    if (action == 9) repairEquipment(hero);
    if (action == 10) cooking(hero);
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
    std::cout << "目标：普攻蓄力中的敌人完成两次打断（被闪避也算），或击败敌人。\n";
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
const char *zoneName(Exploration::Zone zone) {
  switch (zone) {
  case Exploration::Zone::Guardian: return "守卫房间";
  case Exploration::Zone::Hidden: return "隐秘处";
  case Exploration::Zone::Vault: return "上锁的侧室";
  default: return "房间";
  }
}
void puzzleMenu(player &hero, int floor) {
  const auto puzzle = Exploration::puzzle(hero, floor);
  const int size = static_cast<int>(puzzle.elements.size());
  std::cout << puzzle.prompt << '\n';
  for (const auto &clue : puzzle.clues) std::cout << "· " << clue << '\n';
  for (int i = 0; i < size; ++i) std::cout << '[' << i + 1 << "] " << puzzle.elements[i] << ' ';
  std::cout << "\n从第一个到最后一个依次输入编号（0取消；答错损失当前生命5%，不会致死）：\n";
  std::vector<int> order;
  while (static_cast<int>(order.size()) < size) {
    std::cout << "第" << order.size() + 1 << "个：";
    const int choice = readInt();
    if (choice == 0) return;
    if (choice < 1 || choice > size || std::find(order.begin(), order.end(), choice - 1) != order.end()) {
      std::cout << "编号无效或重复。\n";
      continue;
    }
    order.push_back(choice - 1);
  }
  if (!Exploration::solvePuzzle(hero, floor, order).ok) std::cout << "无法作答。\n";
}
void explore(player &hero, int floor, Profile &profile) {
  for (;;) {
    const auto spots = Exploration::survey(hero, floor);
    const auto &run = hero.runState();
    std::cout << "—— 探索第" << floor << "层（剩余 " << Exploration::remaining(hero, floor) << " 处） ——\n"
      << "HP " << hero.gethp() << '/' << hero.getmaxhp() << " MP " << hero.getmp() << '/' << hero.getmaxmp()
      << " 金币 " << hero.getcoin() << " 材料 " << run.materials << " 钥匙 " << run.keys << '\n';
    for (const auto &view : spots)
      std::cout << '[' << view.index + 1 << "] " << view.name << "（" << zoneName(view.spot.zone) << "）"
        << (view.done ? (view.spot.kind == Exploration::SpotKind::Merchant ? " —— 已光顾" : " —— 已探索") : "") << '\n';
    std::cout << "[0] 返回\n";
    const int choice = readInt();
    if (choice == 0) return;
    if (choice < 1 || choice > static_cast<int>(spots.size())) continue;
    const auto &view = spots[choice - 1];
    std::cout << view.name << "：" << view.description << '\n';
    if (view.spot.kind == Exploration::SpotKind::Puzzle && !view.done) {
      puzzleMenu(hero, floor);
      checkAchievements(profile, hero);
      continue;
    }
    const auto options = Exploration::options(hero, floor, view.index);
    if (options.empty()) {
      std::cout << (view.done ? "已经探索过了。\n" : "现在无法探索。\n");
      continue;
    }
    for (const auto &option : options)
      std::cout << '[' << option.id << "] " << option.label << "：" << option.detail
        << (option.available ? "" : "（" + option.reason + "）") << '\n';
    std::cout << "[0] 离开\n";
    const int pick = readInt();
    if (pick == 0) continue;
    const auto result = Exploration::interact(hero, floor, view.index, pick);
    if (!result.ok) {
      std::cout << "没有执行：选项无效或条件不足。\n";
      continue;
    }
    if (result.lorePage && profile.recordLore(result.lorePage)) {
      std::cout << "新的一页收入见闻录（" << profile.loreCount() << "/100）。\n";
      profile.save();
    }
    checkAchievements(profile, hero);
  }
}
bool fight(player &hero, int floor, Profile &profile) {
  std::vector<std::string> notes;
  auto monster = Campaign::makeEnemy(hero, floor, &notes);
  const auto rules = Campaign::rulesFor(hero, floor);
  std::cout << "遭遇：" << monster.getname() << "（第" << floor << "层） HP " << monster.gethp()
    << " 攻击 " << monster.getattack() << '\n';
  for (const auto &note : notes) std::cout << "【探索】" << note << '\n';
  profile.recordEncounter(floor, monster);
  if (rules.elite)
    std::cout << "【精英】至少两个词缀，生命与攻击更高，特殊意图更频繁且被打断后不休整；首通必得本层最高品质档的目标装备。\n";
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
      << "[1] 普攻（可打断蓄力） [2] 技能 [3] 格挡 [4] 药水 [5] 查看敌我详情\n";
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
  for (const Item &item : hero.getSaveEquipments())
    if (item.getDurability() <= 3)
      std::cout << "【耐久告急】" << item.getDisplayName() << " 还能用 " << item.getDurability()
        << " 场，可在营地修理。\n";
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
      << "） [4] 远征记录 [5] 见闻录（" << profile.loreCount() << "/100） [0] 返回\n";
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
    if (action == 5) {
      for (;;) {
        profile.printLoreOverview();
        std::cout << "阅读章节（1-10，0返回）：";
        const int chapter = readInt();
        if (chapter == 0) break;
        if (chapter >= 1 && chapter <= 10) profile.printLoreChapter(chapter - 1);
      }
    }
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
    std::cout << "挑战 [0] 标准 [1] 补给匮乏（资源紧张，敌人更难缠） [2] 无复苏（敌人同标准，只有一条命）：";
    int challenge; do { challenge = readInt(); } while (challenge < 0 || challenge > 2);
    Campaign::initialize(*hero, seed == 0 ? static_cast<unsigned>(std::time(nullptr)) : static_cast<unsigned>(seed), challenge);
    std::cout << "提示：主菜单 [12] 帮助 介绍意图、目标、路线与装备；[13] 探索本层可以寻找补给、钥匙、谜题与见闻录。\n";
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
      << (hero->runState().cleared[floor] ? "已首通" : "待完成")
      << (Campaign::isEliteFloor(floor) ? " | 精英层" : "") << '\n';
    const auto ready = Campaign::readiness(*hero, floor);
    if (!ready.warning.empty()) std::cout << ready.warning << '\n';
    std::cout << "[1] 挑战本层 [2] 背包 [3] 装备 [4] 退出 [5] 存档 [6] 状态 [7] 技能 [8] 营地 [9] 下一层\n"
      << "[10] 前往楼层 [11] 远征档案 [12] 帮助 [13] 探索本层（剩余 " << Exploration::remaining(*hero, floor) << " 处）\n";
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
    if (action == 13) { explore(*hero, floor, profile); continue; }
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
