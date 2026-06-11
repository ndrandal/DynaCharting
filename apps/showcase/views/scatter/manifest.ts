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
 * in records.json.
 *
 * MARKERS — instancedRect@1, mirroring sports-shot-chart (ENC-564): the brief
 * called for points@1 (pos2_clip), but points@1 on WebGPU is a 1px PointList
 * with NO size control (di.pointSize is ignored — see DawnPointsBackend), so the
 * streamed single-pixel dots are effectively invisible at gallery scale. To make
 * the cloud crisp we tessellate each captured (x,y) point into a small AXIS-
 * ALIGNED SQUARE (rect4: x0,y0,x1,y1) at module-eval time — decoding records.json
 * exactly like the renko view decodes its AAPL series — and draw the whole cloud
 * as ONE static instancedRect@1 draw item. Same scatter geometry, just visible.
 *
 * The marker half-size is authored in CLIP space (≈7px at 1k canvas) and inverted
 * through the view.json transform per-axis into DATA space, so every square is
 * the same on-screen size regardless of the price/volume scales. Framing
 * (data→clip) stays in view.json (`transform`), and the chrome overlay maps its
 * axes (price X, volume Y) through that same transform.
 *
 * No `growth` export: the cloud is fully resolved at build time (like
 * sports-shot-chart / renko). useReplay still streams the captured frames into
 * buffer 700, but no geometry is bound to it, so those appends are inert ambient
 * data — the visible scatter is the static rect4 cloud below.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';
import records from './records.json' with { type: 'json' };

// --- structural IDs (local to this view; reused after scene reset) ---
const PANE = 100;
const LAYER = 101;
export const TRANSFORM = 150;
const POINT_GEOMETRY = 200;
const POINT_DRAWITEM = 300;
const MARKER_BUFFER = 701; // rect4 16B static marker squares (the visible cloud)

// view.json transform (data→clip). Mirrored here only to invert clip-space
// marker half-size into data space per axis; framing itself lives in view.json.
const SX = 0.1705381; // x = lastPrice → clip   (mirrors view.json transform)
const SY = 0.000009792795; // y = volume → clip
const TX = -70.348033;
const TY = -0.770121;

// Marker half-size in CLIP units (~7px on a 1024px canvas), inverted per axis so
// every square renders the same on-screen size despite the price/volume scales.
const HALF_CLIP = 0.007;
const HALF_X = HALF_CLIP / SX; // ≈ 0.041 price units
const HALF_Y = HALF_CLIP / SY; // ≈ 715 volume units

// --- build-time tessellation: decode the captured (x,y) cloud → rect squares ---
const OP_APPEND = 1;
const RECORD_HEADER_SIZE = 13;
const POINT_BUFFER_ID = 700; // pos2_clip 8B (x=lastPrice, y=volume)

function b64ToBytes(b64: string): Uint8Array {
  const bin = atob(b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return bytes;
}

/** Decode every captured (lastPrice, volume) point from the records frames. */
function decodePoints(): Array<[number, number]> {
  const out: Array<[number, number]> = [];
  for (const frame of records.frames) {
    const bytes = b64ToBytes(frame.b64);
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let o = 0;
    while (o + RECORD_HEADER_SIZE <= dv.byteLength) {
      const op = dv.getUint8(o);
      const bufId = dv.getUint32(o + 1, true);
      const payloadBytes = dv.getUint32(o + 9, true);
      o += RECORD_HEADER_SIZE;
      if (o + payloadBytes > dv.byteLength) break;
      if (op === OP_APPEND && bufId === POINT_BUFFER_ID) {
        for (let p = 0; p + 8 <= payloadBytes; p += 8) {
          out.push([dv.getFloat32(o + p, true), dv.getFloat32(o + p + 4, true)]);
        }
      }
      o += payloadBytes;
    }
  }
  return out;
}

/** Pack the decoded points as rect4 marker squares (x0,y0,x1,y1) in DATA space. */
function markerRects(): BufferUpload {
  const floats: number[] = [];
  for (const [x, y] of decodePoints()) {
    floats.push(x - HALF_X, y - HALF_Y, x + HALF_X, y + HALF_Y);
  }
  return { bufferId: MARKER_BUFFER, op: 'updateRange', offsetBytes: 0, floats };
}

const MARKERS = markerRects();
const MARKER_COUNT = MARKERS.floats.length / 4;

export const manifest: SceneManifest = {
  label: 'Scatter — Price × Volume',
  commands: [
    // One pane filling most of the clip box, dark surface.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Static marker cloud: each captured (x,y) point as a small rect4 square.
    { cmd: 'createBuffer', id: MARKER_BUFFER, byteLength: MARKER_COUNT * 16 },
    {
      cmd: 'createGeometry',
      id: POINT_GEOMETRY,
      vertexBufferId: MARKER_BUFFER,
      format: 'rect4',
      vertexCount: MARKER_COUNT,
    },

    // Data->clip transform. This view has no `growth` export, so the controller
    // does NOT seed view.json.transform here — we set the same affine explicitly
    // (like renko / sports-shot-chart). It still mirrors view.json so the chrome
    // overlay's axes map through the identical mapping.
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX, sy: SY, tx: TX, ty: TY },

    { cmd: 'createDrawItem', id: POINT_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: POINT_DRAWITEM, pipeline: 'instancedRect@1', geometryId: POINT_GEOMETRY },
    // Bright green markers on the near-black pane (the showcase accent).
    {
      cmd: 'setDrawItemStyle',
      drawItemId: POINT_DRAWITEM,
      r: 0.24, g: 0.86, b: 0.52, a: 0.9,
    },
    { cmd: 'attachTransform', drawItemId: POINT_DRAWITEM, transformId: TRANSFORM },
  ],
  // STATIC: the whole price×volume cloud, tessellated to rect4 squares once.
  uploads: [MARKERS],
};
