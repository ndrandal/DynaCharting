/* apps/showcase/views/correlation-heatmap/manifest.ts
 *
 * COMPOSED / STATIC view — "correlation matrix" heatmap (ENC-537 / T4.5),
 * using the ViewTexture escape hatch (PR #22 / ENC-532).
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * An N×N correlation heatmap colours every cell by a pairwise correlation. The
 * instanced/triangle pipelines can't give each cell its OWN per-cell colour from
 * a JSON manifest (one uniform colour per draw item; the per-cell-colour wall).
 * So the colormap is RASTERIZED upstream into an RGBA8 image and handed to the
 * `texturedQuad@1` pipeline as a producer-rasterized TEXTURE (the escape hatch):
 * the engine merely blits the texture onto a quad covering the pane.
 *
 * Pipeline:
 *   1. .build-composed-static.mjs computes the N×N Pearson correlation of the
 *      four symbols' per-bucket log returns, maps each cell through a diverging
 *      blue↔red colormap, and nearest-neighbour-upscales it to a 256×256 RGBA8
 *      image (crisp cell blocks + thin cell gridlines), base64-encoded below.
 *   2. manifest.textures = [{ textureId, width, height, pixelsB64 }] → applied
 *      via EngineHost.setTexturePixels when the manifest loads.
 *   3. A texturedQuad@1 drawItem covers the pane (one pos2_uv4 instance =
 *      the quad's min/max corners) and is bound to the texture by
 *      setDrawItemTexture(textureId). The shader maps uv 0..1 across the quad and
 *      samples the colormap — image row 0 lands at the quad top.
 *
 * The correlation matrix (AAPL, MSFT, NVDA, TSLA), for reference:
 *        AAPL     MSFT     NVDA     TSLA
 *  AAPL  1.0000   0.1449  -0.1098  -0.0949
 *  MSFT  0.1449   1.0000   0.1458  -0.2081
 *  NVDA -0.1098   0.1458   1.0000  -0.2872
 *  TSLA -0.0949  -0.2081  -0.2872   1.0000
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';
import { HEATMAP_TEXTURE_B64 } from './texture';

const PANE = 100;
const LAYER = 101;
const QUAD_BUFFER = 601; // pos2_uv4 (one quad instance: x0,y0,x1,y1)
const QUAD_GEOMETRY = 201;
const QUAD_DRAWITEM = 301;
const TEXTURE_ID = 700; // logical textureId referenced by setDrawItemTexture

const TEX_DIM = 256; // 4×4 matrix upscaled ×64

// Quad covering a centered square region of the pane (the matrix is square).
// pos2_uv4 record = vec4(x0,y0,x1,y1) = min/max corners in clip space; the
// shader generates uv 0..1 across them and y-negates (image row 0 → top).
const Q = 0.85;
const quadUpload: BufferUpload = {
  bufferId: QUAD_BUFFER,
  op: 'append',
  floats: [-Q, -Q, Q, Q], // x0,y0,x1,y1
};

export const manifest: SceneManifest = {
  label: 'Correlation Heatmap — texture escape hatch',
  // Producer-rasterized colormap texture, applied via setTexturePixels on load.
  textures: [
    {
      textureId: TEXTURE_ID,
      width: TEX_DIM,
      height: TEX_DIM,
      pixelsB64: HEATMAP_TEXTURE_B64,
      format: 1, // RGBA8
    },
  ],
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.98, clipXMax: 0.98, clipYMin: -0.98, clipYMax: 0.98 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // One-instance textured quad covering the pane.
    { cmd: 'createBuffer', id: QUAD_BUFFER, byteLength: 0 },
    { cmd: 'createGeometry', id: QUAD_GEOMETRY, vertexBufferId: QUAD_BUFFER, format: 'pos2_uv4', vertexCount: 1 },
    { cmd: 'createDrawItem', id: QUAD_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: QUAD_DRAWITEM, pipeline: 'texturedQuad@1', geometryId: QUAD_GEOMETRY },
    // texturedQuad multiplies the sampled texel by the draw-item color; white = passthrough.
    { cmd: 'setDrawItemStyle', drawItemId: QUAD_DRAWITEM, r: 1, g: 1, b: 1, a: 1 },
    { cmd: 'setDrawItemTexture', drawItemId: QUAD_DRAWITEM, textureId: TEXTURE_ID },
  ],
  uploads: [quadUpload],
};
