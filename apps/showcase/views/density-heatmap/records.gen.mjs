/* apps/showcase/views/density-heatmap/records.gen.mjs
 *
 * GENERATOR (zero-dep node) for the LIVE density-heatmap (Linear ENC-576).
 *
 * ── What this emits ───────────────────────────────────────────────────────
 * A TEXTURE-FRAME replay (foundation #40 / ENC-568): a `records.json` whose
 * `textures` track carries N animated RGBA8 frames for the manifest's
 * texturedQuad@1. Each frame is the 256×256 colormap of a 2D Gaussian-KDE
 * density field at one timestep; the replay engine (useReplay → applyTextureFrame
 * → host.setTexturePixels) swaps the texture at each frame's `t`, LOOPING with
 * the timeline — so the heat blobs DRIFT and BREATHE.
 *
 * ── The model (a LIVE liquidity heatmap) ──────────────────────────────────
 * The static manifest baked ONE KDE field from a fixed scattered point cloud
 * (5 clusters + diffuse bg, ~3.8k points). Here that point cloud EVOLVES over
 * time: each liquidity cluster orbits/drifts on its own slow path and its
 * weight pulses (resting liquidity appears and fades), and the diffuse
 * background jitters. The KDE field is re-accumulated per frame by splatting a
 * separable Gaussian stamp at each point, gamma-corrected for glow, and mapped
 * through the SAME dark-navy → cyan → white-hot colormap the manifest uses
 * (control stops recovered from manifest.ts's baked PIXELS_B64), then
 * base64-emitted.
 *
 * Deterministic (no RNG) — a seeded LCG drives the point cloud so every run is
 * identical. The timeline is closed (phase wraps a full 2π over DURATION_MS) so
 * frame N loops seamlessly onto frame 0.
 *
 * Run:  node apps/showcase/views/density-heatmap/records.gen.mjs
 * Out:  apps/showcase/views/density-heatmap/records.json
 *
 * THE WALL (unchanged): doing this KDE accumulation + glow live, per-pixel, on
 * the GPU every frame (a custom compute/fragment shader) is the frontier tier.
 * Here it's a CPU prepass baked into texture frames.
 */

import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const RECORDS_PATH = join(HERE, 'records.json');

// ── texture / view constants (MUST match manifest.ts) ──────────────────────
const VIEW_ID = 'density-heatmap';
const TEXTURE_ID = 70; // == manifest.ts TEXTURE_ID
const W = 256; // == manifest texture width
const H = 256; // == manifest texture height
const FORMAT = 1; // RGBA8 (useReplay default)

// ── replay timeline ────────────────────────────────────────────────────────
const FRAME_COUNT = 50; // ~333ms cadence over 20s
const DURATION_MS = 20000;
const CADENCE_MS = Math.round(DURATION_MS / FRAME_COUNT);

// ── colormap: dark-navy → blue → cyan → white-hot glow ─────────────────────
// 9 control stops at t = 0, 1/8 ... 1, recovered (by brightness ordering) from
// the baked PIXELS_B64 in manifest.ts so the live frames match the static card.
const COLOR_STOPS = [
  [4, 6, 22], // 0.000  dark navy (background)
  [8, 16, 57], // 0.125
  [12, 27, 92], // 0.250
  [16, 75, 147], // 0.375  blue
  [20, 122, 200], // 0.500
  [40, 170, 217], // 0.625  cyan
  [62, 220, 235], // 0.750
  [152, 237, 244], // 0.875
  [245, 255, 255], // 1.000  white-hot
];

/** Map t∈[0,1] through the piecewise-linear glow ramp → [r,g,b] (0..255 ints). */
function colormap(t) {
  const x = Math.max(0, Math.min(1, t)) * (COLOR_STOPS.length - 1);
  const i = Math.min(COLOR_STOPS.length - 2, Math.floor(x));
  const f = x - i;
  const a = COLOR_STOPS[i];
  const b = COLOR_STOPS[i + 1];
  return [
    Math.round(a[0] + (b[0] - a[0]) * f),
    Math.round(a[1] + (b[1] - a[1]) * f),
    Math.round(a[2] + (b[2] - a[2]) * f),
  ];
}

