#include "enemy.h"
#include "item.h"
#include "jobs.h"
#include "player.h"
#include "save.h" // ★ 存档系统
#include "combat.h"
#include "world.h"
#include "progression.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace std;

// ─────────────────────────────────────────────
//  前置声明
// ─────────────────────────────────────────────
void showshop(player &hero, int floor, vector<Item> &shopItems);
bool battle(player &hero, enemy &monster);
bool hadEvent(player &hero, int floor);

// ─────────────────────────────────────────────
//  输入辅助：清除 cin 错误状态
// ─────────────────────────────────────────────
static int safeReadInt() {
  int v;
  while (!(cin >> v)) {
    if (cin.eof()) {
      cout << "\n输入结束，退出游戏。" << endl;
      exit(0);
    }
    cin.clear();
    cin.ignore(10000, '\n');
    cout << "请输入一个数字：";
  }
  return v;
}

void manageSkills(player &hero) {
  while (true) {
    hero.showSkills(hero);
    cout << "[1] 升级技能  [2] 返回" << endl;
    int choice = safeReadInt();
    if (choice == 2) return;
    if (choice != 1) {
      cout << "无效选择。" << endl;
      continue;
    }
    cout << "输入技能序号：";
    hero.levelUpSkill(safeReadInt());
  }
}

// ─────────────────────────────────────────────
//  随机事件枚举
// ─────────────────────────────────────────────
enum class eventType { NONE, TREASURE, TRAP, MERCHANT, HOT_SPRING, SHRINE };

eventType generateRandomEvent() {
  int r = rand() % 100;
  if (r < 20)
    return eventType::TREASURE;
  else if (r < 40)
    return eventType::TRAP;
  else if (r < 60)
    return eventType::MERCHANT;
  else if (r < 80)
    return eventType::HOT_SPRING;
  else
    return eventType::SHRINE;
}

// ─────────────────────────────────────────────
//  商店
// ─────────────────────────────────────────────
void showshop(player &hero, int floor, vector<Item> &shopItems) {
  cout << "\n========================================" << endl;
  cout << "欢迎来到商店！当前楼层：" << floor << endl;
  for (int i = 0; i < (int)shopItems.size(); i++) {
    cout << i + 1 << ". " << shopItems[i].getName() << " - 类型: "
         << (shopItems[i].getType() == Weapon  ? "武器"
             : shopItems[i].getType() == Armor ? "防具"
                                               : "药水")
         << "\n   价值: " << shopItems[i].getValue()
         << " 耐久: " << shopItems[i].getDurability()
         << "\n   价格: " << shopItems[i].getPrice()
         << " 数量: " << shopItems[i].getQuantity()
         << "\n   等级需求: " << shopItems[i].getLevelRequirement()
         << "\n   职业限制: " << shopItems[i].getJobRequirementName()
         << "\n   你的金币：" << hero.getcoin()
         << "\n=========================================" << endl;
  }
}

