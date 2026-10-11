// Shared UI pieces: item rows with icons and comparisons, stat blocks.
import { h, bar } from './dom.js';
import { ItemType, Item } from '../rules/item.js';
import { itemIconKind } from '../rules/world.js';
import { EnhancePercent, enhanceCost, idiv, MaxEnhanceLevel, skillRankCap } from '../rules/config.js';
import * as Sprites from '../engine/sprites.js';

export function itemIcon(item) {
  const img = h('img', { class: 'icon', src: Sprites.iconUrl(itemIconKind(item), item.getTier()), alt: '' });
  img.style.setProperty('--tier', Sprites.tierColor(item.getTier()));
  return img;
}

// Gear the hero can wear now that beats what is worn in that slot.
export function isUpgrade(hero, item) {
  if (item.getType() === ItemType.Potion || !hero.canEquip(item)) return false;
  const equipped = hero.equippedOfType(item.getType());
  return !equipped || item.getValue() > equipped.getValue();
}

// What enhancing an item up to `level` would give and cost, by the same steps
// as Player.enhanceEquipment.
export function enhanceProjection(item, level) {
  let value = item.getValue();
  let cost = 0;
  for (let next = item.getEnhanceLevel() + 1; next <= Math.min(level, MaxEnhanceLevel); next++) {
    cost += enhanceCost(item.getPrice(), next);
    value += Math.max(1, idiv(value * EnhancePercent, 100));
  }
  return { value, cost };
}

// Side by side: the worn item of that slot against a candidate, including
// what the candidate would reach if enhanced as far as the worn one.
export function compareTable(hero, candidate) {
  const worn = hero.equippedOfType(candidate.getType());
  const stat = candidate.getType() === ItemType.Weapon ? '攻击' : '生命';
  const effect = (item) => (item?.getEffect() ? Item.effectDescription(item.getEffect()) : '无');
  const cell = (item, fn) => h('td', {}, item ? fn(item) : '—');
  const row = (label, fn, note = null) => h('tr', {}, h('th', {}, label), cell(worn, fn), cell(candidate, fn), note);
  const diff = candidate.getValue() - (worn ? worn.getValue() : 0);
  const lines = [];
  if (worn && worn.getEnhanceLevel() > candidate.getEnhanceLevel()) {
    const { value, cost } = enhanceProjection(candidate, worn.getEnhanceLevel());
    lines.push(`把这件也强化到 +${worn.getEnhanceLevel()}：${stat} ${value}（${value >= worn.getValue() ? '不低于' : '仍低于'}当前的 ${worn.getValue()}），共需 ${cost} 金币。强化留在原装备上，不会转移。`);
  }
  if (worn?.getEffect() && worn.getEffect() !== candidate.getEffect())
    lines.push(`换上后会失去当前的改造效果：${effect(worn)}。数值提升只比较${stat}，不包括改造。`);
  return h('div', {},
    h('table', { class: 'compare' },
      h('thead', {}, h('tr', {}, h('th'), h('th', {}, '当前装备'), h('th', {}, '这件'))),
      h('tbody', {},
        row('名称', (item) => h('span', { style: { color: Sprites.tierColor(item.getTier()) } }, item.getDisplayName())),
        row('品质', (item) => item.getTierName()),
        row(stat, (item) => (item === candidate && worn
          ? h('span', {}, `${item.getValue()} `, h('span', { class: diff > 0 ? 'delta-up' : diff < 0 ? 'delta-down' : 'muted' }, `(${diff >= 0 ? '+' : ''}${diff})`))
          : `${item.getValue()}`)),
        row('强化', (item) => (item.getEnhanceLevel() ? `+${item.getEnhanceLevel()}` : '未强化')),
        row('改造', effect),
        row('耐久', (item) => `${item.getDurability()}/${item.getMaxDurability()}`),
        row('需求', (item) => `Lv.${item.getLevelRequirement()} · ${item.getJobRequirementName()}`))),
    lines.map((text) => h('p', { class: 'muted' }, text)));
}