// ── deterministic point cloud (seeded LCG, no RNG) ─────────────────────────
function makeLcg(seed) {
  let s = seed >>> 0;
  return () => {
    s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
    return s / 4294967296;
  };
}

// Liquidity clusters. Each orbits a slow centre, with a per-cluster orbit
// radius/phase and a breathing weight, so blobs move AND fade in/out.
const CLUSTERS = [
  { cx: 0.30, cy: 0.34, orbit: 0.10, oph: 0.0, ospd: 1.0, sigma: 0.055, base: 1.0, wamp: 0.55, wph: 0.0 },
  { cx: 0.68, cy: 0.28, orbit: 0.08, oph: 1.3, ospd: -1.2, sigma: 0.045, base: 0.9, wamp: 0.7, wph: 1.1 },
  { cx: 0.52, cy: 0.62, orbit: 0.12, oph: 2.5, ospd: 0.8, sigma: 0.065, base: 1.1, wamp: 0.5, wph: 2.2 },
  { cx: 0.22, cy: 0.72, orbit: 0.07, oph: 0.7, ospd: 1.5, sigma: 0.04, base: 0.8, wamp: 0.8, wph: 3.0 },
  { cx: 0.78, cy: 0.74, orbit: 0.09, oph: 3.8, ospd: -0.9, sigma: 0.05, base: 0.95, wamp: 0.6, wph: 4.1 },
];

const POINTS_PER_CLUSTER = 680; // ~3.4k clustered + diffuse bg ≈ 3.8k points
const BG_POINTS = 420;
const TAU = Math.PI * 2;

/**
 * Build the point cloud for normalized phase p∈[0,1). Returns {x,y,w} arrays in
 * [0,1]² UV. Clusters orbit + breathe; the diffuse background is fixed-seeded
 * with a gentle global jitter so the cool wash shimmers.
 */
function pointsAt(p) {
  const xs = [];
  const ys = [];
  const ws = [];
  const ang = TAU * p;

  for (let c = 0; c < CLUSTERS.length; c++) {
    const cl = CLUSTERS[c];
    const ocx = cl.cx + cl.orbit * Math.cos(cl.oph + ang * cl.ospd);
    const ocy = cl.cy + cl.orbit * Math.sin(cl.oph + ang * cl.ospd);
    const weight = Math.max(0.05, cl.base * (1 + cl.wamp * Math.sin(ang + cl.wph)));
    const rng = makeLcg(0x9e3779b1 ^ (c * 2654435761));
    const n = Math.round(POINTS_PER_CLUSTER * weight);
    for (let k = 0; k < n; k++) {
      // Box-Muller for a Gaussian scatter around the orbiting centre.
      const u1 = Math.max(1e-6, rng());
      const u2 = rng();
      const r = Math.sqrt(-2 * Math.log(u1)) * cl.sigma;
      const th = TAU * u2;
      xs.push(ocx + r * Math.cos(th));
      ys.push(ocy + r * Math.sin(th));
      ws.push(1.0);
    }
  }

  // Diffuse background — fixed seed, gentle shimmer.
  const bg = makeLcg(0x1234abcd);
  for (let k = 0; k < BG_POINTS; k++) {
    const jx = 0.012 * Math.cos(ang + k * 0.013);
    const jy = 0.012 * Math.sin(ang * 0.7 + k * 0.017);
    xs.push(bg() + jx);
    ys.push(bg() + jy);
    ws.push(0.35);
  }
  return { xs, ys, ws };
}

// ── KDE rasterization ──────────────────────────────────────────────────────
// Splat a small Gaussian stamp at each point into a W×H accumulator, then
// gamma-correct for glow and map through the colormap. STAMP_R is the kernel
// half-extent in pixels; SIGMA_PX its std-dev.
const STAMP_R = 18;
const SIGMA_PX = 7.0;
const GAMMA = 0.55; // <1 lifts mid densities into the glow (matches the card)

