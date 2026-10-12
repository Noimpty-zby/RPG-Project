// Port of src/storage/profile.{h,cpp}: bestiary, affix log, achievements and
// finished expeditions, kept across runs. Stored as JSON in the browser.
import { addCounter, challengeName, chapterOf, clamp, floorOf, MaxCounter, MaxFloor } from './config.js';
import { AffixCount, Bestiary } from './enemy.js';
import { JobProfiles, jobFromKey } from './jobs.js';

export const ProfileVersion = 2; // 2 adds expedition journal pages.
export const LorePages = 100;
export const MaxRecords = 20;

export const Achievements = [
  { key: 'first_chapter', title: '初出茅庐', description: '击败第10层Boss' },
  { key: 'halfway', title: '半程远征', description: '击败第50层Boss' },
  { key: 'summit', title: '百层登顶', description: '击败第100层Boss，完成远征' },
  { key: 'scarce_summit', title: '精打细算', description: '在补给匮乏模式下完成远征' },
  { key: 'iron_trial', title: '铁人试炼', description: '在无复苏模式下击败第30层Boss' },
  { key: 'iron_summit', title: '不灭传说', description: '在无复苏模式下完成远征' },
  { key: 'flawless_boss', title: '毫发无伤', description: '不受任何伤害击败一名Boss' },
  { key: 'no_potion_boss', title: '以身为盾', description: '不使用药水击败第50层或更高的Boss' },
  { key: 'last_stand', title: '绝境逆转', description: '以低于10%的生命击败一名Boss' },
  { key: 'interrupt_master', title: '打断专家', description: '在一场战斗中打断3次蓄力' },
  { key: 'specialist', title: '术业专攻', description: '为前两个技能都选择专精' },
  { key: 'epic_gear', title: '史诗之选', description: '装备一件史诗品质装备' },
  { key: 'all_routes', title: '条条大路', description: '在同一次远征中用三种路线各完成一章' },
  { key: 'rich', title: '腰缠万贯', description: '同时持有20000金币' },
  { key: 'affix_scholar', title: '词缀学者', description: '遭遇全部7种敌人词缀' },
  { key: 'collector', title: '博物学家', description: '在图鉴中发现全部50种敌人' },
  { key: 'all_jobs', title: '三职业远征', description: '用战士、法师、刺客分别击败第10层Boss' },
  { key: 'explorer', title: '足迹遍布', description: '单次远征探索300处地点' },
  { key: 'vault_hunter', title: '宝库猎人', description: '单次远征打开10座宝库' },
  { key: 'puzzle_master', title: '解谜大师', description: '单次远征解开全部10个章节谜题' },
  { key: 'gourmet', title: '远征厨师', description: '单次远征做20次料理' },
  { key: 'lore_keeper', title: '见闻初录', description: '收集10页见闻录' },
  { key: 'lore_complete', title: '百层见闻', description: '收集全部100页见闻录' },
];

