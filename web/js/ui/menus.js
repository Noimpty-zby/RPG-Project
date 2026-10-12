// Every menu outside battle: character, camp, shop, waystone, events,
// encounters, exploration spots, archive, help, revive and route choice.
import * as Campaign from '../rules/campaign.js';
import * as Exploration from '../rules/exploration.js';
import { Skill, SpecializationBranches } from '../rules/skills.js';
import { Item, ItemType } from '../rules/item.js';
import { affixDescription, affixName, AffixCount, Bestiary } from '../rules/enemy.js';
import { Objective } from '../rules/combat.js';
import { chapterOf, challengeName, EnhancePercent, InheritPercent, MaxEnhanceLevel, refreshCost, skillRankCap } from '../rules/config.js';
import { Achievements, LorePages, Profile } from '../rules/profile.js';
import { HelpTopics } from '../rules/help.js';
import { h, button, openModal, choose, alertBox, toast } from './dom.js';
import { compareTable, isUpgrade, itemRow, statsBlock } from './widgets.js';
import * as Sprites from '../engine/sprites.js';
import { P } from '../engine/palette.js';
import { TILE } from '../engine/tiles.js';
import { sfx, audioSettings, setAudio } from '../engine/audio.js';

const RouteIcons = ['', '⚒', '✚', '⚗'];
const { SpotKind } = Exploration;
const ZoneNames = ['房间', '守卫房间', '隐秘处', '上锁的侧室'];

export class Menus {
  constructor(app) {
    this.app = app;
    // Backpack filters: item type (0 all), minimum tier, upgrades only.
    this.bagFilter = { type: 0, tier: 0, upgrades: false };
  }
  get game() { return this.app.game; }
  get hero() { return this.app.game.hero; }

  // Show events (loot, notices) collected from a rules call as toasts.
  report(events, okText = null) {
    for (const e of events) {
      if (e.type === 'loot') toast(`获得：${e.item.getDisplayName()}${e.upgrade ? '（提升）' : ''}`);
      else if (['notice', 'materials', 'levelup', 'skillRank'].includes(e.type) && e.text) toast(e.text);
    }
    if (okText) toast(okText);
    this.app.hud.refresh();
  }

  // ---- Arrival: revive, route choice ------------------------------------------------
  async onArrive(scene) {
    const game = this.game;
    if (!game.hero.isalive()) {
      const alive = await this.reviveFlow();
      if (!alive) return;
      scene.refreshEntities();
    }
    if (game.needsRoute()) await this.chooseRoute(scene);
  }

  async chooseRoute(scene) {
    const game = this.game;
    const chapter = Campaign.chapter(game.floor);
    const routes = [1, 2, 3];
    let picked = null;
    while (!picked) {
      picked = await new Promise((resolve) => {
        let value = null;
        const modal = openModal({
          title: `第${game.chapter + 1}章 · ${chapter.name}`, closable: false,
          render: () => h('div', {},
            h('p', {}, h('span', { class: 'gold' }, '任务：'), chapter.mission),
            h('p', { class: 'muted' }, '选择本章路线（章内固定，不能更改）：'),
            h('div', { class: 'cards' }, routes.map((route) => h('button', {
              class: 'card', onclick: () => { sfx('click'); value = route; modal.close(); },
            }, h('h4', {}, `${RouteIcons[route]} ${Campaign.RouteNames[route]}`), h('p', {}, Campaign.routeDescription(route).replace(/^[^：]+：/, ''))))),
          ),
          buttons: routes.map((route) => ({ label: Campaign.RouteNames[route], key: String(route), onClick: (m) => { value = route; m.close(); } })),
          onClose: () => resolve(value),
        });
      });
    }
    game.chooseRoute(picked);
    toast(`已选择 ${Campaign.RouteNames[picked]}`);
    scene.refreshEntities();
    this.app.hud.refresh();
  }

  async reviveFlow() {
    const game = this.game;
    const options = game.reviveOptions();
    if (!options) {
      await alertBox('远征结束', h('div', {}, h('p', {}, game.run.challenge === 2 ? '无复苏挑战结束。' : '角色已倒下。'),
        h('p', { class: 'muted' }, '本次远征已记入档案（远征档案 → 远征记录）。')), '返回标题');
      game.endRun();
      this.app.showTitle();
      return false;
    }
    for (;;) {
      const choice = await choose('角色已倒下', h('div', {},
        h('p', {}, '复苏后回到本层入口。路线与首通记录保留，可以调整专精与装备后重试。'),
        h('p', { class: 'muted' }, `金币 ${options.coins}`)), [
        { label: `免费复苏（剩余 ${options.free}）`, value: 'free', cls: 'primary', disabled: options.free <= 0 },
        { label: `花费 ${options.paid} 金币复苏`, value: 'paid', disabled: options.coins <= 0 },
        { label: '结束远征', value: 'end', cls: 'danger' },
      ], { closable: false });
      if (choice === 'end') {
        game.endRun();
        await alertBox('远征结束', '本次远征已记入档案。', '返回标题');
        this.app.showTitle();
        return false;
      }
      const { ok, events } = game.revive(choice === 'free');
      if (ok) {
        sfx('heal');
        this.report(events);
        return true;
      }
      sfx('error');
    }
  }

  // ---- Main character menu ---------------------------------------------------------
  openMain(tab = 0) {
    const game = this.game;
    const tabs = ['状态', '背包', '装备', '技能', '章节日志', '远征档案', '帮助', '系统'];
    openModal({
      title: `${this.hero.getname()} · ${this.hero.getJobName()}`, size: 'wide', tabs, initialTab: tab,
      render: (m) => {
        switch (m.tab()) {
          case 0: return statsBlock(game);
          case 1: return this.backpackView(m);
          case 2: return this.equipmentView(m);
          case 3: return this.skillsView(m);
          case 4: return this.journalView();
          case 5: return this.archiveView(m);
          case 6: return this.helpView(m);
          default: return this.systemView(m);
        }
      },
    });
  }

