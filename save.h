#pragma once
#include "player.h"
#include <memory>
#include <string>

// =========================================================
// SaveSystem —— 存档 / 读档系统
//
// 存档格式（纯文本，UTF-8）：
//   VERSION 2（兼容 VERSION 1）
//   NAME   英雄
//   JOB    Warrior | Mage | Assassin
//   LEVEL  5   EXP 200   COIN 500
//   HP 165  MAX_HP 165  ATTACK 25  MP 20  MAX_MP 20
//   CRIT_RATE 10  FREE_REVIVES 3
//   FLOOR  3
//   HIGHEST_FLOOR 65（历史最高楼层，旧存档取当前楼层）
//   BACKPACK_COUNT 2
//   ITEM  物品名|等级需求|类型|数值|耐久|价格|数量
//   ...
//   EQUIP_COUNT 1
//   EQUIP 物品名|等级需求|类型|数值|耐久|价格|数量
//   ...
//   SKILL_COUNT 3
//   SKILL 技能id|技能等级|当前冷却
//   ...
// =========================================================

class SaveSystem {
public:
  // 保存游戏；成功返回 true
  static bool saveGame(const player &hero, int floor,
                       const std::string &filename = "save.dat");

  // 读档；成功返回有效的 player 指针并写入 floor，
  // 失败返回 nullptr，且不修改 floor
  static std::unique_ptr<player>
  loadGame(int &floor, const std::string &filename = "save.dat");

  // 检查是否存在存档文件
  static bool hasSaveFile(const std::string &filename = "save.dat");

  // 删除存档文件
  static void deleteSave(const std::string &filename = "save.dat");

private:
  // 把 Item 序列化成一行 pipe 分隔的字符串
  static std::string serializeItem(const Item &item);

  // 从一行 pipe 分隔的字符串反序列化 Item
  static Item deserializeItem(const std::string &line);
};
