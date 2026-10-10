// Shared UI pieces: item rows with icons and comparisons, stat blocks.
import { h, bar } from './dom.js';
import { ItemType, Item } from '../rules/item.js';
import { itemIconKind } from '../rules/world.js';
import { skillRankCap } from '../rules/config.js';
import * as Sprites from '../engine/sprites.js';

export function itemIcon(item) {
  const img = h('img', { class: 'icon', src: Sprites.iconUrl(itemIconKind(item), item.getTier()), alt: '' });
  img.style.setProperty('--tier', Sprites.tierColor(item.getTier()));
  return img;
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
