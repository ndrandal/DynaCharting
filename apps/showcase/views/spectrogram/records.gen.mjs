#!/usr/bin/env node
/* apps/showcase/views/spectrogram/records.gen.mjs — SCROLLING spectrogram (ENC-577)
 *
 * Zero-dependency Node generator for the LIVE spectrogram. Emits a records.json
 * with an ANIMATED TEXTURE-FRAME track (foundation ENC-568): the spectrogram is
 * a `texturedQuad@1` whose 256×256 RGBA8 texture is SWAPPED every frame so the
 * frequency×time image COLUMN-SCROLLS in time — the canonical spectrogram
 * animation (newest STFT column appears at the RIGHT edge, older columns shift
 * left).
 *
 * ── The model ──────────────────────────────────────────────────────────────
 * A LONGER (~20 s) signal is synthesized at SAMPLE_RATE: repeating chirps that
 * sweep up in frequency, stepped tones that turn on/off each second, a steady
 * low harmonic stack, and a little noise. A short-time FFT (Hann-windowed,
 * WIN-pt window, HOP-pt hop) is run over the whole signal once on the CPU,
 * producing a full magnitude spectrogram of COLS_TOTAL columns. Each emitted
 * frame then renders the most-recent COLS=256 columns ending at that frame's
 * time cursor — a SLIDING TIME WINDOW. As the cursor advances frame to frame,
 * the window slides, so each new texture is the previous one shifted left with
 * fresh columns revealed on the right: the spectrogram scrolls.
 *
 * Orientation matches manifest.ts / the texturedQuad shader: image row 0 = TOP
 * of the quad = HIGHEST frequency; X = time, left→right = old→new. Magnitude is
 * log-scaled, normalized, mapped through a viridis ramp, packed RGBA8 (alpha
 * 255), tightly packed [ (row*W + col)*4 ], base64-encoded.
 *
 * Run:  node apps/showcase/views/spectrogram/records.gen.mjs
 * Out:  apps/showcase/views/spectrogram/records.json
 *
 *   { meta:{viewId, durationMs, frameCount, cadenceMs}, frames:[],
 *     textures:[{ t, textureId, width, height, pixelsB64, format:1 }, ...] }
 *
 * frames is EMPTY — this view animates purely off the texture track, which
 * useReplay schedules at each `t` (looping) via host.setTexturePixels.
 */

import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT = join(HERE, 'records.json');

// ── contract constants (MUST match manifest.ts) ────────────────────────────
const VIEW_ID = 'spectrogram';
const TEXTURE_ID = 90; // manifest.ts TEXTURE_ID
const WIDTH = 256; // manifest texture width  (X = time columns)
const HEIGHT = 256; // manifest texture height (Y = frequency rows, row 0 = top = high freq)
const FORMAT = 1; // RGBA8

// ── replay timing ──────────────────────────────────────────────────────────
const DURATION_MS = 20000; // total signal/replay span
const FRAME_COUNT = 60; // ~333 ms cadence — smooth scroll
const CADENCE_MS = DURATION_MS / FRAME_COUNT;

// ── signal + STFT params ───────────────────────────────────────────────────
const SAMPLE_RATE = 4096; // Hz (Nyquist 2048 Hz; matches the manifest's regime)
const WIN = 512; // FFT / window length (257 unique mag bins)
const HOP = 128; // hop between STFT columns
const TOTAL_SAMPLES = Math.round((DURATION_MS / 1000) * SAMPLE_RATE);
const COLS_TOTAL = Math.floor((TOTAL_SAMPLES - WIN) / HOP) + 1; // STFT columns over whole signal
const MAG_BINS = WIN / 2 + 1; // 257 magnitude bins (DC..Nyquist)

// ── synthesized signal: chirps + stepped tones + low harmonics + noise ─────
// Deterministic PRNG so the output is reproducible.
let _seed = 0x9e3779b9 >>> 0;
function rand() {
  _seed = (_seed * 1664525 + 1013904223) >>> 0;
  return _seed / 0xffffffff;
}

