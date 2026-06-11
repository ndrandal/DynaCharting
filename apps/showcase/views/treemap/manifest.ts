/* apps/showcase/views/treemap/manifest.ts
 *
 * COMPOSED — "Finviz market map" squarified treemap, a STATIC
 * COMPUTED-GEOMETRY view (Linear ENC-536 / T4.4). The whole nested rect
 * layout is TESSELLATED AT MANIFEST-BUILD TIME from the market datasets and
 * embedded as static manifest `uploads` — no streaming, capture, or replay.
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * Two-level treemap: the four symbols (AAPL/MSFT/NVDA/TSLA) are the "sectors",
 * each sized by its total traded volume; within a sector, five time-bucket
 * groups are the leaf tiles, sized by their own volume share. We run a
 * SQUARIFIED-treemap layout (Bruls/Huizing/van Wijk) at module-eval time:
 * recursively pack children into rows that minimise tile aspect ratio, first
 * over the sectors in the unit rect, then over each sector's leaves in its
 * sub-rect. Every leaf becomes one axis-aligned rect.
 *
 * ── Geometry (triGradient@1 / pos2_color4) ────────────────────────────────
 * Each leaf rect is emitted as TWO triangles (6 vertices) in the pos2_color4
 * format: each vertex carries its clip-space position AND an RGBA color. The
 * color encodes the leaf's price performance over its window — green for gains,
 * red for losses, brightness ∝ magnitude (the Finviz heat). Because color
 * rides the vertex buffer, a SINGLE triGradient draw item renders all 20 tiles
 * with independent per-tile color (no per-item color uniform needed). The
 * vertices are authored directly in clip space, so no transform is attached.
 *
 * No `growth`, no streaming: the layout is fully resolved at build time.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';
import aapl from '../../data/market/AAPL.json';
import msft from '../../data/market/MSFT.json';
import nvda from '../../data/market/NVDA.json';
import tsla from '../../data/market/TSLA.json';

const PANE = 100;
const LAYER = 101;
const TILE_BUFFER = 601; // pos2_color4 triangles (6 verts/tile)
const TILE_GEOMETRY = 201;
const TILE_DRAWITEM = 301;

type Update = { field: string; value: number };
const DATASETS: Record<string, { updates: Update[] }> = {
  AAPL: aapl as { updates: Update[] },
  MSFT: msft as { updates: Update[] },
  NVDA: nvda as { updates: Update[] },
  TSLA: tsla as { updates: Update[] },
};
const SYMBOLS = ['AAPL', 'MSFT', 'NVDA', 'TSLA'];
const LEAVES_PER_SECTOR = 5;
const BUCKETS_PER_LEAF = 8; // 40 OHLC buckets / 5 leaves

function series(updates: Update[], field: string): number[] {
  return updates.filter((u) => u.field === field).map((u) => u.value);
}

// --- build-time data model: sectors → leaves -------------------------------
interface Leaf {
  value: number; // sizing weight (traded volume in the window)
  perf: number; // signed price performance over the window (for color)
}
interface Sector {
  value: number; // total weight (sum of leaf values)
  leaves: Leaf[];
}

function buildSectors(): Sector[] {
  return SYMBOLS.map((sym) => {
    const u = DATASETS[sym].updates;
    const vol = series(u, 'volume');
    const open = series(u, 'open');
    const close = series(u, 'close');
    const leaves: Leaf[] = [];
    for (let g = 0; g < LEAVES_PER_SECTOR; g++) {
      let v = 0;
      for (let i = 0; i < BUCKETS_PER_LEAF; i++) v += vol[g * BUCKETS_PER_LEAF + i];
      const o = open[g * BUCKETS_PER_LEAF];
      const c = close[g * BUCKETS_PER_LEAF + BUCKETS_PER_LEAF - 1];
      leaves.push({ value: v, perf: (c - o) / o });
    }
    return { value: leaves.reduce((a, l) => a + l.value, 0), leaves };
  });
}

// --- squarified treemap (Bruls/Huizing/van Wijk) ---------------------------
interface Rect {
  x: number;
  y: number;
  w: number;
  h: number;
}
interface Placed {
  rect: Rect;
  perf: number;
}

/** Worst aspect ratio of a row of areas laid along the shorter side `side`. */
function worst(row: number[], side: number, sum: number): number {
  if (row.length === 0) return Infinity;
  const s2 = sum * sum;
  const side2 = side * side;
  const max = Math.max(...row);
  const min = Math.min(...row);
  return Math.max((side2 * max) / s2, s2 / (side2 * min));
}

/**
 * Squarify a list of (area, payload) children into `rect`, calling `emit` for
 * each placed child. Areas are pre-scaled to sum to rect.w*rect.h.
 */
