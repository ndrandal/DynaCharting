/* apps/showcase/views/audio-waveform/manifest.ts
 *
 * CROSS-DOMAIN view (ENC-538 / T4.6) — "audio editor waveform".
 *
 * A STREAMING computed view (ENC-571): the classic DAW amplitude envelope, drawn
 * as a dense MIRRORED FILLED AREA by the native `triGradient@1` pipeline — but
 * the trace is no longer baked. A ~20s audio-like signal (a few mixed sinusoids
 * under a swelling/decaying speech-or-music amplitude envelope, plus transient
 * attack/decay "bursts") is synthesized deterministically in records.gen.mjs,
 * its amplitude ENVELOPE computed, and the filled envelope STREAMED column-by-
 * column into a growing `pos2_color4` vertex buffer over the recorded timeline
 * (records.json) — the SAME live data path the candle / ecg / scatter views use
 * (no `uploads`). The band fills IN left→right like a track being recorded.
 *
 * --- The streaming model (growing filled area, mirrors scatter/ecg APPEND) ---
 *
 * The envelope is materialized as a mirrored filled AREA: each pair of adjacent
 * columns forms a quad spanning (+amp .. -amp), split into two triangles = 6
 * pos2_color4 vertices (x, y, r, g, b, a = 6×f32 = 24 bytes/vertex). Each frame
 * APPENDs the column quads that have "played" since the previous frame, so the
 * geometry GROWS. As records land, useReplay's GrowthSync advances
 * geometry.vertexCount = floor(byteLength / 24), so the filled band lengthens to
 * the right exactly like the live candle / ecg series lengthen (ENC-569: the
 * triGradient backend re-reads its grown buffer + redraws once vertexCount
 * advances). The buffer starts EMPTY (byteLength 0) — there is no baked `uploads`
 * block any more; records.gen.mjs produces every frame.
 *
 * --- Why a FILLED area, NOT line2d@1 (ENC-587) ---
 *
 * line2d@1 is a 1px WebGPU LineList that draws DISCONNECTED vertex PAIRS → a
 * thin, broken trace (the exact bug ENC-587 documents for ecg/audio). A waveform
 * envelope reads far more boldly as a filled polygon anyway, and triGradient@1
 * carries PER-VERTEX color, so the fill brightens (teal → cyan) at the peaks and
 * dims at the troughs/mirror — depth without any shader work and zero line
 * pipeline. Vertices are authored directly in CLIP space (Pos2Color4), so NO
 * transform is attached — the geometry is its own framing.
 *
 * The cross-domain point still holds: the SAME gradient-fill pipeline that shades
 * market areas streams a DAW-style audio envelope — the geometry is domain-
 * agnostic, only the data changes.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (local to this view; scene is reset between views) ---
const PANE = 100;
const LAYER = 101;
const WAVE_BUFFER = 500; // pos2_color4 mirrored-area triangles — MUST match records.gen.mjs
const WAVE_GEOMETRY = 200;
const WAVE_DRAWITEM = 300;

const WAVE_STRIDE = 24; // pos2_color4 vertex = [x, y, r, g, b, a] = 6×f32

export const manifest: SceneManifest = {
  label: 'Audio waveform — mirrored amplitude',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.97, clipXMax: 0.97, clipYMin: -0.95, clipYMax: 0.95 },
    // Dark "audio editor" canvas.
    { cmd: 'setPaneClearColor', id: PANE, r: 0.04, g: 0.05, b: 0.07, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Envelope buffer (grown at replay time, column-by-column) -> triGradient
    // geometry. byteLength 0: the buffer starts EMPTY and the records.json frames
    // grow it. vertexCount starts at 3 (a triangle needs >=3 verts); the replay
    // engine advances it to floor(byteLength / 24) as column quads arrive, so the
    // filled envelope lengthens to the right.
    { cmd: 'createBuffer', id: WAVE_BUFFER, byteLength: 0 },
    {
      cmd: 'createGeometry',
      id: WAVE_GEOMETRY,
      vertexBufferId: WAVE_BUFFER,
      format: 'pos2_color4',
      vertexCount: 3,
    },

    { cmd: 'createDrawItem', id: WAVE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: WAVE_DRAWITEM, pipeline: 'triGradient@1', geometryId: WAVE_GEOMETRY },
    // triGradient colors come from the per-vertex r,g,b,a in the geometry; no
    // uniform color/transform needed (geometry is authored in clip space).
  ],
  // STREAMING: no uploads — the envelope buffer is grown column-by-column at
  // replay time from records.json via useReplay over the data plane (the live
  // path). See records.gen.mjs.
};

/**
 * Growth descriptor: the pos2_color4 envelope buffer is the live-growing series
 * the replay engine advances. Each appended column quad is 6 pos2_color4
 * vertices (2 triangles) = 144 bytes, so vertexCount = byteLength / 24 as the
 * band lengthens. The geometry is authored in clip space (no transform / no
 * xAnchor), so xField is unused — kept for the GrowthSync shape.
 */
export const growth: GrowthSync = {
  bufferId: WAVE_BUFFER,
  geometryId: WAVE_GEOMETRY,
  drawItemId: WAVE_DRAWITEM,
  layerId: LAYER,
  stride: WAVE_STRIDE, // pos2_color4: [x, y, r, g, b, a] = 6×f32
  format: 'pos2_color4',
  pipeline: 'triGradient@1',
  transformId: 0, // none (geometry is authored directly in clip space)
  xField: 0, // unused (no xAnchor; clip-space geometry)
};