  backpackView(m) {
    const hero = this.hero;
    const items = hero.getSaveBackpack();
    const f = this.bagFilter;
    const shown = items.map((item, i) => ({ item, index: i + 1 })).filter(({ item }) =>
      (!f.type || item.getType() === f.type) &&
      (item.getType() === ItemType.Potion || item.getTier() >= f.tier) &&
      (!f.upgrades || isUpgrade(hero, item)));
    const rows = shown.map(({ item, index }) => {
      const actions = [];
      if (item.getType() === ItemType.Potion) {
        actions.push(button('使用', () => {
          const before = hero.gethp();
          const { ok } = this.game.act(() => hero.useItem(index));
          if (!ok) { sfx('error'); toast('无法使用：生命已满。'); } else {
            sfx('heal');
            const restored = hero.gethp() - before;
            const overflow = item.getValue() - restored;
            toast(`恢复 ${restored} 生命${overflow > 0 ? `（溢出 ${overflow}）` : ''}，当前 ${hero.gethp()}/${hero.getmaxhp()}。`);
          }
          m.refresh();
        }, { cls: 'small', disabled: hero.gethp() >= hero.getmaxhp() }));
      } else {
        actions.push(button('装备', () => {
          const { ok } = this.game.act(() => hero.equipItem(index));
          if (!ok) { sfx('error'); toast('该物品当前不可装备。'); }
          m.refresh();
        }, { cls: 'small', disabled: !hero.canEquip(item) }));
        actions.push(button('比较', () => openModal({ title: '装备比较', render: () => compareTable(hero, item) }), { cls: 'small' }));
      }
      actions.push(button(item.isLocked() ? '解锁' : '锁定', () => { hero.toggleItemLock(index); m.refresh(); }, { cls: 'small' }));
      actions.push(button('卖出', async () => {
        let quantity = 1;
        if (item.getQuantity() > 1) {
          quantity = await this.askQuantity(item);
          if (!quantity) return;
        }
        const earned = hero.sellItem(index, quantity);
        if (earned) { sfx('coin'); toast(`卖出 ${item.getName()} ×${quantity}，获得 ${earned} 金币。`); }
        m.refresh();
      }, { cls: 'small', disabled: item.isLocked() }));
      return itemRow(item, { hero, actions, sellPrice: true, compareUpgrade: isUpgrade(hero, item) });
    });
    const pick = (label, active, onClick) => button(label, () => { onClick(); m.refresh(); }, { cls: `small ${active ? 'primary' : ''}` });
    const filters = h('div', { class: 'filters' },
      h('div', { class: 'group' }, h('span', { class: 'muted' }, '类型'),
        [[0, '全部'], [ItemType.Weapon, '武器'], [ItemType.Armor, '防具'], [ItemType.Potion, '药水']]
          .map(([type, label]) => pick(label, f.type === type, () => { f.type = type; }))),
      h('div', { class: 'group' }, h('span', { class: 'muted' }, '品质'),
        [[0, '全部'], [1, '精良+'], [2, '稀有+'], [3, '史诗']].map(([tier, label]) => pick(label, f.tier === tier, () => { f.tier = tier; }))),
      pick('只看提升', f.upgrades, () => { f.upgrades = !f.upgrades; }),
      h('span', { class: 'muted' }, `显示 ${shown.length}/${items.length}`));
    return h('div', {},
      h('p', { class: 'muted' }, `金币 ${hero.getcoin()} · 材料 ${hero.runState().materials} · 锁定的物品不会被卖出或拆解。`),
      this.potionBar(m),
      filters,
      rows.length ? h('div', { class: 'grid-list' }, rows) : h('p', {}, items.length ? '没有符合筛选条件的物品。' : '背包空空如也！'),
      h('div', { style: { marginTop: '10px' } }, button('批量拆解未锁定装备', async () => {
        const ok = await choose('批量拆解', '拆解背包中所有未锁定装备换取材料（保留药水及已穿装备）？',
          [{ label: '确认拆解', value: true, cls: 'danger' }, { label: '取消', value: false }]);
        if (ok) { const gained = hero.salvageUnlocked(); toast(`获得材料：${gained}`); m.refresh(); }
      }, { cls: 'small' })));
  }
  // Drink potions up to a share of max HP, after confirming the plan.
  potionBar(m) {
    const hero = this.hero;
    if (!hero.getSaveBackpack().some((item) => item.getType() === ItemType.Potion)) return null;
    const max = hero.getmaxhp();
    return h('div', { class: 'filters' },
      h('span', { class: 'muted' }, `生命 ${hero.gethp()}/${max} · 批量喝药`),
      [[50, '补到50%'], [80, '补到80%'], [100, '补满']].map(([pct, label]) => {
        const target = Math.floor(max * pct / 100);
        return button(label, () => this.drinkTo(m, target), { cls: 'small good', disabled: hero.gethp() >= target });
      }));
  }
  async drinkTo(m, target) {
    const hero = this.hero;
    const plan = hero.planPotions(target);
    if (!plan.uses.length) { sfx('error'); toast('没有可用的药水。'); return; }
    const items = hero.getSaveBackpack();
    const after = hero.gethp() + plan.restored;
    const ok = await choose('批量喝药', h('div', {},
      h('p', {}, '将使用：', plan.uses.map(({ slot, count }) => `${items[slot - 1].getName()}（${items[slot - 1].getValue()}）×${count}`).join('、')),
      h('p', {}, `恢复 ${plan.restored}${plan.overflow ? `，溢出 ${plan.overflow}` : '，无溢出'}；之后 HP ${after}/${hero.getmaxhp()}` +
        `${after < target ? '（药水不足，达不到目标）' : ''}。`)),
    [{ label: '确认', value: true, cls: 'primary' }, { label: '取消', value: false }]);
    if (!ok) return;
    const { result } = this.game.act(() => hero.drinkPotionsTo(target));
    sfx('heal');
    toast(`共恢复 ${result} 生命，当前 ${hero.gethp()}/${hero.getmaxhp()}。`);
    m.refresh();
  }
  askQuantity(item) {
    return new Promise((resolve) => {
      const input = h('input', { type: 'number', min: 1, max: item.getQuantity(), value: 1 });
      let value = 0;
      const modal = openModal({
        title: `卖出 ${item.getName()}`, size: 'narrow',
        render: () => h('div', {}, h('p', {}, `数量（1-${item.getQuantity()}，单价 ${Math.trunc(item.getPrice() / 3)}）`), input),
        buttons: [{ label: '卖出', cls: 'primary', key: 'Enter', keyLabel: '⏎', onClick: (m) => {
          const n = Math.trunc(Number(input.value));
          if (n >= 1 && n <= item.getQuantity()) { value = n; m.close(); } else { sfx('error'); toast('数量无效。'); }
        } }],
        onClose: () => resolve(value),
      });
      setTimeout(() => input.focus(), 50);
    });
  }

  equipmentView(m) {
    const hero = this.hero;
    const equipped = hero.getSaveEquipments();
    return h('div', {},
      equipped.length ? h('div', { class: 'grid-list' }, equipped.map((item, i) => itemRow(item, {
        hero, equipped: true, compare: false,
        actions: [button('卸下', () => { hero.unequipItem(i + 1); m.refresh(); }, { cls: 'small' })],
      }))) : h('p', {}, '没有装备任何物品！'),
      h('p', { class: 'muted', style: { marginTop: '10px' } }, '武器与防具各一件。每场有效战斗耐久 -1，归零即损坏；营火处可修理、强化与改造。'));
  }