function synthesize() {
  const sig = new Float32Array(TOTAL_SAMPLES);
  const dt = 1 / SAMPLE_RATE;
  // Rising chirps: a 2.5 s sweep 120 Hz → 1700 Hz, repeated; instantaneous
  // phase is the integral of the linear frequency ramp.
  const CHIRP_PERIOD = 2.5;
  const F0 = 120,
    F1 = 1700;
  // Stepped tone: jumps up one step each second, wrapping — and gates on/off.
  const STEP_HZ = 220;
  const STEPS = 8;
  // Steady low harmonic stack (a "drone").
  const FUND = 80;

  for (let n = 0; n < TOTAL_SAMPLES; n++) {
    const t = n * dt;
    let s = 0;

    // (1) repeating up-chirp
    const ct = t % CHIRP_PERIOD;
    const k = (F1 - F0) / CHIRP_PERIOD; // Hz/s
    const fInst = F0 + k * ct;
    const chirpPhase = 2 * Math.PI * (F0 * ct + 0.5 * k * ct * ct);
    s += 0.9 * Math.sin(chirpPhase) * (0.6 + 0.4 * Math.sin(Math.PI * (ct / CHIRP_PERIOD)));
    void fInst;

    // (2) stepped tone, turns on/off (gated every other second window)
    const secIdx = Math.floor(t);
    const step = secIdx % STEPS;
    const toneOn = secIdx % 3 !== 2; // off for ~1 of every 3 seconds
    if (toneOn) {
      const fTone = 300 + step * STEP_HZ;
      s += 0.55 * Math.sin(2 * Math.PI * fTone * t);
    }

    // (3) steady low harmonic stack
    s += 0.5 * Math.sin(2 * Math.PI * FUND * t);
    s += 0.25 * Math.sin(2 * Math.PI * 2 * FUND * t);
    s += 0.12 * Math.sin(2 * Math.PI * 3 * FUND * t);

    // (4) broadband noise
    s += 0.06 * (rand() * 2 - 1);

    sig[n] = s;
  }
  return sig;
}

// ── Hann window ────────────────────────────────────────────────────────────
const HANN = new Float32Array(WIN);
for (let i = 0; i < WIN; i++) HANN[i] = 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (WIN - 1));

// ── DFT magnitude of one Hann-windowed frame (real input, naive O(WIN²)) ────
// WIN=512 → 257 bins; COLS_TOTAL frames. Naive DFT is plenty fast at this size.
function frameMagnitudes(sig, start) {
  // Precompute twiddle on first call.
  if (!frameMagnitudes._cos) {
    const cos = new Float32Array(WIN * MAG_BINS);
    const sin = new Float32Array(WIN * MAG_BINS);
    for (let kk = 0; kk < MAG_BINS; kk++) {
      for (let nn = 0; nn < WIN; nn++) {
        const a = (-2 * Math.PI * kk * nn) / WIN;
        cos[kk * WIN + nn] = Math.cos(a);
        sin[kk * WIN + nn] = Math.sin(a);
      }
    }
    frameMagnitudes._cos = cos;
    frameMagnitudes._sin = sin;
  }
  const cos = frameMagnitudes._cos;
  const sin = frameMagnitudes._sin;
  const windowed = new Float32Array(WIN);
  for (let i = 0; i < WIN; i++) {
    const idx = start + i;
    windowed[i] = (idx < sig.length ? sig[idx] : 0) * HANN[i];
  }
  const mag = new Float32Array(MAG_BINS);
  for (let kk = 0; kk < MAG_BINS; kk++) {
    let re = 0,
      im = 0;
    const base = kk * WIN;
    for (let nn = 0; nn < WIN; nn++) {
      const x = windowed[nn];
      re += x * cos[base + nn];
      im += x * sin[base + nn];
    }
    mag[kk] = Math.sqrt(re * re + im * im);
  }
  return mag;
}

// ── viridis-like colormap (perceptual, dark→bright). Anchored control points
// of matplotlib's viridis; piecewise-linear interpolation. t ∈ [0,1]. ────────
const VIRIDIS = [
  [0.267, 0.005, 0.329],
  [0.283, 0.141, 0.458],
  [0.254, 0.265, 0.53],
  [0.207, 0.372, 0.553],
  [0.164, 0.471, 0.558],
  [0.128, 0.567, 0.551],
  [0.135, 0.659, 0.518],
  [0.267, 0.749, 0.441],
  [0.478, 0.821, 0.318],
  [0.741, 0.873, 0.15],
  [0.993, 0.906, 0.144],
];
function viridis(t) {
  const x = Math.max(0, Math.min(1, t)) * (VIRIDIS.length - 1);
  const i = Math.min(VIRIDIS.length - 2, Math.floor(x));
  const f = x - i;
  const a = VIRIDIS[i];
  const b = VIRIDIS[i + 1];
  return [
    Math.round((a[0] + (b[0] - a[0]) * f) * 255),
    Math.round((a[1] + (b[1] - a[1]) * f) * 255),
    Math.round((a[2] + (b[2] - a[2]) * f) * 255),
  ];
}

