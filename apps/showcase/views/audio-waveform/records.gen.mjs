#!/usr/bin/env node
/* apps/showcase/views/audio-waveform/records.gen.mjs — audio-envelope streaming generator (ENC-571)
 *
 * Zero-dependency Node generator. Synthesizes a ~20s audio-like signal, computes
 * its amplitude ENVELOPE, tessellates the envelope as a MIRRORED FILLED AREA
 * (pos2_color4 triangles), and emits a STREAMING record timeline (records.json)
 * that GROWS the audio-waveform view's triGradient@1 buffer column-by-column —
 * so the DAW-style waveform fills IN left→right over the timeline instead of
 * being baked as one static geometry.
 *
 *   node apps/showcase/views/audio-waveform/records.gen.mjs
 *
 * --- The signal (deterministic, speech/music-like) ---------------------------
 * A few mixed sinusoids (a low carrier + two harmonics) modulated by a slowly-
 * drifting vibrato, under a SWELLING/DECAYING amplitude envelope made of a sum of
 * Gaussian "bursts" (speech syllables / musical notes) over a quiet noise floor.
 * The per-sample amplitude ENVELOPE is the |signal| smoothed (a peak-follower /
 * RMS-ish window) and peak-normalised to [0,1] — exactly what a DAW draws.
 *
 * --- The geometry (mirrored filled area, pos2_color4) ------------------------
 * COLUMNS amplitude samples are taken across the timeline; x runs left→right in
 * clip space, y = ±env*Y_SCALE (mirrored about the baseline). Each pair of
 * adjacent columns is a quad (+amp..-amp) split into two triangles = 6
 * pos2_color4 vertices (x,y,r,g,b,a = 6×f32 = 24 bytes/vertex). Per-vertex color
 * grades teal→cyan toward the peaks and dims at the mirror — depth, no shader.
 * Vertices are authored directly in CLIP space (no transform).
 *
 * --- The streaming model (growing area, mirrors scatter/ecg APPEND) ----------
 * Each frame APPENDs the column quads whose columns have "played" since the
 * previous frame. As records land, useReplay's GrowthSync advances
 * geometry.vertexCount = floor(byteLength / 24), so the filled band lengthens to
 * the right (ENC-569 triGradient re-read + redraw). The buffer starts EMPTY.
 *
 * --- Output (apps/showcase/views/<id>/records.json, Records shape) -----------
 *   { meta:{ viewId, durationMs, frameCount, cadenceMs }, frames:[{ t, b64 }] }
 * where each frame's `b64` is base64 of one binary dataplane batch:
 *   [1B op=1 APPEND][4B bufferId LE][4B offset=0 LE][4B payloadBytes LE][payload f32 LE...]
 * (the exact format embassy/useReplay use — see src/scene/commands.ts).
 */

import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const OUT = join(__dirname, 'records.json');

// --- contract IDs (MUST match manifest.ts) -----------------------------------
const VIEW_ID = 'audio-waveform';
const WAVE_BUFFER = 500; // pos2_color4 envelope buffer the manifest grows
const STRIDE = 24; // pos2_color4 vertex = [x, y, r, g, b, a] = 6×f32 = 24 bytes

// --- binary record packing (mirrors src/scene/commands.ts encodeUpload) ------
const OP_APPEND = 1;
const HEADER_SIZE = 13;

