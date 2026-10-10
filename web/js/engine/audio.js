// Synthesized sound effects and a small chiptune sequencer (Web Audio, no
// sound files). Audio starts after the first user gesture, as browsers require.

let ctx = null;
let master = null;
let sfxBus = null;
let musicBus = null;
let noiseBuffer = null;
const settings = { sfx: 0.6, music: 0.35, muted: false };

try {
  const saved = JSON.parse(localStorage.getItem('aincrad.audio') || 'null');
  if (saved) Object.assign(settings, saved);
} catch { /* storage unavailable */ }

function persist() {
  try { localStorage.setItem('aincrad.audio', JSON.stringify(settings)); } catch { /* ignore */ }
}

export function unlockAudio() {
  if (ctx) {
    if (ctx.state === 'suspended') ctx.resume();
    return;
  }
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC) return;
  ctx = new AC();
  master = ctx.createGain();
  master.connect(ctx.destination);
  sfxBus = ctx.createGain();
  musicBus = ctx.createGain();
  sfxBus.connect(master);
  musicBus.connect(master);
  applyVolumes();
  noiseBuffer = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
  const data = noiseBuffer.getChannelData(0);
  for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  if (pendingMusic) playMusic(pendingMusic);
}

function applyVolumes() {
  if (!ctx) return;
  master.gain.value = settings.muted ? 0 : 1;
  sfxBus.gain.value = settings.sfx;
  musicBus.gain.value = settings.music * 0.5;
}
export const audioSettings = () => ({ ...settings });
export function setAudio(changes) {
  Object.assign(settings, changes);
  applyVolumes();
  persist();
}

function tone(freq, time, o = {}) {
  const osc = ctx.createOscillator();
  const gain = ctx.createGain();
  osc.type = o.type ?? 'square';
  osc.frequency.setValueAtTime(freq, time);
  if (o.slide) osc.frequency.exponentialRampToValueAtTime(Math.max(20, o.slide), time + (o.dur ?? 0.15));
  const vol = o.vol ?? 0.2;
  gain.gain.setValueAtTime(0.0001, time);
  gain.gain.exponentialRampToValueAtTime(vol, time + (o.attack ?? 0.005));
  gain.gain.exponentialRampToValueAtTime(0.0001, time + (o.dur ?? 0.15));
  osc.connect(gain);
  gain.connect(o.bus ?? sfxBus);
  osc.start(time);
  osc.stop(time + (o.dur ?? 0.15) + 0.05);
}
function noise(time, o = {}) {
  const src = ctx.createBufferSource();
  src.buffer = noiseBuffer;
  const filter = ctx.createBiquadFilter();
  filter.type = o.filter ?? 'lowpass';
  filter.frequency.setValueAtTime(o.freq ?? 2000, time);
  if (o.sweep) filter.frequency.exponentialRampToValueAtTime(o.sweep, time + (o.dur ?? 0.2));
  const gain = ctx.createGain();
  gain.gain.setValueAtTime(o.vol ?? 0.3, time);
  gain.gain.exponentialRampToValueAtTime(0.0001, time + (o.dur ?? 0.2));
  src.connect(filter);
  filter.connect(gain);
  gain.connect(o.bus ?? sfxBus);
  src.start(time);
  src.stop(time + (o.dur ?? 0.2) + 0.05);
}

