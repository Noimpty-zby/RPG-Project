// Shared colors (based on the Endesga 32 palette) so every sprite, tile and
// effect reads as one art style.
export const P = {
  rust: '#be4a2f', orange2: '#d77643', sand: '#ead4aa', tan: '#e4a672', clay: '#b86f50', brown: '#733e39',
  plum: '#3e2731', wine: '#a22633', red: '#e43b44', orange: '#f77622', gold: '#feae34', yellow: '#fee761',
  green: '#63c74d', forest: '#3e8948', pine: '#265c42', teal: '#193c3e', navy: '#124e89', blue: '#0099db',
  cyan: '#2ce8f5', white: '#ffffff', silver: '#c0cbdc', steel: '#8b9bb4', slate: '#5a6988', ink: '#3a4466',
  night: '#262b44', black: '#181425', crimson: '#ff0044', violet: '#68386c', magenta: '#b55088', pink: '#f6757a',
  skin: '#e8b796', skinShade: '#c28569', outline: '#181425',
};

// Per-chapter colors for tiles, backdrops and ambient effects.
export const ChapterThemes = [
  { key: 'wild', floor: ['#5a8f3c', '#4e7f34', '#679c45'], wall: '#6b4a32', wallTop: '#2f5a2a', accent: '#feae34',
    sky: ['#7ec0ee', '#d8f0ff'], far: '#5d7f9a', mid: '#3e6b3a', ground: '#4e7f34', ambient: 'leaf', dark: 0 },
  { key: 'forest', floor: ['#2f5a3a', '#284e33', '#356442'], wall: '#1c3324', wallTop: '#14281c', accent: '#9be36b',
    sky: ['#14302a', '#3d6b4a'], far: '#1f3b30', mid: '#163024', ground: '#284e33', ambient: 'firefly', dark: 0.25 },
  { key: 'library', floor: ['#3b5c78', '#34536d', '#456a88'], wall: '#4a3428', wallTop: '#2a1e18', accent: '#2ce8f5',
    sky: ['#0f2a40', '#24506e'], far: '#1c3a52', mid: '#2a2230', ground: '#34536d', ambient: 'bubble', dark: 0.2 },
  { key: 'mine', floor: ['#8a4a2e', '#7a4028', '#9a5634'], wall: '#4a2a1e', wallTop: '#2a1610', accent: '#f77622',
    sky: ['#2a1410', '#5a2a1a'], far: '#3a1e16', mid: '#2a160f', ground: '#7a4028', ambient: 'dust', dark: 0.45 },
  { key: 'frost', floor: ['#c0dcef', '#b0cfe4', '#d4e8f6'], wall: '#5a7ea0', wallTop: '#e8f2fa', accent: '#2ce8f5',
    sky: ['#9fc7e6', '#eaf6ff'], far: '#8aa8c6', mid: '#6d8fb0', ground: '#b0cfe4', ambient: 'snow', dark: 0 },
  { key: 'forge', floor: ['#4a3a3a', '#3e3030', '#564444'], wall: '#2a2020', wallTop: '#1e1616', accent: '#ff7a1a',
    sky: ['#2a0a08', '#7a2410'], far: '#401410', mid: '#2a1010', ground: '#3e3030', ambient: 'ember', dark: 0.3 },
  { key: 'theater', floor: ['#5a2a4a', '#4e2440', '#682f56'], wall: '#2a1428', wallTop: '#1e0c1c', accent: '#f6757a',
    sky: ['#1a0a1e', '#4a1a3e'], far: '#2a1230', mid: '#1e0c22', ground: '#4e2440', ambient: 'sparkle', dark: 0.3 },
  { key: 'observatory', floor: ['#2a2e5a', '#24284e', '#323668'], wall: '#14163a', wallTop: '#0a0b24', accent: '#fee761',
    sky: ['#05061a', '#1a1e4a'], far: '#14163a', mid: '#0e1030', ground: '#24284e', ambient: 'star', dark: 0.35 },
  { key: 'ruins', floor: ['#8a8478', '#7a7468', '#9a9488'], wall: '#4a4440', wallTop: '#36322e', accent: '#feae34',
    sky: ['#5a4a6a', '#c49a8a'], far: '#6a5a6e', mid: '#4a3e4e', ground: '#7a7468', ambient: 'ash', dark: 0.15 },
  { key: 'zenith', floor: ['#1a2a4a', '#162440', '#20325a'], wall: '#0a1020', wallTop: '#060a18', accent: '#2ce8f5',
    sky: ['#04040e', '#2a1a5a'], far: '#1a1440', mid: '#0e0a2a', ground: '#162440', ambient: 'circuit', dark: 0.3 },
];

export const TierColors = ['#c0cbdc', '#63c74d', '#0099db', '#b55088'];
