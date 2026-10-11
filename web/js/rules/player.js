// Port of src/player/{player,jobs}.{h,cpp}: stats, growth, backpack,
// equipment, durability and skills. The three jobs share one class.
import { Character } from './character.js';
import {
  addCounter, clamp, EnhancePercent, enhanceCost, idiv, IntMax, MaxCoins, MaxCounter,
  MaxEnhanceLevel, MaxLevel, MaxMaterials, floorOf, repairCostPerPoint,
} from './config.js';
import { Item, ItemEffect, ItemType, JobReq } from './item.js';
import { equipmentClass, Job, jobProfile } from './jobs.js';
import { createSkills, Skill } from './skills.js';
import { below } from './random.js';
import { emit } from './log.js';
import { newRunState } from './run_state.js';

function validItem(item) {
  return item.getType() >= ItemType.Weapon && item.getType() <= ItemType.Potion &&
    item.getJobRequirement() >= JobReq.AnyJob && item.getJobRequirement() <= JobReq.AssassinOnly &&
    item.getLevelRequirement() >= 1 && item.getValue() >= 0 && item.getValue() <= 1000000 &&
    item.getDurability() >= 0 && item.getMaxDurability() >= item.getDurability() &&
    item.getPrice() >= 0 && item.getQuantity() > 0 &&
    item.getTier() >= 0 && item.getTier() <= 3 && item.supportsEffect(item.getEffect()) &&
    item.getEnhanceLevel() >= 0 && item.getEnhanceLevel() <= MaxEnhanceLevel;
}
function samePotion(a, b) {
  return a.getType() === ItemType.Potion && b.getType() === ItemType.Potion &&
    a.getId() === b.getId() && a.getName() === b.getName() && a.getValue() === b.getValue() &&
    a.getPrice() === b.getPrice() && a.getLevelRequirement() === b.getLevelRequirement() &&
    a.getDurability() === b.getDurability() && a.getJobRequirement() === b.getJobRequirement() &&
    a.getTier() === b.getTier() && a.getEffect() === b.getEffect() && a.isLocked() === b.isLocked() &&
    a.getEnhanceLevel() === b.getEnhanceLevel() && a.getMaxDurability() === b.getMaxDurability();
}

export { validItem };

export class Player extends Character {
  constructor(name, job) {
    const profile = jobProfile(job);
    super(name, profile.hp, profile.attack, profile.mp);
    this.jobType = job;
    this.level = 1;
    this.exp = 0;
    this.coin = 0;
    this.free_revives = 3;
    this.critRate = profile.critRate;
    this.dodgeRate = job === Job.Assassin ? 15 : 0;
    this.highestFloor = 1;
    this.healingPercent = 100;
    this.guarding = false;
    this.guardPierced = false;
    this.combatResource = 0;
    this.suppressedHealingTotal = 0;
    this.encounterWearPending = false;
    this.run = newRunState();
    this.backpack = [];
    this.equipments = [];
    this.skills = createSkills(job);
  }
  get side() { return 'hero'; }

  getlevel() { return this.level; }
  getexp() { return this.exp; }
  getcoin() { return this.coin; }
  getfreerevives() { return this.free_revives; }
  getcritRate() { return this.critRate; }
  getHighestFloor() { return this.highestFloor; }
  setHighestFloor(floor) { this.highestFloor = Math.max(this.highestFloor, floorOf(floor)); }
  decreaserevives() { if (this.free_revives > 0) --this.free_revives; }
  resetFreeRevive() { this.free_revives = 3; }
  canWear(item) {
    return item.getJobRequirement() === JobReq.AnyJob || item.getJobRequirement() === equipmentClass(this.jobType);
  }
  job() { return this.jobType; }
  getjob() { return jobProfile(this.jobType).saveKey; }
  getJobName() { return jobProfile(this.jobType).displayName; }
  resourceName() { return jobProfile(this.jobType).resourceName; }
  runState() { return this.run; }
  getSaveBackpack() { return this.backpack; }
  getSaveEquipments() { return this.equipments; }
  getSuppressedHealingTotal() { return this.suppressedHealingTotal; }
  getCombatResource() { return this.combatResource; }
  isGuarding() { return this.guarding; }

