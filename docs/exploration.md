# 楼层探索：规则层与网页接入说明

规则层（C++ `src/game/exploration.*` 与 JS `web/js/rules/exploration*.js`）已经完成，两边逐字节一致，由 `tools/web_test.sh` 保证。终端版已经接好（主菜单 `[13]`、营地 `[11] 料理`、远征档案 `[5] 见闻录`），可以当作交互流程的参考实现，见 `src/main.cpp` 的 `explore()`、`puzzleMenu()`、`cooking()`。

网页版的画面和交互也已接入（README 第九轮）：`web/js/engine/mapgen.js` 的 `placeSpots()` 按 zone 放置地点、凿出壁龛和宝库侧室，`web/js/scenes/explore.js` 绘制和交互，`web/js/ui/menus.js` 的 `openSpot()`、`openPuzzle()`、`cookingView()`、`loreView()` 是各个界面，`web/js/game.js` 的 `explore()`、`solvePuzzle()`、`cook()` 负责写入见闻录、检查成就和自动存档。下面是规则层的接口说明，改界面时可以对照。规则层不知道任何坐标，地点放在哪里完全由地图决定。

## 一层有什么

```js
import * as Exploration from './rules/exploration.js';

const views = Exploration.survey(hero, floor);
// [{ index, spot: { kind, variant, zone }, done, name, description }, ...]
```

- `index`：传给 `options()` / `interact()` 的编号，从 0 开始。顺序固定，不要自己排序后当作编号使用。
- `spot.kind`：`Exploration.SpotKind` 中的一种（见下表）。
- `spot.zone`：建议的位置，见下一节。
- `done`：已经探索过。商人在第一次购买后为 `true`，但仍然可以继续购买剩下的货品。
- 结果只由种子和楼层决定。生成地图时可以放心调用多次，不会影响之后战斗的随机序列（每场战斗开始时会重新设定种子）。但这些函数本身会重设规则随机数的种子，所以**不要在战斗进行中调用**（例如战斗里弹窗时刷新的 HUD），网页版的 HUD 只读取场景缓存的剩余数量。

| kind | 名称示例（第 1 章） | 交互 |
|---|---|---|
| `Cache` | 散落的补给包 / 草丛里的藏匿包 | 1 个选项，免费 |
| `Curio` | 旧补给箱 / 路边神龛 / 锈蚀警钟（`variant` 0/1/2） | 2 个选项：碰运气、付费换确定结果 |
| `Gather` | 野莓灌木 / 林间溪流 | 1 个选项，得到食材 |
| `AffixSource` | 警戒烽火 | 2 个选项：扣血或用材料，削弱本层守卫 |
| `Key` | 宝库钥匙 | 1 个选项 |
| `Vault` | 哨站储藏室（`variant` 1）/ 前哨军械库（`variant` 2） | 1 个选项，需要 `variant` 把钥匙；选项说明里写着宝库内容 |
| `Merchant` | 流浪商人 | 3 个选项，对应 3 件货品（选项 id = 1..3） |
| `QuestItem` | 断裂的望远镜等（`variant` 0..2） | 1 个选项 |
| `Puzzle` | 烽火台点火顺序 | 不走 `interact()`，见「谜题」 |
| `Lore` | 见闻录残页 | 1 个选项，需要写入档案，见「见闻录」 |
| `FogSource` | 瘴气泉眼 | 岔路层：1 个选项，免费；封住后毒雾远路不再扣血 |
| `Vantage` | 哨塔残基 | 岔路层：1 个选项，免费；之后 `fork()` 给出巡逻和储藏的确切内容 |
| `Shortcut` | 长满裂纹的土墙（`variant` 0）/ 吊桥绞盘（`variant` 1） | 岔路层，约一半：1 个选项；打通后走哪条路都能拿远路的补给 |

`Cache` 的 `variant` 1 是毒雾远路上的「遗落的药箱」，必出 1 瓶药水。

每层 5–15 个地点（普通楼层平均约 6 个），Boss 层固定 3 个：补给、珍品、见闻录。

## 地图放置（zone）

