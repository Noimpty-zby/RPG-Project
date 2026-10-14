// Battle HUD: objective, intent, both combatants, commands and the log.
import { affixDescription, affixName, Intent } from '../rules/enemy.js';
import { ActionType, Objective } from '../rules/combat.js';
import { ItemType } from '../rules/item.js';
import { h, button, bar, updateBar, openModal, modalOpen } from './dom.js';
import { sfx } from '../engine/audio.js';
import { objectiveName } from '../rules/campaign.js';
import { itemRow, statsBlock } from './widgets.js';

const IntentTitles = {
  Attack: '普通攻击', Charge: '蓄力', HeavyStrike: '蓄力重击', Wound: '压制治疗', Shatter: '破防攻击', ReadPattern: '预判连招', Recover: '休整',
};

export class BattleUI {
  constructor(app, scene) {
    this.app = app;
    this.scene = scene;
    this.root = document.getElementById('battle-ui');
    this.root.classList.remove('hidden');
    this.mode = 'locked';
    this.resolver = null;
    this.lastType = null;
    this.build();
    this.onKey = (e) => this.key(e);
    addEventListener('keydown', this.onKey);
  }
  get hero() { return this.app.game.hero; }
  get monster() { return this.scene.monster; }
  get battle() { return this.scene.battle; }

  build() {
    const hero = this.hero;
    const monster = this.monster;
    this.objectiveEl = h('div', { class: 'b-objective panel' });
    this.intentEl = h('div', { class: 'b-intent panel' }, h('span', { class: 'muted' }, monster.getTacticalHint()));
    this.heroHp = bar(hero.gethp(), hero.getmaxhp());
    this.heroMp = bar(hero.getmp(), hero.getmaxmp(), 'mp');
    this.heroChips = h('div', { class: 'chips' });
    this.heroCard = h('div', { class: 'b-card hero panel' },
      h('div', { class: 'who' }, h('b', {}, hero.getname()), h('small', {}, `${hero.getJobName()} Lv.${hero.getlevel()}`)),
      this.heroHp, this.heroMp, this.heroChips);
    this.enemyHp = bar(monster.gethp(), monster.getmaxhp(), 'enemy');
    this.enemyChips = h('div', { class: 'chips' });
    this.enemyCard = h('div', { class: 'b-card enemy panel' },
      h('div', { class: 'who' }, h('b', {}, monster.getname()), h('small', {}, monster.getEncounterTitle())),
      this.enemyHp, this.enemyChips);
    this.commands = h('div', { class: 'b-commands panel' });
    this.logEl = h('div', { class: 'b-log panel' });
    const right = h('div', { class: 'b-right' }, this.logEl, this.enemyCard);
    this.root.replaceChildren(
      h('div', { class: 'b-top' }, this.objectiveEl, this.intentEl),
      h('div'),
      h('div', { class: 'b-bottom' }, this.heroCard, this.commands, right),
    );
    this.refresh();
    this.renderCommands();
  }
  destroy() {
    removeEventListener('keydown', this.onKey);
    this.root.classList.add('hidden');
    this.root.replaceChildren();
  }

  showIntro() {
    const rules = this.scene.rules;
    const lines = [];
    if (rules.elite) lines.push('【精英】至少两个词缀，生命与攻击更高，特殊意图更频繁且被打断后不休整。');
    if (rules.bossSabotaged) lines.push('供能设施已破坏，Boss护盾失效。');
    for (const note of this.scene.notes) lines.push(`【探索】${note}`);
    for (const line of lines) this.log({ text: line, type: 'notice' });
    this.log({ text: `${this.monster.getEncounterTitle()}：${this.monster.getTacticalHint()}`, type: 'notice' });
  }

  setIntent(intent) {
    const danger = this.scene.intentDanger();
    this.intentEl.className = `b-intent panel ${danger ? 'danger' : intent.kind === Intent.Recover ? 'calm' : ''}`;
    this.intentEl.replaceChildren(h('span', { class: 'kind' }, `【敌人意图 · ${IntentTitles[intent.kind]}】`),
      this.monster.describeIntent(intent));
  }

