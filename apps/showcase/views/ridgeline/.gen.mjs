/* apps/showcase/views/ridgeline/.gen.mjs
 *
 * Generator for the LIVE ridgeline view's records.json (Linear ENC-582). Zero-dep
 * node script. Turns the previously-STATIC ridgeline (one baked set of 4 KDE
 * density bands) into a breathing, LIVE view via GEOMETRY-FRAME REPLAY.
 *
 * ── Mechanism (geometry-frame replay) ─────────────────────────────────────
 * Each symbol's KDE density is recomputed over a ROLLING WINDOW of its lastPrice
 * series at N timesteps across the 20s timeline. As the window slides forward in
 * time, the distribution shifts (recent ticks dominate, old ones drop out), so
 * the smoothed density curve morphs frame to frame. At every timestep we
 * re-tessellate all four bands into the SAME pos2_color4 triangle layout the
 * manifest bakes (identical bin count → identical triangle count → identical
 * buffer size), and emit ONE UPDATE_RANGE record (op 2, offset 0) overwriting
 * the entire band vertex buffer. The replay engine (useReplay → enqueueData)
 * applies each record; ENC-569's triGradient vertex re-read + redraw repaints
 * the morphed bands — the ridges breathe live.
 *
 * KEY INVARIANT: a CONSTANT vertex count across all frames. Same BINS every
 * frame → same triangle count → same buffer byteLength → every UPDATE_RANGE is a
 * full in-place overwrite at offset 0. The buffer is pre-sized in manifest.ts.
 *
 * The band GEOMETRY (clip framing, per-band baseline/amplitude/hue, the
 * tessellation, the gradient colors) is kept in LOCK-STEP with manifest.ts —
 * both compute density() the same way and tessellate() the same way, so frame-0
 * of the replay is byte-identical to the manifest's seed upload and the live
 * frames land exactly where the seed band sits.
 *
 * Output: apps/showcase/views/ridgeline/records.json
 *   { meta:{viewId,durationMs,frameCount,cadenceMs}, frames:[{t,b64}] }
 *
 * Run:  node apps/showcase/views/ridgeline/.gen.mjs   (from repo root)
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const load = (sym) =>
  JSON.parse(readFileSync(resolve(__dirname, `../../data/market/${sym}.json`), 'utf8'));

// --- binary record format (CONTRACT: commands.ts / IngestProcessor) ----------
// [1B op][4B bufferId LE][4B offsetBytes LE][4B payloadBytes LE][payload...]
const OP_UPDATE_RANGE = 2;
const HEADER_SIZE = 13;

// MUST match manifest.ts -----------------------------------------------------
const BAND_BUFFER = 601; // pos2_color4 triangles
const BINS = 48;

const BANDS = [
  { name: 'AAPL', sym: 'AAPL', hue: [0.38, 0.72, 0.55] },
  { name: 'MSFT', sym: 'MSFT', hue: [0.36, 0.55, 0.92] },
  { name: 'NVDA', sym: 'NVDA', hue: [0.78, 0.55, 0.30] },
  { name: 'TSLA', sym: 'TSLA', hue: [0.82, 0.40, 0.55] },
];

// Clip framing (identical to manifest.ts) ------------------------------------
const X0 = -0.88;
const X1 = 0.88;
const Y_BOTTOM = -0.82;
const Y_TOP = 0.86;
const AMP = 0.46;

// Replay timeline ------------------------------------------------------------
const DURATION_MS = 20000;
const FRAME_COUNT = 40; // 40 frames over 20s → 500ms cadence
// Rolling window: fraction of each symbol's ticks visible in one frame. As the
// window's leading edge slides 0→1 across the timeline, the included tick set
// shifts, so the distribution (and therefore the KDE curve) morphs.
const WINDOW_FRAC = 0.55;

/** Each symbol's lastPrice series as {t, value} in capture order. */
function priceSeries(sym) {
  return load(sym)
    .updates.filter((u) => u.field === 'lastPrice')
    .map((u) => ({ t: u.t, value: u.value }));
}

/**
 * Histogram a value list over a FIXED [min,max] range, Gaussian-smooth, and
 * peak-normalise to unit crest. Identical kernel/normalisation to manifest.ts;
 * the range is fixed per symbol (full-series extent) so bins stay registered
 * frame to frame and the curve morphs in-place rather than rescaling.
 */
function density(values, min, max) {
  const span = max - min || 1;
  const h = new Array(BINS).fill(0);
  for (const v of values) {
    const b = Math.round(((v - min) / span) * (BINS - 1));
    if (b >= 0 && b < BINS) h[b] += 1;
  }
  const k = [0.06, 0.24, 0.4, 0.24, 0.06];
  const sm = h.map((_, i) => {
    let a = 0;
    for (let j = -2; j <= 2; j++) {
      const x = i + j;
      if (x >= 0 && x < BINS) a += h[x] * k[j + 2];
    }
    return a;
  });
  const peak = Math.max(...sm) || 1;
  return sm.map((v) => v / peak);
}

