/* apps/showcase/views/treemap/records.gen.mjs
 *
 * GENERATOR (zero-dep node) for the LIVE treemap (Linear ENC-580).
 *
 * ── What this emits ───────────────────────────────────────────────────────
 * A geometry-frame replay: a `records.json` whose every frame is a single
 * UPDATE_RANGE record (op 2, offset 0) that OVERWRITES the entire tile vertex
 * buffer with a freshly re-tessellated squarified treemap. The squarified
 * layout is re-run at N timesteps with the per-symbol / per-leaf weights
 * DRIFTING along the market dataset's volume time-series, so the tile rects
 * re-lay-out (resize + move) from frame to frame. ENC-569 makes the triGradient
 * vertex backend re-read + redraw an UPDATE_RANGE'd buffer each frame, so the
 * replay (useReplay → enqueueData, fixed-mode like depth-ladder) drives the
 * tiles live.
 *
 * The VERTEX COUNT is constant across all frames (always TILES tiles × 6 verts):
 * tiles RESIZE, the count never changes, so the pre-sized buffer is stable and
 * every frame is a full-buffer overwrite of identical length.
 *
 * Run:  node apps/showcase/views/treemap/records.gen.mjs
 * Out:  apps/showcase/views/treemap/records.json
 *
 * This is the build-time twin of manifest.ts: the layout/color math is kept in
 * lock-step (same constants, same squarify, same clip mapping) so the seeded
 * frame-0 upload in the manifest matches the replay's frame 0 exactly.
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const DATA = join(HERE, '..', '..', 'data', 'market');

// ── replay / buffer constants (must match manifest.ts) ─────────────────────
const TILE_BUFFER = 601; // pos2_color4 triangles, 6 verts/tile, 24B/vert
const VIEW_ID = 'treemap';
const FRAME_COUNT = 40; // N timesteps
const DURATION_MS = 20000; // total replay span
const CADENCE_MS = DURATION_MS / FRAME_COUNT; // 500ms between re-layouts

const SYMBOLS = ['AAPL', 'MSFT', 'NVDA', 'TSLA'];
const LEAVES_PER_SECTOR = 5;
const BUCKETS_PER_LEAF = 8; // 40 volume buckets / 5 leaves

function series(updates, field) {
  return updates.filter((u) => u.field === field).map((u) => u.value);
}

const DATASETS = Object.fromEntries(
  SYMBOLS.map((s) => [s, JSON.parse(readFileSync(join(DATA, `${s}.json`), 'utf8'))]),
);

// Per-symbol raw series, read once.
const RAW = SYMBOLS.map((sym) => {
  const u = DATASETS[sym].updates;
  return { sym, vol: series(u, 'volume'), open: series(u, 'open'), close: series(u, 'close') };
});

// ── time-evolving data model: sectors → leaves at timestep `frac` ──────────
// `frac` ∈ [0,1) sweeps a WINDOW across the 40 volume buckets. Each leaf's
// weight is the volume summed over its 8 buckets but EMPHASIS-weighted by a
// moving gaussian centred at the current time position — so as the window
// drifts, different leaves (and therefore different sectors) swell and shrink,
// forcing the squarified layout to re-pack. Color tracks the same window's
// open→close performance, so tiles can also re-tint over time.
function buildSectors(frac) {
  const center = frac * 40; // bucket-space cursor (0..40)
  const sigma = 7; // window half-width in buckets
  return RAW.map(({ vol, open, close }) => {
    const leaves = [];
    for (let g = 0; g < LEAVES_PER_SECTOR; g++) {
      let v = 0;
      for (let i = 0; i < BUCKETS_PER_LEAF; i++) {
        const b = g * BUCKETS_PER_LEAF + i;
        const d = b - center;
        const w = Math.exp(-(d * d) / (2 * sigma * sigma)); // moving emphasis
        v += vol[b] * (0.25 + w); // keep a floor so no leaf vanishes
      }
      // Color perf over the same emphasised window: open at window start vs
      // close near the cursor.
      const lo = g * BUCKETS_PER_LEAF;
      const hi = lo + BUCKETS_PER_LEAF - 1;
      const o = open[lo];
      const ci = Math.max(lo, Math.min(hi, Math.round(center)));
      const c = close[ci];
      leaves.push({ value: v, perf: (c - o) / o });
    }
    return { value: leaves.reduce((a, l) => a + l.value, 0), leaves };
  });
}

// ── squarified treemap (Bruls/Huizing/van Wijk) — identical to manifest.ts ─
function worst(row, side, sum) {
  if (row.length === 0) return Infinity;
  const s2 = sum * sum;
  const side2 = side * side;
  const max = Math.max(...row);
  const min = Math.min(...row);
  return Math.max((side2 * max) / s2, s2 / (side2 * min));
}

function squarify(children, rect, emit) {
  let { x, y, w, h } = rect;
  const items = children.slice();
  while (items.length > 0) {
    const side = Math.min(w, h);
    const row = [];
    let rowSum = 0;
    while (items.length > 0) {
      const next = items[0];
      const cur = worst(row.map((r) => r.area), side, rowSum);
      const cand = worst([...row.map((r) => r.area), next.area], side, rowSum + next.area);
      if (row.length === 0 || cand <= cur) {
        row.push(next);
        rowSum += next.area;
        items.shift();
      } else break;
    }
    if (w >= h) {
      const colW = rowSum / h;
      let cy = y;
      for (const it of row) {
        const tileH = (it.area / rowSum) * h;
        emit({ x, y: cy, w: colW, h: tileH }, it.payload);
        cy += tileH;
      }
      x += colW;
      w -= colW;
    } else {
      const rowH = rowSum / w;
      let cx = x;
      for (const it of row) {
        const tileW = (it.area / rowSum) * w;
        emit({ x: cx, y, w: tileW, h: rowH }, it.payload);
        cx += tileW;
      }
      y += rowH;
      h -= rowH;
    }
  }
}

// Full nested layout at timestep `frac`. NOTE: children are NOT re-sorted by
// size between frames — they keep a STABLE order (sector index, leaf index) so a
// tile's identity (and color slot) is continuous across frames; only its area
// changes. This keeps the re-layout a smooth resize rather than a reshuffle.
function layout(frac) {
  const sectors = buildSectors(frac);
  const total = sectors.reduce((a, s) => a + s.value, 0);
  const unit = { x: 0, y: 0, w: 1, h: 1 };
  const placed = [];

  const sectorChildren = sectors.map((s) => ({ area: (s.value / total) * (unit.w * unit.h), payload: s }));

  squarify(sectorChildren, unit, (sectorRect, sector) => {
    const pad = 0.006;
    const inner = {
      x: sectorRect.x + pad,
      y: sectorRect.y + pad,
      w: Math.max(sectorRect.w - 2 * pad, 0),
      h: Math.max(sectorRect.h - 2 * pad, 0),
    };
    const leafTotal = sector.leaves.reduce((a, l) => a + l.value, 0);
    const leafChildren = sector.leaves.map((l) => ({
      area: (l.value / leafTotal) * (inner.w * inner.h),
      payload: l,
    }));
    squarify(leafChildren, inner, (leafRect, leaf) => {
      placed.push({ rect: leafRect, perf: leaf.perf });
    });
  });

  return placed;
}

// ── color + clip mapping — identical to manifest.ts ────────────────────────
function perfColor(perf) {
  const t = Math.max(-1, Math.min(1, perf / 0.015));
  if (t >= 0) return [0.16 + 0.12 * (1 - t), 0.5 + 0.32 * t, 0.32 + 0.1 * (1 - t), 1];
  const a = -t;
  return [0.55 + 0.32 * a, 0.26 + 0.1 * (1 - a), 0.26 + 0.06 * (1 - a), 1];
}
function clipX(x) {
  return -0.92 + x * 1.84;
}
function clipY(y) {
  return 0.92 - y * 1.84;
}

function buildVertices(tiles) {
  const floats = [];
  const inset = 0.004;
  for (const tile of tiles) {
    const r = tile.rect;
    const x0 = clipX(r.x + inset);
    const x1 = clipX(r.x + r.w - inset);
    const yTop = clipY(r.y + inset);
    const yBot = clipY(r.y + r.h - inset);
    const [cr, cg, cb, ca] = perfColor(tile.perf);
    const v = (x, y) => floats.push(x, y, cr, cg, cb, ca);
    v(x0, yTop);
    v(x0, yBot);
    v(x1, yBot);
    v(x0, yTop);
    v(x1, yBot);
    v(x1, yTop);
  }
  return floats;
}

// ── pack one UPDATE_RANGE record + base64 it ───────────────────────────────
const OP_UPDATE_RANGE = 2;
const HEADER_SIZE = 13;
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

// ── generate frames ────────────────────────────────────────────────────────
// Constant tile count assertion: every timestep yields the same number of leaf
// tiles (SYMBOLS × LEAVES_PER_SECTOR), so the buffer length is stable.
const EXPECTED_TILES = SYMBOLS.length * LEAVES_PER_SECTOR;
const frames = [];
for (let f = 0; f < FRAME_COUNT; f++) {
  const frac = f / FRAME_COUNT; // 0 .. (1 - 1/N)
  const tiles = layout(frac);
  if (tiles.length !== EXPECTED_TILES) {
    throw new Error(`frame ${f}: ${tiles.length} tiles, expected ${EXPECTED_TILES}`);
  }
  const floats = buildVertices(tiles);
  const t = Math.round(f * CADENCE_MS);
  frames.push({ t, b64: encodeUpdateRange(TILE_BUFFER, 0, floats) });
}

const VERTEX_COUNT = EXPECTED_TILES * 6;
const records = {
  meta: { viewId: VIEW_ID, durationMs: DURATION_MS, frameCount: FRAME_COUNT, cadenceMs: CADENCE_MS },
  frames,
};

writeFileSync(join(HERE, 'records.json'), JSON.stringify(records));

// eslint-disable-next-line no-console
console.log(
  `wrote records.json: ${FRAME_COUNT} frames, ${EXPECTED_TILES} tiles, ` +
    `${VERTEX_COUNT} verts/frame, ${VERTEX_COUNT * 24}B payload/frame`,
);