  refresh() {
    const hero = this.hero;
    const monster = this.monster;
    const battle = this.battle;
    updateBar(this.heroHp, hero.gethp(), hero.getmaxhp(), `HP ${hero.gethp()}/${hero.getmaxhp()}`);
    updateBar(this.heroMp, hero.getmp(), hero.getmaxmp(), `MP ${hero.getmp()}/${hero.getmaxmp()}`);
    updateBar(this.enemyHp, monster.gethp(), monster.getmaxhp(), `HP ${monster.gethp()}/${monster.getmaxhp()}`);
    const pips = h('span', { class: 'pips' }, [0, 1].map((i) => h('i', { class: i < hero.getCombatResource() ? 'on' : '' })));
    this.heroChips.replaceChildren(...[
      h('span', { class: 'chip' }, `${hero.resourceName()}`, pips),
      h('span', { class: 'chip' }, `攻击 ${hero.getattack()}`),
      h('span', { class: 'chip' }, `药水 ${this.app.game.potionCount()}`),
      hero.healingPercent < 100 ? h('span', { class: 'chip warn' }, `治疗 ${hero.healingPercent}%`) : null,
    ].filter(Boolean));
    const chips = [h('span', { class: 'chip' }, `攻击 ${monster.getattack()}`), h('span', { class: 'chip' }, `闪避 ${monster.getdodgeRate()}%`)];
    if (monster.attackWeakenTurns > 0) chips.push(h('span', { class: 'chip info' }, `减攻 ${monster.attackWeakenPercent}%·${monster.attackWeakenTurns}`));
    for (const affix of monster.getAffixes())
      chips.push(h('span', { class: 'chip warn', title: affixDescription(affix) }, affixName(affix)));
    this.enemyChips.replaceChildren(...chips);
    const rules = this.scene.rules;
    this.objectiveEl.replaceChildren(h('b', {}, `目标·${objectiveName(rules.objective)}　`), battle.objectiveDescription(),
      h('div', { class: 'muted', style: { fontSize: '12px', marginTop: '2px' } }, `回合 ${battle.turns()}/${battle.turnLimit()}`));
    if (this.mode !== 'locked') this.renderCommands();
  }

  // ---- Commands -------------------------------------------------------------------------
  waitForAction() {
    this.mode = 'main';
    this.renderCommands();
    return new Promise((resolve) => { this.resolver = resolve; });
  }
  lock() {
    this.mode = 'locked';
    this.renderCommands();
  }
  submit(action) {
    if (!this.resolver) return;
    const resolve = this.resolver;
    this.resolver = null;
    this.mode = 'locked';
    this.renderCommands();
    resolve(action);
  }
  flashHint(text) {
    this.hintText = text;
    this.renderCommands();
    setTimeout(() => { if (this.hintText === text) { this.hintText = ''; this.renderCommands(); } }, 2000);
  }

