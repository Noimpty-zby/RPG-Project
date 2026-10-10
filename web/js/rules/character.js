// Port of src/character/character.{h,cpp}.
import { clamp } from './config.js';
import { emit } from './log.js';

export class Character {
  constructor(name, hp, attack, mp) {
    this.name = name;
    this.hp = hp;
    this.attack = attack;
    this.mp = mp;
    this.max_hp = hp;
    this.max_mp = mp;
  }
  // 'hero' or 'enemy': which side of the battle stage events refer to.
  get side() { return 'enemy'; }
  getname() { return this.name; }
  gethp() { return this.hp; }
  getattack() { return this.attack; }
  getmp() { return this.mp; }
  getmaxhp() { return this.max_hp; }
  getmaxmp() { return this.max_mp; }
  isalive() { return this.hp > 0; }
  setmp(value) { this.mp = clamp(value, 0, this.max_mp); }
  sethp(value) { this.hp = clamp(value, 0, this.max_hp); }
  takedamage(damage) {
    damage = Math.max(0, damage);
    this.hp = Math.max(0, this.hp - damage);
    emit('damage', `${this.getname()} 受到 ${damage} 伤害（HP ${this.hp}/${this.max_hp}）。`,
      { side: this.side, amount: damage, hp: this.hp, maxHp: this.max_hp });
  }
}