| zone | 含义 | 建议 |
|---|---|---|
| `Zone.Middle` | 中间房间 | 均匀分散到入口营地和守卫房间之间的各个房间，别挤在一起 |
| `Zone.Guardian` | 守卫房间 | 只有词缀源头使用，放在守卫身边，让玩家一眼看出它和守卫有关 |
| `Zone.Hidden` | 隐秘处 | 墙角凹室、墙上的裂缝。现在的 `mapgen.js` 已经有装饰凹室，可以把它们用作隐秘处；做成走近才显形或有细微闪光会更有发现感 |
| `Zone.Vault` | 上锁的侧室 | 需要地图新增一间带门的小侧室，门就是交互点；打开后可以画成门开着的样子 |
| `Zone.Branch` | 某条岔路上（`spot.branch`） | 目前只有毒雾远路（`Branch.Fog`）有地点：本层露天的补给包、采集点和药箱 |

岔路层上，`Zone.Guardian` 的地点（词缀源头、捷径）在岔路之后，其余地点都必须在选路之前就能走到。

地图布局请继续使用 `VisualRng`（以种子和楼层为输入）决定坐标，**不要**调用 `rules/random.js`，否则会和 C++ 版失去一致。

## 交互流程

```js
const choices = Exploration.options(hero, floor, view.index);
// [{ id, label, detail, available, reason }]
//   label：按钮文字；detail：代价、概率和结果（都已写好，直接显示即可）
//   available=false 时把按钮置灰，并显示 reason
// 返回空数组表示：已探索、楼层没到过、角色已倒下，或编号无效

const { result, events } = game.act(() => Exploration.interact(hero, floor, view.index, choice.id));
// result: { ok, text, lorePage }
//   ok=false 表示什么都没有发生（选项不可用等），可以提示 reason
//   events 中 type === 'explore' 的事件带有 text 和 { floor, index, kind, lorePage }
//   获得物品时还会有 'loot' 事件，升级时会有 'levelup' 等，和战斗结算一样处理即可
```

注意 `game.act()` 返回的 `ok` 等于 `Boolean(result)`，对探索结果永远是 `true`，所以要用 `result.ok` 判断成功与否。成功后建议调用 `game.autosave()`，并刷新 HUD（钥匙数、材料、金币可能变了）。

### 见闻录

`result.lorePage`（1..100）不为 0 时，把这页写入跨局档案：

```js
if (result.lorePage && game.profile.recordLore(result.lorePage)) {
  game.saveProfile();
  game.checkAchievements();
}
```

远征档案可以新增「见闻录」页：按章节列出 `game.profile.loreFound(page)`，已收集的显示 `Exploration.loreTitle(page)` 和 `Exploration.loreText(page)`，没收集的显示「？？？（第 N 层）」。终端版的效果见 `Profile::printLoreChapter`。

### 谜题

```js
const p = Exploration.puzzle(hero, floor);
// { title, prompt, elements: ['东烽火', '西烽火', '南烽火'], clues: ['西烽火在东烽火之前', ...] }
const result = Exploration.solvePuzzle(hero, floor, order); // order：元素编号，从第一个到最后一个
```

- 适合把元素画成地图上的物件（烽火台、符文、书架、阀门、镜子……），按点击顺序记录，点满一轮就调用 `solvePuzzle`。线索可以刻在石碑上，点击石碑查看。
- 元素数量：第 1–3 章 3 个，第 4–6 章 4 个，第 7–10 章 5 个。
- 答错：`result.ok === true`，文字提示顺序不对，并扣除当前生命 5%（不会致死），可以重试。答对后该地点 `done`，奖励 1 把钥匙、材料和经验。
- `order` 不是完整排列时（长度不对、有重复），返回 `ok=false` 且不扣血，可以用来做「取消」。

### 商人

`options()` 返回 3 个选项，id 1..3 对应 `Exploration.merchantGoods(hero, floor)` 里的 3 件货品（`{ kind, name, description, price, sold }`）。已售出的选项 `available=false`，`reason` 为「已售出」。

### 料理（营火）

```js
Exploration.recipes();            // [{ id, name, needs: [5 种食材的数量], description }]
Exploration.canCook(hero, id);
game.act(() => Exploration.cook(hero, id));  // result.ok / result.text
Exploration.ingredientName(i);    // 鲜鱼、草药、矿砂、霜晶、星尘
```

料理只会设置**下一场战斗**的祝福（`hero.runState().boon`），新祝福会替换旧的。`远征杂烩`（id 6）用数量最多的两种不同食材各 1 份。

## 岔路

每章 3 层是岔路层（`Exploration.isForkFloor(seed, floor)`，`chapterPlan().forkFloors`）。`Exploration.branches(seed, floor)` 按「近路、远路、钥匙」的顺序给出这一层的路，毒雾远路一定在其中：