/** Pack one APPEND batch of f32 values for `bufferId` into base64. */
function encodeAppend(bufferId, floats) {
  const payloadBytes = floats.length * 4;
  const buf = Buffer.alloc(HEADER_SIZE + payloadBytes);
  buf.writeUInt8(OP_APPEND, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(0, 5); // APPEND ignores offset
  buf.writeUInt32LE(payloadBytes, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

// --- timeline / geometry framing ---------------------------------------------
const DURATION_MS = 20000;
const COLUMNS = 480; // amplitude columns across the timeline
const FRAME_COUNT = 100; // 100 frames over 20s → 200ms cadence (smooth stream)
const X_LEFT = -0.94;
const X_RIGHT = 0.94;
const Y_SCALE = 0.8; // peak envelope → ±0.8 clip (matches old static view)

// --- deterministic audio-like signal synthesis -------------------------------
// High-rate signal we will envelope-follow, then downsample to COLUMNS.
const SR = COLUMNS * 16; // 7680 "samples" of carrier under the envelope
const TWO_PI = Math.PI * 2;

/**
 * Per-sample amplitude envelope made of Gaussian "bursts" (syllables / notes)
 * that swell and decay, over a quiet noise floor + a slow sustained mid swell.
 * Deterministic (fixed burst table), in [~0.05, 1].
 */
function envelopeAt(u) {
  // u in [0,1] across the whole track.
  // A handful of bursts: { center, width, gain } — speech/music-like phrasing.
  const bursts = [
    [0.06, 0.035, 0.85],
    [0.13, 0.05, 1.0],
    [0.21, 0.03, 0.7],
    [0.3, 0.06, 0.95],
    [0.4, 0.025, 0.6],
    [0.5, 0.07, 1.0],
    [0.6, 0.03, 0.75],
    [0.68, 0.045, 0.9],
    [0.78, 0.035, 0.8],
    [0.88, 0.055, 1.0],
    [0.95, 0.03, 0.65],
  ];
  let e = 0;
  for (const [c, w, g] of bursts) {
    const d = (u - c) / w;
    e += g * Math.exp(-0.5 * d * d);
  }
  // Sustained mid swell (a held pad/vowel) + quiet floor.
  e += 0.18 * Math.sin(Math.PI * Math.min(1, Math.max(0, u))) ** 2;
  e += 0.05;
  return e;
}

/** The carrier sample at high-rate index i (mixed sinusoids + vibrato). */
function carrierAt(i) {
  const t = i / SR; // seconds-ish (track is "1 unit" long)
  const vib = 1 + 0.08 * Math.sin(TWO_PI * 3.5 * t); // slow vibrato
  const f0 = 220 * vib;
  let s = Math.sin(TWO_PI * f0 * t);
  s += 0.5 * Math.sin(TWO_PI * 2 * f0 * t); // 2nd harmonic
  s += 0.3 * Math.sin(TWO_PI * 3 * f0 * t); // 3rd harmonic
  // A touch of deterministic "noise" texture (hashed, no RNG dep).
  const h = Math.sin(i * 12.9898) * 43758.5453;
  s += 0.15 * (h - Math.floor(h) - 0.5) * 2;
  return s / 1.95; // ~[-1,1]
}

/**
 * Compute the COLUMNS-length amplitude envelope: peak-follow |carrier*env| over
 * each column's window of high-rate samples, then peak-normalise to [0,1].
 */
function computeEnvelope() {
  const env = new Array(COLUMNS).fill(0);
  const win = Math.floor(SR / COLUMNS); // 16
  for (let c = 0; c < COLUMNS; c++) {
    const u = c / (COLUMNS - 1);
    const eAmp = envelopeAt(u);
    let peak = 0;
    for (let k = 0; k < win; k++) {
      const i = c * win + k;
      const v = Math.abs(carrierAt(i) * eAmp);
      if (v > peak) peak = v;
    }
    env[c] = peak;
  }
  const max = Math.max(...env) || 1;
  return env.map((v) => v / max);
}

// --- color ramp (teal → cyan at peaks; dimmed at the mirror) -----------------
function crestColor(amp) {
  // amp in [0,1]. Teal base → bright cyan crest.
  const r = 0.10 + 0.20 * amp;
  const g = 0.55 + 0.40 * amp;
  const b = 0.63 + 0.32 * amp;
  return [r, g, b, 1];
}
function mirrorColor(amp) {
  // Dimmer below the baseline for depth.
  const r = 0.08 + 0.10 * amp;
  const g = 0.45 + 0.25 * amp;
  const b = 0.55 + 0.20 * amp;
  return [r, g, b, 1];
}

/**
 * Tessellate ONE column quad (between column ci and ci+1) into 6 pos2_color4
 * vertices (2 triangles), spanning (+amp .. -amp). Mirrors the old static
 * buildVertices layout so the band reads identically — just streamed.
 */
function columnQuad(env, ci) {
  const xOf = (c) => X_LEFT + (c / (COLUMNS - 1)) * (X_RIGHT - X_LEFT);
  const xa = xOf(ci);
  const xb = xOf(ci + 1);
  const aA = env[ci];
  const aB = env[ci + 1];
  const yaT = aA * Y_SCALE;
  const ybT = aB * Y_SCALE;
  const [tar, tag, tab, taa] = crestColor(aA);
  const [tbr, tbg, tbb, tba] = crestColor(aB);
  const [mar, mag, mab, maa] = mirrorColor(aA);
  const [mbr, mbg, mbb, mba] = mirrorColor(aB);
  return [
    // tri 1: topA, topB, botA
    xa, yaT, tar, tag, tab, taa,
    xb, ybT, tbr, tbg, tbb, tba,
    xa, -yaT, mar, mag, mab, maa,
    // tri 2: topB, botB, botA
    xb, ybT, tbr, tbg, tbb, tba,
    xb, -ybT, mbr, mbg, mbb, mba,
    xa, -yaT, mar, mag, mab, maa,
  ];
}

function main() {
  const env = computeEnvelope();
  const quadCount = COLUMNS - 1; // 479 column quads

  // Distribute the 479 quads across FRAME_COUNT frames so the band grows
  // smoothly. Each frame APPENDs the quads that have "played" since the prior.
  const frames = [];
  let emitted = 0;
  for (let f = 0; f < FRAME_COUNT; f++) {
    const target = Math.round(((f + 1) / FRAME_COUNT) * quadCount);
    const floats = [];
    for (let q = emitted; q < target; q++) floats.push(...columnQuad(env, q));
    emitted = target;
    if (!floats.length) continue; // skip empty frames (keeps every frame meaningful)
    const t = Math.round((f / (FRAME_COUNT - 1)) * DURATION_MS);
    frames.push({ t, b64: encodeAppend(WAVE_BUFFER, floats) });
  }

  const cadenceMs = Math.round(DURATION_MS / (FRAME_COUNT - 1));
  const out = {
    meta: { viewId: VIEW_ID, durationMs: DURATION_MS, frameCount: frames.length, cadenceMs },
    frames,
  };
  writeFileSync(OUT, JSON.stringify(out));

  const totalVerts = quadCount * 6;
  // eslint-disable-next-line no-console
  console.log(
    `[audio-waveform.gen] ${COLUMNS} columns → ${quadCount} quads (${totalVerts} verts, ` +
      `${totalVerts * STRIDE} bytes) over ${frames.length} frames / ${DURATION_MS}ms ` +
      `(cadence≈${cadenceMs}ms, stride ${STRIDE}, buffer ${WAVE_BUFFER}) → ${OUT}`,
  );
}

main();
