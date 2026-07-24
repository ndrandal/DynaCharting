/* packages/dc-wasm/src/chart/text.ts — ENC-715
 *
 * GPU text (textSDF@1) authoring helper. Wraps the WASM core's already-compiled
 * SDF glyph-atlas text path (loadFont + setTextGeometry, ENC-589) so axis ticks,
 * titles, and legends can render ON THE GPU instead of as a DOM/SVG overlay —
 * and therefore export cleanly to PNG with the rest of the scene.
 *
 * WHY a helper (and why no JS-side atlas): the SDF glyph atlas is owned by the
 * C++ core (stb_truetype rasterization → shelf packing → chamfer distance-
 * transform SDF, uploaded as an R8 texture by DawnTextSdfBackend). It is NOT a
 * `setTexturePixels`/`textureId` texture, so JS cannot (and must not) build or
 * substitute its own atlas — doing so would be a shim that could never match the
 * compiled shader's baked SDF range + hardcoded smoothstep(0.45,0.55) edge. The
 * ONLY correct way to drive textSDF@1 is: `loadFont(ttfBytes)` once, then, per
 * label, lay the string out with `setTextGeometry` into a `glyph8` geometry.
 *
 * ORDER IS LOAD-BEARING (verified against the compiled wasm):
 *   createBuffer → createGeometry(format:"glyph8") → setTextGeometry(layout,
 *   which sets the geometry's vertexCount to the glyph count) → createDrawItem →
 *   bindDrawItem(textSDF@1) → setDrawItemColor.
 * Binding BEFORE the layout fails VALIDATION_BAD_VERTEX_COUNT (vertexCount must
 * be ≥ 1); binding a non-glyph8 geometry fails VALIDATION_VERTEX_FORMAT_MISMATCH.
 * This helper emits exactly that order so callers never trip either.
 *
 * PLACEMENT: text is positioned directly in clip space at (clipX, clipY) — the
 * SAME convention the retired DOM overlay used (px = (clipX+1)/2·W), so a label
 * that sat at a given pixel under the overlay lands at the same pixel on the GPU.
 * The clip↔pixel maps below are the inverse pair the overlay math implies; use
 * them to place a label at a known pixel/edge. Text quads are emitted pre-baked
 * in clip space, so a text DrawItem needs NO transform attached.
 *
 * PURE + FRAMEWORK-AGNOSTIC: `planTextDraw` builds the ordered step list with no
 * engine and no DOM (fully unit-testable); `drawText` executes those steps
 * against any {@link TextTarget} (the real EngineHost, or a capture mock).
 */

import { createIdAllocator, type IdAllocator } from "./ids";
import type { Rgba } from "./SceneBuilder";

export type { Rgba };

// ---- clip ↔ pixel mapping (inverse of the retired DOM overlay) -------------
// The overlay mapped clip→pixel as px = (clipX+1)/2·W and py = (1−clipY)/2·H
// (pixels are top-down: clipY=+1 is the top row, clipY=−1 the bottom). These are
// that map and its inverse, so a caller can place a label at a known pixel/edge.

/** clip-X [-1,1] → pixel-X [0,W]. px = (clipX+1)/2 · w. */
export function clipXToPx(clipX: number, w: number): number {
  return ((clipX + 1) * 0.5) * w;
}
/** clip-Y [-1,1] → pixel-Y [0,H], top-down. py = (1−clipY)/2 · h. */
export function clipYToPx(clipY: number, h: number): number {
  return ((1 - clipY) * 0.5) * h;
}
/** pixel-X [0,W] → clip-X [-1,1]. Inverse of {@link clipXToPx}. */
export function pxToClipX(px: number, w: number): number {
  return w === 0 ? 0 : (px / w) * 2 - 1;
}
/** pixel-Y [0,H] top-down → clip-Y [-1,1]. Inverse of {@link clipYToPx}. */
export function pxToClipY(py: number, h: number): number {
  return h === 0 ? 0 : 1 - (py / h) * 2;
}

// ---- Spec + plan types -----------------------------------------------------
/** A single GPU text label to render via textSDF@1. */
export type TextDrawSpec = {
  /** The string to render. Whitespace produces no glyph (advance only). */
  text: string;
  /** Clip-space X baseline origin of the text, in [-1, 1]. */
  clipX: number;
  /** Clip-space Y baseline origin of the text, in [-1, 1]. */
  clipY: number;
  /** Font size in pixels (the core lays glyphs out at this pixel size). */
  fontSize: number;
  /** Text color (u_color), components in [0,1]; alpha defaults to 1. */
  color: Rgba;
};

/**
 * One ordered step of a text draw. `control` steps go to `applyControl`; the
 * single `layout` step is the `setTextGeometry` call (NOT an applyControl
 * command — it writes glyph instances into the geometry's buffer and sets its
 * vertexCount). The step order is the load-bearing sequence documented above.
 */
export type TextStep =
  | { op: "control"; command: Record<string, unknown> }
  | {
      op: "layout";
      bufferId: number;
      geometryId: number;
      text: string;
      clipX: number;
      clipY: number;
      fontSize: number;
    };

/** The plan for one text label: the ids it owns + its ordered steps. */
export type TextPlan = {
  readonly drawItemId: number;
  readonly bufferId: number;
  readonly geometryId: number;
  /** Ordered steps: buffer → geometry(glyph8) → layout → drawItem → bind → color. */
  readonly steps: ReadonlyArray<TextStep>;
};