```js
const fork = Exploration.fork(hero, floor);
// { branches: [{ kind, name, cost, reward, available, reason }], taken, hint }
//   kind：Exploration.Branch.Patrol / Fog / Locked；cost、reward 已写好，直接显示
//   taken：已经走的路（Branch.None 表示还没选）；hint：一行提示
Exploration.guardianReachable(hero, floor); // 岔路层没选路时为 false：先选路才能挑战守卫
```

- **远路、钥匙小径**：`game.act(() => Exploration.takeBranch(hero, floor, kind))`，返回值与 `interact()` 相同（`result.ok`、`result.text`）。远路在没封住源头时扣当前生命 20%（不会致死），钥匙小径扣 1 把钥匙并立即给出储藏。
- **巡逻近路**：打一场战斗。`Campaign.makePatrol(hero, floor)` 生成敌人，`Campaign.patrolRules(hero, floor)` 是战斗规则，开战前 `Campaign.seedStream(hero, floor, Campaign.PatrolBattleStream)`，结束后 `Campaign.settlePatrol(hero, monster, battle, floor)`：赢了记为走了近路并发放经验、金币和 1 份材料；输了什么都不记，可以重试或改走别的路。巡逻不吃也不消耗祝福和诅咒。
- 选了一条路，其他路就关闭（`fork().branches` 里 `available=false`，`reason` 说明原因）。远路上的地点只有走了远路或打通捷径后才能探索；其他时候 `survey()` 的 `blocked` 写着原因，`options()` 的选项全部不可选。走了远路之后，捷径不再计入剩余地点。
- 记录在 `run.explored[floor]` 的第 19–20 位（`Exploration.BranchBit`），存档格式不变。

网页地图（`mapgen.js`）把每条路画成一条一格宽的路，`layout.lanes[]` 给出每条路的 `branch`、两端 `entry`/`exit`（关卡所在的格子）、两端前面的格子 `entryFront`/`exitFront`、路上所有格子 `tiles`，以及毒雾覆盖的格子 `fogTiles`；有捷径时 `exitSpot` 是捷径的地点编号，捷径就画在远路的出口上。大厅里的路牌位置是 `layout.forkSign`。

## 战斗前后

- 预览和开战时可以拿到探索带来的所有变化：

  ```js
  const notes = [];
  const monster = Campaign.makeEnemy(hero, floor, notes);
  // notes: ['警戒烽火：移除【护盾】', '祝福：敌人攻击 -8%', '斥候的偷袭：生命 -8%', ...]
  ```

  网页版在每场战斗的战前弹窗里列出这些变化（诅咒标红），`game.startBattle()` 也把 `notes` 传给战斗场景，开战日志里显示为【探索】。
- 祝福和诅咒在 `Campaign.settleEncounter` 里清除，不需要额外处理。
- HUD 建议显示：钥匙数（`run.keys`），以及当前的祝福/诅咒（`Exploration.boonDescription(run.boon)`、`baneDescription(run.bane)`，值为 0 时不显示）。

## 其他可用数据

- `Campaign.journal(hero, floor)` 新增字段：`exploration`（与终端版相同的两行文字）、`explorePlan`（`{ keyFloors, vaultFloors, merchantFloors, questFloors, puzzleFloor }`）、`keys`、`ingredients`、`boon`、`bane`、`questItems`。
- `Exploration.remaining(hero, floor)`：本层还没探索的地点数（不含商人），可以用在楼层卡片上，例如「探索 3/7」。
- 快速前往（`travelTargets()`）可以把未探索的地点加进列表。
- 帮助页 `HelpTopics` 已经新增「楼层探索」。
- 存档已升级为 JSON version 2，`game.save()` / `game.load()` 不需要改动；version 1 的存档读取后没有探索进度。

## 改数值

所有概率、代价和效果都集中在两个文件开头的常量里：`src/game/exploration.cpp` 与 `web/js/rules/exploration.js`，两边必须同步修改。文本表在 `exploration_text.cpp/js`。敌人整体强度在 `EnemyTuning::BaseAttackPercent/BaseHpPercent`（`src/game/world.h` 和 `web/js/rules/world.js`）。

修改后运行：

```sh
bash tools/test.sh        # C++ 全部测试与模拟门槛
bash tools/web_test.sh    # C++/JS 一致性；指纹变化时按失败信息更新两边的常量
./build-local/campaign_sim --report             # 节奏报告（含探索统计）
./build-local/campaign_sim --report --no-explore  # 对照：不探索
```
