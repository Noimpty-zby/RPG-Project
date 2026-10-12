#include "game/exploration.h"
#include "core/game_config.h"
#include "core/random.h"
#include "game/campaign.h"
#include "game/exploration_text.h"
#include "game/world.h"
#include "player/player.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <numeric>

namespace {
using namespace Exploration;

// Seed streams. Each consumer reseeds, so exploring never shifts battles,
// shops or loot. Spot outcomes use SpotStream + index.
constexpr unsigned FloorStream = 700, PlanStream = 701, SpotStream = 720, VaultStream = 740,
                   VaultGearStream = 741, MerchantStream = 750, BlindBoxStream = 760, PuzzleStream = 780;

// Outcome tables as cumulative percent thresholds on one roll of 0..99, and
// the other tuning knobs. Option texts are built from these.
constexpr int CacheGold = 60, CachePotion = 66;         // Else 1 material and half the gold.
constexpr int ContainerGold = 55, ContainerPotion = 70; // Else a trap.
constexpr int TrapHealthPercent = 8;
constexpr int ShrineBoon = 40, ShrineGold = 75;         // Else a curse.
constexpr int RelicMaterials = 40, RelicExperience = 75; // Else a backlash.
constexpr int RelicManaPercent = 40, BacklashManaPercent = 35;
constexpr int GatherPercent = 50, SecondGatherPercent = 10, GatherBonusPercent = 20;
constexpr int AffixSourcePercent = 40, SourceHealthPercent = 10, SourceMaterials = 2;
constexpr int VaultMaterialsRoll = 40, VaultPotionRoll = 70; // Else gold.
constexpr int BoxRefund = 35, BoxPotion = 60, BoxMaterials = 78, BoxIngredients = 92; // Else a key.
constexpr int WeakenPercent = 90;   // Attack left when a source or boon finds no affix.
constexpr int BoonAttackPercent = 92, BoonHealthPercent = 92, BoonDodgePoints = 8;
constexpr int BaneAttackPercent = 112, BaneHealthPercent = 110;
constexpr int AmbushHealthPercent = 92;
constexpr int PuzzleHealthPercent = 5;

const char *const ingredientNames[IngredientCount] = {"鲜鱼", "草药", "矿砂", "霜晶", "星尘"};
// Dangerous affixes in the order an affix source or the boon removes them.
const Affix strippable[] = {Affix::SHIELDING, Affix::POISONOUS, Affix::HEALING, Affix::EXPLOSIVE};

bool validFloor(int floor) { return floor >= 1 && floor <= GameConfig::MaxFloor; }
int localOf(int floor) { return (floor - 1) % GameConfig::FloorsPerChapter + 1; }
// Campaign::seedStream's mixing, from the seed alone.
void seedFor(std::uint32_t seed, int floor, unsigned stream) {
  const unsigned f = static_cast<unsigned>(GameConfig::floor(floor));
  unsigned value = seed ^ (f * 0x9e3779b9U) ^ (stream * 0x85ebca6bU);
  value ^= value >> 16; value *= 0x7feb352dU; value ^= value >> 15;
  Random::seed(value);
}
int pick(std::vector<int> &pool) {
  const int index = Random::below(static_cast<int>(pool.size()));
  const int value = pool[index];
  pool.erase(pool.begin() + index);
  return value;
}
// Small rewards scale with the floor: about a twentieth of a fight's coins and a third of its experience.
int coinUnit(int floor) { return 4 + floor / 2; }
int expUnit(int floor) { return 10 + floor * 8; }
int relicManaCost(const player &hero) { return std::max(1, hero.getmaxmp() * RelicManaPercent / 100); }
int shrinePrice(int floor) { return GameConfig::restCost(floor); }
int puzzleSize(int chapter) { return 3 + (chapter >= 3 ? 1 : 0) + (chapter >= 6 ? 1 : 0); }
int puzzleMaterials(int chapter) { return 2 + chapter / 3; }
int vaultMaterials(int chapter) { return 3 + chapter / 3; }

bool reachable(const player &hero, int floor) {
  return validFloor(floor) && floor <= hero.getHighestFloor() && hero.isalive();
}
bool spotDone(const RunState &run, int floor, int index) { return (run.explored[floor] >> index) & 1; }
void markExplored(RunState &run, int floor, int index) {
  if (spotDone(run, floor, index)) return;
  run.explored[floor] |= 1 << index;
  run.spotsExplored = GameConfig::addCounter(run.spotsExplored, 1);
}
void loseHealthSafely(player &hero, int percent) {
  const int loss = std::max(1, static_cast<int>(static_cast<long long>(hero.gethp()) * percent / 100));
  hero.sethp(std::max(1, hero.gethp() - loss));
}
int addMaterials(player &hero, int amount) {
  auto &run = hero.runState();
  const int before = run.materials;
  run.materials = std::min(GameConfig::MaxMaterials, run.materials + amount);
  return run.materials - before;
}
int addIngredient(RunState &run, int ingredient, int amount) {
  int &count = run.ingredients[std::clamp(ingredient, 0, IngredientCount - 1)];
  const int before = count;
  count = std::min(MaxIngredients, count + amount);
  return count - before;
}
int addKeys(RunState &run, int amount) {
  const int before = run.keys;
  run.keys = std::min(MaxKeys, run.keys + amount);
  return run.keys - before;
}
void givePotions(player &hero, int floor, int quantity) {
  Item potion = generateTargetedItem(floor, AnyJob, Potion);
  potion.setQuantity(quantity);
  Campaign::grantItem(hero, potion);
}
std::string potionText(int quantity) { return std::to_string(quantity) + " 瓶药水"; }
int merchantPrice(const player &hero, int floor, int kind) {
  const int base = GameConfig::restCost(floor);
  int price = kind == 1 ? base * 3 : (kind == 3 ? std::max(1, base / 2) : base);
  if (hero.runState().challenge == 1) price = price * 125 / 100;
  return price;
}

// ---- Puzzle generation --------------------------------------------------------
// Clue types: 0 a is at position b, 1 a is not at position b, 2 a before b,
// 3 a next to b, 4 a not next to b.
struct Clue { int type, a, b; };
int positionOf(const std::vector<int> &order, int element) {
  return static_cast<int>(std::find(order.begin(), order.end(), element) - order.begin());
}
bool holds(const Clue &clue, const std::vector<int> &order) {
  const int pa = positionOf(order, clue.a);
  switch (clue.type) {
  case 0: return pa == clue.b;
  case 1: return pa != clue.b;
  case 2: return pa < positionOf(order, clue.b);
  case 3: return std::abs(pa - positionOf(order, clue.b)) == 1;
  default: return std::abs(pa - positionOf(order, clue.b)) != 1;
  }
}
std::vector<std::vector<int>> permutations(int n) {
  std::vector<int> order(n);
  std::iota(order.begin(), order.end(), 0);
  std::vector<std::vector<int>> all;
  do all.push_back(order); while (std::next_permutation(order.begin(), order.end()));
  return all;
}
int countMatching(const std::vector<Clue> &clues, const std::vector<std::vector<int>> &all) {
  int count = 0;
  for (const auto &order : all)
    if (std::all_of(clues.begin(), clues.end(), [&](const Clue &c) { return holds(c, order); })) ++count;
  return count;
}
struct GeneratedPuzzle { std::vector<int> solution; std::vector<Clue> clues; };
// The solution is a random order; clues are true facts added until only that
// order remains, relations before fixed positions, then pruned of redundancy.
GeneratedPuzzle generatePuzzle(std::uint32_t seed, int floor) {
  const int n = puzzleSize(GameConfig::chapter(floor));
  seedFor(seed, floor, PuzzleStream);
  GeneratedPuzzle result;
  result.solution.resize(n);
  std::iota(result.solution.begin(), result.solution.end(), 0);
  for (int i = n - 1; i > 0; --i) std::swap(result.solution[i], result.solution[Random::below(i + 1)]);
  const auto &order = result.solution;
  std::vector<Clue> pool;
  for (int a = 0; a < n; ++a)
    for (int k = 0; k < n; ++k) pool.push_back({positionOf(order, a) == k ? 0 : 1, a, k});
  for (int a = 0; a < n; ++a)
    for (int b = a + 1; b < n; ++b) {
      const int pa = positionOf(order, a), pb = positionOf(order, b);
      pool.push_back(pa < pb ? Clue{2, a, b} : Clue{2, b, a});
      pool.push_back({std::abs(pa - pb) == 1 ? 3 : 4, a, b});
    }
  for (int i = static_cast<int>(pool.size()) - 1; i > 0; --i) std::swap(pool[i], pool[Random::below(i + 1)]);
  std::vector<Clue> ordered;
  for (const auto &clue : pool) if (clue.type != 0) ordered.push_back(clue);
  for (const auto &clue : pool) if (clue.type == 0) ordered.push_back(clue);
  const auto all = permutations(n);
  std::vector<std::vector<int>> candidates = all;
  for (const auto &clue : ordered) {
    if (candidates.size() == 1) break;
    std::vector<std::vector<int>> kept;
    for (const auto &candidate : candidates) if (holds(clue, candidate)) kept.push_back(candidate);
    if (kept.size() < candidates.size()) {
      result.clues.push_back(clue);
      candidates = kept;
    }
  }
  for (std::size_t i = 0; i < result.clues.size();) {
    auto without = result.clues;
    without.erase(without.begin() + static_cast<long>(i));
    if (countMatching(without, all) == 1) result.clues = without;
    else ++i;
  }
  return result;
}
std::string clueText(const Clue &clue, const ExplorationText::ChapterText &text) {
  const std::string a = text.puzzleElements[clue.a];
  switch (clue.type) {
  case 0: return a + "是第" + std::to_string(clue.b + 1) + "个";
  case 1: return a + "不是第" + std::to_string(clue.b + 1) + "个";
  case 2: return a + "在" + text.puzzleElements[clue.b] + "之前";
  case 3: return a + "和" + text.puzzleElements[clue.b] + "相邻";
  default: return a + "和" + text.puzzleElements[clue.b] + "不相邻";
  }
}

// ---- Spot texts and options ---------------------------------------------------------
std::string spotName(const Spot &spot, int floor) {
  const auto &text = ExplorationText::chapter(GameConfig::chapter(floor));
  switch (spot.kind) {
  case SpotKind::Cache: return spot.zone == Zone::Hidden ? text.hiddenCache : text.cache;
  case SpotKind::Curio: return text.curios[spot.variant];
  case SpotKind::Gather: return text.gather[spot.variant];
  case SpotKind::AffixSource: return text.affixSource;
  case SpotKind::Key: return "宝库钥匙";
  case SpotKind::Vault: return text.vaults[spot.variant - 1];
  case SpotKind::Merchant: return "流浪商人";
  case SpotKind::QuestItem: return text.questItems[spot.variant];
  case SpotKind::Puzzle: return text.puzzleTitle;
  case SpotKind::Lore: return "见闻录残页";
  }
  return "";
}
std::string spotDescription(const player &hero, const Spot &spot, int floor) {
  const int c = GameConfig::chapter(floor);
  const auto &text = ExplorationText::chapter(c);
  switch (spot.kind) {
  case SpotKind::Cache: return "可能有金币、药水或材料。";
  case SpotKind::Curio: return text.curioDescriptions[spot.variant];
  case SpotKind::Gather:
    return std::string(text.gatherDescriptions[spot.variant]) + "（可采集" +
           ingredientNames[text.gatherIngredient[spot.variant]] + "）";
  case SpotKind::AffixSource: return text.affixSourceDescription;
  case SpotKind::Key: return "能打开任意章节的宝库，钥匙跨章节保留。";
  case SpotKind::Vault: return "上锁的储藏室，需要 " + std::to_string(spot.variant) + " 把钥匙。";
  case SpotKind::Merchant: return "背着大包的商人，只在这一层停留。";
  case SpotKind::QuestItem:
    return std::string(Campaign::chapter(floor).ally) + "遗落的物品（本章已找到 " +
           std::to_string(hero.runState().questItems[c]) + "/3）。";
  case SpotKind::Puzzle: return text.puzzlePrompt;
  case SpotKind::Lore: return "墙缝里夹着一页泛黄的纸。";
  }
  return "";
}
Option makeOption(int id, std::string label, std::string detail, bool available = true,
                  std::string reason = "") {
  Option option;
  option.id = id;
  option.label = std::move(label);
  option.detail = std::move(detail);
  option.available = available;
  option.reason = available ? "" : std::move(reason);
  return option;
}
const std::string stripText = "本层守卫失去最危险的一个词缀（护盾 > 剧毒 > 治疗 > 爆炸）；没有这些词缀时改为攻击 -" +
                              std::to_string(100 - WeakenPercent) + "%";
std::string vaultContents(const player &hero, int floor, int keys) {
  const int c = GameConfig::chapter(floor);
  if (keys == 2)
    return std::string("本职业") + (hero.runState().lootTarget == 2 ? "防具" : "武器") +
           "（保底目标部位，本层最高品质档）与 2 份材料";
  seedFor(hero.runState().seed, floor, VaultStream);
  const int roll = Random::below(100);
  if (roll < VaultMaterialsRoll) return std::to_string(vaultMaterials(c)) + " 份材料";
  if (roll < VaultPotionRoll) return potionText(1) + "与 " + std::to_string(coinUnit(floor) * 2) + " 金币";
  return std::to_string(coinUnit(floor) * 5) + " 金币";
}

Result finish(Result result) {
  if (result.ok && !result.text.empty()) std::cout << result.text << '\n';
  return result;
}
void stripAffix(enemy &monster, const std::string &source, std::vector<std::string> *notes) {
  for (Affix affix : strippable)
    if (monster.hasAffix(affix)) {
      monster.removeAffix(affix);
      if (notes) notes->push_back(source + "：移除【" + affixName(affix) + "】");
      return;
    }
  monster.resetattack(std::max(1, monster.getattack() * WeakenPercent / 100));
  if (notes) notes->push_back(source + "：攻击 -" + std::to_string(100 - WeakenPercent) + "%");
}
} // namespace

