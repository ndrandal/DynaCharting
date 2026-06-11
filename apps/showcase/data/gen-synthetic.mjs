#!/usr/bin/env node
// ENC-522 (T1.2) — synthetic non-equities datasets.
//
//   1) heatmap  — a 16x16 scalar field animated over 20s, emitted as a row/col
//                 scalar-fan (field.r{row}.c{col}) -> fixed-mode snapshot buffer.
//   2) ecg      — a single high-cadence amplitude waveform (synthetic ECG, ~250Hz)
//                 -> append-mode growing line buffer.
//
// Output: apps/showcase/data/synthetic/heatmap.json, synthetic/ecg.json
//
//   node apps/showcase/data/gen-synthetic.mjs

import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { mulberry32, round, f32, buildDataset, writeDataset } from "./lib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT_DIR = join(HERE, "synthetic");

const DURATION_MS = 20_000;
const SEED = 2027;

// ---- 1) heatmap: 16x16 animated scalar field ---------------------------------
// A travelling 2D wave + a slow Gaussian blob, refreshed at 10 fps. Small grid
// (256 cells) so it rides the fixed scalar-fan (one binding per cell) rather than
// needing a texture. Values normalized to [0,1].
function genHeatmap() {
  const ROWS = 16;
  const COLS = 16;
  const FRAME_MS = 100; // 10 fps -> 200 frames
  const nFrames = DURATION_MS / FRAME_MS;
  const updates = [];

  for (let frame = 0; frame < nFrames; frame++) {
    const t = frame * FRAME_MS;
    const phase = (frame / nFrames) * 2 * Math.PI * 3; // 3 wave cycles over the tape
    // blob centre orbits the grid
    const bcx = COLS / 2 + (COLS / 3) * Math.cos((frame / nFrames) * 2 * Math.PI);
    const bcy = ROWS / 2 + (ROWS / 3) * Math.sin((frame / nFrames) * 2 * Math.PI);
    for (let r = 0; r < ROWS; r++) {
      for (let c = 0; c < COLS; c++) {
        const wave = 0.5 + 0.5 * Math.sin((c / COLS) * 2 * Math.PI * 2 + (r / ROWS) * Math.PI + phase);
        const d2 = (c - bcx) ** 2 + (r - bcy) ** 2;
        const blob = Math.exp(-d2 / 8);
        const v = Math.min(1, Math.max(0, 0.6 * wave + 0.7 * blob));
        updates.push({ t, streamKey: "heatmap", field: `field.r${r}.c${c}`, value: round(v, 4) });
      }
    }
  }

  return buildDataset(
    {
      datasetId: "heatmap",
      kind: "synthetic",
      subkind: "scalar-field-2d",
      durationMs: DURATION_MS,
      cadenceMs: FRAME_MS,
      rows: ROWS,
      cols: COLS,
      symbols: [],
      valueRange: [0, 1],
      seed: SEED,
    },
    updates,
  );
}

// ---- 2) ecg: single high-cadence amplitude waveform --------------------------
// Synthetic ECG: repeating PQRST complex at ~72 bpm, sampled at 250 Hz, with a
// touch of seeded baseline noise. Amplitude in millivolts, roughly [-0.5, 1.5].
function genEcg() {
  const FS = 250; // sample rate (Hz)
  const SAMPLE_MS = 1000 / FS; // 4 ms
  const nSamples = Math.floor(DURATION_MS / SAMPLE_MS);
  const bpm = 72;
  const beatMs = 60000 / bpm;
  const rng = mulberry32(SEED);
  const updates = [];

  // Gaussian bump helper for PQRST morphology (phase in ms within the beat).
  const bump = (x, centre, width, amp) => amp * Math.exp(-((x - centre) ** 2) / (2 * width ** 2));

  for (let i = 0; i < nSamples; i++) {
    const t = Math.round(i * SAMPLE_MS);
    const phase = (i * SAMPLE_MS) % beatMs; // ms into the current beat
    let v = 0;
    v += bump(phase, 130, 18, 0.12); // P wave
    v += bump(phase, 195, 6, -0.18); // Q
    v += bump(phase, 210, 5, 1.2); // R spike
    v += bump(phase, 225, 7, -0.32); // S
    v += bump(phase, 330, 30, 0.28); // T wave
    v += 0.02 * (rng() - 0.5); // baseline noise
    updates.push({ t, streamKey: "ecg", field: "amp", value: round(v, 4) });
  }

  return buildDataset(
    {
      datasetId: "ecg",
      kind: "synthetic",
      subkind: "waveform",
      durationMs: DURATION_MS,
      cadenceMs: SAMPLE_MS,
      sampleRateHz: FS,
      bpm,
      symbols: [],
      valueRange: [-0.5, 1.5],
      seed: SEED,
    },
    updates,
  );
}

// ---- main ---------------------------------------------------------------------
for (const [name, ds] of [
  ["heatmap", genHeatmap()],
  ["ecg", genEcg()],
]) {
  const n = writeDataset(join(OUT_DIR, `${name}.json`), ds);
  void f32;
  console.log(`  synthetic/${name}.json  ${n} updates, ${ds.meta.fields.length} fields`);
}
console.log("synthetic: heatmap + ecg written");
