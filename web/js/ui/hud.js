// Exploration HUD: hero card, floor card, menu/sound buttons, prompts and
// touch controls.
import { h, bar, modalOpen, onModalChange } from './dom.js';
import * as Campaign from '../rules/campaign.js';
import { Objective } from '../rules/combat.js';
import { chapterOf } from '../rules/config.js';
import * as Sprites from '../engine/sprites.js';
import { audioSettings, setAudio, unlockAudio } from '../engine/audio.js';

export class Hud {
  constructor(app) {
    this.app = app;
    this.root = document.getElementById('hud');
    this.heroEl = document.getElementById('hud-hero');
    this.floorEl = document.getElementById('hud-floor');
    this.buttons = document.getElementById('hud-buttons');
    this.promptEl = document.getElementById('prompt');
    this.touch = document.getElementById('touch');
    this.coarse = matchMedia('(pointer: coarse)').matches;
    onModalChange(() => this.refresh());
    document.getElementById('btn-menu').addEventListener('click', () => { if (!this.app.busy) this.app.menus.openMain(); });
    document.getElementById('btn-travel').addEventListener('click', () => {
      if (!this.app.busy && !modalOpen() && this.app.explore) this.app.menus.openTravel(this.app.explore);
    });
    const sound = document.getElementById('btn-sound');
    sound.addEventListener('click', () => {
      unlockAudio();
      setAudio({ muted: !audioSettings().muted });
      this.refresh();
    });
    for (const btn of this.touch.querySelectorAll('[data-key]')) {
      const key = btn.dataset.key;
      const down = (e) => {
        e.preventDefault();
        unlockAudio();
        btn.classList.add('on');
        if (key === 'action') app.input.tap('action');
        else app.input.press(key);
      };
      const up = (e) => {
        e.preventDefault();
        btn.classList.remove('on');
        if (key !== 'action') app.input.release(key);
      };
      btn.addEventListener('pointerdown', down);
      btn.addEventListener('pointerup', up);
      btn.addEventListener('pointercancel', up);
      btn.addEventListener('pointerleave', up);
    }
    addEventListener('touchstart', () => {
      if (!this.coarse) { this.coarse = true; if (this.visible) this.touch.classList.remove('hidden'); }
    }, { passive: true, once: true });
  }

  show(visible) {
    this.visible = visible;
    this.root.classList.toggle('hidden', !visible);
    this.buttons.classList.toggle('hidden', !visible);
    this.touch.classList.toggle('hidden', !visible || !this.coarse);
    if (!visible) this.prompt(null);
  }

  refresh() {
    const game = this.app.game;
    const hero = game.hero;
    if (!hero) return;
    document.getElementById('btn-sound').classList.toggle('off', audioSettings().muted);
    const portrait = h('img', { class: 'portrait px', src: Sprites.spriteUrl(`portrait-${hero.job()}`, Sprites.heroWalker(hero.job()).down[0]), alt: '' });
    portrait.style.imageRendering = 'pixelated';
    this.heroEl.className = 'hud-card panel';
    this.heroEl.replaceChildren(
      portrait,
      h('div', { class: 'name' }, hero.getname(), h('small', {}, `${hero.getJobName()} Lv.${hero.getlevel()}`)),
      bar(hero.gethp(), hero.getmaxhp(), '', `HP ${hero.gethp()}/${hero.getmaxhp()}`),
      bar(hero.getmp(), hero.getmaxmp(), 'mp', `MP ${hero.getmp()}/${hero.getmaxmp()}`),
      h('div', { class: 'stats' },
        h('span', {}, '金币 ', h('b', {}, hero.getcoin())),
        h('span', {}, '材料 ', h('b', {}, game.run.materials)),
        h('span', {}, '药水 ', h('b', {}, game.potionCount()))),
    );
    const floor = game.floor;
    const c = chapterOf(floor);
    const rules = Campaign.rulesFor(hero, floor);
    const route = game.run.routes[c];
    const tags = [];
    if (route) tags.push(h('span', { class: 'tag route' }, Campaign.RouteNames[route]));
    if (game.cleared()) tags.push(h('span', { class: 'tag done' }, '已首通'));
    if (floor % 10 === 0) tags.push(h('span', { class: 'tag boss' }, 'BOSS'));
    else if (rules.elite) tags.push(h('span', { class: 'tag elite' }, '精英'));
    if (rules.objective !== Objective.Defeat) tags.push(h('span', { class: 'tag goal' }, Campaign.objectiveName(rules.objective)));
    this.floorEl.className = 'hud-card panel';
    this.floorEl.replaceChildren(
      h('div', { class: 'title' }, `第 ${floor} 层`),
      h('div', { class: 'line' }, `第${c + 1}章 · ${Campaign.chapter(floor).name}`),
      h('div', { class: 'line' }, tags.length ? tags : h('span', { class: 'muted' }, '待完成')),
    );
  }

  prompt(text) {
    if (!text) {
      this.promptEl.classList.add('hidden');
      return;
    }
    this.promptEl.className = 'panel';
    this.promptEl.replaceChildren(this.coarse ? '点「交互」' : h('span', {}, '按 ', h('kbd', {}, '空格'), ' '), `　${text}`);
  }
}