function squarify<T>(
  children: { area: number; payload: T }[],
  rect: Rect,
  emit: (r: Rect, payload: T) => void,
): void {
  let { x, y, w, h } = rect;
  let items = children.slice();
  while (items.length > 0) {
    const side = Math.min(w, h);
    const row: { area: number; payload: T }[] = [];
    let rowSum = 0;
    // Greedily extend the row while it improves (lowers) the worst aspect.
    while (items.length > 0) {
      const next = items[0];
      const cur = worst(row.map((r) => r.area), side, rowSum);
      const cand = worst(
        [...row.map((r) => r.area), next.area],
        side,
        rowSum + next.area,
      );
      if (row.length === 0 || cand <= cur) {
        row.push(next);
        rowSum += next.area;
        items.shift();
      } else {
        break;
      }
    }
    // Lay the row along the shorter side; advance the remaining rect.
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

/** Full nested layout: squarify sectors in the unit rect, then leaves within. */
function layout(): Placed[] {
  const sectors = buildSectors();
  const total = sectors.reduce((a, s) => a + s.value, 0);
  const unit: Rect = { x: 0, y: 0, w: 1, h: 1 };
  const placed: Placed[] = [];

  // Scale sector areas to fill the unit rect, then squarify.
  const sectorChildren = sectors
    .map((s) => ({ area: (s.value / total) * (unit.w * unit.h), payload: s }))
    .sort((a, b) => b.area - a.area);

  squarify(sectorChildren, unit, (sectorRect, sector) => {
    const pad = 0.006; // gutter between sectors
    const inner: Rect = {
      x: sectorRect.x + pad,
      y: sectorRect.y + pad,
      w: Math.max(sectorRect.w - 2 * pad, 0),
      h: Math.max(sectorRect.h - 2 * pad, 0),
    };
    const leafTotal = sector.leaves.reduce((a, l) => a + l.value, 0);
    const leafChildren = sector.leaves
      .map((l) => ({ area: (l.value / leafTotal) * (inner.w * inner.h), payload: l }))
      .sort((a, b) => b.area - a.area);
    squarify(leafChildren, inner, (leafRect, leaf) => {
      placed.push({ rect: leafRect, perf: leaf.perf });
    });
  });

  return placed;
}

const TILES = layout();

// --- color: signed perf → green/red heat (Finviz palette) ------------------
function perfColor(perf: number): [number, number, number, number] {
  // Normalise to a comfortable range; clamp magnitude for the brightest tiles.
  const t = Math.max(-1, Math.min(1, perf / 0.015));
  if (t >= 0) {
    return [0.16 + 0.12 * (1 - t), 0.5 + 0.32 * t, 0.32 + 0.1 * (1 - t), 1];
  }
  const a = -t;
  return [0.55 + 0.32 * a, 0.26 + 0.1 * (1 - a), 0.26 + 0.06 * (1 - a), 1];
}

// --- tessellate tiles → pos2_color4 triangles ------------------------------
// Layout is in [0,1]² (y down from top); map to clip [-0.92,0.92] with +Y up.
function clipX(x: number): number {
  return -0.92 + x * 1.84;
}
function clipY(y: number): number {
  return 0.92 - y * 1.84; // flip so y=0 is top
}

function buildVertices(): number[] {
  const floats: number[] = [];
  const inset = 0.004; // tile gutter in layout units
  for (const tile of TILES) {
    const r = tile.rect;
    const x0 = clipX(r.x + inset);
    const x1 = clipX(r.x + r.w - inset);
    const yTop = clipY(r.y + inset);
    const yBot = clipY(r.y + r.h - inset);
    const [cr, cg, cb, ca] = perfColor(tile.perf);
    const v = (x: number, y: number) => floats.push(x, y, cr, cg, cb, ca);
    // two triangles (TL,BL,BR) + (TL,BR,TR)
    v(x0, yTop);
    v(x0, yBot);
    v(x1, yBot);
    v(x0, yTop);
    v(x1, yBot);
    v(x1, yTop);
  }
  return floats;
}

const VERTS = buildVertices();
const VERTEX_COUNT = VERTS.length / 6; // pos2 + color4 = 6 floats/vertex

export const manifest: SceneManifest = {
  label: 'Treemap — market map (Finviz)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.04, g: 0.05, b: 0.07, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    { cmd: 'createBuffer', id: TILE_BUFFER, byteLength: 0 },
    { cmd: 'createGeometry', id: TILE_GEOMETRY, vertexBufferId: TILE_BUFFER, format: 'pos2_color4', vertexCount: VERTEX_COUNT },

    { cmd: 'createDrawItem', id: TILE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: TILE_DRAWITEM, pipeline: 'triGradient@1', geometryId: TILE_GEOMETRY },
  ],
  // The full nested treemap, tessellated at build time as per-vertex-colored
  // triangles (one static APPEND upload).
  uploads: [{ bufferId: TILE_BUFFER, floats: VERTS } as BufferUpload],
};