  skillsView(m) {
    const hero = this.hero;
    const rows = hero.skills.map((skill, i) => {
      const slot = i + 1;
      const unlocked = skill.isUnlocked(hero);
      const canUp = skill.canUpgrade(hero.getlevel(), hero.getHighestFloor());
      const actions = [skill.getskilllevel() >= 5 ? h('span', { class: 'gold' }, '已满阶') : button(`升阶 ${skill.getlevelUpCost()}金`, () => {
        const { ok, events } = this.game.act(() => hero.levelUpSkill(slot));
        if (!ok) { sfx('error'); toast('未升级：检查金币、等级需求、楼层解锁和阶数上限。'); } else { sfx('levelup'); this.report(events); }
        m.refresh();
      }, { cls: 'small', disabled: !canUp || hero.getcoin() < skill.getlevelUpCost() })];
      const branches = [];
      if (slot <= 2) {
        const floor = Skill.specializationFloor(slot);
        const options = SpecializationBranches[hero.job()][slot - 1];
        const open = hero.getHighestFloor() >= floor && hero.getlevel() >= skill.getlevelrequired();
        const cost = skill.getSpecialization() === 0 ? 0 : hero.respecCost();
        branches.push(h('div', { class: 'choice-row', style: { marginTop: '6px' } },
          [0, 1, 2].map((branch) => button(branch === 0 ? '基础' : options[branch - 1].split('：')[0], async () => {
            const { ok } = this.game.act(() => hero.chooseSpecialization(slot, branch));
            if (!ok) { sfx('error'); toast('未更改：检查解锁、金币和当前分支。'); } else toast('专精已更改。');
            m.refresh();
          }, { cls: `small ${skill.getSpecialization() === branch ? 'primary' : ''}`, disabled: !open || skill.getSpecialization() === branch,
            title: branch ? options[branch - 1] : '基础形态' }))),
        h('div', { class: 'meta' }, open
          ? `专精（${options.join('；')}）。${cost ? `重选费用 ${cost} 金币` : '初选免费'}。`
          : `第${floor}层开放专精。`));
      }
      return h('div', { class: 'row-item' },
        h('div', { class: 'icon', style: { display: 'grid', placeItems: 'center', fontSize: '18px', color: unlocked ? '#feae34' : '#5a6988' } }, String(slot)),
        h('div', {},
          h('div', { class: 'title' }, `${skill.getskillname()} `, h('span', { class: 'muted' }, `${skill.getskilllevel()}阶 · ${skill.getSpecializationName(hero)}`),
            unlocked ? null : h('span', { class: 'tag elite', style: { marginLeft: '6px' } }, `第${skill.getunlockfloor()}层 / Lv.${skill.getlevelrequired()} 解锁`)),
          h('div', { class: 'meta' }, `MP ${skill.getActualCost(hero)} · 冷却 ${skill.getcurrentcooldown()}/${skill.getcooldown()} · ${skill.getPreview(hero)}`),
          unlocked ? h('div', { class: 'meta upgrade' }, skill.getUpgradePreview(hero)) : null,
          branches),
        h('div', { class: 'actions' }, actions));
    });
    return h('div', {},
      h('p', { class: 'muted' }, `金币 ${hero.getcoin()} · 当前阶数上限 ${skillRankCap(hero.getHighestFloor())}（第20/40/65/85层提升）`),
      h('div', { class: 'grid-list' }, rows));
  }

  journalView() {
    const j = this.game.journal();
    return h('div', {},
      h('h3', {}, `第${j.chapter + 1}章：${j.name}`),
      h('p', {}, j.mission),
      h('p', {}, h('span', { class: 'tag route' }, Campaign.RouteNames[j.route]), j.routeText),
      h('p', {}, `章节进度 ${j.progress}/9 · 订单 ${j.orderClaimed ? '已领取' : `未领取（需完成 ${j.orderNeed} 层）`} · 材料 ${j.materials}`),
      h('p', {}, `装备保底目标 ${j.lootTarget === 1 ? '武器' : '防具'} · 保底进度 ${j.pity}/${j.pityNeed}`),
      h('p', {}, '本章遭遇：', j.goals.map((g) => h('span', { class: `tag ${g.label === '精英' ? 'elite' : g.label === 'Boss' ? 'boss' : 'goal'}` },
        `${g.floor}层 ${g.label}`))),
      h('p', {}, j.eventText),
      h('h3', {}, '本章悬赏'),
      h('p', { class: 'muted' }, `可选目标，在本章楼层获胜的战斗都计入（重玩也算），达成各奖励 ${Campaign.BountyMaterials} 材料与 1 瓶药水。`),
      h('ul', { class: 'lines' }, j.bounties.map((b) => h('li', { class: b.progress >= b.target ? 'good' : '' },
        `${b.name}：${b.goal}（${b.progress >= b.target ? '已完成' : `${b.progress}/${b.target}`}）`))),
      j.letter ? h('p', { class: 'cyan' }, j.letter) : null,
      this.explorationJournal(j),
      h('h3', {}, '远征统计'),
      h('p', { class: 'muted' }, `种子 ${j.seed} · 挑战 ${j.challenge} · 战斗 ${j.battles} · 回合 ${j.turns} · 阵亡 ${j.deaths} · 药水 ${j.potions} · 装备提升 ${j.usefulDrops} · 最长装备空窗 ${j.longestGearDrought} 层`));
  }
  // The chapter's exploration plan and what is left on the floors reached.
  explorationJournal(j) {
    const hero = this.hero;
    const plan = j.explorePlan;
    const first = j.chapter * 10 + 1;
    const last = Math.min(hero.getHighestFloor(), j.chapter * 10 + 10);
    const floors = [];
    for (let f = first; f <= last; f++) {
      const left = Exploration.remaining(hero, f);
      floors.push(h('span', { class: `tag ${left ? 'goal' : 'done'}`, title: left ? `还有 ${left} 处未探索` : '已探索完' }, `${f}层 ${left || '✓'}`));
    }
    return h('div', {},
      h('h3', {}, '本章探索'),
      h('p', {}, `钥匙 ${j.keys} · ${Campaign.chapter(this.game.floor).ally}的遗物 ${j.questItems}/3（集齐后本章 Boss 生命 -8%）`),
      h('p', {}, `宝库 第${plan.vaultFloors[0]}层（1 把钥匙）、第${plan.vaultFloors[1]}层（2 把） · 谜题 第${plan.puzzleFloor}层 · ` +
        `流浪商人 第${plan.merchantFloors[0]}、${plan.merchantFloors[1]}层`),
      h('p', {}, '剩余地点：', floors),
      h('p', {}, '食材：', Exploration.IngredientNames.map((name, i) => `${name} ${j.ingredients[i]}`).join(' · ')),
      h('p', { class: j.bane ? 'bad' : 'muted' }, `下一场战斗：祝福 ${Exploration.boonDescription(j.boon)}；诅咒 ${Exploration.baneDescription(j.bane)}`));
  }
  openJournal() {
    openModal({ title: `第 ${this.game.floor} 层告示`, render: () => this.journalView() });
  }

