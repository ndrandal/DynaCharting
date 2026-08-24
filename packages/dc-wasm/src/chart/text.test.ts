// ENC-715: tests for the GPU text (textSDF@1) authoring helper.
//
// TWO LAYERS OF PROOF:
//  1. PURE unit tests (no engine): the clip↔pixel inverse maps, and that
//     planTextDraw / drawText emit the load-bearing command order — a glyph8
//     geometry, the layout (setTextGeometry) BEFORE the textSDF@1 bind, and no
//     bind at all for empty / no-font text.
//  2. REAL-WASM integration (the actual compiled dc_engine_host.wasm + the
//     repo's test font): loadFont + drawText through the true core, asserting the
//     glyph counts and that the textSDF@1 DrawItem binds (which only succeeds if
//     the layout set the geometry's vertexCount). GPU render()/pick() need WebGPU
//     (absent in node), so pixels are proven by the existing C++ Dawn tests
//     (d3_3_dawn_text_sdf, enc589_host_text) + the browser harness; here we prove
//     the JS→core text-authoring contract end-to-end against the real artifact.

import { describe, it, expect, beforeAll } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import {
  drawText,
  planTextDraw,
  clipXToPx,
  clipYToPx,
  pxToClipX,
  pxToClipY,
  type TextStep,
  type TextTarget,
} from "./text";
import { createIdAllocator } from "./ids";
import { EngineHost } from "../EngineHost";
import type { DcEngineHostFactory } from "../wasm";

const HERE = dirname(fileURLToPath(import.meta.url));
// src/chart → repo root is four levels up.
const REPO_ROOT = resolve(HERE, "../../../..");
const FONT_PATH = resolve(REPO_ROOT, "third_party/test_font.ttf");
const WASM_JS = resolve(HERE, "../../wasm/dc_engine_host.js");

// ---------------------------------------------------------------------------
// 1. clip ↔ pixel mapping (inverse of the retired DOM overlay px=(clipX+1)/2·W)
// ---------------------------------------------------------------------------
describe("clip ↔ pixel mapping (text.ts)", () => {
  it("clipXToPx matches the overlay convention at the edges and center", () => {
    expect(clipXToPx(-1, 800)).toBe(0); // left edge
    expect(clipXToPx(0, 800)).toBe(400); // center
    expect(clipXToPx(1, 800)).toBe(800); // right edge
  });

  it("clipYToPx is top-down: clipY=+1 is the top row (py=0)", () => {
    expect(clipYToPx(1, 600)).toBe(0); // top
    expect(clipYToPx(0, 600)).toBe(300); // center
    expect(clipYToPx(-1, 600)).toBe(600); // bottom
  });

  it("pxToClipX / pxToClipY invert clipXToPx / clipYToPx", () => {
    const w = 1280;
    const h = 720;
    for (const clip of [-1, -0.37, 0, 0.5, 1]) {
      expect(pxToClipX(clipXToPx(clip, w), w)).toBeCloseTo(clip, 10);
      expect(pxToClipY(clipYToPx(clip, h), h)).toBeCloseTo(clip, 10);
    }
  });

  it("guards a zero-sized viewport (no NaN/Infinity)", () => {
    expect(pxToClipX(123, 0)).toBe(0);
    expect(pxToClipY(123, 0)).toBe(0);
  });
});

