// Port of src/item/item.{h,cpp}. C++ copies items by value; callers here use
// clone() wherever the C++ code makes a copy.

export const ItemType = { Weapon: 1, Armor: 2, Potion: 3 };
export const JobReq = { AnyJob: 0, WarriorOnly: 1, MageOnly: 2, AssassinOnly: 3 };
export const ItemEffect = { None: 0, ManaOnAttack: 1, GuardRecovery: 2, SkillEfficiency: 3, ControlFocus: 4 };

const encoder = new TextEncoder();
const FnvOffset = 14695981039346656037n;
const FnvPrime = 1099511628211n;
const Mask = (1n << 64n) - 1n;

export function fnv1a(bytes, hash = FnvOffset) {
  for (const byte of bytes) {
    hash ^= BigInt(byte);
    hash = (hash * FnvPrime) & Mask;
  }
  return hash;
}
export const utf8 = (text) => encoder.encode(text);

const TierNames = ['普通', '精良', '稀有', '史诗'];
const EffectNames = ['无', '普攻回蓝', '格挡回蓝', '技能节能', '控制强化'];
const EffectDescriptions = [
  '无',
  '普攻回蓝：普攻命中额外回复3MP（仅武器）',
  '格挡回蓝：格挡额外回复3MP（仅防具）',
  '技能节能：固定耗蓝技能消耗-10%',
  '控制强化：减攻控制延长1次行动（战士/法师装备）',
];
const JobReqNames = ['通用', '战士专用', '法师专用', '刺客专用'];

export class Item {
  constructor(name, levelRequirement, type, value, durability, price, quantity,
    jobRequirement = JobReq.AnyJob, tier = 0, effect = ItemEffect.None, locked = false, id = '') {
    this.name = name;
    this.levelRequirement = levelRequirement;
    this.type = type;
    this.value = value;
    this.durability = durability;
    this.maxDurability = durability;
    this.price = price;
    this.quantity = quantity;
    this.jobRequirement = jobRequirement;
    this.tier = tier;
    this.effect = effect;
    this.locked = locked;
    this.id = id || Item.templateId(name, type, jobRequirement, tier);
    this.enhanceLevel = 0;
  }

  clone() {
    const copy = Object.create(Item.prototype);
    Object.assign(copy, this);
    return copy;
  }

  static templateId(name, type, job, tier) {
    const key = `${job}:${type}:${tier}:${name}`;
    return 'item-' + fnv1a(utf8(key)).toString(16).padStart(16, '0');
  }

  getName() { return this.name; }
  getDisplayName() { return this.enhanceLevel > 0 ? `${this.name} +${this.enhanceLevel}` : this.name; }
  getEnhanceLevel() { return this.enhanceLevel; }
  setEnhanceLevel(level) { this.enhanceLevel = level; }
  getLevelRequirement() { return this.levelRequirement; }
  getType() { return this.type; }
  getValue() { return this.value; }
  getDurability() { return this.durability; }
  getMaxDurability() { return this.maxDurability; }
  getPrice() { return this.price; }
  getQuantity() { return this.quantity; }
  getJobRequirement() { return this.jobRequirement; }
  getTier() { return this.tier; }
  getEffect() { return this.effect; }
  isLocked() { return this.locked; }
  getId() { return this.id; }
  getTierName() { return TierNames[this.tier] ?? '普通'; }
  static effectName(effect) { return EffectNames[effect] ?? '无'; }
  static effectDescription(effect) { return EffectDescriptions[effect] ?? '无'; }
  getEffectName() { return Item.effectName(this.effect); }
  getEffectDescription() { return Item.effectDescription(this.effect); }
  getJobRequirementName() { return JobReqNames[this.jobRequirement] ?? '通用'; }

  supportsEffect(candidate) {
    if (candidate === ItemEffect.None) return true;
    if (this.type !== ItemType.Weapon && this.type !== ItemType.Armor) return false;
    if (candidate === ItemEffect.ManaOnAttack) return this.type === ItemType.Weapon;
    if (candidate === ItemEffect.GuardRecovery) return this.type === ItemType.Armor;
    if (candidate === ItemEffect.SkillEfficiency) return true;
    if (candidate === ItemEffect.ControlFocus)
      return this.jobRequirement === JobReq.WarriorOnly || this.jobRequirement === JobReq.MageOnly;
    return false;
  }
  setEffect(candidate) {
    if (!this.supportsEffect(candidate)) return false;
    this.effect = candidate;
    return true;
  }
  setLocked(value) { this.locked = value; }
  setQuantity(q) { this.quantity = q; }
  setValue(v) { this.value = v; }
  setDurability(d) { this.durability = d; }
  setMaxDurability(v) { this.maxDurability = v; }
}