  renderCommands() {
    const hero = this.hero;
    const el = this.commands;
    const locked = this.mode === 'locked';
    const intent = this.scene.intent;
    if (this.mode === 'skills') {
      const list = h('div', { class: 'sublist' });
      for (let i = 1; i <= hero.getSkillCount(); i++) {
        const skill = hero.getSkill(i);
        const usable = skill.canUse(hero);
        const reason = skill.blockedReason(hero);
        const predicted = intent?.kind === Intent.ReadPattern && intent.predictedAction === i;
        list.append(button(`${skill.getskillname()}${skill.getSpecialization() ? `·${skill.getSpecializationName(hero)}` : ''}　MP ${skill.getActualCost(hero)}${reason ? `　[${reason}]` : ''}${predicted ? '　⚠被预判' : ''}`,
          () => this.submit({ type: ActionType.Skill, index: i }),
          { key: String(i), disabled: !usable, sub: skill.isUnlocked(hero) ? skill.getPreview(hero) : `第${skill.getunlockfloor()}层、${skill.getlevelrequired()}级解锁` }));
      }
      list.append(button('返回', () => { this.mode = 'main'; this.renderCommands(); }, { key: '0', cls: 'small' }));
      el.replaceChildren(list);
      return;
    }
    if (this.mode === 'potions') {
      const list = h('div', { class: 'sublist' });
      const potions = hero.getSaveBackpack().map((item, i) => ({ item, index: i + 1 })).filter(({ item }) => item.getType() === ItemType.Potion);
      potions.forEach(({ item, index }, n) => {
        const effective = Math.trunc(item.getValue() * hero.healingPercent / 100);
        list.append(button(`${item.getName()} ×${item.getQuantity()}`, () => this.submit({ type: ActionType.Potion, index }),
          { key: String(n + 1), disabled: hero.gethp() >= hero.getmaxhp(),
            sub: `恢复 ${item.getValue()} HP${hero.healingPercent < 100 ? `（压制下只生效 ${effective}）` : ''}` }));
      });
      if (!potions.length) list.append(h('div', { class: 'hint' }, '没有药水。'));
      list.append(button('返回', () => { this.mode = 'main'; this.renderCommands(); }, { key: '0', cls: 'small' }));
      el.replaceChildren(list);
      return;
    }
    const interrupt = intent?.interruptible;
    const predicted = intent?.kind === Intent.ReadPattern ? intent.predictedAction : -99;
    const warn = (id) => (predicted === id ? '⚠' : '');
    const cmds = h('div', { class: 'cmds' },
      button(`普攻${warn(0)}`, () => this.submit({ type: ActionType.Attack }), { key: '1', disabled: locked, cls: interrupt ? 'primary' : '' }),
      button('技能', () => { this.mode = 'skills'; this.renderCommands(); }, { key: '2', disabled: locked }),
      button(`格挡${warn(-1)}`, () => this.submit({ type: ActionType.Guard }), { key: '3', disabled: locked,
        cls: this.battle.objective() === Objective.Escape && this.battle.turns() >= 3 ? 'good' : '' }),
      button(`药水${warn(-2)}`, () => { this.mode = 'potions'; this.renderCommands(); }, { key: '4', disabled: locked }),
      button('查看', () => this.inspect(), { key: '5' }),
    );
    let hint = this.hintText || '';
    if (!hint && !locked && intent) {
      if (interrupt) hint = '敌人正在蓄力：普攻即可打断！';
      else if (intent.kind === Intent.HeavyStrike) hint = '重击来袭：格挡可减半伤害。';
      else if (intent.kind === Intent.Wound) hint = '压制治疗：这回合先别回血。';
      else if (intent.kind === Intent.Shatter) hint = '破防攻击无视格挡：主动进攻更划算。';
      else if (intent.kind === Intent.ReadPattern && predicted !== -99) hint = '敌人在预判你的行动：换一种行动！';
      else if (intent.kind === Intent.Recover) hint = '敌人休整中：回血或爆发的好时机。';
      else if (this.battle.objective() === Objective.Escape && this.battle.turns() >= 3) hint = '已可撤离：格挡即可完成目标。';
    }
    if (locked && !hint) hint = '…';
    el.replaceChildren(cmds, h('div', { class: 'hint' }, hint));
  }

  key(e) {
    if (modalOpen()) return;
    const k = e.key;
    if (this.mode === 'locked') {
      if (k === '5') { e.preventDefault(); this.inspect(); }
      return;
    }
    if (this.mode === 'main') {
      const map = { '1': 0, '2': 1, '3': 2, '4': 3, '5': 4 };
      if (k in map) {
        e.preventDefault();
        this.commands.querySelectorAll('.cmds .btn')[map[k]]?.click();
      }
    } else if (this.mode === 'skills' || this.mode === 'potions') {
      if (k === 'Escape' || k === '0' || k === 'Backspace') {
        e.preventDefault();
        sfx('click');
        this.mode = 'main';
        this.renderCommands();
        return;
      }
      const target = [...this.commands.querySelectorAll('[data-hotkey]')].find((b) => b.dataset.hotkey === k && !b.disabled);
      if (target) { e.preventDefault(); target.click(); }
    }
  }

  inspect() {
    const hero = this.hero;
    const monster = this.monster;
    openModal({
      title: '敌我详情', size: 'wide',
      render: () => h('div', {},
        h('h3', {}, `${monster.getname()} · ${monster.getEncounterTitle()}`),
        h('p', {}, `HP ${monster.gethp()}/${monster.getmaxhp()} | 攻击 ${monster.getattack()} | 闪避 ${monster.getdodgeRate()}%`),
        monster.getAffixes().length
          ? h('ul', { class: 'lines' }, monster.getAffixes().map((a) => h('li', {}, h('span', { class: 'bad' }, `[${affixName(a)}] `), affixDescription(a))))
          : h('p', { class: 'muted' }, '无词缀。'),
        h('p', {}, h('span', { class: 'gold' }, '战术提示：'), monster.getTacticalHint()),
        h('p', {}, h('span', { class: 'gold' }, '当前目标：'), this.battle.objectiveDescription()),
        h('h3', {}, '自身'),
        statsBlock(this.app.game),
        h('h3', {}, '装备'),
        hero.getSaveEquipments().length ? h('div', { class: 'grid-list' }, hero.getSaveEquipments().map((item) => itemRow(item, { hero }))) : h('p', { class: 'muted' }, '没有装备。'),
        h('h3', {}, '技能'),
        h('ul', { class: 'lines' }, hero.skills.map((s, i) => h('li', {}, `${i + 1}. ${s.getskillname()} [${s.getSpecializationName(hero)}] MP ${s.getActualCost(hero)} | ${s.canUse(hero) ? '可用' : s.blockedReason(hero)} | ${s.getPreview(hero)}`))),
      ),
    });
  }

