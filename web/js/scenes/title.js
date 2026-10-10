// Title, character creation and ending screens. The canvas shows the
// floating tower; menus are DOM.
import { h, button, choose } from '../ui/dom.js';
import { JobProfiles } from '../rules/jobs.js';
import { challengeName } from '../rules/config.js';
import * as Sprites from '../engine/sprites.js';
import { playMusic, sfx } from '../engine/audio.js';
import { P } from '../engine/palette.js';
import { hash2 } from '../engine/tiles.js';

const JobBlurbs = [
  '生命最高。普攻命中或格挡积累「战意」，格挡回复更多 MP；反击、坚守、振奋等专精。',
  '魔力最高，每回合多回复 MP，普攻命中回复 8 MP。用「专注」在恢复、控制和爆发之间安排回合。',
  '暴击最高，15% 几率闪避攻击。积累「破绽」后爆发，或改用穿盾与干扰。',
];
const ChallengeBlurbs = [
  '每局 3 次免费复苏，之后花费三分之一金币复苏。',
  '资源紧张：每章只能休息一次，商店 +25%，开局药水更少；敌人更难缠。',
  '敌人强度同标准，但只有一条命：阵亡即结束远征。',
];

export class TitleScene {
  constructor(app) {
    this.app = app;
    this.time = 0;
    this.mode = 'title';
  }
  enter() {
    this.app.display.setMode('explore');
    this.app.hud.show(false);
    playMusic('title');
    this.showMenu();
  }
  exit() {
    document.getElementById('screen').classList.add('hidden');
  }
  screen(content) {
    const el = document.getElementById('screen');
    el.classList.remove('hidden');
    el.replaceChildren(content);
    return el;
  }

  showMenu() {
    this.mode = 'title';
    const game = this.app.game;
    const auto = game.slotInfo('auto');
    const manual = game.slotInfo('manual');
    const describe = (s) => `${s.name} · ${s.job} Lv.${s.level} · 第${s.floor}层 · ${challengeName(s.challenge)}`;
    this.screen(h('div', { class: 'title-wrap' },
      h('h1', { class: 'logo' }, 'AINCRAD'),
      h('div', { class: 'logo-sub' }, '百 层 远 征'),
      h('div', { class: 'menu-list' },
        auto ? button('继续', () => this.app.loadSlot('auto'), { cls: 'primary', sub: describe(auto) }) : null,
        manual ? button('读取存档', () => this.app.loadSlot('manual'), { sub: describe(manual) }) : null,
        button('新的远征', () => this.showCreate(), { cls: auto ? '' : 'primary' }),
        button('远征档案', () => this.app.menus.openArchive()),
        button('帮助', () => this.app.menus.openHelp()),
      ),
      game.profileWarning ? h('p', { class: 'bad', style: { fontSize: '13px' } }, game.profileWarning) : null,
      h('div', { class: 'title-foot' },
        '方向键 / WASD 移动 · 空格 交互 · Esc 菜单 · 战斗中数字键选择行动', h('br'),
        '规则与 C++ 终端版逐位一致：同一个种子在任何平台都生成相同的远征。', h('br'),
        h('a', { href: 'https://github.com/Noimpty-zby/RPG-Project', target: '_blank', rel: 'noopener' }, '源代码'))));
  }