// ---------------------------------------------------------------------------
// 2a. planTextDraw — the ordered command plan (pure)
// ---------------------------------------------------------------------------
describe("planTextDraw ordering + layout (ENC-715)", () => {
  const spec = {
    text: "Axis",
    clipX: -0.9,
    clipY: 0.8,
    fontSize: 24,
    color: { r: 1, g: 1, b: 1 },
  };

  function cmdsOf(steps: ReadonlyArray<TextStep>): string[] {
    return steps.map((s) =>
      s.op === "layout" ? "layout" : String((s.command as { cmd: string }).cmd),
    );
  }

  it("emits buffer → geometry(glyph8) → layout → drawItem → bind(textSDF@1) → color", () => {
    const plan = planTextDraw(createIdAllocator(), 2, spec);
    expect(cmdsOf(plan.steps)).toEqual([
      "createBuffer",
      "createGeometry",
      "layout",
      "createDrawItem",
      "bindDrawItem",
      "setDrawItemColor",
    ]);
  });

  it("puts the layout step BEFORE the bind (vertexCount must be set first)", () => {
    const plan = planTextDraw(createIdAllocator(), 2, spec);
    const names = cmdsOf(plan.steps);
    expect(names.indexOf("layout")).toBeLessThan(names.indexOf("bindDrawItem"));
  });

  it("creates a glyph8 geometry bound to the textSDF@1 pipeline", () => {
    const plan = planTextDraw(createIdAllocator(), 2, spec);
    const geo = plan.steps.find(
      (s): s is Extract<TextStep, { op: "control" }> =>
        s.op === "control" && s.command.cmd === "createGeometry",
    )!;
    expect(geo.command.format).toBe("glyph8");
    expect(geo.command.vertexBufferId).toBe(plan.bufferId);

    const bind = plan.steps.find(
      (s): s is Extract<TextStep, { op: "control" }> =>
        s.op === "control" && s.command.cmd === "bindDrawItem",
    )!;
    expect(bind.command.pipeline).toBe("textSDF@1");
    expect(bind.command.geometryId).toBe(plan.geometryId);
    expect(bind.command.drawItemId).toBe(plan.drawItemId);
  });

  it("allocates distinct buffer/geometry/drawItem ids and defaults alpha to 1", () => {
    const plan = planTextDraw(createIdAllocator(), 2, spec);
    const set = new Set([plan.bufferId, plan.geometryId, plan.drawItemId]);
    expect(set.size).toBe(3);
    const color = plan.steps.find(
      (s): s is Extract<TextStep, { op: "control" }> =>
        s.op === "control" && s.command.cmd === "setDrawItemColor",
    )!;
    expect(color.command.a).toBe(1);
  });
});

// ---------------------------------------------------------------------------
// 2b. drawText — execution against a capture mock
// ---------------------------------------------------------------------------
/** A TextTarget that records applyControl commands + the setTextGeometry call,
 *  returning a configurable glyph count for the layout. */
function captureTarget(glyphCount: number) {
  const controls: Array<Record<string, unknown>> = [];
  const layouts: Array<{
    bufferId: number;
    geometryId: number;
    text: string;
    clipX: number;
    clipY: number;
    fontSize: number;
  }> = [];
  const target: TextTarget = {
    applyControl(command: object) {
      controls.push(command as Record<string, unknown>);
      return { ok: true };
    },
    setTextGeometry(bufferId, geometryId, text, clipX, clipY, fontSize) {
      layouts.push({ bufferId, geometryId, text, clipX, clipY, fontSize });
      return glyphCount;
    },
  };
  return { target, controls, layouts };
}

describe("drawText execution (ENC-715)", () => {
  const spec = {
    text: "Hi",
    clipX: 0.1,
    clipY: 0.2,
    fontSize: 32,
    color: { r: 0.2, g: 0.4, b: 0.6, a: 0.8 },
  };

  it("lays out then binds when glyphs are produced; forwards the layout params", () => {
    const { target, controls, layouts } = captureTarget(2);
    const h = drawText(target, 7, spec);

    expect(h.glyphCount).toBe(2);
    expect(layouts).toHaveLength(1);
    expect(layouts[0]).toMatchObject({
      bufferId: h.bufferId,
      geometryId: h.geometryId,
      text: "Hi",
      clipX: 0.1,
      clipY: 0.2,
      fontSize: 32,
    });
    // Full scaffold emitted, on the given layer, with the right color+alpha.
    expect(controls.map((c) => c.cmd)).toEqual([
      "createBuffer",
      "createGeometry",
      "createDrawItem",
      "bindDrawItem",
      "setDrawItemColor",
    ]);
    expect(controls.find((c) => c.cmd === "createDrawItem")!.layerId).toBe(7);
    expect(controls.find((c) => c.cmd === "setDrawItemColor")).toMatchObject({
      r: 0.2,
      g: 0.4,
      b: 0.6,
      a: 0.8,
    });
  });

  it("does NOT bind a DrawItem for empty/whitespace text (0 glyphs)", () => {
    const { target, controls } = captureTarget(0);
    const h = drawText(target, 7, { ...spec, text: "   " });
    expect(h.glyphCount).toBe(0);
    // Only the pre-layout scaffold ran; no createDrawItem/bind/color.
    expect(controls.map((c) => c.cmd)).toEqual(["createBuffer", "createGeometry"]);
  });

  it("does NOT bind when no font is loaded (layout returns -1)", () => {
    const { target, controls } = captureTarget(-1);
    const h = drawText(target, 7, spec);
    expect(h.glyphCount).toBe(-1);
    expect(controls.map((c) => c.cmd)).toEqual(["createBuffer", "createGeometry"]);
  });
});