const Sounds = {
  click: (t) => tone(880, t, { dur: 0.05, vol: 0.08 }),
  hover: (t) => tone(1320, t, { dur: 0.03, vol: 0.04, type: 'triangle' }),
  step: (t) => noise(t, { freq: 600, dur: 0.04, vol: 0.05 }),
  bump: (t) => tone(120, t, { dur: 0.08, vol: 0.1, type: 'triangle' }),
  open: (t) => { tone(520, t, { dur: 0.08, vol: 0.08 }); tone(780, t + 0.05, { dur: 0.1, vol: 0.08 }); },
  close: (t) => { tone(600, t, { dur: 0.06, vol: 0.06 }); tone(400, t + 0.04, { dur: 0.08, vol: 0.06 }); },
  error: (t) => { tone(200, t, { dur: 0.12, vol: 0.12, type: 'sawtooth' }); tone(150, t + 0.08, { dur: 0.15, vol: 0.1, type: 'sawtooth' }); },
  hit: (t) => { noise(t, { freq: 3000, sweep: 300, dur: 0.12, vol: 0.35 }); tone(140, t, { dur: 0.1, slide: 60, vol: 0.25, type: 'triangle' }); },
  crit: (t) => { noise(t, { freq: 5000, sweep: 400, dur: 0.18, vol: 0.4 }); tone(1600, t, { dur: 0.15, vol: 0.12 }); tone(90, t, { dur: 0.2, slide: 40, vol: 0.3, type: 'triangle' }); },
  slash: (t) => noise(t, { filter: 'bandpass', freq: 4000, sweep: 800, dur: 0.15, vol: 0.3 }),
  heavy: (t) => { noise(t, { freq: 1200, sweep: 100, dur: 0.35, vol: 0.5 }); tone(80, t, { dur: 0.35, slide: 30, vol: 0.4, type: 'triangle' }); },
  fire: (t) => { noise(t, { freq: 900, sweep: 200, dur: 0.4, vol: 0.35 }); tone(220, t, { dur: 0.25, slide: 110, vol: 0.1, type: 'sawtooth' }); },
  ice: (t) => { for (let i = 0; i < 5; i++) tone(1800 + i * 300, t + i * 0.03, { dur: 0.12, vol: 0.06, type: 'sine' }); },
  magic: (t) => tone(400, t, { dur: 0.3, slide: 1400, vol: 0.12, type: 'sine' }),
  arcane: (t) => { tone(200, t, { dur: 0.5, slide: 1200, vol: 0.15, type: 'sawtooth' }); noise(t + 0.1, { filter: 'highpass', freq: 3000, dur: 0.3, vol: 0.15 }); },
  poison: (t) => { for (let i = 0; i < 4; i++) tone(300 + Math.random() * 200, t + i * 0.05, { dur: 0.06, vol: 0.06, type: 'sine' }); },
  heal: (t) => { [523, 659, 784, 1047].forEach((f, i) => tone(f, t + i * 0.06, { dur: 0.18, vol: 0.08, type: 'triangle' })); },
  mana: (t) => tone(880, t, { dur: 0.15, slide: 1760, vol: 0.06, type: 'sine' }),
  guard: (t) => { tone(1200, t, { dur: 0.08, vol: 0.1, type: 'square' }); tone(900, t + 0.03, { dur: 0.12, vol: 0.08, type: 'triangle' }); },
  dodge: (t) => noise(t, { filter: 'bandpass', freq: 1500, sweep: 4000, dur: 0.15, vol: 0.2 }),
  charge: (t) => tone(150, t, { dur: 0.6, slide: 600, vol: 0.12, type: 'sawtooth' }),
  interrupt: (t) => { tone(1000, t, { dur: 0.1, vol: 0.12 }); tone(500, t + 0.06, { dur: 0.2, vol: 0.12 }); noise(t, { freq: 4000, dur: 0.1, vol: 0.2 }); },
  explode: (t) => { noise(t, { freq: 800, sweep: 60, dur: 0.6, vol: 0.6 }); tone(60, t, { dur: 0.5, slide: 25, vol: 0.4, type: 'triangle' }); },
  coin: (t) => { tone(988, t, { dur: 0.06, vol: 0.08 }); tone(1319, t + 0.06, { dur: 0.15, vol: 0.08 }); },
  chest: (t) => { [392, 523, 659, 784, 1047].forEach((f, i) => tone(f, t + i * 0.05, { dur: 0.15, vol: 0.07 })); },
  levelup: (t) => { [523, 659, 784, 1047, 1319].forEach((f, i) => tone(f, t + i * 0.08, { dur: 0.25, vol: 0.09, type: 'square' })); },
  victory: (t) => { [523, 523, 523, 659, 784, 659, 784].forEach((f, i) => tone(f, t + [0, 0.1, 0.2, 0.3, 0.5, 0.65, 0.8][i], { dur: 0.2, vol: 0.1 })); },
  defeat: (t) => { [392, 349, 330, 262].forEach((f, i) => tone(f, t + i * 0.25, { dur: 0.35, vol: 0.1, type: 'triangle' })); },
  stairs: (t) => { [262, 330, 392, 523].forEach((f, i) => tone(f, t + i * 0.07, { dur: 0.12, vol: 0.07, type: 'triangle' })); },
  encounter: (t) => { noise(t, { freq: 6000, sweep: 300, dur: 0.4, vol: 0.25 }); [880, 660, 880, 660].forEach((f, i) => tone(f, t + i * 0.06, { dur: 0.06, vol: 0.08 })); },
  shatter: (t) => { noise(t, { filter: 'highpass', freq: 2500, dur: 0.3, vol: 0.35 }); for (let i = 0; i < 4; i++) tone(2000 + i * 500, t + i * 0.02, { dur: 0.08, vol: 0.05, type: 'sine' }); },
  read: (t) => { tone(300, t, { dur: 0.25, slide: 900, vol: 0.1, type: 'sine' }); tone(900, t + 0.2, { dur: 0.2, slide: 300, vol: 0.08, type: 'sine' }); },
};