namespace Exploration {
ChapterPlan chapterPlan(std::uint32_t seed, int chapter) {
  chapter = std::clamp(chapter, 0, 9);
  seedFor(seed, chapter * 10 + 1, PlanStream);
  const int base = chapter * 10;
  ChapterPlan plan;
  std::vector<int> pool{3, 4, 5, 6, 7, 8, 9};
  // Draw one floor at a time, in this order; each list is then sorted.
  for (int &floor : plan.vaultFloors) floor = base + pick(pool);
  std::sort(plan.vaultFloors.begin(), plan.vaultFloors.end());
  pool = {1, 2, 3, 4, 5, 6, 7, 8};
  for (int &floor : plan.keyFloors) floor = base + pick(pool);
  std::sort(plan.keyFloors.begin(), plan.keyFloors.end());
  pool = {2, 3, 4, 5, 6, 7, 8, 9};
  for (int &floor : plan.merchantFloors) floor = base + pick(pool);
  std::sort(plan.merchantFloors.begin(), plan.merchantFloors.end());
  pool = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (int &floor : plan.questFloors) floor = base + pick(pool);
  std::sort(plan.questFloors.begin(), plan.questFloors.end());
  pool = {3, 4, 5, 6, 7, 8, 9};
  plan.puzzleFloor = base + pick(pool);
  return plan;
}

std::vector<Spot> generate(std::uint32_t seed, int floor) {
  if (!validFloor(floor)) return {};
  const int local = localOf(floor);
  const bool boss = local == 10;
  const auto plan = chapterPlan(seed, GameConfig::chapter(floor));
  seedFor(seed, floor, FloorStream);
  std::vector<Spot> spots;
  const int caches = boss ? 1 : 1 + Random::below(2);
  for (int i = 0; i < caches; ++i)
    spots.push_back({SpotKind::Cache, 0, Random::below(100) < 25 ? Zone::Hidden : Zone::Middle});
  const int curios = boss ? 1 : 1 + Random::below(2);
  for (int i = 0; i < curios; ++i) spots.push_back({SpotKind::Curio, Random::below(3), Zone::Middle});
  if (!boss) {
    // Two separate statements: the order of draws must not depend on the compiler.
    const int first = Random::below(100) < GatherPercent ? 1 : 0;
    const int gathers = first + (Random::below(100) < SecondGatherPercent ? 1 : 0);
    for (int i = 0; i < gathers; ++i) spots.push_back({SpotKind::Gather, Random::below(2), Zone::Middle});
    const int roll = Random::below(100);
    if (local >= 2 && roll < AffixSourcePercent)
      spots.push_back({SpotKind::AffixSource, 0, Zone::Guardian});
  }
  for (int keyFloor : plan.keyFloors)
    if (keyFloor == floor)
      spots.push_back({SpotKind::Key, 0, Random::below(100) < 50 ? Zone::Hidden : Zone::Middle});
  for (int i = 0; i < 2; ++i)
    if (plan.vaultFloors[i] == floor) spots.push_back({SpotKind::Vault, i + 1, Zone::Vault});
  for (int merchantFloor : plan.merchantFloors)
    if (merchantFloor == floor) spots.push_back({SpotKind::Merchant, 0, Zone::Middle});
  for (int i = 0; i < QuestItemsPerChapter; ++i)
    if (plan.questFloors[i] == floor) spots.push_back({SpotKind::QuestItem, i, Zone::Middle});
  if (plan.puzzleFloor == floor) spots.push_back({SpotKind::Puzzle, 0, Zone::Middle});
  spots.push_back({SpotKind::Lore, floor, Zone::Hidden});
  return spots;
}

bool countsTowardCompletion(SpotKind kind) { return kind != SpotKind::Merchant; }

std::vector<SpotView> survey(const player &hero, int floor) {
  std::vector<SpotView> views;
  const auto spots = generate(hero.runState().seed, floor);
  for (std::size_t i = 0; i < spots.size(); ++i) {
    SpotView view;
    view.index = static_cast<int>(i);
    view.spot = spots[i];
    view.done = spotDone(hero.runState(), floor, view.index);
    view.name = spotName(spots[i], floor);
    view.description = spotDescription(hero, spots[i], floor);
    views.push_back(view);
  }
  return views;
}

int remaining(const player &hero, int floor) {
  int count = 0;
  for (const auto &view : survey(hero, floor))
    count += !view.done && countsTowardCompletion(view.spot.kind);
  return count;
}

std::vector<Option> options(const player &hero, int floor, int index) {
  if (!reachable(hero, floor)) return {};
  const auto spots = generate(hero.runState().seed, floor);
  if (index < 0 || index >= static_cast<int>(spots.size())) return {};
  const Spot spot = spots[index];
  const auto &run = hero.runState();
  if (spot.kind != SpotKind::Merchant && spotDone(run, floor, index)) return {};
  const int c = GameConfig::chapter(floor);
  const auto &text = ExplorationText::chapter(c);
  const std::string unit = std::to_string(coinUnit(floor)), unit2 = std::to_string(coinUnit(floor) * 2);
  const std::string expText = std::to_string(expUnit(floor));
  const std::string materials = "（现有 " + std::to_string(run.materials) + "）";
  std::vector<Option> list;
  switch (spot.kind) {
  case SpotKind::Cache:
    list.push_back(makeOption(1, "打开", std::to_string(CacheGold) + "%：" + unit + " 金币；" +
        std::to_string(CachePotion - CacheGold) + "%：1 瓶药水；" + std::to_string(100 - CachePotion) +
        "%：1 份材料与 " + std::to_string(coinUnit(floor) / 2) + " 金币"));
    break;
  case SpotKind::Curio:
    if (spot.variant == 0) {
      list.push_back(makeOption(1, "徒手打开", std::to_string(ContainerGold) + "%：" + unit2 + " 金币；" +
          std::to_string(ContainerPotion - ContainerGold) + "%：1 瓶药水；" + std::to_string(100 - ContainerPotion) +
          "%：陷阱，损失当前生命 " + std::to_string(TrapHealthPercent) + "%（不会致死）"));
      list.push_back(makeOption(2, "用 2 份材料撬开", "必定获得 " + unit2 + " 金币和 1 瓶药水" + materials,
                                run.materials >= 2, "材料不足（需要 2，现有 " + std::to_string(run.materials) + "）"));
    } else if (spot.variant == 1) {
      list.push_back(makeOption(1, "祈祷", std::to_string(ShrineBoon) + "%：祝福（下一场战斗" + boonDescription(1) + "）；" +
                                std::to_string(ShrineGold - ShrineBoon) + "%：" + unit + " 金币；" +
                                std::to_string(100 - ShrineGold) + "%：诅咒（下一场战斗" + baneDescription(1) + "）"));
      const int price = shrinePrice(floor);
      list.push_back(makeOption(2, "供奉 " + std::to_string(price) + " 金币",
                                "必定获得祝福（下一场战斗" + boonDescription(1) + "），并解除已有的诅咒",
                                hero.getcoin() >= price, "金币不足"));
    } else {
      list.push_back(makeOption(1, "触碰", std::to_string(RelicMaterials) + "%：2 份材料；" +
                                std::to_string(RelicExperience - RelicMaterials) + "%：" + expText + " 经验；" +
                                std::to_string(100 - RelicExperience) + "%：反噬，失去当前魔力的 " +
                                std::to_string(BacklashManaPercent) + "%，下一场战斗" + baneDescription(2)));
      const int mana = relicManaCost(hero);
      list.push_back(makeOption(2, "注入 " + std::to_string(mana) + " 魔力（最大魔力的 " + std::to_string(RelicManaPercent) + "%）",
                                "必定获得 2 份材料和 " + expText + " 经验", hero.getmp() >= mana, "魔力不足"));
    }
    break;
  case SpotKind::Gather: {
    const int ingredient = text.gatherIngredient[spot.variant];
    list.push_back(makeOption(1, "采集", std::string("获得 1 份") + ingredientNames[ingredient] + "，" +
                              std::to_string(GatherBonusPercent) + "% 再多 1 份（现有 " +
                              std::to_string(run.ingredients[ingredient]) + "，上限 99）",
                              run.ingredients[ingredient] < MaxIngredients, "这种食材已经带满了"));
    break;
  }
  case SpotKind::AffixSource:
    list.push_back(makeOption(1, "强行破坏", "损失当前生命 " + std::to_string(SourceHealthPercent) + "%（不会致死）；" + stripText));
    list.push_back(makeOption(2, "小心拆除（" + std::to_string(SourceMaterials) + " 份材料）", stripText + materials,
                              run.materials >= SourceMaterials, "材料不足（需要 " + std::to_string(SourceMaterials) +
                              "，现有 " + std::to_string(run.materials) + "）"));
    break;
  case SpotKind::Key:
    list.push_back(makeOption(1, "拾取", "钥匙 +1（现有 " + std::to_string(run.keys) + "，上限 99）",
                              run.keys < MaxKeys, "钥匙已经带满了"));
    break;
  case SpotKind::Vault:
    list.push_back(makeOption(1, "用 " + std::to_string(spot.variant) + " 把钥匙打开",
                              "里面是：" + vaultContents(hero, floor, spot.variant) + "（现有钥匙 " +
                              std::to_string(run.keys) + "）", run.keys >= spot.variant,
                              "钥匙不足（需要 " + std::to_string(spot.variant) + "，现有 " + std::to_string(run.keys) + "）"));
    break;
  case SpotKind::Merchant: {
    const auto goods = merchantGoods(hero, floor);
    for (std::size_t i = 0; i < goods.size(); ++i) {
      const auto &good = goods[i];
      const bool full = good.kind == 1 && run.keys >= MaxKeys;
      list.push_back(makeOption(static_cast<int>(i) + 1, "购买" + good.name + "（" + std::to_string(good.price) + " 金币）",
                                good.description, !good.sold && !full && hero.getcoin() >= good.price,
                                good.sold ? "已售出" : (full ? "钥匙已经带满了" : "金币不足")));
    }
    break;
  }
  case SpotKind::QuestItem:
    list.push_back(makeOption(1, "收好", std::string(Campaign::chapter(floor).ally) + "的遗物（本章 " +
                              std::to_string(run.questItems[c]) + "/3）；集齐 3 件获得 2 份材料，本章 Boss 战开局生命 -" +
                              std::to_string(100 - AmbushHealthPercent) + "%"));
    break;
  case SpotKind::Puzzle:
    list.push_back(makeOption(1, "解谜", "按线索排出唯一正确的顺序；答错损失当前生命 " + std::to_string(PuzzleHealthPercent) +
                              "%（不会致死），可以重试。奖励：1 把钥匙、" +
                              std::to_string(puzzleMaterials(c)) + " 份材料、" + expText + " 经验"));
    break;
  case SpotKind::Lore:
    list.push_back(makeOption(1, "阅读", "收入见闻录（远征档案中查看，跨远征保留）"));
    break;
  }
  return list;
}

Result interact(player &hero, int floor, int index, int option) {
  Result result;
  if (!reachable(hero, floor)) return result;
  const std::uint32_t seed = hero.runState().seed;
  const auto spots = generate(seed, floor);
  if (index < 0 || index >= static_cast<int>(spots.size())) return result;
  const Spot spot = spots[index];
  auto &run = hero.runState();
  if (spot.kind != SpotKind::Merchant && spotDone(run, floor, index)) return result;
  const auto available = options(hero, floor, index);
  const auto chosen = std::find_if(available.begin(), available.end(), [&](const Option &o) { return o.id == option; });
  if (chosen == available.end() || !chosen->available || spot.kind == SpotKind::Puzzle) return result;
  const int c = GameConfig::chapter(floor);
  const auto &text = ExplorationText::chapter(c);
  const std::string name = spotName(spot, floor);
  seedFor(seed, floor, SpotStream + static_cast<unsigned>(index));
  std::string outcome;
  int experience = 0; // Granted after the text, so level-up messages follow it.
  switch (spot.kind) {
  case SpotKind::Cache: {
    const int roll = Random::below(100);
    if (roll < CacheGold) {
      hero.gaincoin(coinUnit(floor));
      outcome = "获得 " + std::to_string(coinUnit(floor)) + " 金币";
    } else if (roll < CachePotion) {
      givePotions(hero, floor, 1);
      outcome = "获得 1 瓶药水";
    } else {
      const int gained = addMaterials(hero, 1);
      hero.gaincoin(coinUnit(floor) / 2);
      outcome = "获得 " + std::to_string(gained) + " 份材料与 " + std::to_string(coinUnit(floor) / 2) + " 金币";
    }
    break;
  }
  case SpotKind::Curio: {
    if (spot.variant == 0) {
      if (option == 2) {
        run.materials -= 2;
        hero.gaincoin(coinUnit(floor) * 2);
        givePotions(hero, floor, 1);
        outcome = "用材料撬开：获得 " + std::to_string(coinUnit(floor) * 2) + " 金币和 1 瓶药水";
      } else {
        const int roll = Random::below(100);
        if (roll < ContainerGold) {
          hero.gaincoin(coinUnit(floor) * 2);
          outcome = "获得 " + std::to_string(coinUnit(floor) * 2) + " 金币";
        } else if (roll < ContainerPotion) {
          givePotions(hero, floor, 1);
          outcome = "获得 1 瓶药水";
        } else {
          loseHealthSafely(hero, TrapHealthPercent);
          outcome = "触发了陷阱，损失当前生命的 " + std::to_string(TrapHealthPercent) + "%";
        }
      }
    } else if (spot.variant == 1) {
      if (option == 2) {
        hero.costcoin(shrinePrice(floor));
        run.boon = 1;
        run.bane = 0;
        outcome = "供奉后获得祝福：" + boonDescription(1) + "（下一场战斗）";
      } else {
        const int roll = Random::below(100);
        if (roll < ShrineBoon) {
          run.boon = 1;
          outcome = "获得祝福：" + boonDescription(1) + "（下一场战斗）";
        } else if (roll < ShrineGold) {
          hero.gaincoin(coinUnit(floor));
          outcome = "神龛前散落着供品：获得 " + std::to_string(coinUnit(floor)) + " 金币";
        } else {
          run.bane = 1;
          outcome = "受到诅咒：" + baneDescription(1) + "（下一场战斗）";
        }
      }
    } else {
      const int exp = expUnit(floor);
      if (option == 2) {
        hero.setmp(hero.getmp() - relicManaCost(hero));
        const int gained = addMaterials(hero, 2);
        outcome = "注入魔力后遗物平静下来：获得 " + std::to_string(gained) + " 份材料和 " + std::to_string(exp) + " 经验";
        experience = exp;
      } else {
        const int roll = Random::below(100);
        if (roll < RelicMaterials) {
          outcome = "获得 " + std::to_string(addMaterials(hero, 2)) + " 份材料";
        } else if (roll < RelicExperience) {
          outcome = "遗物里封着前人的记忆：获得 " + std::to_string(exp) + " 经验";
          experience = exp;
        } else {
          hero.setmp(hero.getmp() - hero.getmp() * BacklashManaPercent / 100);
          run.bane = 2;
          outcome = "遗物反噬：失去当前魔力的 " + std::to_string(BacklashManaPercent) + "%，" + baneDescription(2) + "（下一场战斗）";
        }
      }
    }
    break;
  }
  case SpotKind::Gather: {
    const int ingredient = text.gatherIngredient[spot.variant];
    const int amount = 1 + (Random::below(100) < GatherBonusPercent ? 1 : 0);
    const int gained = addIngredient(run, ingredient, amount);
    outcome = "采集到 " + std::to_string(gained) + " 份" + ingredientNames[ingredient];
    break;
  }
  case SpotKind::AffixSource:
    if (option == 2) run.materials -= SourceMaterials;
    else loseHealthSafely(hero, SourceHealthPercent);
    outcome = std::string(option == 2 ? "小心拆除了" : "强行破坏了") + name + "：本层守卫会失去最危险的词缀（没有时攻击 -" +
              std::to_string(100 - WeakenPercent) + "%）";
    break;
  case SpotKind::Key:
    addKeys(run, 1);
    outcome = "拾得宝库钥匙（现有 " + std::to_string(run.keys) + "）";
    break;
  case SpotKind::Vault: {
    run.keys -= spot.variant;
    run.vaultsOpened = GameConfig::addCounter(run.vaultsOpened, 1);
    if (spot.variant == 2) {
      seedFor(seed, floor, VaultGearStream);
      std::cout << "打开" << name << "：\n";
      Campaign::grantItem(hero, generateEliteReward(hero, floor));
      outcome = "另获得 " + std::to_string(addMaterials(hero, 2)) + " 份材料";
    } else {
      seedFor(seed, floor, VaultStream);
      const int roll = Random::below(100);
      if (roll < VaultMaterialsRoll) {
        outcome = "打开" + name + "：获得 " + std::to_string(addMaterials(hero, vaultMaterials(c))) + " 份材料";
      } else if (roll < VaultPotionRoll) {
        givePotions(hero, floor, 1);
        hero.gaincoin(coinUnit(floor) * 2);
        outcome = "打开" + name + "：获得 1 瓶药水与 " + std::to_string(coinUnit(floor) * 2) + " 金币";
      } else {
        hero.gaincoin(coinUnit(floor) * 5);
        outcome = "打开" + name + "：获得 " + std::to_string(coinUnit(floor) * 5) + " 金币";
      }
    }
    break;
  }
  case SpotKind::Merchant: {
    const auto goods = merchantGoods(hero, floor);
    const Good good = goods[option - 1];
    hero.costcoin(good.price);
    run.explored[floor] |= 1 << (MerchantBit + option - 1);
    markExplored(run, floor, index);
    if (good.kind == 0) {
      seedFor(seed, floor, BlindBoxStream + static_cast<unsigned>(option));
      const int roll = Random::below(100);
      if (roll < BoxRefund) {
        hero.gaincoin(good.price);
        outcome = "盲盒里是一袋金币：" + std::to_string(good.price);
      } else if (roll < BoxPotion) {
        givePotions(hero, floor, 1);
        outcome = "盲盒里是 1 瓶药水";
      } else if (roll < BoxMaterials) {
        outcome = "盲盒里是 " + std::to_string(addMaterials(hero, 2)) + " 份材料";
      } else if (roll < BoxIngredients) {
        const int a = Random::below(IngredientCount);
        const int b = Random::below(IngredientCount);
        addIngredient(run, a, 1);
        addIngredient(run, b, 1);
        outcome = std::string("盲盒里是") + ingredientNames[a] + "和" + ingredientNames[b];
      } else if (addKeys(run, 1)) {
        outcome = "盲盒里是一把宝库钥匙！";
      } else {
        hero.gaincoin(good.price);
        outcome = "盲盒里是一把钥匙，但钥匙已经带满，商人退回了 " + std::to_string(good.price) + " 金币";
      }
    } else if (good.kind == 1) {
      addKeys(run, 1);
      outcome = "买到宝库钥匙（现有 " + std::to_string(run.keys) + "）";
    } else if (good.kind == 2) {
      run.boon = 2;
      outcome = "戴上护符：" + boonDescription(2) + "（下一场战斗）";
    } else {
      const int a = text.gatherIngredient[0], b = text.gatherIngredient[1];
      addIngredient(run, a, 1);
      addIngredient(run, b, 1);
      outcome = std::string("买到") + ingredientNames[a] + "和" + ingredientNames[b] + "各 1 份";
    }
    result.ok = true;
    result.text = outcome;
    return finish(result);
  }
  case SpotKind::QuestItem: {
    run.questItems[c] = std::min(QuestItemsPerChapter, run.questItems[c] + 1);
    const std::string ally = Campaign::chapter(floor).ally;
    outcome = "收好" + name + "（" + ally + "的遗物 " + std::to_string(run.questItems[c]) + "/3）";
    if (run.questItems[c] == QuestItemsPerChapter)
      outcome += "。集齐了！获得 " + std::to_string(addMaterials(hero, 2)) + " 份材料；第" +
                 std::to_string((c + 1) * 10) + "层 Boss 战开局，" + ally + "会先手偷袭（Boss 生命 -" +
                 std::to_string(100 - AmbushHealthPercent) + "%）";
    break;
  }
  case SpotKind::Lore: {
    const auto &page = ExplorationText::lore(floor);
    result.lorePage = floor;
    outcome = std::string("【见闻录 ") + std::to_string(floor) + "/100 · " + page.title + "】" + page.text;
    break;
  }
  case SpotKind::Puzzle:
    return result;
  }
  markExplored(run, floor, index);
  result.ok = true;
  result.text = outcome;
  finish(result);
  hero.gainexp(experience);
  return result;
}

std::vector<Good> merchantGoods(const player &hero, int floor) {
  const auto spots = generate(hero.runState().seed, floor);
  if (std::none_of(spots.begin(), spots.end(), [](const Spot &s) { return s.kind == SpotKind::Merchant; }))
    return {};
  seedFor(hero.runState().seed, floor, MerchantStream);
  std::vector<int> kinds{0, 1, 2, 3};
  kinds.erase(kinds.begin() + Random::below(4));
  const auto &text = ExplorationText::chapter(GameConfig::chapter(floor));
  std::vector<Good> goods;
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    Good good;
    good.kind = kinds[i];
    good.price = merchantPrice(hero, floor, good.kind);
    good.sold = (hero.runState().explored[floor] >> (MerchantBit + static_cast<int>(i))) & 1;
    switch (good.kind) {
    case 0:
      good.name = "神秘盲盒";
      good.description = std::to_string(BoxRefund) + "%：退回等额金币；" + std::to_string(BoxPotion - BoxRefund) +
                         "%：1 瓶药水；" + std::to_string(BoxMaterials - BoxPotion) + "%：2 份材料；" +
                         std::to_string(BoxIngredients - BoxMaterials) + "%：两份随机食材；" +
                         std::to_string(100 - BoxIngredients) + "%：1 把宝库钥匙";
      break;
    case 1:
      good.name = "宝库钥匙";
      good.description = "钥匙 +1，跨章节保留";
      break;
    case 2:
      good.name = "守护护符";
      good.description = "祝福：" + boonDescription(2) + "（下一场战斗；替换已有的祝福）";
      break;
    default:
      good.name = "特产食材";
      good.description = std::string(ingredientNames[text.gatherIngredient[0]]) + "和" +
                         ingredientNames[text.gatherIngredient[1]] + "各 1 份";
      break;
    }
    goods.push_back(good);
  }
  return goods;
}

