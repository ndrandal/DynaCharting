/* apps/showcase/views/scatter/manifest.ts
 *
 * The scatter view's SceneManifest (CONTRACT-view-catalog.md). A scatter plot of
 * two correlated AAPL series — lastPrice (x) and cumulative volume (y) — joined
 * into ONE (x,y) record per tick.
 *
 * The join is the showcase-explicit-v1 "dual-dynamic-slot" trick (DESIGN-
 * buffer-binding.md): a single compound buffer (700, stride 8) where BOTH lanes
 * are dynamic — x = lastPrice, y = volume — and embassy emits one 8-byte (x,y)
 * APPEND per tick only when BOTH subscriptions have fired. The captured cloud is
 * in records.json (instruction.json documents that capture).
 *
 * LIVE STREAMING (ENC-574) — the cloud fills IN over the ~20s replay, one point
 * per tick, instead of appearing all at once. The view used to be STATIC: the
 * chrome batch tessellated every captured (x,y) point into ONE static
 * instancedRect@1 upload. Now `records.json` (rebuilt by records.gen.mjs) streams
 * each point as a small rect4 marker (16B) APPENDED to MARKER_BUFFER over the
 * timeline, and the manifest exports a `growth` descriptor so the replay engine
 * advances the instancedRect geometry's vertexCount as markers land (ENC-558:
 * the instanced backend caches its GPU buffer per geometryId, so growth =
 * fresh-geometry rebind). There is NO static upload — the buffer starts empty
 * (byteLength 0) and grows live.
 *
 * MARKERS — instancedRect@1, mirroring sports-shot-chart (ENC-564): the brief
 * called for points@1 (pos2_clip), but points@1 on WebGPU is a 1px PointList
 * with NO size control (di.pointSize is ignored — see DawnPointsBackend), so the
 * streamed single-pixel dots are effectively invisible at gallery scale. So each
 * captured (x,y) point is emitted as a small AXIS-ALIGNED SQUARE (rect4:
 * x0,y0,x1,y1) — same scatter geometry, just visible. The marker half-size is
 * authored in CLIP space (≈7px at 1k canvas) and inverted through the view.json
 * transform per-axis into DATA space (in records.gen.mjs), so every square is the
 * same on-screen size regardless of the price/volume scales.
 *
 * Framing (data→clip) stays in view.json (`transform`), baked over the FULL
 * price/volume range so the late-arriving points are never clipped, and the
 * chrome overlay maps its axes (price X, volume Y) through that same transform.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (local to this view; reused after scene reset) ---
const PANE = 100;
const LAYER = 101;
export const TRANSFORM = 150;
const MARKER_GEOMETRY = 200;
const MARKER_DRAWITEM = 300;
const MARKER_BUFFER = 701; // rect4 16B marker squares (streamed live, grows)

const MARKER_STRIDE = 16; // rect4: x0,y0,x1,y1 = 4×f32

// view.json transform (data→clip), mirrored here only so the explicit
// setTransform below matches view.json's framing (this view has no xAnchor /
// embassy-seeded transform — like renko / sports-shot-chart). The chrome overlay
// maps its axes through the identical mapping.
const SX = 0.1705381; // x = lastPrice → clip
const SY = 0.000009792795; // y = volume → clip
const TX = -70.348033;
const TY = -0.770121;

export const manifest: SceneManifest = {
  label: 'Scatter — Price × Volume',
  commands: [
    // One pane filling most of the clip box, dark surface.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Marker buffer grown at replay time (byteLength 0). The replay engine
    // advances vertexCount = floor(byteLength / 16) as rect4 markers land.
    { cmd: 'createBuffer', id: MARKER_BUFFER, byteLength: 0 },
    // vertexCount starts at 1 (instanced pipelines reject 0); growth advances it
    // to the real marker count as frames arrive.
    {
      cmd: 'createGeometry',
      id: MARKER_GEOMETRY,
      vertexBufferId: MARKER_BUFFER,
      format: 'rect4',
      vertexCount: 1,
    },

    // Data->clip transform. This view has no `growth` X-anchor, so we set the
    // same affine explicitly (mirrors view.json so the chrome overlay's axes map
    // through the identical mapping). Baked over the full price/volume range so
    // late-arriving points stay on-screen.
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX, sy: SY, tx: TX, ty: TY },

    { cmd: 'createDrawItem', id: MARKER_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: MARKER_DRAWITEM, pipeline: 'instancedRect@1', geometryId: MARKER_GEOMETRY },
    // Bright green markers on the near-black pane (the showcase accent).
    {
      cmd: 'setDrawItemStyle',
      drawItemId: MARKER_DRAWITEM,
      r: 0.24, g: 0.86, b: 0.52, a: 0.9,
    },
    { cmd: 'attachTransform', drawItemId: MARKER_DRAWITEM, transformId: TRANSFORM },
  ],
  // No uploads — the cloud streams in live via useReplay (CONTRACT-view-catalog.md).
};

/**
 * Growth descriptor for the replay engine. The renderer draws
 * geometry.vertexCount instances; the dataplane only grows the buffer's bytes,
 * so the replay engine advances vertexCount = floor(byteLength / stride) as the
 * rect4 markers land (fresh-geometry rebind — ENC-558 backend caching). No
 * xField/xAnchor: the transform is baked over the full range in view.json, so
 * the cloud frames statically while the points stream in.
 */
export const growth: GrowthSync = {
  bufferId: MARKER_BUFFER,
  geometryId: MARKER_GEOMETRY,
  drawItemId: MARKER_DRAWITEM,
  layerId: LAYER,
  stride: MARKER_STRIDE, // rect4: x0,y0,x1,y1 = 4×f32
  format: 'rect4',
  pipeline: 'instancedRect@1',
  transformId: TRANSFORM,
  xField: 0, // unused (no xAnchor in view.json); kept for the GrowthSync shape
};