  archiveView(m) {
    const profile = this.game.profile;
    const section = this.archiveTab ?? 0;
    const tabs = h('div', { class: 'choice-row', style: { marginBottom: '10px' } },
      ['敌人图鉴', '词缀图鉴', '成就', '远征记录', `见闻录 ${profile.loreCount()}/${LorePages}`].map((label, i) => button(label, () => { this.archiveTab = i; m.refresh(); },
        { cls: `small ${section === i ? 'primary' : ''}` })));
    let content;
    if (section === 0) {
      const chapters = [];
      for (let c = 0; c < 10; c++) {
        const cards = [];
        for (let slot = 0; slot < 5; slot++) {
          const id = c * 5 + slot;
          const record = profile.entry(id);
          const known = record.seen > 0;
          const sprite = Sprites.monster(id).idle;
          const img = h('img', { src: Sprites.spriteUrl(`m${id}`, sprite), width: 64, height: 64, class: 'px',
            style: { filter: known ? 'none' : 'brightness(0) opacity(.5)', imageRendering: 'pixelated' }, alt: '' });
          cards.push(h('div', { class: 'card', style: { cursor: 'default' } }, img,
            h('h4', {}, `${Bestiary.entryIsBoss(id) ? '★ ' : ''}${known ? Bestiary.entryName(id) : '？？？'}`),
            known ? h('p', {}, `首遇第${record.firstFloor}层 · 遭遇 ${record.seen} · 战胜 ${record.won}`) : h('p', {}, '尚未遭遇'),
            known ? h('p', {}, Bestiary.entryDescription(id)) : null));
        }
        const found = [0, 1, 2, 3, 4].filter((s) => profile.entry(c * 5 + s).seen > 0).length;
        chapters.push(h('h3', {}, `第${c + 1}章 ${Campaign.Chapters[c].name}（${found}/5）`), h('div', { class: 'cards' }, cards));
      }
      content = h('div', {}, h('p', { class: 'muted' }, `已发现 ${profile.discovered()}/${Bestiary.EntryCount}`), chapters);
    } else if (section === 1) {
      const rows = [];
      for (let id = 0; id < AffixCount; id++)
        rows.push(h('li', {}, profile.affixSeen(id) ? [h('span', { class: 'bad' }, `[${affixName(id)}] `), `遭遇 ${profile.affixSeen(id)} 次：${affixDescription(id)}`] : '？？？（尚未遭遇）'));
      content = h('div', {}, h('p', { class: 'muted' }, `已发现 ${profile.affixesDiscovered()}/${AffixCount}`), h('ul', { class: 'lines' }, rows));
    } else if (section === 2) {
      content = h('div', {}, h('p', { class: 'muted' }, `成就 ${profile.unlockedCount()}/${Achievements.length}`),
        h('div', { class: 'cards' }, Achievements.map((a) => h('div', { class: `card ${profile.unlocked(a.key) ? 'active' : ''}`, style: { cursor: 'default' } },
          h('h4', {}, `${profile.unlocked(a.key) ? '★' : '☆'} ${a.title}`), h('p', {}, a.description)))));
    } else if (section === 4) {
      content = this.loreView(m);
    } else {
      const records = [...profile.records()].reverse();
      content = h('div', {},
        records.length ? h('ul', { class: 'lines' }, records.map((r) => h('li', {}, Profile.recordSummary(r))))
          : h('p', {}, '还没有完成的远征。通关或阵亡结束远征后会记录在这里。'),
        h('p', { class: 'muted' }, '各职业最佳：', profile.bestByJob().map((b) => `${b.job} ${b.text}`).join('　')));
    }
    return h('div', {}, this.game.profileWarning ? h('p', { class: 'bad' }, this.game.profileWarning) : null, tabs, content);
  }
  // Expedition journal pages, one chapter at a time; kept across runs.
  loreView(m) {
    const profile = this.game.profile;
    const chapter = this.loreChapter ?? 0;
    const found = (c) => Array.from({ length: 10 }, (_, i) => profile.loreFound(c * 10 + i + 1)).filter(Boolean).length;
    const pages = [];
    for (let page = chapter * 10 + 1; page <= chapter * 10 + 10; page++) {
      pages.push(profile.loreFound(page)
        ? h('li', {}, h('span', { class: 'gold' }, `第${page}页 · ${Exploration.loreTitle(page)}`), h('div', {}, Exploration.loreText(page)))
        : h('li', { class: 'muted' }, `第${page}页：？？？（藏在第${page}层的隐秘处）`));
    }
    return h('div', {},
      h('p', { class: 'muted' }, `已收集 ${profile.loreCount()}/${LorePages}。每层一页，藏在墙缝后的隐秘处；收集跨远征保留。`),
      h('div', { class: 'choice-row', style: { marginBottom: '10px' } }, Campaign.Chapters.map((c, i) =>
        button(`${i + 1} ${c.name} ${found(i)}/10`, () => { this.loreChapter = i; m.refresh(); }, { cls: `small ${chapter === i ? 'primary' : ''}` }))),
      h('ul', { class: 'lines lore' }, pages));
  }
  openArchive() {
    openModal({ title: '远征档案', size: 'wide', render: (m) => this.archiveView(m) });
  }

  helpView(m) {
    const topic = this.helpTopic ?? 0;
    return h('div', {},
      h('div', { class: 'choice-row', style: { marginBottom: '10px' } },
        HelpTopics.map((t, i) => button(t.title, () => { this.helpTopic = i; m.refresh(); }, { cls: `small ${topic === i ? 'primary' : ''}` }))),
      h('h3', {}, HelpTopics[topic].title),
      h('ul', { class: 'lines' }, HelpTopics[topic].lines().map((line) => h('li', {}, line))));
  }
  openHelp() {
    openModal({ title: '帮助', size: 'wide', render: (m) => this.helpView(m) });
  }

  systemView(m) {
    const game = this.game;
    const audio = audioSettings();
    const manual = game.slotInfo('manual');
    return h('div', {},
      h('h3', {}, '存档'),
      h('p', { class: 'muted' }, manual ? `手动存档：${manual.name} · ${manual.job} Lv.${manual.level} · 第${manual.floor}层 · ${new Date(manual.savedAt).toLocaleString()}` : '还没有手动存档。'),
      h('div', { class: 'choice-row' },
        button('手动存档', () => {
          if (!game.hero.isalive()) { sfx('error'); toast('角色倒下时不能存档。'); return; }
          if (game.save('manual')) { sfx('coin'); toast('已保存到手动存档。'); } else toast('保存失败：浏览器存储不可用。');
          m.refresh();
        }, { cls: 'primary' }),
        button('读取手动存档', async () => {
          if (!manual) return;
          const ok = await choose('读取存档', '当前未保存的进度会丢失（自动存档也会被覆盖）。', [{ label: '读取', value: true, cls: 'danger' }, { label: '取消', value: false }]);
          if (ok) { m.close(); this.app.loadSlot('manual'); }
        }, { disabled: !manual })),
      h('p', { class: 'muted', style: { fontSize: '12px' } }, '每场战斗结算和换层时会自动写入「自动存档」。存档保存在本浏览器中。'),
      h('h3', {}, '声音'),
      h('div', { class: 'choice-row' },
        button(audio.muted ? '取消静音' : '静音', () => { setAudio({ muted: !audio.muted }); m.refresh(); }, { cls: 'small' }),
        button(`音效 ${Math.round(audio.sfx * 100)}%`, () => { setAudio({ sfx: audio.sfx >= 1 ? 0 : Math.round((audio.sfx + 0.2) * 10) / 10 }); m.refresh(); }, { cls: 'small' }),
        button(`音乐 ${Math.round(audio.music * 100)}%`, () => { setAudio({ music: audio.music >= 1 ? 0 : Math.round((audio.music + 0.2) * 10) / 10 }); m.refresh(); }, { cls: 'small' })),
      h('h3', {}, '其他'),
      h('div', { class: 'choice-row' },
        button('返回标题', async () => {
          const ok = await choose('返回标题', '未手动保存的进度可从「继续（自动存档）」恢复。', [{ label: '返回标题', value: true }, { label: '取消', value: false }]);
          if (ok) { m.close(); this.app.showTitle(); }
        }, { cls: 'small' })),
      h('p', { class: 'muted', style: { fontSize: '12px', marginTop: '10px' } }, `种子 ${game.run.seed} · ${challengeName(game.run.challenge)}模式`));
  }

  // The next hard fight, MP, rests and cooldowns; red when this floor's fight is at risk.
  readinessView() {
    const { summary, warning } = Campaign.readiness(this.hero, this.game.floor);
    return h('div', { class: `readiness ${warning ? 'risky' : ''}` },
      h('p', {}, summary), warning ? h('p', { class: 'bad' }, warning) : null);
  }

