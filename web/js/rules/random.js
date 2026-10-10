// Port of src/core/random.cpp: PCG32 with a fixed stream, so a seed gives the
// same expedition here as in the C++ build.
const Mask = (1n << 64n) - 1n;
const Multiplier = 6364136223846793005n;
const Stream = 0xda3e39cb94b95bdbn;

let state = 0x853c49e6748fea9bn;
let increment = 0xda3e39cb94b95bdbn;

export function next() {
  const old = state;
  state = (old * Multiplier + increment) & Mask;
  const xorshifted = Number((((old >> 18n) ^ old) >> 27n) & 0xffffffffn);
  const rotation = Number(old >> 59n);
  return ((xorshifted >>> rotation) | (xorshifted << ((32 - rotation) & 31))) >>> 0;
}

export function seed(value) {
  state = 0n;
  increment = ((Stream << 1n) | 1n) & Mask;
  next();
  state = (state + BigInt.asUintN(64, BigInt(value))) & Mask;
  next();
}

// Uniform integer in [0, bound) without modulo bias; 0 when bound <= 1.
export function below(bound) {
  if (bound <= 1) return 0;
  const range = bound >>> 0;
  const threshold = (0x100000000 - range) % range;
  for (;;) {
    const value = next();
    if (value >= threshold) return value % range;
  }
}

// Cosmetic randomness (map layouts, particles) must never consume the game
// stream above, or seeds would stop matching the C++ build. Mulberry32.
export class VisualRng {
  constructor(seedValue = 1) { this.s = seedValue >>> 0; }
  next() {
    let t = (this.s = (this.s + 0x6d2b79f5) >>> 0);
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }
  int(n) { return Math.floor(this.next() * n); }
  range(a, b) { return a + this.next() * (b - a); }
  pick(list) { return list[this.int(list.length)]; }
  chance(p) { return this.next() < p; }
}
