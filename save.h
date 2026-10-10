#pragma once
#include "player.h"
#include <memory>
#include <string>

// =========================================================
// SaveSystem —— 存档 / 读档系统
//
// 存档格式（纯文本，UTF-8）：
//   VERSION 3（兼容 VERSION 1/2，显式保留旧规则标记）
//   NAME   英雄
//   JOB    Warrior | Mage | Assassin
//   LEVEL  5   EXP 200   COIN 500
//   HP 165  MAX_HP 165  ATTACK 25  MP 20  MAX_MP 20
//   CRIT_RATE 10  FREE_REVIVES 3
//   FLOOR  3
//   HIGHEST_FLOOR 65（历史最高楼层；V1缺少此字段时取当前楼层，封顶100）
//   BACKPACK_COUNT 2
//   ITEM  名称|等级|类型|数值|耐久|价格|数量|职业|品质|效果|锁定|模板ID
//   ...
//   EQUIP_COUNT 1
//   EQUIP 名称|等级|类型|数值|耐久|价格|数量|职业|品质|效果|锁定|模板ID
//   ...
//   SKILL_COUNT 3
//   SKILL 技能id|技能等级|当前冷却|专精
//   RunState：种子、挑战、路线、事件、订单、商店、通关、保底与统计
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

  // 只在玩家明确选择迁移后调用。保留楼层/物品/章节，规范等级与技能。
  // 已使用新规则时返回false，不重复发放补偿。
  static bool migrateLegacy(player &hero);

  // 检查是否存在存档文件
  static bool hasSaveFile(const std::string &filename = "save.dat");

  // 写临时文件后替换目标；失败时保留原文件。存档与档案文件共用。
  static bool writeFileAtomically(const std::string &filename, const std::string &contents);

private:
  // 把 Item 序列化成一行 pipe 分隔的字符串
  static std::string serializeItem(const Item &item);

  // 从一行 pipe 分隔的字符串反序列化 Item
  static Item deserializeItem(const std::string &line);
};
