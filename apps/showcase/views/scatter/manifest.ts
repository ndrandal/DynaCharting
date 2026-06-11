/* apps/showcase/views/scatter/manifest.ts
 *
 * The scatter view's SceneManifest (CONTRACT-view-catalog.md). A points@1
 * scatter plot: two correlated AAPL series — lastPrice (x) and volume (y) —
 * joined into ONE pos2_clip point per tick and drawn as GPU points.
 *
 * The join is the showcase-explicit-v1 "dual-dynamic-slot" trick (DESIGN-
 * buffer-binding.md): a single compound buffer (700, stride 8) where BOTH
 * lanes are dynamic — x = lastPrice at intraOffset 0 / slotBit 0, y = volume
 * at intraOffset 4 / slotBit 1, fullMask 0b11. embassy's routeCompound emits
 * one APPEND of an 8-byte (x,y) record only when BOTH subscriptions have
 * fired for the tick, so the two streams literally join into one point record
 * (pos2_clip). No staticFields — both lanes carry real data, not recordIndex.
 *
 * Like the candle view, the point buffer (700) is grown record-by-record at
 * replay time by useReplay (GROWTH below): the points@1 backend caches its GPU
 * buffer per geometryId (ENC-558), so growth = fresh-geometry rebind as records
 * land. There are no `uploads` — data arrives via the captured records.json.
 *
 * Transform framing (data→clip) lives in view.json (`transform`), baked from
 * the OBSERVED (x,y) range in records.json. xAnchor is OFF: x is lastPrice (a
 * dynamic field), not a record index, so we bake a full static affine that maps
 * the captured price×volume cloud into the pane's clip box.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (local to this view; reused after scene reset) ---
const PANE = 100;
const LAYER = 101;
export const TRANSFORM = 150;
const POINT_BUFFER = 700; // pos2_clip 8B (x=lastPrice, y=volume) — MUST match instruction bufferId
const POINT_GEOMETRY = 200;
const POINT_DRAWITEM = 300;

export const manifest: SceneManifest = {
  label: 'Scatter — Price × Volume',
  commands: [
    // One pane filling most of the clip box, dark surface.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Point buffer (grown at replay time) -> pos2_clip geometry -> drawItem.
    { cmd: 'createBuffer', id: POINT_BUFFER, byteLength: 0 },
    // vertexCount starts at 1 (pipelines reject 0); useReplay advances it to the
    // real record count (bytes/8) as the joined (x,y) records arrive.
    {
      cmd: 'createGeometry',
      id: POINT_GEOMETRY,
      vertexBufferId: POINT_BUFFER,
      format: 'pos2_clip',
      vertexCount: 1,
    },

    // Data->clip transform. The baked sx/sy/tx/ty are seeded from view.json by
    // the switch controller (here we create it; values set via setTransform).
    { cmd: 'createTransform', id: TRANSFORM },

    { cmd: 'createDrawItem', id: POINT_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: POINT_DRAWITEM, pipeline: 'points@1', geometryId: POINT_GEOMETRY },
    // Bright green points on the near-black pane (the showcase accent).
    {
      cmd: 'setDrawItemStyle',
      drawItemId: POINT_DRAWITEM,
      colorR: 0.24, colorG: 0.86, colorB: 0.52, colorA: 0.9,
    },
    { cmd: 'attachTransform', drawItemId: POINT_DRAWITEM, transformId: TRANSFORM },
  ],
  // No uploads — data arrives via useReplay (CONTRACT-view-catalog.md).
};

/**
 * Growth descriptor for the points geometry. useReplay draws
 * geometry.vertexCount points and the dataplane only grows the buffer's bytes,
 * so it advances vertexCount = floor(byteLength / 8) via fresh-geometry rebind
 * (ENC-558 backend-caching workaround) as joined (x,y) records land.
 *
 * xField = 0 is the byte offset of the x lane (lastPrice) within a record; it
 * feeds useReplay's optional X-anchor. This view does NOT set view.json.xAnchor
 * (x is a price, not a record index), so the anchor path is inert and the baked
 * static transform from view.json frames the cloud.
 */
export const growth: GrowthSync = {
  bufferId: POINT_BUFFER,
  geometryId: POINT_GEOMETRY,
  drawItemId: POINT_DRAWITEM,
  layerId: LAYER,
  stride: 8, // pos2_clip: [x, y] = 2×f32
  format: 'pos2_clip',
  pipeline: 'points@1',
  transformId: TRANSFORM,
  xField: 0, // byte offset of x (lastPrice) within a pos2_clip record
};