bool hasPuzzle(const player &hero, int floor) {
  const auto spots = generate(hero.runState().seed, floor);
  return std::any_of(spots.begin(), spots.end(), [](const Spot &s) { return s.kind == SpotKind::Puzzle; });
}

Puzzle puzzle(const player &hero, int floor) {
  Puzzle result;
  if (!hasPuzzle(hero, floor)) return result;
  const auto &text = ExplorationText::chapter(GameConfig::chapter(floor));
  const auto generated = generatePuzzle(hero.runState().seed, floor);
  result.title = text.puzzleTitle;
  result.prompt = text.puzzlePrompt;
  for (std::size_t i = 0; i < generated.solution.size(); ++i) result.elements.push_back(text.puzzleElements[i]);
  for (const auto &clue : generated.clues) result.clues.push_back(clueText(clue, text));
  return result;
}

std::vector<int> puzzleSolution(std::uint32_t seed, int floor) {
  if (!validFloor(floor)) return {};
  return generatePuzzle(seed, floor).solution;
}

Result solvePuzzle(player &hero, int floor, const std::vector<int> &order) {
  Result result;
  if (!reachable(hero, floor)) return result;
  const auto spots = generate(hero.runState().seed, floor);
  int index = -1;
  for (std::size_t i = 0; i < spots.size(); ++i) if (spots[i].kind == SpotKind::Puzzle) index = static_cast<int>(i);
  auto &run = hero.runState();
  if (index < 0 || spotDone(run, floor, index)) return result;
  const auto solution = puzzleSolution(run.seed, floor);
  std::vector<int> sorted = order;
  std::sort(sorted.begin(), sorted.end());
  std::vector<int> expected(solution.size());
  std::iota(expected.begin(), expected.end(), 0);
  if (sorted != expected) return result; // Not an order of every element: nothing happens.
  result.ok = true;
  if (order != solution) {
    loseHealthSafely(hero, PuzzleHealthPercent);
    result.text = "顺序不对，机关反震：损失当前生命的 " + std::to_string(PuzzleHealthPercent) + "%。线索都是真的，再想想。";
    return finish(result);
  }
  const int c = GameConfig::chapter(floor);
  markExplored(run, floor, index);
  run.puzzlesSolved = GameConfig::addCounter(run.puzzlesSolved, 1);
  addKeys(run, 1);
  const int materials = addMaterials(hero, puzzleMaterials(c));
  const int exp = expUnit(floor);
  result.text = std::string("解开了") + ExplorationText::chapter(c).puzzleTitle + "！获得 1 把宝库钥匙、" +
                std::to_string(materials) + " 份材料和 " + std::to_string(exp) + " 经验";
  finish(result);
  hero.gainexp(exp);
  return result;
}