  gainexp(amount) {
    if (amount <= 0) return;
    this.exp = addCounter(this.exp, amount);
    while (this.level < MaxLevel && this.exp >= this.level * 100) {
      this.exp -= this.level * 100;
      this.levelup();
    }
    if (this.level >= MaxLevel) this.exp = Math.min(this.exp, 9999);
  }
  gaincoin(amount) {
    if (amount > 0) this.coin = Math.min(this.coin + amount, MaxCoins);
  }
  costcoin(amount) {
    if (amount > 0 && this.coin >= amount) this.coin -= amount;
  }
  levelup() {
    if (this.level >= MaxLevel) return;
    const growth = jobProfile(this.jobType);
    ++this.level;
    this.attack = addCounter(this.attack, growth.attackGrowth);
    this.max_hp = addCounter(this.max_hp, growth.hpGrowth);
    this.max_mp = addCounter(this.max_mp, growth.mpGrowth);
    this.sethp(this.hp + Math.max(1, idiv(this.getRecoveryBaseHp() * 12, 100)));
    this.setmp(this.mp + Math.max(1, idiv(this.max_mp, 5)));
    emit('levelup', `${this.getname()} 升到 ${this.level} 级，恢复基础生命12%及最大魔力20%。`, { level: this.level });
  }
  revive() {
    this.clearCombatModifiers();
    this.combatResource = 0;
    this.hp = this.max_hp;
    emit('revive', `${this.getname()}复苏，HP ${this.hp}。`, { side: 'hero' });
  }
  getRecoveryBaseHp() {
    let base = this.max_hp;
    for (const item of this.equipments) if (item.getType() === ItemType.Armor) base -= item.getValue();
    return clamp(base, 1, MaxCounter);
  }
  heal(amount) {
    if (amount <= 0 || !this.isalive()) return;
    const available = Math.max(0, this.max_hp - this.hp);
    if (available === 0) return;
    const unsuppressed = Math.min(available, amount);
    const effective = idiv(amount * this.healingPercent, 100);
    const restored = Math.min(available, effective);
    this.suppressedHealingTotal = addCounter(this.suppressedHealingTotal, unsuppressed - restored);
    this.hp += restored;
    let text = `HP 恢复 ${restored}`;
    if (this.healingPercent < 100) text += `（抑疗后效果 ${this.healingPercent}%）`;
    emit('heal', text + '。', { side: 'hero', amount: restored, suppressed: this.healingPercent < 100,
      hp: this.hp, maxHp: this.max_hp });
  }
  gainMana(amount) {
    const before = this.mp;
    this.setmp(this.mp + amount);
    if (this.mp > before) emit('mana', `恢复 ${this.mp - before} MP。`, { side: 'hero', amount: this.mp - before });
  }
  setHealingPercent(percent) { this.healingPercent = clamp(percent, 0, 100); }
  guard(pierced) {
    if (!this.isalive()) return;
    this.guarding = true;
    this.guardPierced = pierced;
    const recovered = (this.jobType === Job.Warrior ? 6 : 4) + 3 * this.equipmentEffect(ItemEffect.GuardRecovery);
    this.setmp(this.mp + recovered);
    if (this.jobType === Job.Warrior) this.addCombatResource(1);
    emit('guard', `格挡本回合伤害，恢复 ${recovered} MP。`, { side: 'hero', amount: recovered, pierced });
  }
  clearCombatModifiers() {
    this.healingPercent = 100;
    this.guarding = false;
    this.guardPierced = false;
  }
  addCombatResource(amount) { this.combatResource = clamp(this.combatResource + amount, 0, 2); }
  consumeCombatResource() {
    const amount = this.combatResource;
    this.combatResource = 0;
    return amount;
  }
  resetEncounterResources() {
    this.combatResource = 0;
    this.encounterWearPending = true;
    this.clearCombatModifiers();
  }
  restoreTurnResources() { this.setmp(this.mp + (this.jobType === Job.Mage ? 3 : 2)); }