  // ---- Quick travel ----------------------------------------------------------------------
  openTravel(scene) {
    const spots = scene.travelTargets();
    openModal({
      title: '前往', size: 'narrow',
      render: () => h('p', { class: 'muted' }, '自动走到目的地并与之交互；按方向键可随时停下。'),
      buttons: spots.map((spot, i) => ({ label: spot.label, key: String(i + 1), cls: spot.primary ? 'primary' : '',
        onClick: (m) => { m.close(); scene.travelTo(spot.entity); } })),
    });
  }

  // ---- Camp ------------------------------------------------------------------------------
  openCamp(scene) {
    const game = this.game;
    const hero = this.hero;
    openModal({
      title: '营火', size: 'wide', tabs: ['休整', '强化', '修理', '改造', '料理', '技能与专精'],
      render: (m) => {
        const floor = game.floor;
        if (m.tab() === 0) {
          const run = game.run;
          const rests = Campaign.restsRemaining(hero, floor);
          const price = Campaign.restPrice(hero, floor);
          const percent = run.challenge === 1 ? 25 : 40;
          const j = game.journal();
          return h('div', {},
            this.readinessView(),
            h('p', {}, `HP ${hero.gethp()}/${hero.getmaxhp()} · MP ${hero.getmp()}/${hero.getmaxmp()} · 金币 `, h('span', { class: 'gold' }, hero.getcoin())),
            h('div', { class: 'grid-list' },
              h('div', { class: 'row-item' }, h('div', { class: 'icon', style: { display: 'grid', placeItems: 'center', color: '#f77622' } }, '火'),
                h('div', {}, h('div', { class: 'title' }, '休息'), h('div', { class: 'meta' }, `恢复基础HP的${percent}%与最大MP的50%；本章剩余 ${rests} 次，本次 ${price} 金币（价格逐次增加）`)),
                h('div', { class: 'actions' }, button('休息', () => {
                  const { ok } = game.rest();
                  if (!ok) {
                    sfx('error');
                    toast(rests === 0 ? '本章休息次数已用完。' : hero.gethp() === hero.getmaxhp() && hero.getmp() === hero.getmaxmp() ? '生命和魔力都是满的。' : `金币不足（需要 ${price}）。`);
                  } else { sfx('heal'); scene?.fx.burst(scene.player.x + 8, scene.player.y, { color: ['#63c74d', '#ffffff'], count: 16, speed: 30, gravity: -40 }); }
                  m.refresh();
                }, { cls: 'small good', disabled: rests === 0 }))),
              h('div', { class: 'row-item' }, h('div', { class: 'icon', style: { display: 'grid', placeItems: 'center', color: '#feae34' } }, '令'),
                h('div', {}, h('div', { class: 'title' }, '章节订单'),
                  h('div', { class: 'meta' }, j.orderClaimed ? '本章订单已领取。' : `完成本章 ${j.orderNeed} 个非Boss楼层后领取本职业${j.lootTarget === 1 ? '武器' : '防具'}（当前 ${j.progress}）。`)),
                h('div', { class: 'actions' }, button('领取', () => {
                  const { ok, events } = game.claimOrder();
                  if (!ok) { sfx('error'); toast('订单尚未达标或已领取。'); } else { sfx('chest'); this.report(events); }
                  m.refresh();
                }, { cls: 'small', disabled: j.orderClaimed || j.progress < j.orderNeed || !j.route }))),
              h('div', { class: 'row-item' }, h('div', { class: 'icon', style: { display: 'grid', placeItems: 'center', color: '#2ce8f5' } }, '标'),
                h('div', {}, h('div', { class: 'title' }, '装备保底目标'),
                  h('div', { class: 'meta' }, `当前 ${run.lootTarget === 1 ? '武器' : '防具'}，进度 ${run.pity}/${run.lootTarget === 1 ? 6 : 8}。更换目标会清空保底计数。`)),
                h('div', { class: 'actions' },
                  button('武器', () => { game.setLootTarget(1); m.refresh(); }, { cls: `small ${run.lootTarget === 1 ? 'primary' : ''}` }),
                  button('防具', () => { game.setLootTarget(2); m.refresh(); }, { cls: `small ${run.lootTarget === 2 ? 'primary' : ''}` })))),
          );
        }
        if (m.tab() === 1) {
          const equipped = hero.getSaveEquipments();
          return h('div', {},
            h('p', { class: 'muted' }, `金币 ${hero.getcoin()} · 每级数值 +${EnhancePercent}%，最高 +${MaxEnhanceLevel}；强化保留在装备上。`),
            equipped.length ? h('div', { class: 'grid-list' }, equipped.map((item, i) => {
              const cost = hero.enhanceCost(i + 1);
              return itemRow(item, { hero, equipped: true, compare: false, actions: [cost
                ? button(`→ ${item.getValue() + hero.enhanceGain(i + 1)}（${cost}金）`, () => {
                  const { ok } = game.act(() => hero.enhanceEquipment(i + 1));
                  if (!ok) { sfx('error'); toast('金币不足。'); } else { sfx('levelup'); toast(`${item.getDisplayName()} 强化成功！`); }
                  m.refresh();
                }, { cls: 'small primary', disabled: hero.getcoin() < cost })
                : h('span', { class: 'muted' }, '已满级')] });
            })) : h('p', {}, '没有装备任何物品！'),
            this.inheritView(m));
        }
        if (m.tab() === 2) {
          const equipped = hero.getSaveEquipments();
          return h('div', {},
            h('p', { class: 'muted' }, `金币 ${hero.getcoin()} · 修到耐久上限，品质越高越贵。`),
            equipped.length ? h('div', { class: 'grid-list' }, equipped.map((item, i) => {
              const cost = hero.repairCost(i + 1);
              return itemRow(item, { hero, equipped: true, compare: false, actions: [cost
                ? button(`修理（${cost}金）`, () => {
                  const { ok } = game.act(() => hero.repairEquipment(i + 1));
                  if (!ok) { sfx('error'); toast('金币不足。'); } else { sfx('guard'); toast('修理完成。'); }
                  m.refresh();
                }, { cls: 'small', disabled: hero.getcoin() < cost })
                : h('span', { class: 'muted' }, '无需修理')] });
            })) : h('p', {}, '没有装备任何物品！'));
        }
        if (m.tab() === 3) {
          const equipped = hero.getSaveEquipments();
          return h('div', {},
            h('p', { class: 'muted' }, `材料 ${game.run.materials} · 每件装备一个效果槽；改造消耗 3 + 品质序号×2 材料。`),
            equipped.length ? h('div', { class: 'grid-list' }, equipped.map((item, i) => itemRow(item, {
              hero, equipped: true, compare: false,
              actions: [1, 2, 3, 4].map((effect) => button(Item.effectName(effect), () => {
                const { ok } = game.act(() => hero.modifyEquipment(i + 1, effect));
                if (!ok) { sfx('error'); toast('改造未执行：材料不足、效果相同或该部位不支持此效果。'); } else { sfx('chest'); toast('改造完成。'); }
                m.refresh();
              }, { cls: `small ${item.getEffect() === effect ? 'primary' : ''}`, title: Item.effectDescription(effect),
                disabled: !item.supportsEffect(effect) || item.getEffect() === effect || game.run.materials < hero.modifyCost(i + 1) })),
            }))) : h('p', {}, '没有装备任何物品！'),
            h('ul', { class: 'lines muted', style: { fontSize: '13px', marginTop: '10px' } }, [1, 2, 3, 4].map((effect) => h('li', {}, Item.effectDescription(effect)))));
        }
        if (m.tab() === 4) return this.cookingView(m);
        return this.skillsView(m);
      },
    });
  }