const std::vector<Recipe> &recipes() {
  static const std::vector<Recipe> list = {
      {1, "烤鱼串", "祝福：" + boonDescription(1), {2, 0, 0, 0, 0}},
      {2, "药草汤", "祝福：" + boonDescription(2), {0, 2, 0, 0, 0}},
      {3, "岩盐烤饼", "祝福：" + boonDescription(1), {0, 0, 2, 0, 0}},
      {4, "霜晶冰沙", "祝福：" + boonDescription(3), {0, 0, 0, 2, 0}},
      {5, "星尘糖", "祝福：" + boonDescription(4), {0, 0, 0, 0, 2}},
      {6, "远征杂烩", "任意两种不同食材各 1 份（用最多的两种）；祝福：" + boonDescription(2), {0, 0, 0, 0, 0}},
  };
  return list;
}

const char *ingredientName(int ingredient) {
  return ingredientNames[std::clamp(ingredient, 0, IngredientCount - 1)];
}

namespace {
// The stew's two ingredients: the two most plentiful kinds, lower id on ties.
std::array<int, 2> stewIngredients(const RunState &run) {
  std::array<int, 2> best{-1, -1};
  for (int i = 0; i < IngredientCount; ++i) {
    if (run.ingredients[i] <= 0) continue;
    if (best[0] < 0 || run.ingredients[i] > run.ingredients[best[0]]) {
      best[1] = best[0];
      best[0] = i;
    } else if (best[1] < 0 || run.ingredients[i] > run.ingredients[best[1]]) {
      best[1] = i;
    }
  }
  return best;
}
} // namespace