  attacktarget(target) { this.attacktargetDealt(target, this.attack); }
  attacktargetDealt(target, baseDamage) {
    if (!this.isalive() || !target.isalive() || baseDamage < 0) return;
    let damage = baseDamage;
    const crit = below(100) < clamp(this.critRate, 0, 100);
    if (crit) damage *= 2;
    emit('attack', crit ? '暴击！' : '', { side: 'hero', crit });
    const before = target.gethp();
    target.takedamage(Math.min(damage, MaxCounter));
    if (target.gethp() < before) {
      const mana = (this.jobType === Job.Mage ? 8 : this.jobType === Job.Assassin ? 5 : 3) +
        3 * this.equipmentEffect(ItemEffect.ManaOnAttack);
      this.setmp(this.mp + mana);
      this.addCombatResource(1);
      emit('attackHit', `普攻命中：恢复 ${mana} MP，${this.resourceName()} ${this.combatResource}/2。`,
        { side: 'hero', mana, resource: this.combatResource });
    }
  }
  takedamage(damage) {
    // Assassins' ghost step dodges attacks; poison and blasts bypass this.
    if (this.jobType === Job.Assassin) {
      if (damage <= 0) return;
      if (below(100) < this.dodgeRate) {
        emit('dodge', '【鬼步】闪避！', { side: 'hero' });
        return;
      }
    }
    damage = Math.max(0, damage);
    const guarded = this.guarding && !this.guardPierced;
    if (guarded) damage = idiv(damage, 2) + (damage % 2);
    this.takeUnavoidableDamage(damage, guarded);
  }
  takeUnavoidableDamage(damage, guarded = false) {
    damage = Math.max(0, damage);
    this.hp = Math.max(0, this.hp - damage);
    emit('damage', `${this.getname()} 受到 ${damage} 伤害（HP ${this.hp}/${this.max_hp}）。`,
      { side: 'hero', amount: damage, hp: this.hp, maxHp: this.max_hp, guarded });
  }
  finishEncounterWear() {
    if (!this.encounterWearPending) return;
    this.encounterWearPending = false;
    for (let i = this.equipments.length - 1; i >= 0; --i) {
      const item = this.equipments[i];
      item.setDurability(Math.max(0, item.getDurability() - 1));
      if (item.getDurability() > 0) continue;
      emit('broken', `【装备破损】${item.getDisplayName()}`, { name: item.getDisplayName() });
      if (item.getType() === ItemType.Weapon) this.attack = Math.max(0, this.attack - item.getValue());
      if (item.getType() === ItemType.Armor) {
        this.max_hp = Math.max(1, this.max_hp - item.getValue());
        this.hp = Math.min(this.hp, this.max_hp);
      }
      this.equipments.splice(i, 1);
    }
    this.combatResource = 0;
    this.clearCombatModifiers();
  }
  equipmentEffect(effectId) {
    if (effectId < 1 || effectId > 4) return 0;
    let count = 0;
    for (const item of this.equipments)
      if (item.getDurability() > 0 && item.getEffect() === effectId) ++count;
    return Math.min(2, count);
  }