// ---------------------------------------------------------------------------
// 3. REAL-WASM integration: the actual compiled core + the repo test font.
// ---------------------------------------------------------------------------
describe("textSDF@1 against the real compiled wasm (ENC-715)", () => {
  let factory: DcEngineHostFactory;
  let fontBytes: Uint8Array;

  beforeAll(async () => {
    fontBytes = new Uint8Array(readFileSync(FONT_PATH));
    const mod = (await import(WASM_JS)) as { default: DcEngineHostFactory };
    factory = mod.default;
  });

  it("loadFont + setTextGeometry drive the real SDF glyph atlas", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost();
    try {
      expect(core.loadFont(fontBytes)).toBe(true);

      // Minimal glyph8 scaffold, then lay out — same sequence planTextDraw emits.
      const ok = (o: object) => core.applyControl(JSON.stringify(o)).ok;
      expect(ok({ cmd: "createPane", id: 1 })).toBe(true);
      expect(ok({ cmd: "createLayer", id: 2, paneId: 1 })).toBe(true);
      expect(ok({ cmd: "createBuffer", id: 10, byteLength: 0 })).toBe(true);
      expect(
        ok({ cmd: "createGeometry", id: 100, vertexBufferId: 10, vertexCount: 0, format: "glyph8" }),
      ).toBe(true);
      expect(ok({ cmd: "createDrawItem", id: 3, layerId: 2 })).toBe(true);

      // Binding BEFORE the layout must fail (vertexCount still 0).
      const early = core.applyControl(
        JSON.stringify({ cmd: "bindDrawItem", drawItemId: 3, pipeline: "textSDF@1", geometryId: 100 }),
      );
      expect(early.ok).toBe(false);
      expect(early.error).toContain("VERTEX_COUNT");

      // Lay "Axis" out: 4 non-whitespace glyphs; sets the geometry vertexCount.
      const n = core.setTextGeometry(10, 100, "Axis", -0.9, 0.8, 24);
      expect(n).toBe(4);

      // Now the textSDF@1 bind succeeds — proof the layout set vertexCount ≥ 1.
      const late = core.applyControl(
        JSON.stringify({ cmd: "bindDrawItem", drawItemId: 3, pipeline: "textSDF@1", geometryId: 100 }),
      );
      expect(late.ok).toBe(true);
      expect(core.drawItemCount()).toBe(1);
      expect(core.geometryCount()).toBe(1);
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });

  it("counts only non-whitespace glyphs and returns -1 with no font", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost();
    try {
      core.applyControl(JSON.stringify({ cmd: "createBuffer", id: 10, byteLength: 0 }));
      core.applyControl(
        JSON.stringify({ cmd: "createGeometry", id: 100, vertexBufferId: 10, vertexCount: 0, format: "glyph8" }),
      );
      // No font yet → -1.
      expect(core.setTextGeometry(10, 100, "Hi", 0, 0, 24)).toBe(-1);

      expect(core.loadFont(fontBytes)).toBe(true);
      expect(core.setTextGeometry(10, 100, "AB", 0, 0, 24)).toBe(2);
      expect(core.setTextGeometry(10, 100, "A B", 0, 0, 24)).toBe(2); // space omitted
      expect(core.setTextGeometry(10, 100, "  ", 0, 0, 24)).toBe(0);
      expect(core.setTextGeometry(10, 100, "hello", 0, 0, 24)).toBe(5);
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });

  it("drawText helper drives the real core through EngineHost (loadFont + passthrough)", async () => {
    const host = new EngineHost({ factory });
    // A DOM-less fake canvas: init only calls getContext('2d') (null is fine —
    // we never render here) and reads width/height.
    const fakeCanvas = {
      getContext: () => null,
      width: 800,
      height: 600,
    } as unknown as HTMLCanvasElement;
    host.init(fakeCanvas);
    await host.whenReady();

    // loadFont through the wrapper (applied immediately — host is ready).
    expect(host.loadFont(fontBytes)).toBe(true);

    // Author a pane+layer, then draw a label through the helper against the host.
    host.applyControl({ cmd: "createPane", id: 1 });
    host.applyControl({ cmd: "createLayer", id: 2, paneId: 1 });
    const ids = createIdAllocator(50); // above the hand-picked 1/2 pane/layer ids
    const handle = drawText(host, 2, {
      text: "Price",
      clipX: pxToClipX(12, 800),
      clipY: pxToClipY(20, 600),
      fontSize: 20,
      color: { r: 0.9, g: 0.9, b: 0.9 },
    }, ids);

    expect(handle.glyphCount).toBe(5); // "Price"
    // The core now holds a bound textSDF@1 DrawItem for the label.
    const host2 = host as unknown as { core: { drawItemCount(): number; listResources(): string } };
    expect(host2.core.drawItemCount()).toBe(1);
    expect(host2.core.listResources()).toContain(String(handle.drawItemId));

    host.shutdown();
  });
});