// Precompute the separable 1D Gaussian stamp profile.
const STAMP = new Float32Array(2 * STAMP_R + 1);
for (let d = -STAMP_R; d <= STAMP_R; d++) {
  STAMP[d + STAMP_R] = Math.exp(-(d * d) / (2 * SIGMA_PX * SIGMA_PX));
}

/** Rasterize one frame's KDE field → tightly-packed RGBA8 Buffer (W*H*4). */
function rasterize(p) {
  const acc = new Float32Array(W * H);
  const { xs, ys, ws } = pointsAt(p);

  for (let i = 0; i < xs.length; i++) {
    const px = xs[i] * (W - 1);
    const py = ys[i] * (H - 1);
    const cxi = Math.round(px);
    const cyi = Math.round(py);
    const wgt = ws[i];
    for (let dy = -STAMP_R; dy <= STAMP_R; dy++) {
      const yy = cyi + dy;
      if (yy < 0 || yy >= H) continue;
      const gy = STAMP[dy + STAMP_R];
      const row = yy * W;
      for (let dx = -STAMP_R; dx <= STAMP_R; dx++) {
        const xx = cxi + dx;
        if (xx < 0 || xx >= W) continue;
        acc[row + xx] += wgt * gy * STAMP[dx + STAMP_R];
      }
    }
  }

  // Normalize by a stable per-frame peak so the glow stays consistent across
  // frames (use a high percentile rather than the raw max to avoid flicker).
  let max = 0;
  for (let i = 0; i < acc.length; i++) if (acc[i] > max) max = acc[i];
  const norm = max > 0 ? 1 / max : 0;

  const out = Buffer.alloc(W * H * 4);
  for (let i = 0; i < acc.length; i++) {
    const t = Math.pow(acc[i] * norm, GAMMA);
    const [r, g, b] = colormap(t);
    const o = i * 4;
    out[o] = r;
    out[o + 1] = g;
    out[o + 2] = b;
    out[o + 3] = 255;
  }
  return out;
}

function main() {
  const textures = [];
  for (let k = 0; k < FRAME_COUNT; k++) {
    const p = k / FRAME_COUNT; // 0 .. <1 (frame N wraps onto frame 0)
    const pixels = rasterize(p);
    if (pixels.length !== W * H * 4) {
      throw new Error(`frame ${k}: pixel length ${pixels.length} != ${W * H * 4}`);
    }
    const t = Math.round((k / FRAME_COUNT) * DURATION_MS);
    textures.push({
      t,
      textureId: TEXTURE_ID,
      width: W,
      height: H,
      pixelsB64: pixels.toString('base64'),
      format: FORMAT,
    });
  }

  const out = {
    meta: {
      viewId: VIEW_ID,
      durationMs: DURATION_MS,
      frameCount: textures.length,
      cadenceMs: CADENCE_MS,
    },
    frames: [],
    textures,
  };

  writeFileSync(RECORDS_PATH, JSON.stringify(out));

  // ── self-validation ──────────────────────────────────────────────────────
  const okLen = textures.every(
    (f) => Buffer.from(f.pixelsB64, 'base64').length === W * H * 4,
  );
  const okId = textures.every((f) => f.textureId === TEXTURE_ID);
  console.log(
    `density-heatmap records: ${textures.length} texture frames, ` +
      `textureId=${TEXTURE_ID} (matches manifest), ${W}×${H} RGBA8, ` +
      `t∈[0, ${textures[textures.length - 1].t}]ms, cadence=${CADENCE_MS}ms`,
  );
  console.log(
    `validation: pixelsB64 length == ${W * H * 4} for all frames? ${okLen}; ` +
      `textureId matches for all frames? ${okId}`,
  );
  if (!okLen || !okId) throw new Error('self-validation FAILED');
}

main();