  addItem(item) {
    if (!validItem(item)) return;
    for (const existing of this.backpack) {
      if (samePotion(existing, item)) {
        const combined = existing.getQuantity() + item.getQuantity();
        if (combined <= IntMax) {
          existing.setQuantity(combined);
          return;
        }
      }
    }
    this.backpack.push(item.clone());
  }
  // 1-based indices, as in the C++ menus.
  useItem(index) {
    if (!this.isalive() || index < 1 || index > this.backpack.length) return false;
    const item = this.backpack[index - 1];
    if (item.getType() !== ItemType.Potion || this.hp >= this.max_hp || item.getValue() <= 0 ||
      idiv(item.getValue() * this.healingPercent, 100) <= 0) return false;
    emit('potion', `使用 ${item.getName()}。`, { side: 'hero', name: item.getName() });
    this.heal(item.getValue());
    this.run.potionsUsed = addCounter(this.run.potionsUsed, 1);
    item.setQuantity(item.getQuantity() - 1);
    if (item.getQuantity() <= 0) this.backpack.splice(index - 1, 1);
    return true;
  }
  // Potion choice shared by planPotions and drinkPotionsTo.
  potionHeal(item) {
    if (item.getType() !== ItemType.Potion || item.getValue() <= 0) return 0;
    return idiv(item.getValue() * this.healingPercent, 100);
  }
  nextPotion(left, missing) {
    let fit = 0;
    let smallest = 0;
    for (let i = 0; i < this.backpack.length; i++) {
      const heal = this.potionHeal(this.backpack[i]);
      if (left[i] <= 0 || heal <= 0) continue;
      if (heal <= missing && (!fit || heal > this.potionHeal(this.backpack[fit - 1]))) fit = i + 1;
      if (!smallest || heal < this.potionHeal(this.backpack[smallest - 1])) smallest = i + 1;
    }
    return fit || smallest;
  }
  // Out-of-battle potions up to targetHp: each drink takes the largest potion
  // that does not overshoot the HP still missing, or the smallest one when
  // every potion would. planPotions consumes nothing; drinkPotionsTo makes the
  // same choices and returns the HP restored. Overflow is healing past max HP.
  planPotions(targetHp) {
    const plan = { uses: [], restored: 0, overflow: 0 };
    if (!this.isalive()) return plan;
    const left = this.backpack.map((item) => item.getQuantity());
    const target = clamp(targetHp, 0, this.max_hp);
    for (let current = this.hp; current < target;) {
      const slot = this.nextPotion(left, target - current);
      if (!slot) break;
      const heal = this.potionHeal(this.backpack[slot - 1]);
      const restored = Math.min(heal, this.max_hp - current);
      plan.restored += restored;
      plan.overflow += heal - restored;
      current += restored;
      --left[slot - 1];
      const use = plan.uses.find((u) => u.slot === slot);
      if (use) ++use.count;
      else plan.uses.push({ slot, count: 1 });
    }
    return plan;
  }
  drinkPotionsTo(targetHp) {
    const before = this.hp;
    const target = clamp(targetHp, 0, this.max_hp);
    while (this.hp < target) {
      const slot = this.nextPotion(this.backpack.map((item) => item.getQuantity()), target - this.hp);
      if (!slot || !this.useItem(slot)) break;
    }
    return this.hp - before;
  }
  canEquip(item) {
    return item.getType() !== ItemType.Potion && item.getLevelRequirement() <= this.level &&
      item.getDurability() > 0 && this.canWear(item);
  }
  equipItem(index) {
    if (index < 1 || index > this.backpack.length) return false;
    const selected = this.backpack[index - 1].clone();
    if (!this.canEquip(selected)) {
      emit('notice', '该物品当前不可装备。');
      return false;
    }
    if (selected.getQuantity() === 1) this.backpack.splice(index - 1, 1);
    else this.backpack[index - 1].setQuantity(selected.getQuantity() - 1);
    selected.setQuantity(1);
    this.equipItem_T(selected);
    return true;
  }
  unequipItem(index) {
    if (index < 1 || index > this.equipments.length) return;
    const removed = this.equipments[index - 1];
    this.equipments.splice(index - 1, 1);
    if (removed.getType() === ItemType.Weapon) this.attack = Math.max(0, this.attack - removed.getValue());
    if (removed.getType() === ItemType.Armor) {
      this.max_hp = Math.max(1, this.max_hp - removed.getValue());
      this.hp = Math.min(this.hp, this.max_hp);
    }
    this.addItem(removed);
  }
  equipItem_T(item, restoring = false) {
    const selected = item.clone();
    if (!validItem(selected) || selected.getType() === ItemType.Potion || selected.getDurability() <= 0 ||
      !this.canWear(selected) || (!restoring && selected.getLevelRequirement() > this.level)) {
      if (!restoring) this.addItem(selected);
      return;
    }
    let resultingStat = selected.getType() === ItemType.Weapon ? this.attack : this.max_hp;
    for (const current of this.equipments)
      if (current.getType() === selected.getType()) resultingStat -= current.getValue();
    resultingStat += selected.getValue();
    if (resultingStat > MaxCounter) {
      if (!restoring) this.addItem(selected);
      return;
    }
    const hpBeforeSwap = this.hp;
    for (let i = this.equipments.length - 1; i >= 0; --i)
      if (this.equipments[i].getType() === selected.getType()) this.unequipItem(i + 1);
    selected.setQuantity(1);
    if (selected.getType() === ItemType.Weapon) this.attack = addCounter(this.attack, selected.getValue());
    else this.max_hp = addCounter(this.max_hp, selected.getValue());
    this.hp = Math.min(hpBeforeSwap, this.max_hp);
    this.equipments.push(selected);
  }
  equippedOfType(type) { return this.equipments.find((item) => item.getType() === type) ?? null; }
  sellItem(index, quantity) {
    if (index < 1 || index > this.backpack.length) return 0;
    const item = this.backpack[index - 1];
    if (item.isLocked() || quantity < 1 || quantity > item.getQuantity()) return 0;
    const earned = Math.min(idiv(item.getPrice(), 3) * quantity, MaxCoins);
    this.gaincoin(earned);
    if (quantity === item.getQuantity()) this.backpack.splice(index - 1, 1);
    else item.setQuantity(item.getQuantity() - quantity);
    return earned;
  }
  toggleItemLock(index) {
    if (index < 1 || index > this.backpack.length) return false;
    const item = this.backpack[index - 1];
    item.setLocked(!item.isLocked());
    return true;
  }
  salvageUnlocked() {
    let gained = 0;
    for (let i = this.backpack.length - 1; i >= 0; --i) {
      const item = this.backpack[i];
      if (item.isLocked() || item.getType() === ItemType.Potion) continue;
      const quantity = item.getQuantity() * (item.getTier() + 1);
      const space = MaxMaterials - this.run.materials;
      if (quantity > space) continue;
      this.run.materials += quantity;
      gained += quantity;
      this.backpack.splice(i, 1);
    }
    return gained;
  }
  modifyCost(index) {
    if (index < 1 || index > this.equipments.length) return 0;
    return 3 + this.equipments[index - 1].getTier() * 2;
  }
  modifyEquipment(index, effectId) {
    if (index < 1 || index > this.equipments.length || effectId < 1 || effectId > 4) return false;
    const item = this.equipments[index - 1];
    if (item.getEffect() === effectId) return false;
    const cost = 3 + item.getTier() * 2;
    if (this.run.materials < cost || !item.setEffect(effectId)) return false;
    this.run.materials -= cost;
    return true;
  }
  enhanceCost(index) {
    if (index < 1 || index > this.equipments.length) return 0;
    const item = this.equipments[index - 1];
    if (item.getEnhanceLevel() >= MaxEnhanceLevel) return 0;
    return enhanceCost(item.getPrice(), item.getEnhanceLevel() + 1);
  }
  enhanceGain(index) {
    if (index < 1 || index > this.equipments.length) return 0;
    return Math.max(1, idiv(this.equipments[index - 1].getValue() * EnhancePercent, 100));
  }
  enhanceEquipment(index) {
    const cost = this.enhanceCost(index);
    if (cost <= 0 || this.coin < cost) return false;
    const item = this.equipments[index - 1];
    const gain = Math.max(1, idiv(item.getValue() * EnhancePercent, 100));
    if (item.getValue() + gain > 1000000) return false;
    this.costcoin(cost);
    item.setValue(item.getValue() + gain);
    item.setEnhanceLevel(item.getEnhanceLevel() + 1);
    if (item.getType() === ItemType.Weapon) this.attack = addCounter(this.attack, gain);
    else this.max_hp = addCounter(this.max_hp, gain);
    return true;
  }
  repairCost(index) {
    if (index < 1 || index > this.equipments.length) return 0;
    const item = this.equipments[index - 1];
    const missing = Math.max(0, item.getMaxDurability() - item.getDurability());
    return Math.min(missing * repairCostPerPoint(item.getPrice(), item.getTier()), MaxCoins);
  }
  repairEquipment(index) {
    const cost = this.repairCost(index);
    if (cost <= 0 || this.coin < cost) return false;
    this.costcoin(cost);
    const item = this.equipments[index - 1];
    item.setDurability(item.getMaxDurability());
    return true;
  }