// ── compute the full magnitude spectrogram, log-scaled + globally normalized ─
const signal = synthesize();
const spec = []; // spec[col] = Float32Array(MAG_BINS) of normalized log-magnitude
let gMin = Infinity,
  gMax = -Infinity;
const logCols = [];
for (let c = 0; c < COLS_TOTAL; c++) {
  const mag = frameMagnitudes(signal, c * HOP);
  const lc = new Float32Array(MAG_BINS);
  for (let b = 0; b < MAG_BINS; b++) {
    const v = Math.log10(1 + mag[b]); // log-magnitude
    lc[b] = v;
    if (v < gMin) gMin = v;
    if (v > gMax) gMax = v;
  }
  logCols.push(lc);
}
const gRange = gMax - gMin || 1;
for (let c = 0; c < COLS_TOTAL; c++) {
  const lc = logCols[c];
  const nc = new Float32Array(MAG_BINS);
  for (let b = 0; b < MAG_BINS; b++) nc[b] = (lc[b] - gMin) / gRange;
  spec.push(nc);
}

// ── render one frame: the most-recent WIDTH columns ending at `endCol`. ─────
// Columns to the LEFT of column 0 (before the signal start) render as silence
// (colormap floor), so the very first frames scroll the history IN from the
// right. Row 0 = top = highest frequency. Frequency bins (MAG_BINS) are mapped
// onto HEIGHT rows by nearest-neighbour.
function renderFrame(endCol) {
  const px = new Uint8Array(WIDTH * HEIGHT * 4);
  for (let xCol = 0; xCol < WIDTH; xCol++) {
    // time axis: rightmost column (xCol = WIDTH-1) = newest = endCol.
    const srcCol = endCol - (WIDTH - 1 - xCol);
    const column = srcCol >= 0 && srcCol < COLS_TOTAL ? spec[srcCol] : null;
    for (let yRow = 0; yRow < HEIGHT; yRow++) {
      // yRow 0 = top = highest freq → bin MAG_BINS-1.
      const freqFrac = 1 - yRow / (HEIGHT - 1); // 1 at top, 0 at bottom
      const bin = Math.min(MAG_BINS - 1, Math.round(freqFrac * (MAG_BINS - 1)));
      const v = column ? column[bin] : 0;
      const [r, g, b] = viridis(v);
      const o = (yRow * WIDTH + xCol) * 4;
      px[o] = r;
      px[o + 1] = g;
      px[o + 2] = b;
      px[o + 3] = 255;
    }
  }
  return px;
}

// ── emit one texture frame per timestep; the cursor sweeps the spectrogram. ─
// Frame f's cursor ends at endCol so the window has advanced by an even share
// of the total columns. We let the cursor sweep slightly past the end-of-window
// so newest content keeps arriving across all frames.
const textures = [];
for (let f = 0; f < FRAME_COUNT; f++) {
  const frac = (f + 1) / FRAME_COUNT; // (0,1]
  const endCol = Math.min(COLS_TOTAL - 1, Math.round(frac * (COLS_TOTAL - 1)));
  const px = renderFrame(endCol);
  if (px.length !== WIDTH * HEIGHT * 4) {
    throw new Error(`frame ${f}: ${px.length} bytes, expected ${WIDTH * HEIGHT * 4}`);
  }
  const t = Math.round(f * CADENCE_MS);
  textures.push({
    t,
    textureId: TEXTURE_ID,
    width: WIDTH,
    height: HEIGHT,
    pixelsB64: Buffer.from(px).toString('base64'),
    format: FORMAT,
  });
}

const records = {
  meta: { viewId: VIEW_ID, durationMs: DURATION_MS, frameCount: FRAME_COUNT, cadenceMs: CADENCE_MS },
  frames: [],
  textures,
};

writeFileSync(OUT, JSON.stringify(records));

// eslint-disable-next-line no-console
console.log(
  `wrote records.json: ${textures.length} texture frames, ${WIDTH}x${HEIGHT} RGBA8, ` +
    `textureId=${TEXTURE_ID}, ${WIN}-pt Hann STFT, ${COLS_TOTAL} total STFT cols, ` +
    `cadence ${CADENCE_MS}ms, ${WIDTH * HEIGHT * 4}B/frame`,
);