bool canCook(const player &hero, int recipe) {
  if (!hero.isalive() || recipe < 1 || recipe > RecipeCount) return false;
  const auto &run = hero.runState();
  if (recipe == 6) return stewIngredients(run)[1] >= 0;
  const auto &needs = recipes()[recipe - 1].needs;
  for (int i = 0; i < IngredientCount; ++i)
    if (run.ingredients[i] < needs[i]) return false;
  return true;
}

Result cook(player &hero, int recipe) {
  Result result;
  if (!canCook(hero, recipe)) return result;
  auto &run = hero.runState();
  if (recipe == 6) {
    for (int ingredient : stewIngredients(run)) --run.ingredients[ingredient];
  } else {
    const auto &needs = recipes()[recipe - 1].needs;
    for (int i = 0; i < IngredientCount; ++i) run.ingredients[i] -= needs[i];
  }
  run.mealsCooked = GameConfig::addCounter(run.mealsCooked, 1);
  // Meals only bless the next fight. Healing and MP stay with potions and rests:
  // a full MP bar before a boss is worth more than any blessing here.
  const int boons[RecipeCount] = {1, 2, 1, 3, 4, 2};
  run.boon = boons[recipe - 1];
  result.ok = true;
  result.text = "做好了" + recipes()[recipe - 1].name + "：祝福 " + boonDescription(run.boon) + "（下一场战斗）";
  return finish(result);
}