  showCreate() {
    this.mode = 'create';
    let job = 0;
    let challenge = 0;
    const nameInput = h('input', { type: 'text', maxlength: 24, placeholder: '旅人', value: '' });
    const seedInput = h('input', { type: 'number', min: 0, max: 2147483647, placeholder: '0 = 随机', value: '' });
    const jobCards = h('div', { class: 'cards' });
    const challengeCards = h('div', { class: 'cards' });
    const renderCards = () => {
      jobCards.replaceChildren(...JobProfiles.map((profile, i) => {
        const sprite = Sprites.heroBattle(i).idle;
        const img = h('img', { src: Sprites.spriteUrl(`hero-${i}`, sprite), width: 96, height: 96, class: 'px', alt: '' });
        img.style.imageRendering = 'pixelated';
        return h('button', { class: `card ${job === i ? 'active' : ''}`, onclick: () => { sfx('click'); job = i; renderCards(); } },
          img, h('h4', {}, profile.displayName),
          h('p', {}, `生命 ${profile.hp} · 攻击 ${profile.attack} · 魔力 ${profile.mp} · 暴击 ${profile.critRate}%`),
          h('p', {}, JobBlurbs[i]));
      }));
      challengeCards.replaceChildren(...[0, 1, 2].map((c) => h('button', {
        class: `card ${challenge === c ? 'active' : ''}`, onclick: () => { sfx('click'); challenge = c; renderCards(); },
      }, h('h4', {}, challengeName(c)), h('p', {}, ChallengeBlurbs[c]))));
    };
    renderCards();
    const start = async () => {
      const name = nameInput.value.trim() || '旅人';
      const seed = Math.max(0, Math.min(2147483647, Math.trunc(Number(seedInput.value) || 0)));
      if (this.app.game.slotInfo('auto')) {
        const ok = await choose('开始新的远征', '自动存档会被新远征覆盖（手动存档保留）。', [{ label: '开始', value: true, cls: 'primary' }, { label: '取消', value: false }]);
        if (!ok) return;
      }
      this.app.startNewGame(name, job, seed, challenge);
    };
    this.screen(h('div', { class: 'create panel' },
      h('h2', {}, '新的远征'),
      h('label', { class: 'field' }, '角色名字'), nameInput,
      h('label', { class: 'field' }, '职业'), jobCards,
      h('label', { class: 'field' }, '挑战模式'), challengeCards,
      h('label', { class: 'field' }, '远征种子（1..2147483647，0 或留空为随机；同种子同选择可复现）'), seedInput,
      h('div', { class: 'choice-row', style: { marginTop: '14px', justifyContent: 'flex-end' } },
        button('返回', () => this.showMenu()),
        button('出发！', start, { cls: 'primary' }))));
    setTimeout(() => nameInput.focus(), 50);
  }

  update(dt) {
    this.time += dt;
    const keys = this.app.input.take();
    if (keys.includes('menu') && this.mode === 'create') this.showMenu();
  }

  draw(ctx) {
    const view = this.app.display;
    const w = view.viewW;
    const hgt = view.viewH;
    const t = this.time;
    const sky = ctx.createLinearGradient(0, 0, 0, hgt);
    sky.addColorStop(0, '#05061a');
    sky.addColorStop(0.6, '#2a1a5a');
    sky.addColorStop(1, '#b55088');
    ctx.fillStyle = sky;
    ctx.fillRect(0, 0, w, hgt);
    for (let i = 0; i < 160; i++) {
      const x = hash2(i, 1, 5) * w;
      const y = hash2(i, 2, 5) * hgt * 0.7;
      const tw = Math.sin(t * (1 + hash2(i, 3, 5) * 3) + i) > 0.6;
      ctx.fillStyle = tw ? '#ffffff' : '#8b9bb4';
      ctx.fillRect(Math.round(x), Math.round(y), 1, 1);
    }
    // The floating tower: stacked tiers that narrow toward the top.
    const wide = w > hgt * 1.2;
    const cx = wide ? w * 0.76 : w / 2;
    const base = hgt * (wide ? 0.62 : 0.72) + Math.sin(t * 0.8) * 2;
    ctx.globalAlpha = wide ? 1 : 0.45;
    for (let tier = 0; tier < 10; tier++) {
      const tw = 120 - tier * 10;
      const th = 9;
      const y = base - tier * (th + 1);
      ctx.fillStyle = tier % 2 ? '#3a4466' : '#262b44';
      ctx.fillRect(Math.round(cx - tw / 2), Math.round(y - th), tw, th);
      ctx.fillStyle = '#5a6988';
      ctx.fillRect(Math.round(cx - tw / 2), Math.round(y - th), tw, 1);
      for (let x = -tw / 2 + 4; x < tw / 2 - 2; x += 7) {
        const lit = Math.sin(t * 2 + x + tier) > 0.3;
        ctx.fillStyle = lit ? '#fee761' : '#181425';
        ctx.fillRect(Math.round(cx + x), Math.round(y - th + 4), 2, 3);
      }
    }
    ctx.fillStyle = '#2ce8f5';
    ctx.globalAlpha = 0.6 + Math.sin(t * 3) * 0.3;
    ctx.fillRect(Math.round(cx - 1), Math.round(base - 104), 2, 6);
    ctx.globalAlpha = 1;
    // Underside rocks.
    ctx.fillStyle = '#181425';
    ctx.beginPath();
    ctx.moveTo(cx - 62, base);
    ctx.lineTo(cx + 62, base);
    ctx.lineTo(cx + 20, base + 26);
    ctx.lineTo(cx - 8, base + 40);
    ctx.lineTo(cx - 30, base + 20);
    ctx.closePath();
    ctx.fill();
    ctx.globalAlpha = 1;
    // Clouds.
    for (let i = 0; i < 6; i++) {
      const x = ((hash2(i, 7, 5) * w + t * (4 + i)) % (w + 80)) - 40;
      const y = hgt * 0.5 + hash2(i, 8, 5) * hgt * 0.4;
      ctx.fillStyle = 'rgba(246,117,122,0.25)';
      ctx.fillRect(Math.round(x), Math.round(y), 50, 4);
      ctx.fillRect(Math.round(x + 10), Math.round(y - 3), 26, 3);
    }
    // The three heroes on a cliff.
    const cliffY = hgt - 22;
    ctx.fillStyle = '#0e0b16';
    ctx.fillRect(0, cliffY, w, hgt - cliffY);
    ctx.fillStyle = '#262b44';
    ctx.fillRect(0, cliffY, w, 2);
    for (let job = 0; job < 3; job++) {
      const sprite = Sprites.heroBattle(job).idle;
      const x = Math.round(w * 0.12 + job * 26);
      const bob = Math.round(Math.sin(t * 3 + job) * 0.8);
      ctx.drawImage(sprite, x, cliffY - 31 + bob);
    }
    if (Math.random() < 0.05) {
      const sx = Math.random() * w;
      this.shooting = { x: sx, y: Math.random() * hgt * 0.3, t: 0 };
    }
    if (this.shooting) {
      const s = this.shooting;
      s.t += 1 / 60;
      ctx.strokeStyle = `rgba(255,255,255,${1 - s.t * 2})`;
      ctx.beginPath();
      ctx.moveTo(s.x - s.t * 200, s.y + s.t * 80);
      ctx.lineTo(s.x - s.t * 200 + 10, s.y + s.t * 80 - 4);
      ctx.stroke();
      if (s.t > 0.5) this.shooting = null;
    }
  }
}

