// Entry point: wires the display, input, game state, scenes and menus.
import { Display, Input, Loop } from './engine/core.js';
import { unlockAudio, sfx } from './engine/audio.js';
import { Game } from './game.js';
import { Hud } from './ui/hud.js';
import { Menus } from './ui/menus.js';
import { bindInput, fade, toast, alertBox } from './ui/dom.js';
import { ExploreScene } from './scenes/explore.js';
import { BattleScene } from './scenes/battle.js';
import { TitleScene, EndingScene } from './scenes/title.js';

const display = new Display(document.getElementById('game'));
const input = new Input();
bindInput(input);
const loop = new Loop(display);

const app = {
  display,
  input,
  loop,
  game: new Game(),
  busy: false,
  toast,
  async transition(during) {
    this.busy = true;
    try {
      await fade(during);
    } finally {
      this.busy = false;
    }
  },
  showTitle() {
    this.busy = false;
    loop.setScene(new TitleScene(this));
  },
  async startNewGame(name, job, seed, challenge) {
    await this.transition(() => {
      this.game.newGame(name, job, seed, challenge);
      this.explore = new ExploreScene(this);
      loop.setScene(this.explore);
    });
    toast('提示：Esc / ☰ 打开菜单，「帮助」介绍意图、目标、路线与装备。');
  },
  async loadSlot(slot) {
    try {
      this.game.load(slot);
    } catch (error) {
      sfx('error');
      await alertBox('读档失败', `存档文件损坏或格式错误：${error.message}`);
      return;
    }
    await this.transition(() => {
      this.explore = new ExploreScene(this);
      loop.setScene(this.explore);
    });
    toast(`欢迎回来，${this.game.hero.getname()}！当前楼层：${this.game.floor}`);
  },
  resumeExplore() {
    if (!this.explore) this.explore = new ExploreScene(this);
    loop.setScene(this.explore);
  },
  async startBattle(scene) {
    if (this.busy) return;
    this.busy = true;
    sfx('encounter');
    scene.fx.flash('#ffffff', 0.25);
    scene.fx.shake(3, 0.2);
    const { monster, rules, battle } = this.game.startBattle();
    const keep = { tx: scene.player.tx, ty: scene.player.ty, dir: scene.player.dir };
    await fade(() => {
      this.busy = false;
      loop.setScene(new BattleScene(this, {
        monster, rules, battle, floor: this.game.floor,
        onFinish: (result) => this.endBattle(scene, result, keep),
      }));
    });
  },
  async endBattle(scene, result, keep) {
    await this.transition(() => {
      scene.resume = result.alive ? keep : null;
      scene.pendingChest = result.won ? result.events.filter((e) => e.type === 'loot').map((e) => e.item) : [];
      loop.setScene(scene);
    });
    for (const line of result.worn) toast(line);
    if (result.summit) this.showEnding();
  },
  showEnding() {
    loop.setScene(new EndingScene(this));
  },
};
app.hud = new Hud(app);
app.menus = new Menus(app);
app.game.onEvent((e) => {
  if (e.type === 'achievements') for (const a of e.unlocked) { toast(`★ 成就解锁：${a.title} —— ${a.description}`, 'achievement'); sfx('levelup'); }
});

const unlock = () => unlockAudio();
addEventListener('pointerdown', unlock);
addEventListener('keydown', unlock);
window.__app = app; // Handy for debugging from the console.

app.showTitle();
loop.start();