std::string boonDescription(int boon) {
  switch (boon) {
  case 1: return "敌人攻击 -" + std::to_string(100 - BoonAttackPercent) + "%";
  case 2: return "敌人生命 -" + std::to_string(100 - BoonHealthPercent) + "%";
  case 3: return "敌人闪避 -" + std::to_string(BoonDodgePoints) + " 个百分点";
  case 4: return "敌人失去最危险的一个词缀（没有时攻击 -" + std::to_string(100 - WeakenPercent) + "%）";
  default: return "无";
  }
}

std::string baneDescription(int bane) {
  switch (bane) {
  case 1: return "敌人攻击 +" + std::to_string(BaneAttackPercent - 100) + "%";
  case 2: return "敌人生命 +" + std::to_string(BaneHealthPercent - 100) + "%";
  default: return "无";
  }
}

void applyToEncounter(const player &hero, int floor, enemy &monster, std::vector<std::string> *notes) {
  if (!validFloor(floor)) return;
  const auto &run = hero.runState();
  const int c = GameConfig::chapter(floor);
  // Without exploration nothing below draws random numbers or changes the enemy.
  if (run.explored[floor] & 0xffff) {
    const auto spots = generate(run.seed, floor);
    for (std::size_t i = 0; i < spots.size(); ++i)
      if (spots[i].kind == SpotKind::AffixSource && ((run.explored[floor] >> i) & 1))
        stripAffix(monster, ExplorationText::chapter(c).affixSource, notes);
  }
  switch (run.boon) {
  case 1:
    monster.resetattack(std::max(1, monster.getattack() * BoonAttackPercent / 100));
    if (notes) notes->push_back("祝福：" + boonDescription(1));
    break;
  case 2:
    monster.resethp(std::max(1, monster.getmaxhp() * BoonHealthPercent / 100));
    if (notes) notes->push_back("祝福：" + boonDescription(2));
    break;
  case 3:
    monster.setdodgeRate(std::max(0, monster.getdodgeRate() - BoonDodgePoints));
    if (notes) notes->push_back("祝福：" + boonDescription(3));
    break;
  case 4:
    stripAffix(monster, "祝福", notes);
    break;
  default:
    break;
  }
  if (run.bane == 1) {
    monster.resetattack(std::max(1, monster.getattack() * BaneAttackPercent / 100));
    if (notes) notes->push_back("诅咒：" + baneDescription(1));
  } else if (run.bane == 2) {
    monster.resethp(std::max(1, monster.getmaxhp() * BaneHealthPercent / 100));
    if (notes) notes->push_back("诅咒：" + baneDescription(2));
  }
  if (localOf(floor) == 10 && run.questItems[c] >= QuestItemsPerChapter) {
    monster.resethp(std::max(1, monster.getmaxhp() * AmbushHealthPercent / 100));
    if (notes) notes->push_back(std::string(Campaign::chapter(floor).ally) + "的偷袭：生命 -" +
                                std::to_string(100 - AmbushHealthPercent) + "%");
  }
}