// One item: icon, name, stats and optional action buttons.
export function itemRow(item, o = {}) {
  const hero = o.hero;
  const isPotion = item.getType() === ItemType.Potion;
  const meta = [];
  if (isPotion) meta.push(`恢复 ${item.getValue()} HP`);
  else {
    const stat = item.getType() === ItemType.Weapon ? '攻击' : '生命';
    let delta = null;
    if (hero && o.compare !== false) {
      const equipped = hero.equippedOfType(item.getType());
      const diff = item.getValue() - (equipped ? equipped.getValue() : 0);
      if (equipped !== item) delta = h('span', { class: diff > 0 ? 'delta-up' : diff < 0 ? 'delta-down' : 'muted' }, ` (${diff >= 0 ? '+' : ''}${diff})`);
    }
    meta.push(h('span', {}, `${stat} +${item.getValue()}`, delta));
    meta.push(`耐久 ${item.getDurability()}/${item.getMaxDurability()}`);
    meta.push(`需 Lv.${item.getLevelRequirement()}`);
    meta.push(item.getJobRequirementName());
    if (item.getEffect()) meta.push(h('span', { class: 'cyan' }, Item.effectDescription(item.getEffect())));
  }
  if (o.price !== undefined) meta.push(h('span', { class: 'gold' }, `价格 ${o.price}`));
  if (o.sellPrice) meta.push(`售价 ${Math.trunc(item.getPrice() / 3)}`);
  const unusable = hero && !isPotion && (!hero.canWear(item) || item.getLevelRequirement() > hero.getlevel());
  const title = h('div', { class: 'title' },
    item.isLocked() ? h('span', { class: 'tag', style: { background: '#feae34', color: '#2a1408' } }, '锁') : '',
    h('span', { style: { color: Sprites.tierColor(item.getTier()) } }, item.getDisplayName()),
    item.getQuantity() > 1 ? ` ×${item.getQuantity()}` : '',
    h('span', { class: 'muted', style: { fontSize: '12px' } }, `　${item.getTierName()}`),
    o.equipped ? h('span', { class: 'tag done', style: { marginLeft: '6px' } }, '已装备') : null,
    o.compareUpgrade ? h('span', { class: 'tag done', style: { marginLeft: '6px' } }, '提升') : null,
    unusable ? h('span', { class: 'tag elite', style: { marginLeft: '6px' } }, '不可装备') : null);
  const metaEl = h('div', { class: 'meta' });
  meta.forEach((m, i) => { if (i) metaEl.append(' · '); metaEl.append(m); });
  return h('div', { class: `row-item ${item.isLocked() ? 'locked-item' : ''}` }, itemIcon(item), h('div', {}, title, metaEl),
    o.actions ? h('div', { class: 'actions' }, o.actions) : h('div'));
}

export function statsBlock(game) {
  const hero = game.hero;
  const run = hero.runState();
  return h('div', {},
    h('div', { style: { display: 'grid', gap: '4px', marginBottom: '8px' } },
      bar(hero.gethp(), hero.getmaxhp(), '', `HP ${hero.gethp()}/${hero.getmaxhp()}`),
      bar(hero.getmp(), hero.getmaxmp(), 'mp', `MP ${hero.getmp()}/${hero.getmaxmp()}`),
      bar(hero.getexp(), hero.getlevel() * 100, 'exp', hero.getlevel() >= 100 ? 'EXP MAX' : `EXP ${hero.getexp()}/${hero.getlevel() * 100}`)),
    h('dl', { class: 'kv' },
      h('dt', {}, '职业'), h('dd', {}, `${hero.getJobName()}（资源「${hero.resourceName()}」 ${hero.getCombatResource()}/2）`),
      h('dt', {}, '等级'), h('dd', {}, `${hero.getlevel()}`),
      h('dt', {}, '攻击'), h('dd', {}, `${hero.getattack()}`),
      h('dt', {}, '暴击'), h('dd', {}, `${hero.getcritRate()}%`),
      h('dt', {}, '金币'), h('dd', { class: 'gold' }, `${hero.getcoin()}`),
      h('dt', {}, '改造材料'), h('dd', {}, `${run.materials}`),
      h('dt', {}, '免费复苏'), h('dd', {}, run.challenge === 2 ? '无（无复苏模式）' : `${hero.getfreerevives()}`),
      h('dt', {}, '最高层'), h('dd', {}, `${hero.getHighestFloor()}（技能阶数上限 ${skillRankCap(hero.getHighestFloor())}）`)));
}