export function sfx(name) {
  if (!ctx || settings.muted || !Sounds[name]) return;
  try { Sounds[name](ctx.currentTime + 0.01); } catch { /* ignore audio glitches */ }
}

// ---- Music --------------------------------------------------------------------
const Notes = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
const freqOf = (note) => {
  const m = /^([A-G])(#?)(\d)$/.exec(note);
  if (!m) return 0;
  const semis = Notes[m[1]] + (m[2] ? 1 : 0) + (Number(m[3]) - 4) * 12 - 9;
  return 440 * Math.pow(2, semis / 12);
};
// Each song: tempo, bass line, arpeggio chords (one per bar), optional lead.
const Songs = {
  title: { bpm: 84, bass: ['A2', 'F2', 'C3', 'G2'], chords: [['A3', 'C4', 'E4'], ['F3', 'A3', 'C4'], ['C4', 'E4', 'G4'], ['G3', 'B3', 'D4']],
    lead: ['E5', '', 'D5', 'C5', 'E5', '', 'G5', '', 'F5', 'E5', 'D5', '', 'C5', '', 'D5', ''] },
  explore: { bpm: 96, bass: ['C3', 'A2', 'F2', 'G2'], chords: [['C4', 'E4', 'G4'], ['A3', 'C4', 'E4'], ['F3', 'A3', 'C4'], ['G3', 'B3', 'D4']] },
  dark: { bpm: 80, bass: ['D2', 'A#2', 'F2', 'C3'], chords: [['D4', 'F4', 'A4'], ['A#3', 'D4', 'F4'], ['F3', 'A3', 'C4'], ['C4', 'E4', 'G4']] },
  battle: { bpm: 140, bass: ['A2', 'A2', 'F2', 'G2'], chords: [['A3', 'C4', 'E4'], ['A3', 'C4', 'E4'], ['F3', 'A3', 'C4'], ['G3', 'B3', 'D4']],
    lead: ['A4', 'C5', 'E5', 'C5', 'A4', 'C5', 'E5', 'G5', 'F5', 'E5', 'C5', 'A4', 'G4', 'B4', 'D5', 'B4'] },
  boss: { bpm: 150, bass: ['D2', 'D2', 'A#2', 'C3'], chords: [['D4', 'F4', 'A4'], ['D4', 'F4', 'A4'], ['A#3', 'D4', 'F4'], ['C4', 'E4', 'G4']],
    lead: ['D5', 'D5', 'F5', 'D5', 'A5', 'G5', 'F5', 'E5', 'F5', 'D5', 'A#4', 'D5', 'C5', 'E5', 'G5', 'E5'] },
};
let current = null;
let pendingMusic = null;
let scheduler = null;

export function playMusic(name) {
  if (current && current.name === name) return;
  stopMusic();
  pendingMusic = name;
  if (!ctx) return;
  const song = Songs[name];
  if (!song) return;
  const sixteenth = 60 / song.bpm / 4;
  let step = 0;
  let nextTime = ctx.currentTime + 0.1;
  current = { name };
  scheduler = setInterval(() => {
    if (!ctx) return;
    while (nextTime < ctx.currentTime + 0.25) {
      const bar = Math.floor(step / 16) % song.bass.length;
      const pos = step % 16;
      if (pos % 4 === 0) tone(freqOf(song.bass[bar]), nextTime, { dur: sixteenth * 3.5, vol: 0.13, type: 'triangle', bus: musicBus });
      const chord = song.chords[bar];
      if (pos % 2 === 0) tone(freqOf(chord[(pos / 2) % chord.length]) * 2, nextTime, { dur: sixteenth * 1.5, vol: 0.035, type: 'square', bus: musicBus });
      if (song.lead) {
        const note = song.lead[(step % (song.lead.length * 2)) >> 1];
        if (note && step % 2 === 0) tone(freqOf(note), nextTime, { dur: sixteenth * 1.8, vol: 0.04, type: 'square', bus: musicBus });
      }
      if (name === 'battle' || name === 'boss') {
        if (pos % 4 === 0) noise(nextTime, { freq: 200, dur: 0.08, vol: 0.12, bus: musicBus });
        if (pos % 8 === 4) noise(nextTime, { filter: 'highpass', freq: 2000, dur: 0.06, vol: 0.06, bus: musicBus });
      }
      nextTime += sixteenth;
      step++;
    }
  }, 60);
}
export function stopMusic() {
  if (scheduler) clearInterval(scheduler);
  scheduler = null;
  current = null;
}