  // Camp cooking: ingredients from gathering spots become a blessing for the next fight.
  cookingView(m) {
    const run = this.game.run;
    return h('div', {},
      h('p', {}, '食材：', Exploration.IngredientNames.map((name, i) =>
        h('span', { class: run.ingredients[i] ? '' : 'muted', style: { marginRight: '12px' } }, `${name} ${run.ingredients[i]}`))),
      h('p', { class: 'muted' }, `当前祝福：${Exploration.boonDescription(run.boon)}${run.bane ? `；诅咒：${Exploration.baneDescription(run.bane)}` : ''}。` +
        '料理的祝福只作用于下一场战斗，新祝福会替换旧的；食材来自地图上的采集点和流浪商人。'),
      h('div', { class: 'grid-list' }, Exploration.recipes().map((recipe) => {
        const needs = recipe.needs.map((n, i) => (n ? `${Exploration.ingredientName(i)}×${n}` : '')).filter(Boolean).join('、');
        return h('div', { class: 'row-item option-row' },
          h('div', {}, h('div', { class: 'title' }, recipe.name, needs ? h('span', { class: 'muted' }, `（${needs}）`) : null),
            h('div', { class: 'meta' }, recipe.description)),
          h('div', { class: 'actions' }, button('烹饪', () => {
            const result = this.game.cook(recipe.id);
            if (!result.ok) { sfx('error'); toast('食材不足。'); } else { sfx('heal'); toast(result.text); }
            m.refresh();
          }, { cls: 'small primary', disabled: !Exploration.canCook(this.hero, recipe.id) })));
      })));
  }

  // Backpack items that can take over the worn item's enhancement.
  inheritView(m) {
    const hero = this.hero;
    const rows = hero.getSaveBackpack().map((item, i) => ({ item, index: i + 1, cost: hero.inheritCost(i + 1) }))
      .filter(({ cost }) => cost > 0).map(({ item, index, cost }) => {
        const worn = hero.equippedOfType(item.getType());
        const stat = item.getType() === ItemType.Weapon ? '攻击' : '生命';
        const value = hero.inheritValue(index);
        return h('div', {}, itemRow(item, { hero, actions: [button(`继承（${cost}金）`, async () => {
          const ok = await choose('继承强化', h('div', {},
            h('p', {}, `${item.getDisplayName()} 直接强化到 +${worn.getEnhanceLevel()}（${stat} ${item.getValue()} → ${value}）并换上，花费 ${cost} 金币。`),
            h('p', { class: 'bad' }, `当前的 ${worn.getDisplayName()} 会被消耗，不回到背包。`),
            worn.getEffect() && worn.getEffect() !== item.getEffect() ? h('p', { class: 'muted' }, `改造效果不随强化转移：会失去「${Item.effectName(worn.getEffect())}」。`) : null),
          [{ label: '继承', value: true, cls: 'primary' }, { label: '取消', value: false }]);
          if (!ok) return;
          const { ok: done } = this.game.act(() => hero.inheritEnhancement(index));
          if (!done) { sfx('error'); toast('金币不足。'); } else { sfx('levelup'); toast(`${item.getName()} 已继承 +${worn.getEnhanceLevel()} 并换上。`); }
          m.refresh();
        }, { cls: `small ${value > worn.getValue() ? 'primary' : ''}`, disabled: hero.getcoin() < cost })] }),
        h('p', { class: value > worn.getValue() ? 'muted' : 'bad', style: { fontSize: '12px', margin: '2px 0 0 52px' } },
          `继承 ${worn.getDisplayName()}：${stat} ${value}（当前 ${worn.getValue()}${value > worn.getValue() ? '' : '，不比现在强'}），费用为重新强化的 ${InheritPercent}%。`));
      });
    return h('div', {},
      h('h3', { style: { marginTop: '14px' } }, '继承强化'),
      rows.length ? h('div', { class: 'grid-list' }, rows)
        : h('p', { class: 'muted' }, `背包里没有可继承的同部位装备。把已强化装备的等级转给新装备，费用为重新强化的 ${InheritPercent}%，旧装备会被消耗。`));
  }

  // ---- Shop ------------------------------------------------------------------------------
  openShop() {
    const game = this.game;
    const hero = this.hero;
    openModal({
      title: `章节商店 · ${Campaign.chapter(game.floor).name}`, size: 'wide',
      render: (m) => {
        const c = game.chapter;
        const stock = game.shopStock();
        const purchased = game.run.shopPurchased[c];
        const refreshes = game.run.shopRefreshes[c];
        const cost = refreshCost(game.floor, refreshes);
        return h('div', {},
          h('p', {}, '金币 ', h('span', { class: 'gold' }, hero.getcoin()), h('span', { class: 'muted' }, '　库存跨楼层、跨读档保留。')),
          h('div', { class: 'grid-list' }, stock.map((item, i) => {
            const sold = purchased & (1 << i);
            const price = game.purchasePrice(item);
            if (sold) return h('div', { class: 'row-item' }, h('div', { class: 'icon' }), h('div', { class: 'muted' }, '已售出'), h('div'));
            return itemRow(item, { hero, price, actions: [button('购买', () => {
              const { ok, events } = game.purchase(i + 1);
              if (!ok) { sfx('error'); toast('金币不足。'); } else { sfx('coin'); this.report(events); }
              m.refresh();
            }, { cls: 'small primary', disabled: hero.getcoin() < price })] });
          })),
          h('div', { class: 'choice-row', style: { marginTop: '10px' } },
            button(`刷新库存（${cost}金，本章 ${refreshes}/20）`, () => {
              const { ok } = game.refreshShop();
              if (!ok) { sfx('error'); toast('未刷新：金币不足或达到章内上限。'); } else sfx('coin');
              m.refresh();
            }, { cls: 'small', disabled: refreshes >= 20 || hero.getcoin() < cost })),
          h('h3', {}, '出售背包物品'),
          h('p', { class: 'muted' }, '按单价的三分之一卖出；在背包（Esc → 背包）里可以按数量卖出、锁定或批量拆解。'));
      },
    });
  }