/**
 * Tessellate all four bands at one timestep into one pos2_color4 float array.
 * `densities[bi]` is the normalised BINS-length curve for band bi. Layout is
 * byte-identical to manifest.ts.buildVertices (back→front, same triangle order),
 * so the vertex COUNT is constant across every frame.
 */
function tessellate(densities) {
  const floats = [];
  const n = BANDS.length;
  const spacing = (Y_TOP - Y_BOTTOM) / (n - 1);
  for (let bi = n - 1; bi >= 0; bi--) {
    const band = BANDS[bi];
    const d = densities[bi];
    const baseY = Y_BOTTOM + bi * spacing;
    const [hr, hg, hb] = band.hue;
    const baseC = [hr * 0.28, hg * 0.28, hb * 0.28, 0.85];
    const x = (i) => X0 + (i / (BINS - 1)) * (X1 - X0);
    const crestC = (v) => [
      hr * (0.55 + 0.45 * v),
      hg * (0.55 + 0.45 * v),
      hb * (0.55 + 0.45 * v),
      0.92,
    ];
    for (let i = 0; i < BINS - 1; i++) {
      const xa = x(i);
      const xb = x(i + 1);
      const ya = baseY + d[i] * AMP;
      const yb = baseY + d[i + 1] * AMP;
      const [ar, ag, ab, aa] = crestC(d[i]);
      const [br, bg, bb, ba] = crestC(d[i + 1]);
      const [er, eg, eb, ea] = baseC;
      // tri 1: base_a, crest_a, crest_b
      floats.push(xa, baseY, er, eg, eb, ea);
      floats.push(xa, ya, ar, ag, ab, aa);
      floats.push(xb, yb, br, bg, bb, ba);
      // tri 2: base_a, crest_b, base_b
      floats.push(xa, baseY, er, eg, eb, ea);
      floats.push(xb, yb, br, bg, bb, ba);
      floats.push(xb, baseY, er, eg, eb, ea);
    }
  }
  return floats;
}

/** Pack one UPDATE_RANGE record (13B header + payload) → base64. */
function encodeUpdateRange(bufferId, offsetBytes, floats) {
  const payloadBytes = floats.length * 4;
  const buf = Buffer.alloc(HEADER_SIZE + payloadBytes);
  buf.writeUInt8(OP_UPDATE_RANGE, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(offsetBytes, 5);
  buf.writeUInt32LE(payloadBytes, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

function main() {
  // Per-symbol: full series + FIXED [min,max] over the whole series (bins stay
  // registered so the morph is in-place, not a rescale).
  const syms = BANDS.map((b) => {
    const series = priceSeries(b.sym);
    const values = series.map((s) => s.value);
    return { series, values, min: Math.min(...values), max: Math.max(...values) };
  });

  const frames = [];
  let vertexCount = null;
  for (let f = 0; f < FRAME_COUNT; f++) {
    const t = Math.round((f / (FRAME_COUNT - 1)) * DURATION_MS);
    // Window leading edge slides 0→1 across the timeline; the window covers
    // [edge-WINDOW_FRAC, edge] of each symbol's tick index range (clamped). As
    // it advances the included ticks rotate → the distribution shifts.
    const edge = f / (FRAME_COUNT - 1); // 0..1
    const densities = syms.map((s) => {
      const len = s.values.length;
      const hi = Math.max(1, Math.round(edge * len));
      const lo = Math.max(0, Math.round((edge - WINDOW_FRAC) * len));
      const win = s.values.slice(lo, hi);
      return density(win.length ? win : s.values.slice(0, 1), s.min, s.max);
    });
    const verts = tessellate(densities);
    if (vertexCount === null) vertexCount = verts.length / 6;
    frames.push({ t, b64: encodeUpdateRange(BAND_BUFFER, 0, verts) });
  }

  const cadenceMs = Math.round(DURATION_MS / (FRAME_COUNT - 1));
  const out = {
    meta: { viewId: 'ridgeline', durationMs: DURATION_MS, frameCount: frames.length, cadenceMs },
    frames,
  };
  writeFileSync(resolve(__dirname, 'records.json'), JSON.stringify(out) + '\n', 'utf8');

  process.stderr.write(
    `[ridgeline.gen] frames=${frames.length} bins=${BINS} bands=${BANDS.length} ` +
      `vertexCount=${vertexCount} payloadBytes=${vertexCount * 6 * 4} ` +
      `cadenceMs=${cadenceMs} windowFrac=${WINDOW_FRAC}\n`,
  );
}

main();