// ─────────────────────────────────────────────
//  随机事件触发
// ─────────────────────────────────────────────
bool hadEvent(player &hero, int floor) {
  int eventChance = min(30 + floor, 80);
  if ((rand() % 100) >= eventChance)
    return false;

  eventType event = generateRandomEvent();
  switch (event) {

  case eventType::TREASURE: {
    cout << "你发现了一个宝箱！里面有一些金币和一个随机物品！" << endl;
    int coins = 40 + floor * 15 + rand() % (floor * 5 + 20); // 宝箱金币有随机性
    hero.gaincoin(coins);
    cout << "你获得了 " << coins << " 金币！" << endl;
    Item randomItem = generateRandomItem(floor);
    cout << "你获得了一个"
         << (randomItem.getType() == Potion   ? "药水"
             : randomItem.getType() == Weapon ? "武器"
                                              : "防具")
         << "：" << randomItem.getName() << "！" << endl;
    cout << "价值 " << randomItem.getValue() << "，耐久 "
         << randomItem.getDurability() << "，等级需求 "
         << randomItem.getLevelRequirement() << "！" << endl;
    // 显示职业限制提示
    if (randomItem.getJobRequirement() != AnyJob)
      cout << "  ? 此物品为【" << randomItem.getJobRequirementName() << "】"
           << endl;
    cout << "[1] 加入背包  [2] 直接装备  [3] 丢弃（获少量金币）" << endl;
    int c;
    c = safeReadInt();
    if (c == 1) {
      hero.addItem(randomItem);
      cout << "物品已加入背包！" << endl;
    } else if (c == 2) {
      hero.equipItem_T(randomItem); // 职业检查在内部处理
    } else {
      int junkGold = randomItem.getPrice() / 3;
      hero.gaincoin(junkGold);
      cout << "物品被丢弃，获得 " << junkGold << " 金币。" << endl;
    }
  } break;

  case eventType::TRAP: {
    cout << "你触发了一个陷阱！受到了一些伤害！" << endl;
    // 陷阱伤害：约等于玩家当前最大HP的8%~15%，有痛感但不致命
    int trapDamage = 8 + floor * 2 + rand() % (floor + 5);
    hero.takedamage(trapDamage);
  } break;

  case eventType::MERCHANT: {
    cout << "你遇到了一个商人！他有一些物品出售！" << endl;
    vector<Item> shopItems;
    for (int i = 0; i < 5; i++)
      shopItems.push_back(generateRandomItem(floor));
    showshop(hero, floor, shopItems);
    cout << "[1] 购买物品  [2] 离开" << endl;
    int sc;
    sc = safeReadInt();
    if (sc == 1) {
      bool buying = true;
      while (buying) {
        if (shopItems.empty()) {
          cout << "商店已售罄！" << endl;
          break;
        }
        cout << "输入物品序号购买：";
        int bi;
        bi = safeReadInt();
        if (bi >= 1 && bi <= (int)shopItems.size()) {
          Item &item = shopItems[bi - 1];
          if (hero.getcoin() >= item.getPrice()) {
            hero.costcoin(item.getPrice());
            cout << "购买了 " << item.getName() << "！" << endl;
            cout << "[1] 背包  [2] 装备  [3] 丢弃" << endl;
            int cc;
            cc = safeReadInt();
            if (cc == 1)
              hero.addItem(item);
            else if (cc == 2)
              hero.equipItem_T(item);
            shopItems.erase(shopItems.begin() + bi - 1);
          } else
            cout << "金币不足！" << endl;
        } else
          cout << "无效序号！" << endl;
        showshop(hero, floor, shopItems);
        cout << "继续购买？[1] 是  [2] 否" << endl;
        int again;
        again = safeReadInt();
        if (again != 1)
          buying = false;
      }
    }
  } break;

  case eventType::HOT_SPRING: {
    cout << "你发现了一个温泉！休息一下，恢复一些 HP 和 MP！" << endl;
    int hpR = 40 + floor * 8, mpR = 20 + floor * 4; // 温泉：大回复，鼓励冒险
    hero.sethp(hero.gethp() + hpR);
    hero.setmp(hero.getmp() + mpR);
    cout << "你恢复了 " << hpR << " 点 HP 和 " << mpR << " 点 MP！" << endl;
  } break;

  case eventType::SHRINE: {
    cout << "你发现了一个神社！许个愿吧！" << endl;
    int outcome = rand() % 3;
    if (outcome == 0) {
      cout << "神明赐予了你金币和经验！" << endl;
      int gc = 80 + floor * 30, ec = 40 + floor * 20; // 神赐：大量经验和金币
      hero.gaincoin(gc);
      hero.gainexp(ec);
      cout << "获得 " << gc << " 金币，" << ec << " 经验！" << endl;
    } else if (outcome == 1) {
      cout << "神明拒绝了你！失去了一些 HP 和 MP！" << endl;
      int hl = 10 + floor * 3, ml = 8 + floor * 3; // 神罚：较轻，避免无聊死亡
      hero.takedamage(hl);
      hero.setmp(max(0, hero.getmp() - ml));
      cout << "失去了 " << hl << " 点 HP 和 " << ml << " 点 MP！" << endl;
    } else {
      cout << "神明沉默不语..." << endl;
    }
  } break;

  default:
    break;
  }

  if (hero.gethp() <= 0) {
    cout << "你在事件中不幸死亡了..." << endl;
    return true;
  }
  return false;
}