/** Handle returned after a label is drawn against a live target. */
export type TextHandle = {
  readonly drawItemId: number;
  readonly bufferId: number;
  readonly geometryId: number;
  /**
   * Glyphs the core laid out: > 0 on success, 0 for empty/all-whitespace text
   * (no DrawItem is bound in that case), or -1 if no font has been loaded (call
   * {@link TextTarget.loadFont} / EngineHost.loadFont first).
   */
  readonly glyphCount: number;
};

/**
 * Build the ordered step list to render `spec` on `layerId` using freshly
 * allocated buffer/geometry/drawItem ids from `ids`. Pure — allocates ids and
 * shapes commands but touches no engine. The emitted order is exactly the
 * sequence the compiled wasm requires (layout BEFORE bind; glyph8 geometry;
 * textSDF@1 pipeline).
 */
export function planTextDraw(
  ids: IdAllocator,
  layerId: number,
  spec: TextDrawSpec,
): TextPlan {
  const bufferId = ids.nextFor("buffer");
  const geometryId = ids.nextFor("geometry");
  const drawItemId = ids.nextFor("drawItem");
  const c = spec.color;

  const steps: TextStep[] = [
    // A textSDF geometry starts empty; setTextGeometry grows the buffer and sets
    // the real vertexCount. vertexCount:0 is legal at create time (bind is what
    // enforces ≥1) so the geometry exists for the layout step to target.
    { op: "control", command: { cmd: "createBuffer", id: bufferId, byteLength: 0 } },
    {
      op: "control",
      command: {
        cmd: "createGeometry",
        id: geometryId,
        vertexBufferId: bufferId,
        vertexCount: 0,
        format: "glyph8",
      },
    },
    // Lay the string out — sets the geometry's vertexCount to the glyph count.
    {
      op: "layout",
      bufferId,
      geometryId,
      text: spec.text,
      clipX: spec.clipX,
      clipY: spec.clipY,
      fontSize: spec.fontSize,
    },
    { op: "control", command: { cmd: "createDrawItem", id: drawItemId, layerId } },
    // Bind AFTER layout so vertexCount ≥ 1 (else VALIDATION_BAD_VERTEX_COUNT).
    {
      op: "control",
      command: {
        cmd: "bindDrawItem",
        drawItemId,
        pipeline: "textSDF@1",
        geometryId,
      },
    },
    {
      op: "control",
      command: {
        cmd: "setDrawItemColor",
        drawItemId,
        r: c.r,
        g: c.g,
        b: c.b,
        a: c.a ?? 1,
      },
    },
  ];

  return { drawItemId, bufferId, geometryId, steps };
}

// ---- Live execution --------------------------------------------------------
/**
 * The minimal engine surface `drawText` drives: `applyControl` for the scaffold
 * commands and `setTextGeometry` for the glyph layout. The real `EngineHost`
 * satisfies this (it now exposes `setTextGeometry`); tests pass a capture mock.
 */
export interface TextTarget {
  applyControl(command: object): unknown;
  setTextGeometry(
    bufferId: number,
    geometryId: number,
    text: string,
    clipX: number,
    clipY: number,
    fontSize: number,
  ): number;
}

/**
 * Render one GPU text label. Executes {@link planTextDraw}'s steps in order
 * against `target`: emits the scaffold, lays the string out, then (only if the
 * layout produced ≥ 1 glyph) creates + binds + colors the textSDF@1 DrawItem.
 *
 * A font MUST have been loaded first (EngineHost.loadFont); otherwise the layout
 * returns -1 and no DrawItem is bound. Empty/all-whitespace text lays out to 0
 * glyphs and likewise binds nothing (there is nothing to draw) — the ids are
 * still returned so the caller can dispose them if desired.
 *
 * Author text when the host is READY and NOT mid-render (same discipline as
 * SceneBuilder / applyControl): the WASM core is single-async-op, so touching it
 * while a render is in flight aborts the runtime.
 *
 * @param target the engine surface (an EngineHost, or a capture mock).
 * @param layerId the layer the text DrawItem belongs to.
 * @param spec the label to render.
 * @param ids optional id allocator (defaults to a fresh one).
 */
export function drawText(
  target: TextTarget,
  layerId: number,
  spec: TextDrawSpec,
  ids: IdAllocator = createIdAllocator(),
): TextHandle {
  const plan = planTextDraw(ids, layerId, spec);
  let glyphCount = 0;

  for (const step of plan.steps) {
    if (step.op === "layout") {
      glyphCount = target.setTextGeometry(
        step.bufferId,
        step.geometryId,
        step.text,
        step.clipX,
        step.clipY,
        step.fontSize,
      );
      // Nothing to bind if the font is missing (-1) or the string is empty (0):
      // stop before the create/bind/color steps so we never trip
      // VALIDATION_BAD_VERTEX_COUNT on a zero-glyph geometry.
      if (glyphCount <= 0) break;
      continue;
    }
    target.applyControl(step.command);
  }

  return {
    drawItemId: plan.drawItemId,
    bufferId: plan.bufferId,
    geometryId: plan.geometryId,
    glyphCount,
  };
}