export function todayDate() {
  const now = new Date();
  const pad = (n) => String(n).padStart(2, '0');
  return `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
}
const validDate = (date) => typeof date === 'string' && /^\d{4}-\d{2}-\d{2}$/.test(date);
const int = (value, low, high, field) => {
  if (!Number.isInteger(value) || value < low || value > high) throw new Error(`档案数值无效: ${field}`);
  return value;
};

export class Profile {
  constructor() {
    this.bestiary = Array.from({ length: Bestiary.EntryCount }, () => ({ seen: 0, won: 0, firstFloor: 0 }));
    this.affixes = new Array(AffixCount).fill(0);
    this.unlockedKeys = new Set();
    this.chapterOneJobs = 0;
    this.runRecords = [];
    this.lorePages = new Array(LorePages).fill(false);
  }

  static parse(data) {
    if (!data || typeof data !== 'object' || data.version === undefined) throw new Error('缺少档案头');
    if (data.version > ProfileVersion) {
      const error = new Error('档案来自更新的游戏版本');
      error.newer = true;
      throw error;
    }
    const profile = new Profile();
    profile.chapterOneJobs = int(data.chapterOneJobs ?? 0, 0, 7, '职业记录');
    for (const row of data.bestiary ?? []) {
      const entry = int(row.id, 0, Bestiary.EntryCount - 1, '图鉴编号');
      const record = profile.bestiary[entry];
      if (record.seen) throw new Error('图鉴重复');
      record.seen = int(row.seen, 1, MaxCounter, '遭遇次数');
      record.won = int(row.won, 0, record.seen, '战胜次数');
      record.firstFloor = int(row.firstFloor, 1, MaxFloor, '首遇楼层');
      if (chapterOf(record.firstFloor) !== Bestiary.entryChapter(entry)) throw new Error('图鉴楼层与章节不符');
    }
    for (const row of data.affixes ?? []) {
      const affix = int(row.id, 0, AffixCount - 1, '词缀编号');
      if (profile.affixes[affix]) throw new Error('词缀重复');
      profile.affixes[affix] = int(row.count, 1, MaxCounter, '词缀次数');
    }
    for (const key of data.achievements ?? []) {
      if (typeof key !== 'string' || !key || key.length > 64) throw new Error('成就键无效');
      profile.unlockedKeys.add(key);
    }
    if (data.lore !== undefined) {
      if (typeof data.lore !== 'string' || data.lore.length !== LorePages || /[^01]/.test(data.lore))
        throw new Error('见闻录记录无效');
      profile.lorePages = [...data.lore].map((c) => c === '1');
    }
    for (const row of data.records ?? []) {
      if (profile.runRecords.length >= MaxRecords) throw new Error('记录过多');
      if (!validDate(row.date)) throw new Error('记录日期无效');
      if (jobFromKey(row.job) < 0) throw new Error('记录职业无效');
      if (typeof row.name !== 'string' || !row.name || row.name.length > 256) throw new Error('记录名字无效');
      profile.runRecords.push({
        date: row.date, job: row.job, challenge: int(row.challenge, 0, 2, '挑战'), cleared: Boolean(row.cleared),
        floor: int(row.floor, 1, MaxFloor, '楼层'), level: int(row.level, 1, 10000, '等级'),
        battles: int(row.battles, 0, MaxCounter, '战斗'), turns: int(row.turns, 0, MaxCounter, '回合'),
        deaths: int(row.deaths, 0, MaxCounter, '阵亡'), potions: int(row.potions, 0, MaxCounter, '药水'),
        seed: int(row.seed, 0, 4294967295, '种子'), name: row.name,
      });
    }
    return profile;
  }
  serialize() {
    return {
      version: ProfileVersion,
      chapterOneJobs: this.chapterOneJobs,
      bestiary: this.bestiary.map((record, id) => ({ id, ...record })).filter((record) => record.seen > 0),
      affixes: this.affixes.map((count, id) => ({ id, count })).filter((row) => row.count > 0),
      achievements: [...this.unlockedKeys],
      ...(this.loreCount() ? { lore: this.lorePages.map((found) => (found ? '1' : '0')).join('') } : {}),
      records: this.runRecords,
    };
  }

  // Expedition journal pages (1..100). recordLore returns true for a new page.
  recordLore(page) {
    if (!Number.isInteger(page) || page < 1 || page > LorePages || this.lorePages[page - 1]) return false;
    this.lorePages[page - 1] = true;
    return true;
  }
  loreFound(page) { return Number.isInteger(page) && page >= 1 && page <= LorePages && this.lorePages[page - 1]; }
  loreCount() { return this.lorePages.filter(Boolean).length; }

  recordEncounter(floor, monster) {
    const record = this.bestiary[Bestiary.entryForFloor(floor)];
    if (record.seen === 0) record.firstFloor = floorOf(floor);
    record.seen = addCounter(record.seen, 1);
    for (const affix of monster.getAffixes()) this.affixes[affix] = addCounter(this.affixes[affix], 1);
  }
  recordVictory(floor) {
    const record = this.bestiary[Bestiary.entryForFloor(floor)];
    if (record.seen > record.won) ++record.won;
  }
  addRecord(record) {
    record = { ...record };
    record.name = String(record.name || '旅人').replace(/[\r\n]/g, ' ').slice(0, 256) || '旅人';
    if (!validDate(record.date)) record.date = todayDate();
    record.floor = floorOf(record.floor);
    this.runRecords.push(record);
    if (this.runRecords.length > MaxRecords) this.runRecords.splice(0, this.runRecords.length - MaxRecords);
  }

  // Unlocks every achievement whose condition now holds; returns the new ones.
  checkAchievements(hero, battle = null) {
    const run = hero.runState();
    if (run.cleared[10]) this.chapterOneJobs |= 1 << hero.job();
    const bossWin = Boolean(battle && battle.won && battle.floor % 10 === 0);
    let specialist = hero.getSkillCount() >= 2;
    for (let slot = 1; slot <= 2 && specialist; ++slot) specialist = hero.getSkill(slot).getSpecialization() !== 0;
    const epic = hero.getSaveEquipments().some((item) => item.getTier() === 3);
    const routesCompleted = new Set();
    for (let c = 0; c < 10; ++c) if (run.cleared[c * 10 + 10] && run.routes[c] !== 0) routesCompleted.add(run.routes[c]);
    const holds = (key) => {
      switch (key) {
        case 'first_chapter': return run.cleared[10] !== 0;
        case 'halfway': return run.cleared[50] !== 0;
        case 'summit': return run.cleared[100] !== 0;
        case 'scarce_summit': return run.challenge === 1 && run.cleared[100] !== 0;
        case 'iron_trial': return run.challenge === 2 && run.cleared[30] !== 0;
        case 'iron_summit': return run.challenge === 2 && run.cleared[100] !== 0;
        case 'flawless_boss': return bossWin && battle.damageTaken === 0;
        case 'no_potion_boss': return bossWin && battle.floor >= 50 && battle.potionsUsed === 0;
        case 'last_stand': return bossWin && battle.hpAfter * 10 < battle.maxHp;
        case 'interrupt_master': return Boolean(battle) && battle.interrupts >= 3;
        case 'specialist': return specialist;
        case 'epic_gear': return epic;
        case 'all_routes': return routesCompleted.size === 3;
        case 'rich': return hero.getcoin() >= 20000;
        case 'affix_scholar': return this.affixesDiscovered() === AffixCount;
        case 'collector': return this.discovered() === Bestiary.EntryCount;
        case 'all_jobs': return this.chapterOneJobs === 7;
        case 'explorer': return run.spotsExplored >= 300;
        case 'vault_hunter': return run.vaultsOpened >= 10;
        case 'puzzle_master': return run.puzzlesSolved >= 10;
        case 'gourmet': return run.mealsCooked >= 20;
        case 'lore_keeper': return this.loreCount() >= 10;
        case 'lore_complete': return this.loreCount() === LorePages;
        default: return false;
      }
    };
    const unlockedNow = [];
    for (const achievement of Achievements)
      if (!this.unlocked(achievement.key) && holds(achievement.key)) {
        this.unlockedKeys.add(achievement.key);
        unlockedNow.push(achievement);
      }
    return unlockedNow;
  }

  entry(id) { return this.bestiary[clamp(id, 0, Bestiary.EntryCount - 1)]; }
  discovered() { return this.bestiary.filter((e) => e.seen > 0).length; }
  affixSeen(affix) { return this.affixes[affix]; }
  affixesDiscovered() { return this.affixes.filter((n) => n > 0).length; }
  unlocked(key) { return this.unlockedKeys.has(key); }
  unlockedCount() { return Achievements.filter((a) => this.unlocked(a.key)).length; }
  records() { return this.runRecords; }
  bestByJob() {
    return JobProfiles.map((job) => {
      let best = 0;
      let cleared = false;
      for (const record of this.runRecords)
        if (record.job === job.saveKey) {
          best = Math.max(best, record.floor);
          cleared ||= record.cleared;
        }
      return { job: job.displayName, text: cleared ? '通关' : best ? `第${best}层` : '—' };
    });
  }
  static recordSummary(record) {
    const job = JobProfiles[jobFromKey(record.job)]?.displayName ?? '未知';
    return `${record.date} | ${record.name} | ${job} | ${challengeName(record.challenge)} | ` +
      `${record.cleared ? '通关' : `止步第${record.floor}层`} | Lv.${record.level} | 战斗 ${record.battles} | ` +
      `阵亡 ${record.deaths} | 药水 ${record.potions} | 种子 ${record.seed}`;
  }
}
