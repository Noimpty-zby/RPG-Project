// Job table from src/player/player.cpp: base stats and growth in one place.
import { JobReq } from './item.js';

export const Job = { Warrior: 0, Mage: 1, Assassin: 2 };

export const JobProfiles = [
  { saveKey: 'Warrior', displayName: '战士', resourceName: '战意', hp: 200, attack: 18, mp: 24, critRate: 8,
    hpGrowth: 20, attackGrowth: 3, mpGrowth: 3 },
  { saveKey: 'Mage', displayName: '法师', resourceName: '专注', hp: 150, attack: 12, mp: 40, critRate: 12,
    hpGrowth: 17, attackGrowth: 2, mpGrowth: 8 },
  { saveKey: 'Assassin', displayName: '刺客', resourceName: '破绽', hp: 150, attack: 15, mp: 25, critRate: 20,
    hpGrowth: 15, attackGrowth: 3, mpGrowth: 4 },
];

export const jobProfile = (job) => JobProfiles[job];
export const equipmentClass = (job) =>
  job === Job.Warrior ? JobReq.WarriorOnly : job === Job.Mage ? JobReq.MageOnly : JobReq.AssassinOnly;
export const jobFromKey = (key) => JobProfiles.findIndex((profile) => profile.saveKey === key);