void finishEncounter(player &hero) {
  hero.runState().boon = 0;
  hero.runState().bane = 0;
}

std::string loreTitle(int page) { return ExplorationText::lore(page).title; }
std::string loreText(int page) { return ExplorationText::lore(page).text; }

std::string chapterSummary(const player &hero, int floor) {
  if (!validFloor(floor)) return "";
  const auto &run = hero.runState();
  const int c = GameConfig::chapter(floor);
  const auto plan = chapterPlan(run.seed, c);
  std::string text = "探索：钥匙 " + std::to_string(run.keys) + "；宝库 第" + std::to_string(plan.vaultFloors[0]) +
                     "层（1把）、第" + std::to_string(plan.vaultFloors[1]) + "层（2把）；谜题 第" +
                     std::to_string(plan.puzzleFloor) + "层；商人 第" + std::to_string(plan.merchantFloors[0]) +
                     "、" + std::to_string(plan.merchantFloors[1]) + "层；" + Campaign::chapter(floor).ally +
                     "的遗物 " + std::to_string(run.questItems[c]) + "/3\n食材：";
  for (int i = 0; i < IngredientCount; ++i)
    text += std::string(i ? " " : "") + ingredientNames[i] + std::to_string(run.ingredients[i]);
  text += "；下一场战斗：祝福 " + boonDescription(run.boon) + "，诅咒 " + baneDescription(run.bane);
  return text;
}
} // namespace Exploration