// ─────────────────────────────────────────────
//  战斗逻辑
// ─────────────────────────────────────────────
bool battle(player &hero, enemy &monster) {
  if (!monster.isalive() || !hero.isalive())
    return false;
  cout << "\n>>> " << monster.getname() << " 出现了！<<<" << endl;
  BattleSession combat(hero, monster);
  while (combat.beginTurn()) {
    monster.displayIntent();
    cout << "HP " << hero.gethp() << "/" << hero.getmaxhp()
         << " | MP " << hero.getmp() << "/" << hero.getmaxmp() << endl;
    cout << "[1] 普攻  [2] 技能  [3] 格挡（减伤50%，回6MP）  [4] 药水" << endl;
    int choice = safeReadInt();
    BattleAction action{ActionType::Attack};
    if (choice == 2) {
      hero.showSkills(hero);
      cout << "选择技能（0返回）：";
      action = {ActionType::Skill, safeReadInt()};
      if (action.index == 0) continue;
    } else if (choice == 3) {
      action.type = ActionType::Guard;
    } else if (choice == 4) {
      hero.printBackpack();
      cout << "选择药水（0返回；使用会消耗本回合）：";
      action = {ActionType::Potion, safeReadInt()};
      if (action.index == 0) continue;
    } else if (choice != 1) {
      cout << "无效选择，请重新选择。" << endl;
      continue;
    }
    if (!combat.act(action))
      cout << "行动未执行，请重新选择。" << endl;
  }
  if (!hero.isalive()) {
    cout << ">>> YOU LOSE..." << endl;
    return false;
  }
  hero.gainexp(monster.getexp_reward());
  hero.gaincoin(monster.getcoin_reward());
  int recoveryPercent = hero.getHighestFloor() < 40 ? 10 : 4;
  hero.heal(hero.getmaxhp() * recoveryPercent / 100);
  if (rand() % 100 < 60) {
    int dropFloor = monster.isBoss() ? monster.getlevel() - 3 : monster.getlevel();
    Item dropped = generateRandomItem(dropFloor);
    cout << "掉落：" << dropped.getName() << endl;
    hero.addItem(dropped);
  }
  cout << ">>> YOU WIN!（" << combat.turns() << " 回合）" << endl;
  return true;
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────
int main() {
  srand((unsigned)time(nullptr));

  cout << "=== Link Start! 欢迎来到 Aincrad! ===" << endl;
  cout << "规则提醒：\n"
       << "  - 死亡可使用免费复活，退后一层\n"
       << "  - 有金币死亡→损失一半金币，退后一层\n"
       << "  - 请珍惜生命、金币与免费复活机会！\n"
       << endl;

  // ────────────────────────────
  //  ★ 存档检测：询问是否读档
  // ────────────────────────────
  unique_ptr<player> ptr;
  int floor = 1;

  if (SaveSystem::hasSaveFile()) {
    cout << "检测到存档文件！\n"
         << "[1] 读取存档继续游戏\n"
         << "[2] 开始新游戏（将覆盖存档）\n"
         << "请选择：";
    int loadChoice;
    loadChoice = safeReadInt();
    if (loadChoice == 1) {
      ptr = SaveSystem::loadGame(floor);
      if (!ptr) {
        cout << "读档失败，将开始新游戏。" << endl;
      }
    } else {
      SaveSystem::deleteSave();
    }
  }

  // ────────────────────────────
  //  未读档 → 新建角色
  // ────────────────────────────
  if (!ptr) {
    cout << "请输入你的名字：";
    string player_name;
    cin >> player_name;
    cout << "选择职业：[1] 战士  [2] 法师  [3] 刺客" << endl;
    int job_choice;
    job_choice = safeReadInt();
    switch (job_choice) {
    case 1:
      ptr = make_unique<Warrior>(player_name);
      break;
    case 2:
      ptr = make_unique<Mage>(player_name);
      break;
    case 3:
      ptr = make_unique<Assassin>(player_name);
      break;
    default:
      cout << "无效职业，默认选择战士。" << endl;
      ptr = make_unique<Warrior>(player_name);
    }
  }

  // ────────────────────────────
  //  主循环
  // ────────────────────────────
  while (floor <= 100) {
    ptr->setHighestFloor(floor);
    vector<Item> shopItems;
    for (int i = 0; i < 5; i++)
      shopItems.push_back(generateRandomItem(floor));

    // 内层：本层刷怪
    // needNewMonster 控制是否重新生成怪物
    // 初次进入或战斗结束后才生成新怪，查看背包/状态等不刷新
    bool needNewMonster = true;
    enemy monster = generateEnemy(floor); // 占位初始化，马上被覆盖

    while (true) {
      if (needNewMonster) {
        monster = generateEnemy(floor);
        monster.displayAffixes();
        needNewMonster = false;
      }
      cout << "\n========================================" << endl;
      cout << "楼层：" << floor << endl;
      cout << "玩家：Lv." << ptr->getlevel() << " | HP：" << ptr->gethp()
           << " | 金币：" << ptr->getcoin() << " | MP：" << ptr->getmp()
           << endl;
      cout << "敌人：Lv." << monster.getlevel() << " | HP：" << monster.gethp()
           << endl;
      cout << "----------------------------------------" << endl;
      cout << "[1] 战斗  [2] 背包  [3] 装备  [4] 退出\n"
           << "[5] 存档  [6] 查看状态  [7] 技能" << endl;

      int choice;
      choice = safeReadInt();

      // ── 存档 ──────────────────────────────────────────
      if (choice == 5) {
        SaveSystem::saveGame(*ptr, floor);
        continue;
      }

      if (choice == 6) {
        ptr->printstats();
        continue;
      }

      if (choice == 7) {
        manageSkills(*ptr);
        continue;
      }

      if (choice == 2) {
        ptr->printBackpack();
        cout << "[1] 使用物品  [2] 装备物品  [3] 丢弃物品  [4] 返回" << endl;
        int bmc = safeReadInt();
        if (bmc == 1) {
          cout << "输入物品序号：";
          int idx = safeReadInt();
          ptr->useItem(idx);
        } else if (bmc == 2) {
          cout << "输入物品序号：";
          int idx = safeReadInt();
          ptr->equipItem(idx);
        } else if (bmc == 3) {
          cout << "输入要丢弃的物品序号（丢弃可获得少量金币）：";
          int idx = safeReadInt();
          ptr->dropItem(idx);
        }
        continue;
      }

      if (choice == 3) {
        ptr->showEquippedItems();
        continue;
      }

      if (choice == 4) {
        // ── 退出前询问是否存档 ──────────────────────
        cout << "退出前是否保存游戏？[1] 是  [2] 否" << endl;
        int sc;
        sc = safeReadInt();
        if (sc == 1)
          SaveSystem::saveGame(*ptr, floor);
        cout << "再见！" << endl;
        return 0;
      }

      // ── 战斗 ─────────────────────────────────────────
      if (choice == 1) {
        // 事件
        bool eventKilled = hadEvent(*ptr, floor);
        bool won = !eventKilled && battle(*ptr, monster);

        if (won) {
          // ── 战后菜单 ────────────────────────────
        post_battle_decision:
          cout << "\n战后选择：\n"
               << "[1] 继续刷怪  [2] 前往下一层  [3] 退出\n"
               << "[4] 查看状态  [5] 背包        [6] 装备\n"
               << "[7] 商店      [8] 技能        [9] 存档" << endl; // ★ 选项 9

          int pbc;
          pbc = safeReadInt();

          if (pbc == 1) {
            needNewMonster = true; // 生成新怪物
            continue;
          }

          else if (pbc == 2) {
            if (floor == 100)
              cout << "恭喜通关！" << endl;
            else
              cout << "前往下一层..." << endl;
            shopItems.clear();
            floor++;
            break;
          }

          else if (pbc == 3) {
            cout << "退出前是否保存游戏？[1] 是  [2] 否" << endl;
            int sc;
            sc = safeReadInt();
            if (sc == 1)
              SaveSystem::saveGame(*ptr, floor);
            cout << "再见！" << endl;
            return 0;
          }

          else if (pbc == 4) {
            ptr->printstats();
            goto post_battle_decision;
          }

          else if (pbc == 5) {
            ptr->printBackpack();
            cout << "[1] 使用物品  [2] 装备物品  [3] 丢弃物品  [4] 返回"
                 << endl;
            int bc = safeReadInt();
            if (bc == 1) {
              cout << "输入物品序号：";
              int idx = safeReadInt();
              ptr->useItem(idx);
            } else if (bc == 2) {
              cout << "输入物品序号：";
              int idx = safeReadInt();
              ptr->equipItem(idx);
            } else if (bc == 3) {
              cout << "输入要丢弃的物品序号（丢弃可获得少量金币）：";
              int idx = safeReadInt();
              ptr->dropItem(idx);
            }
            goto post_battle_decision;
          }

          else if (pbc == 6) {
            ptr->showEquippedItems();
            cout << "[1] 卸下装备  [2] 返回" << endl;
            int ec;
            ec = safeReadInt();
            if (ec == 1) {
              cout << "输入装备序号：";
              int idx;
              idx = safeReadInt();
              ptr->unequipItem(idx);
            }
            goto post_battle_decision;
          }

          else if (pbc == 7) {
            showshop(*ptr, floor, shopItems);
          post_shop_decision:
            cout << "[1] 购买  [2] 返回  [3] 刷新商店（50金币）" << endl;
            int sc;
            sc = safeReadInt();
            if (sc == 1) {
              bool buying = true;
              while (buying) {
                if (shopItems.empty()) {
                  cout << "商店已售罄！是否刷新（50金币）？[1] 是  [2] 否"
                       << endl;
                  int rc;
                  rc = safeReadInt();
                  if (rc == 1 && ptr->getcoin() >= 50) {
                    ptr->costcoin(50);
                    for (int i = 0; i < 5; i++)
                      shopItems.push_back(generateRandomItem(floor));
                    showshop(*ptr, floor, shopItems);
                  } else
                    break;
                }
                cout << "输入购买序号：";
                int bi;
                bi = safeReadInt();
                if (bi >= 1 && bi <= (int)shopItems.size()) {
                  Item item = shopItems[bi - 1];
                  if (ptr->getcoin() >= item.getPrice()) {
                    ptr->costcoin(item.getPrice());
                    ptr->addItem(item);
                    shopItems.erase(shopItems.begin() + bi - 1);
                    cout << "购买了 " << item.getName() << "！" << endl;
                  } else
                    cout << "金币不足！" << endl;
                } else
                  cout << "无效序号！" << endl;
                showshop(*ptr, floor, shopItems);
                cout << "继续购买？[1] 是  [2] 否" << endl;
                int again;
                again = safeReadInt();
                if (again != 1)
                  buying = false;
              }
            } else if (sc == 3) {
              if (ptr->getcoin() >= 50) {
                ptr->costcoin(50);
                shopItems.clear();
                for (int i = 0; i < 5; i++)
                  shopItems.push_back(generateRandomItem(floor));
                showshop(*ptr, floor, shopItems);
              } else
                cout << "金币不足，无法刷新商店！" << endl;
              goto post_shop_decision;
            }
            goto post_battle_decision;
          }

          else if (pbc == 8) {
            manageSkills(*ptr);
            goto post_battle_decision;
          }

          // ★ 战后存档选项
          else if (pbc == 9) {
            SaveSystem::saveGame(*ptr, floor);
            goto post_battle_decision;
          }
          else {
            cout << "无效选择，请重新选择。" << endl;
            goto post_battle_decision;
          }

        } else {
          // ── 死亡处理 ─────────────────────────────
          // 优先使用免费复活（无楼层限制）
          if (ptr->getfreerevives() > 0) {
            cout << "你还有 " << ptr->getfreerevives()
                 << " 次免费复活机会，是否使用？[1] 是  [2] 否" << endl;
            int rc;
            rc = safeReadInt();
            if (rc == 1) {
              ptr->decreaserevives();
              ptr->revive();
              if (floor > 1) {
                floor--;
              }
              cout << "已免费复活！退回到第 " << floor << " 层。" << endl;
              break;
            }
          }
          // 无免费复活：尝试花金币复活
          if (ptr->getcoin() == 0) {
            cout << "没有金币，也没有免费复活次数！游戏结束！" << endl;
            return 0;
          }
          int coinLoss = max(1, ptr->getcoin() / 2);
          cout << "\n是否花费 " << coinLoss << " 金币复活？"
               << "[1] 复活（退后一层）  [2] 放弃" << endl;
          int rc;
          rc = safeReadInt();
          if (rc == 1) {
            ptr->revive();
            ptr->costcoin(coinLoss);
            cout << "已复活！损失 " << coinLoss << " 金币。" << endl;
            if (floor > 1) {
              floor--;
              cout << "退回到第 " << floor << " 层。" << endl;
            } else
              cout << "在第 1 层复活。" << endl;
            break;
          } else {
            cout << "游戏结束！" << endl;
            return 0;
          }
        }
      } // end choice == 1
    } // end inner while
  } // end outer while

  cout << "游戏结束！感谢游玩！" << endl;
  return 0;
}