export class EndingScene {
  constructor(app) { this.app = app; this.time = 0; }
  enter() {
    const game = this.app.game;
    const hero = game.hero;
    const run = game.run;
    this.app.hud.show(false);
    playMusic('title');
    sfx('victory');
    const el = document.getElementById('screen');
    el.classList.remove('hidden');
    el.replaceChildren(h('div', { class: 'title-wrap panel', style: { padding: '20px' } },
      h('h1', { class: 'logo', style: { fontSize: 'clamp(28px, 6vw, 48px)' } }, '百层登顶'),
      h('p', { class: 'logo-sub', style: { letterSpacing: '4px' } }, '恭喜通关百层远征！'),
      h('p', {}, `${hero.getname()} · ${hero.getJobName()} Lv.${hero.getlevel()} · ${challengeName(run.challenge)}模式`),
      h('p', { class: 'muted' }, `战斗 ${run.battles} · 回合 ${run.turns} · 阵亡 ${run.deaths} · 药水 ${run.potionsUsed} · 种子 ${run.seed}`),
      h('p', { class: 'muted' }, '本次远征已记入档案。可以继续在塔中补练，或开始新的远征。'),
      h('div', { class: 'menu-list', style: { marginTop: '14px' } },
        button('留在塔中', () => { el.classList.add('hidden'); this.app.resumeExplore(); }, { cls: 'primary' }),
        button('返回标题', () => this.app.showTitle()))));
  }
  exit() { document.getElementById('screen').classList.add('hidden'); }
  update(dt) { this.time += dt; this.app.input.take(); }
  draw(ctx) {
    const view = this.app.display;
    const t = this.time;
    ctx.fillStyle = '#05061a';
    ctx.fillRect(0, 0, view.viewW, view.viewH);
    for (let i = 0; i < 120; i++) {
      const x = hash2(i, 4, 9) * view.viewW;
      const y = (hash2(i, 5, 9) * view.viewH + t * (10 + (i % 5) * 6)) % view.viewH;
      ctx.fillStyle = i % 3 ? P.yellow : P.white;
      ctx.fillRect(Math.round(x), Math.round(view.viewH - y), 1, 2);
    }
  }
}