  // ---- Waystone ----------------------------------------------------------------------------
  openWaystone(scene) {
    const game = this.game;
    openModal({
      title: '传送石', size: 'wide',
      render: (m) => {
        const highest = game.hero.getHighestFloor();
        const chapters = [];
        for (let c = 0; c <= chapterOf(highest); c++) {
          const floors = [];
          for (let f = c * 10 + 1; f <= Math.min(highest, c * 10 + 10); f++) {
            const cleared = game.cleared(f);
            const label = `${f}${f % 10 === 0 ? '★' : Campaign.isEliteFloor(f) ? '◆' : ''}`;
            floors.push(button(label, async () => {
              if (f === game.floor) return;
              m.close();
              sfx('magic');
              await this.app.transition(() => {
                game.travel(f);
                scene.spawnAt = 'waystone';
                scene.chest = null;
                scene.enter();
              });
            }, { cls: `small ${f === game.floor ? 'primary' : cleared ? '' : 'good'}`, title: cleared ? '已首通' : '待完成' }));
          }
          chapters.push(h('div', { style: { marginBottom: '8px' } }, h('div', { class: 'muted', style: { fontSize: '13px', margin: '4px 0' } },
            `第${c + 1}章 ${Campaign.Chapters[c].name}`), h('div', { class: 'choice-row' }, floors)));
        }
        return h('div', {},
          h('p', { class: 'muted' }, '回到已到达的楼层补练：重复挑战收益逐次降低，首通奖励、订单和事件不会重复获得。绿色为待完成楼层，★为Boss，◆为精英。'),
          chapters,
          h('h3', {}, '存档'),
          h('div', { class: 'choice-row' }, button('在此存档（手动存档）', () => {
            if (game.save('manual')) { sfx('coin'); toast('已保存到手动存档。'); }
          }, { cls: 'primary' })));
      },
    });
  }

  // ---- Allies and events -----------------------------------------------------------------
  async talkAlly(scene, e) {
    const game = this.game;
    if (!game.eventPending()) {
      const lines = [
        '前面的路不好走，多留意敌人的意图。', '谢谢你的帮助，这一章的路我会记住。', '书架后面有风声……小心那些会读招的家伙。',
        '矿道深处的引爆装置还在滴答作响。', '风雪很大，记得在营火边休整。', '炉火的温度越来越高了，快走吧。',
        '镜子里的人……真的是我吗？', '星图还差最后几页，拜托了。', '王座的封印比想象的更坚固。', '大家都在等你，冲出去！',
      ];
      await alertBox(e.name, lines[game.chapter], '告辞');
      return;
    }
    await this.eventDialog(scene);
  }
  async eventDialog(scene) {
    const game = this.game;
    const options = Campaign.eventOptions(game.floor);
    const cost = options.pay.cost;
    for (;;) {
      const choice = await choose(`遇到${options.ally}`, h('div', {},
        h('p', {}, `「${Campaign.chapter(game.floor).mission}」—— ${options.ally}需要你的帮助。效果持续到本章结束。`),
        h('p', { class: 'muted' }, `金币 ${game.hero.getcoin()}`)), [
        { label: options.pay.text, value: 1, cls: 'primary', disabled: game.hero.getcoin() < cost },
        { label: options.risk.text, value: 2 },
      ], { size: '' });
      if (choice === null) return false;
      const { ok, events } = game.resolveEvent(choice);
      if (ok) {
        sfx(choice === 1 ? 'coin' : 'chest');
        this.report(events);
        scene.refreshEntities();
        return true;
      }
      sfx('error');
      toast('选择无效或金币不足；选项二无需金币。');
    }
  }
  inspectDevice() {
    const cleared = this.game.cleared();
    alertBox('装置', cleared ? '装置已被摧毁。完成本章第2、7层后，Boss 会失去护盾、重击减弱。'
      : '破坏线目标：首通本层后装置会被摧毁。完成本章第2、7层后，Boss 失去护盾、重击减弱。');
  }

  // ---- Exploration ---------------------------------------------------------------------------
  // A spot's choices with their odds and costs. Merchants stay open for more
  // purchases; other spots close and show what they gave.
  openSpot(scene, e) {
    if (e.kind === SpotKind.Puzzle && !e.done) {
      this.openPuzzle(scene, e);
      return;
    }
    const game = this.game;
    const hero = this.hero;
    let outcome = '';
    openModal({
      title: e.name,
      render: (m) => {
        const view = Exploration.survey(hero, game.floor)[e.index];
        const options = Exploration.options(hero, game.floor, e.index);
        return h('div', {},
          h('div', { class: 'spot-head' }, this.spotIcon(scene, e),
            h('div', {}, h('p', {}, view.description), h('p', { class: 'muted' }, ZoneNames[view.spot.zone]))),
          outcome ? h('p', { class: 'outcome' }, outcome) : null,
          options.length ? h('div', { class: 'grid-list' }, options.map((o) => h('div', { class: 'row-item option-row' },
            h('div', {},
              h('div', { class: 'title' }, o.label),
              h('div', { class: 'meta' }, o.detail),
              o.available ? null : h('div', { class: 'meta bad' }, o.reason)),
            h('div', { class: 'actions' }, button('选择', () => {
              if (e.kind === SpotKind.Merchant) {
                outcome = this.resolveSpot(scene, e, o.id) ?? outcome;
                m.refresh();
              } else {
                m.close();
                this.resolveSpot(scene, e, o.id);
              }
            }, { cls: 'small primary', disabled: !o.available, key: String(o.id) })))))
            : h('p', { class: 'muted' }, view.done ? '这里已经探索过了。' : '现在无法探索。'),
          this.resourceLine());
      },
    });
  }
  // Applies a choice; returns its text, or null when nothing happened.
  resolveSpot(scene, e, option) {
    const hero = this.hero;
    const before = { hp: hero.gethp(), mp: hero.getmp(), bane: this.game.run.bane };
    const result = this.game.explore(e.index, option);
    if (!result.ok) {
      sfx('error');
      toast('没有执行：条件不足。');
      return null;
    }
    const hurt = hero.gethp() < before.hp || this.game.run.bane > before.bane ||
      (hero.getmp() < before.mp && e.kind === SpotKind.Curio && option === 1);
    this.celebrate(scene, e, hurt);
    this.report(result.events);
    scene.refreshEntities();
    if (result.lorePage) this.showLore(result.lorePage, result.newPage);
    else if (e.kind !== SpotKind.Merchant)
      alertBox(e.name, h('div', {}, h('p', { class: hurt ? 'bad' : 'good' }, result.text), this.resourceLine()), '好的');
    return result.text;
  }
  celebrate(scene, e, hurt) {
    const x = e.x * TILE + 8;
    const y = e.y * TILE + 6;
    if (hurt) {
      sfx('hit');
      scene.fx.shake(2, 0.15);
      scene.fx.burst(x, y, { color: [P.red, P.plum], count: 14, speed: 40 });
      return;
    }
    const sound = {
      [SpotKind.Curio]: 'magic', [SpotKind.Gather]: 'heal', [SpotKind.AffixSource]: 'shatter', [SpotKind.Merchant]: 'coin',
      [SpotKind.Lore]: 'read', [SpotKind.Puzzle]: 'levelup',
    }[e.kind] ?? 'chest';
    sfx(sound);
    if (e.kind === SpotKind.AffixSource) scene.fx.shake(3, 0.2);
    scene.fx.burst(x, y, { color: [P.yellow, P.gold, P.white], count: 22, speed: 55, gravity: 60, glow: true });
  }
  spotIcon(scene, e) {
    const canvas = scene.spotSprite(e);
    if (!canvas) return null;
    const key = `spot-${scene.theme}-${e.kind}-${e.variant}-${e.done ? 1 : 0}-${e.door ? 1 : 0}`;
    return h('img', { class: 'px spot-icon', src: Sprites.spriteUrl(key, canvas), width: canvas.width * 3, height: canvas.height * 3, alt: '' });
  }
  resourceLine() {
    const hero = this.hero;
    const run = this.game.run;
    return h('p', { class: 'muted', style: { fontSize: '12px' } },
      `HP ${hero.gethp()}/${hero.getmaxhp()} · MP ${hero.getmp()}/${hero.getmaxmp()} · 金币 ${hero.getcoin()} · 材料 ${run.materials} · 钥匙 ${run.keys}`);
  }
  showLore(page, isNew) {
    sfx('read');
    alertBox(`见闻录 · 第${page}页`, h('div', { class: 'lore-page' },
      h('h3', {}, Exploration.loreTitle(page)),
      h('p', {}, Exploration.loreText(page)),
      h('p', { class: isNew ? 'good' : 'muted' }, isNew
        ? `新的一页收入见闻录（${this.game.profile.loreCount()}/${LorePages}），可以在远征档案里重读。`
        : '这一页之前的远征已经收集过了。')), '收好');
  }

