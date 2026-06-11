/* apps/showcase/src/scene/sampleManifest.ts
 *
 * ONE hardcoded sample SceneDocument for the T0.3 render shell. It exercises the
 * full path end-to-end with a simple, visibly-non-blank scene:
 *
 *   - a dark pane filling the viewport,
 *   - two filled triangles (triSolid@1, pos2_clip vertices) in clip space,
 *   - a polyline (line2d@1) driven through a transform (proves the transform
 *     plumbing without needing live data).
 *
 * Buffer IDs follow the showcase convention (CONTRACT-buffer-id.md): 1xx
 * structural, 5xx time-series/vertex data, 2xx geometry, 3xx drawItems. All IDs
 * are local to this manifest (the engine is scene-reset between manifests).
 *
 * Vertices are authored directly in clip space (-1..1, +Y up) so no data->clip
 * transform is needed for the triangles; the line uses an explicit transform.
 */

import type { SceneManifest } from './commands';

// --- structural ---
const PANE = 100;
const LAYER = 101;
const TRANSFORM = 110;

// --- triangles ---
const TRI_BUFFER = 500;
const TRI_GEOMETRY = 200;
const TRI_DRAWITEM = 300;

// --- line ---
const LINE_BUFFER = 501;
const LINE_GEOMETRY = 201;
const LINE_DRAWITEM = 301;

// Two filled triangles in clip space (pos2_clip = x,y per vertex). 3 verts each.
const TRIANGLE_VERTS = [
  // left triangle
  -0.75, -0.4,
  -0.25, -0.4,
  -0.5, 0.45,
  // right triangle
  0.25, -0.4,
  0.75, -0.4,
  0.5, 0.45,
];

// A zig-zag polyline in a "data-ish" space; the transform maps it into clip.
// line2d@1 also reads pos2_clip pairs; here the raw values sit roughly in
// [0..10] x [0..1] and the transform scales/translates them into clip space.
const LINE_POINTS = [
  0.0, 0.2,
  1.5, 0.7,
  3.0, 0.3,
  4.5, 0.8,
  6.0, 0.25,
  7.5, 0.6,
  9.0, 0.35,
  10.0, 0.55,
];

export const SAMPLE_MANIFEST: SceneManifest = {
  label: 'Sample scene — triSolid + line2d',
  commands: [
    // Pane + layer.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -1, clipXMax: 1, clipYMin: -1, clipYMax: 1 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.043, g: 0.051, b: 0.071, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Triangles: buffer -> geometry (pos2_clip, 6 verts) -> drawItem (triSolid@1).
    { cmd: 'createBuffer', id: TRI_BUFFER, byteLength: 0 },
    {
      cmd: 'createGeometry',
      id: TRI_GEOMETRY,
      vertexBufferId: TRI_BUFFER,
      format: 'pos2_clip',
      vertexCount: TRIANGLE_VERTS.length / 2,
    },
    { cmd: 'createDrawItem', id: TRI_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: TRI_DRAWITEM, pipeline: 'triSolid@1', geometryId: TRI_GEOMETRY },
    { cmd: 'setDrawItemColor', drawItemId: TRI_DRAWITEM, r: 0.3, g: 0.69, b: 0.31, a: 1 },

    // Line: a transform that maps the [0..10]x[0..1] points into clip space.
    // sx = 2/10 = 0.2, tx = -1; sy = 1.2, ty = -0.5 (lifts the line into view).
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: 0.2, sy: 1.2, tx: -1, ty: -0.5 },

    { cmd: 'createBuffer', id: LINE_BUFFER, byteLength: 0 },
    {
      cmd: 'createGeometry',
      id: LINE_GEOMETRY,
      vertexBufferId: LINE_BUFFER,
      format: 'pos2_clip',
      vertexCount: LINE_POINTS.length / 2,
    },
    { cmd: 'createDrawItem', id: LINE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: LINE_DRAWITEM, pipeline: 'line2d@1', geometryId: LINE_GEOMETRY },
    { cmd: 'setDrawItemColor', drawItemId: LINE_DRAWITEM, r: 0.96, g: 0.76, b: 0.2, a: 1 },
    { cmd: 'setDrawItemStyle', drawItemId: LINE_DRAWITEM, lineWidth: 2.5, pointSize: 0 },
    { cmd: 'attachTransform', drawItemId: LINE_DRAWITEM, transformId: TRANSFORM },
  ],
  uploads: [
    { bufferId: TRI_BUFFER, floats: TRIANGLE_VERTS },
    { bufferId: LINE_BUFFER, floats: LINE_POINTS },
  ],
};