  // ---- Log --------------------------------------------------------------------------------
  log(e) {
    this.lastType = e.type;
    if (!e.text || e.type === 'intent') return;
    const cls = e.type === 'damage' && e.side === 'hero' ? 't-bad' : e.type === 'heal' || e.type === 'interrupt' ? 't-good' :
      e.type === 'cast' || e.type === 'notice' ? 't-gold' : '';
    this.logEl.append(h('div', { class: cls }, e.text));
    while (this.logEl.childElementCount > 60) this.logEl.firstChild.remove();
    this.logEl.scrollTop = this.logEl.scrollHeight;
  }
  tick() {}

  // ---- Result -------------------------------------------------------------------------------
  showResult(result, onContinue) {
    const hero = this.hero;
    const items = result.events.filter((e) => e.type === 'loot');
    const reward = result.events.find((e) => e.type === 'reward');
    const levels = result.events.filter((e) => e.type === 'levelup');
    const materials = result.events.filter((e) => e.type === 'materials').reduce((n, e) => n + e.amount, 0);
    const notices = result.events.filter((e) => ['chapterClear', 'broken', 'notice'].includes(e.type));
    const title = result.won ? (this.monster.isalive() ? '目标完成' : '胜利') : hero.isalive() ? (result.timeout ? '目标超时' : '撤退') : '战败';
    if (result.won) sfx('coin');
    if (levels.length) setTimeout(() => sfx('levelup'), 400);
    openModal({
      title: '战斗结算', closable: false, size: 'narrow',
      render: () => h('div', { class: 'result' },
        h('div', { class: `big ${result.won ? '' : 'lose'}` }, title),
        reward ? h('p', {}, `获得 `, h('span', { class: 'gold' }, `${reward.exp} 经验`), '、', h('span', { class: 'gold' }, `${reward.coins} 金币`),
          reward.first ? '' : h('span', { class: 'muted' }, `（重复挑战 ${reward.percent}%）`)) : null,
        levels.length ? h('p', { class: 'good' }, `升级！现在是 ${levels[levels.length - 1].level} 级（恢复基础生命12%、最大魔力20%）`) : null,
        materials ? h('p', {}, `材料 +${materials}`) : null,
        items.length ? h('div', { class: 'loot-list' }, items.map((e) => itemRow(e.item, { hero, compareUpgrade: e.upgrade }))) : null,
        items.length ? h('p', { class: 'muted' }, '战利品已放入背包，宝箱就在守卫原来的位置。') : null,
        result.patrol ? h('p', { class: result.won ? 'good' : 'muted' }, result.won ? '近路打通了，可以去挑战守卫。'
          : '巡逻还守在路口。可以重试，也可以改走别的路。') : null,
        notices.map((e) => h('p', { class: 'gold' }, e.text)),
        result.achievements.map((a) => h('p', { class: 'gold' }, `【成就解锁】${a.title}：${a.description}`)),
        result.worn.map((line) => h('p', { class: 'bad' }, line)),
        !result.won || this.battle.report.readPunishments || this.battle.report.suppressedHealing || this.battle.report.poisonDamage
          ? h('div', { style: { textAlign: 'left', marginTop: '10px' } }, result.review.map((line) => h('p', { class: 'muted', style: { fontSize: '13px' } }, line)))
          : h('p', { class: 'muted', style: { fontSize: '13px' } }, result.review[0]),
      ),
      buttons: [{ label: '继续', cls: 'primary', key: 'Enter', keyLabel: '⏎', onClick: (m) => { m.close(); onContinue(); } }],
    });
  }
}