  useSkill(index, target) {
    const ability = this.getSkill(index);
    return Boolean(ability && ability.useSkill(this, target));
  }
  getSkill(index) { return index >= 1 && index <= this.skills.length ? this.skills[index - 1] : null; }
  getSkillCount() { return this.skills.length; }
  reduceallCooldowns() { for (const ability of this.skills) ability.reducecooldown(); }
  levelUpSkill(index) {
    const ability = this.getSkill(index);
    if (!ability) return false;
    const cost = ability.getlevelUpCost();
    if (!ability.levelUpSkill(this.level, this.coin, this.highestFloor)) return false;
    this.costcoin(cost);
    return true;
  }
  respecCost() { return 100 + this.highestFloor * 5; }
  chooseSpecialization(slot, branch) {
    const ability = this.getSkill(slot);
    if (!ability || slot > 2 || branch < 0 || branch > 2 ||
      this.highestFloor < Skill.specializationFloor(slot) || this.level < ability.getlevelrequired() ||
      ability.getSpecialization() === branch || this.encounterWearPending) return false;
    const cost = ability.getSpecialization() === 0 ? 0 : this.respecCost();
    if (this.coin < cost || !ability.setSpecialization(branch)) return false;
    this.costcoin(cost);
    return true;
  }
  potionCount() {
    let count = 0;
    for (const item of this.backpack) if (item.getType() === ItemType.Potion) count += item.getQuantity();
    return count;
  }
  potionIndex() {
    const index = this.backpack.findIndex((item) => item.getType() === ItemType.Potion);
    return index + 1;
  }
  loadStats(level, exp, coin, hp, maxHp, attack, mp, maxMp, critRate, freeRevives) {
    this.level = clamp(level, 1, 10000);
    this.exp = Math.max(0, exp);
    this.coin = clamp(coin, 0, MaxCoins);
    this.max_hp = clamp(maxHp, 1, MaxCounter);
    this.hp = clamp(hp, 0, this.max_hp);
    this.attack = clamp(attack, 0, MaxCounter);
    this.max_mp = clamp(maxMp, 0, MaxCounter);
    this.mp = clamp(mp, 0, this.max_mp);
    this.critRate = clamp(critRate, 0, 100);
    this.free_revives = clamp(freeRevives, 0, 1000);
    this.clearCombatModifiers();
    this.combatResource = 0;
    this.encounterWearPending = false;
  }
}

export function createHero(name, job) { return new Player(name, job); }
export { Item };