  // The chapter puzzle: pick the elements in order; a wrong answer costs 5% HP.
  openPuzzle(scene, e) {
    const game = this.game;
    const hero = this.hero;
    const puzzle = Exploration.puzzle(hero, game.floor);
    let order = [];
    let feedback = '';
    openModal({
      title: puzzle.title, size: 'wide',
      render: (m) => h('div', {},
        h('div', { class: 'spot-head' }, this.spotIcon(scene, e), h('p', {}, puzzle.prompt)),
        h('ul', { class: 'lines clues' }, puzzle.clues.map((clue) => h('li', {}, clue))),
        h('div', { class: 'puzzle-order' }, puzzle.elements.map((_, i) => h('span', { class: `slot ${i < order.length ? 'filled' : ''}` },
          h('small', {}, `第${i + 1}个`), i < order.length ? puzzle.elements[order[i]] : '？'))),
        h('div', { class: 'choice-row' }, puzzle.elements.map((name, i) => button(name, () => {
          order.push(i);
          feedback = '';
          m.refresh();
        }, { cls: 'small', disabled: order.includes(i), key: String(i + 1) }))),
        feedback ? h('p', { class: 'bad' }, feedback) : null,
        h('p', { class: 'muted', style: { fontSize: '12px' } },
          `按线索从第一个到最后一个依次点选。只有一种顺序符合全部线索；答错损失当前生命 5%（不会致死），可以重试。HP ${hero.gethp()}/${hero.getmaxhp()}`)),
      buttons: (m) => [
        { label: '撤销', disabled: !order.length, onClick: () => { order.pop(); m.refresh(); } },
        { label: '清空', disabled: !order.length, onClick: () => { order = []; m.refresh(); } },
        { label: '确认顺序', cls: 'primary', disabled: order.length < puzzle.elements.length, key: 'Enter', keyLabel: '⏎', onClick: () => {
          const result = game.solvePuzzle(order);
          if (!result.ok) { sfx('error'); return; }
          const solved = Exploration.survey(hero, game.floor)[e.index].done;
          if (!solved) {
            sfx('hit');
            scene.fx.shake(2, 0.15);
            feedback = result.text;
            order = [];
            m.refresh();
            return;
          }
          m.close();
          this.celebrate(scene, e, false);
          this.report(result.events);
          scene.refreshEntities();
          alertBox(puzzle.title, h('div', {}, h('p', { class: 'good' }, result.text), this.resourceLine()), '好的');
        } },
      ],
    });
  }

  // ---- Encounter ---------------------------------------------------------------------------
  async challenge(scene) {
    const game = this.game;
    if (game.eventPending()) {
      toast(`先与${Campaign.chapter(game.floor).ally}交谈。`);
      const resolved = await this.eventDialog(scene);
      if (!resolved) return;
    }
    const notes = [];
    const preview = Campaign.makeEnemy(game.hero, game.floor, notes);
    const rules = Campaign.rulesFor(game.hero, game.floor);
    preview.setTacticalPressure(rules.difficulty);
    const first = !game.cleared();
    const sprite = Sprites.monster(preview.bestiaryEntry()).idle;
    const goalText = {
      [Objective.Defeat]: '击败敌人（限80回合）。',
      [Objective.Survive]: `存活 ${rules.targetTurns} 回合，或击败敌人。`,
      [Objective.Disrupt]: '普攻蓄力中的敌人完成两次打断（被闪避也算），或击败敌人（限20回合）。',
      [Objective.Escape]: '撑过 3 回合后格挡撤离，或击败敌人（限15回合）。',
    }[rules.objective];
    const ok = await choose(first ? '楼层守卫' : '再次挑战', h('div', {},
      h('div', { style: { display: 'flex', gap: '12px', alignItems: 'center' } },
        h('img', { src: Sprites.spriteUrl(`m${preview.bestiaryEntry()}`, sprite), width: 96, height: 96, class: 'px', style: { imageRendering: 'pixelated', background: '#0e0b16', border: '2px solid #000' }, alt: '' }),
        h('div', {},
          h('h3', {}, preview.getname()),
          h('p', { class: 'muted' }, `${preview.getEncounterTitle()} · HP ${preview.getmaxhp()} · 攻击 ${preview.getattack()} · 闪避 ${preview.getdodgeRate()}%`),
          h('p', {}, preview.getAffixes().length ? preview.getAffixes().map((a) => h('span', { class: 'tag elite', title: affixDescription(a) }, affixName(a))) : h('span', { class: 'muted' }, '无词缀')))),
      h('p', {}, h('span', { class: 'gold' }, `目标·${Campaign.objectiveName(rules.objective)}：`), goalText),
      rules.elite ? h('p', { class: 'bad' }, '【精英】至少两个词缀，生命与攻击更高，特殊意图更频繁且被打断后不休整；首通必得本层最高品质档的目标装备。') : null,
      rules.bossSabotaged ? h('p', { class: 'good' }, '供能设施已破坏，Boss 护盾失效、重击减弱。') : null,
      notes.length ? h('ul', { class: 'lines', style: { fontSize: '13px' } }, notes.map((note) =>
        h('li', { class: note.startsWith('诅咒') ? 'bad' : 'good' }, `【探索】${note}`))) : null,
      rules.elite || game.floor % 10 === 0 ? this.readinessView() : null,
      h('p', { class: 'muted', style: { fontSize: '13px' } }, '本章悬赏：', game.journal().bounties.map((b) =>
        `${b.goal}（${b.progress >= b.target ? '已完成' : `${b.progress}/${b.target}`}）`).join('；')),
      h('p', { class: 'muted' }, `战术提示：${preview.getTacticalHint()}`),
      preview.getAffixes().length ? h('ul', { class: 'lines muted', style: { fontSize: '13px' } }, preview.getAffixes().map((a) => h('li', {}, `[${affixName(a)}] ${affixDescription(a)}`))) : null,
      first ? null : h('p', { class: 'muted' }, '已首通：重复挑战的奖励逐次降低（30% → 10% → 0）。')),
    [{ label: '开战', value: true, cls: 'primary' }, { label: '返回', value: false }], { size: '' });
    if (!ok) return;
    this.app.startBattle(scene);
  }

  showChest(items) {
    openModal({
      title: '宝箱', size: 'narrow',
      render: () => h('div', {}, h('p', { class: 'muted' }, '这场战斗的战利品（已放入背包）：'),
        h('div', { class: 'loot-list' }, items.map((item) => itemRow(item, { hero: this.hero })))),
      buttons: [{ label: '好的', cls: 'primary', key: 'Enter', keyLabel: '⏎', onClick: (m) => m.close() }],
    });
  }
}

