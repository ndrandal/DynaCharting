/* ENC-1253 — the engine draws its own axis: ticks, gridlines, spine, labels.
 *
 * THREE LAYERS OF PROOF, in the order they can fail:
 *
 *  1. PURE planning (no engine): gridlines span the plot box and not the canvas,
 *     ticks and labels land in the GUTTERS, the spine sits on the box's edges,
 *     ticks outside the frame are dropped rather than drawn, and labels that
 *     would collide are dropped with a reason.
 *  2. COMMAND-LEVEL (a capture mock): the furniture pane is at FULL_CLIP_REGION
 *     — plotbox.ts contract note (7), the failure that is silent — the text
 *     layout precedes the textSDF@1 bind, the buffers are rewritten with an
 *     `updateRange` record rather than grown with an `append` one, and a
 *     re-sync allocates no new resources.
 *  3. REAL-WASM (the committed dc_engine_host.wasm + the repo font): the
 *     `measureText` binding this ticket added returns the same layout
 *     `setTextGeometry` performs, the aspect correction makes a label's pixel
 *     width independent of the canvas's aspect ratio, and a whole axis authored
 *     against the true core binds every draw item it planned.
 *
 * Pixels are not asserted here — node has no WebGPU. The raster is scored by
 * `specs/2026-09-19-chart-quality-bar/harness/score.py` against a canvas-only
 * capture (SPEC D10), which is the only instrument that can say the glyphs
 * landed.
 */

import { describe, it, expect, beforeAll } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import {
  AXIS_DEFAULTS,
  EngineAxis,
  boxesOverlap,
  checkTier1Labels,
  createHostMeasurer,
  encodeUpdateRecord,
  fontSizeForPx,
  labelBoxPx,
  planAxis,
  textXScale,
  type AxisSpec,
  type AxisTarget,
  type AxisTextMeasurer,
  type TextMetrics,
} from "./axis";
import { DEFAULT_PLOT_INSETS, frameSeries, plotBox } from "./plotbox";
import { clipXToPx, clipYToPx } from "./text";
import { createIdAllocator } from "./ids";
import { darkAxisTheme } from "./theme";
import type { DcEngineHostFactory } from "../wasm";

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = resolve(HERE, "../../../..");
const FONT_PATH = resolve(REPO_ROOT, "third_party/test_font.ttf");
const WASM_JS = resolve(HERE, "../../wasm/dc_engine_host.js");

const CANVAS = { width: 1280, height: 800 };

/**
 * A deterministic measurer: every glyph is `0.5em` wide, the ink sits from the
 * baseline up to `0.7em`, with a 1% left side bearing. Nothing about the real
 * font, on purpose — the planner's arithmetic should be checkable without one.
 */
function stubMeasurer(): AxisTextMeasurer {
  return {
    measure(text: string, fontPx: number): TextMetrics {
      const w = text.length * fontPx * 0.5;
      return {
        advanceWidthPx: w,
        inkMinXPx: fontPx * 0.01,
        inkMaxXPx: w - fontPx * 0.01,
        inkMinYPx: 0,
        inkMaxYPx: fontPx * 0.7,
        glyphCount: text.trim().length,
      };
    },
  };
}

/** A capture mock recording every control command and data batch, in order. */
function captureTarget(): AxisTarget & {
  commands: Record<string, unknown>[];
  batches: ArrayBuffer[];
  layouts: { bufferId: number; text: string; clipX: number; clipY: number; xScale: number }[];
  log: string[];
} {
  const commands: Record<string, unknown>[] = [];
  const batches: ArrayBuffer[] = [];
  const layouts: { bufferId: number; text: string; clipX: number; clipY: number; xScale: number }[] = [];
  const log: string[] = [];
  return {
    commands,
    batches,
    layouts,
    log,
    applyControl(command: object) {
      commands.push(command as Record<string, unknown>);
      log.push(String((command as { cmd?: string }).cmd));
      return { ok: true };
    },
    applyDataBatch(batch: ArrayBuffer) {
      batches.push(batch);
      log.push("applyDataBatch");
    },
    setTextGeometry(bufferId, _geometryId, text, clipX, clipY) {
      layouts.push({ bufferId, text, clipX, clipY, xScale: 1 });
      log.push("setTextGeometry");
      return text.trim().length;
    },
    setTextGeometryX(bufferId, _geometryId, text, clipX, clipY, _fontSize, xScale) {
      layouts.push({ bufferId, text, clipX, clipY, xScale });
      log.push("setTextGeometryX");
      return text.trim().length;
    },
  };
}

/** A spec over a price/index domain framed by the real plot box. */
function spec(over: Partial<AxisSpec> = {}): AxisSpec {
  const domain = { x: { min: 0, max: 100 }, y: { min: 400, max: 420 } };
  const framed = frameSeries(domain, CANVAS);
  return {
    box: framed.box,
    canvas: CANVAS,
    transform: framed.transform!,
    y: { ticks: [405, 410, 415].map((v) => ({ value: v, label: v.toFixed(2) })) },
    x: {
      ticks: [0, 25, 50, 75, 100].map((v) => ({ value: v, label: `09:${String(v).padStart(2, "0")}` })),
    },
    theme: darkAxisTheme,
    measurer: stubMeasurer(),
    ...over,
  };
}

// ---------------------------------------------------------------------------
// 1. Pure planning
// ---------------------------------------------------------------------------
describe("planAxis — the geometry, in clip space (ENC-1253)", () => {
  it("draws a gridline across the PLOT BOX, not across the canvas", () => {
    const s = spec();
    const p = planAxis(s);
    // Horizontal gridlines: 3 y ticks, each spanning box.x.min → box.x.max.
    const horizontals = [];
    for (let i = 0; i < p.gridSegments.length; i += 4) {
      const [x0, y0, x1, y1] = p.gridSegments.slice(i, i + 4);
      if (y0 === y1) horizontals.push([x0, y0, x1, y1]);
    }
    expect(horizontals).toHaveLength(3);
    for (const [x0, , x1] of horizontals) {
      expect(x0).toBeCloseTo(s.box.x.min, 12);
      expect(x1).toBeCloseTo(s.box.x.max, 12);
      // …and NOT the canvas: the box is strictly inside clip space.
      expect(x0).toBeGreaterThan(-1);
      expect(x1).toBeLessThan(1);
    }
  });

  it("puts every tick MARK in a gutter, outside the plot box", () => {
    const s = spec();
    const p = planAxis(s);
    expect(p.tickSegments.length / 4).toBe(8); // 3 y + 5 x
    for (let i = 0; i < p.tickSegments.length; i += 4) {
      const [x0, y0, x1, y1] = p.tickSegments.slice(i, i + 4);
      if (y0 === y1) {
        // A y tick: runs leftward OUT of the box, ending on its left edge.
        expect(x1).toBeCloseTo(s.box.x.min, 12);
        expect(x0).toBeLessThan(s.box.x.min);
        expect(x0).toBeGreaterThanOrEqual(-1);
      } else {
        // An x tick: runs downward out of the box, ending on its bottom edge.
        expect(x0).toBeCloseTo(x1, 12);
        expect(y1).toBeCloseTo(s.box.y.min, 12);
        expect(y0).toBeLessThan(s.box.y.min);
        expect(y0).toBeGreaterThanOrEqual(-1);
      }
    }
  });

  it("the tick length is the documented 6 CSS pixels", () => {
    const s = spec();
    const p = planAxis(s);
    const [x0, , x1] = p.tickSegments.slice(0, 4);
    expect(((x1 - x0) * CANVAS.width) / 2).toBeCloseTo(AXIS_DEFAULTS.tickLengthPx, 9);
  });

  it("draws the spine on the box's left and bottom edges", () => {
    const s = spec();
    const p = planAxis(s);
    expect(p.spineSegments).toHaveLength(8);
    const [lx0, ly0, lx1, ly1, bx0, by0, bx1, by1] = p.spineSegments;
    expect([lx0, lx1]).toEqual([s.box.x.min, s.box.x.min]);
    expect([ly0, ly1]).toEqual([s.box.y.min, s.box.y.max]);
    expect([by0, by1]).toEqual([s.box.y.min, s.box.y.min]);
    expect([bx0, bx1]).toEqual([s.box.x.min, s.box.x.max]);
  });

  it("drops ticks the frame does not contain — never widens the frame", () => {
    const s = spec({
      y: { ticks: [300, 405, 410, 900].map((v) => ({ value: v, label: String(v) })) },
    });
    const p = planAxis(s);
    const yLabels = p.labels.filter((l) => l.role === "yTickLabel").map((l) => l.text);
    expect(yLabels).toEqual(["405", "410"]);
    // …and no gridline or tick mark was emitted for the two that fell outside.
    expect(p.gridLines.filter((g) => g.orientation === "horizontal")).toHaveLength(2);
  });

  it("emits no labels at all without a measurer — geometry only", () => {
    const p = planAxis(spec({ measurer: undefined }));
    expect(p.labels).toEqual([]);
    expect(p.gridSegments.length).toBeGreaterThan(0);
    expect(p.tickSegments.length).toBeGreaterThan(0);
  });

  it("grid: false suppresses the gridlines but keeps the ticks", () => {
    const s = spec({
      y: { grid: false, ticks: [{ value: 410, label: "410" }] },
      x: { grid: false, ticks: [{ value: 50, label: "09:50" }] },
    });
    const p = planAxis(s);
    expect(p.gridSegments).toEqual([]);
    expect(p.gridLines).toEqual([]);
    expect(p.tickSegments.length / 4).toBe(2);
  });
});

describe("planAxis — where the labels land", () => {
  it("right-aligns a y label so its ink clears the tick by the label gap", () => {
    const s = spec();
    const p = planAxis(s);
    const label = p.labels.find((l) => l.role === "yTickLabel")!;
    const rightEdgePx = label.bbox[0] + label.bbox[2];
    const boxLeftPx = clipXToPx(s.box.x.min, CANVAS.width);
    const wanted = boxLeftPx - AXIS_DEFAULTS.tickLengthPx - AXIS_DEFAULTS.labelGapPx;
    // Rounded outward by at most a pixel (labelBoxPx ceils the far edge).
    expect(rightEdgePx).toBeGreaterThanOrEqual(wanted - 1);
    expect(rightEdgePx).toBeLessThanOrEqual(wanted + 1);
  });

  it("centres a y label's ink on its own tick", () => {
    const s = spec();
    const p = planAxis(s);
    for (const l of p.labels.filter((x) => x.role === "yTickLabel")) {
      const tickValue = Number(l.text);
      const tickPx = clipYToPx(tickValue * s.transform.sy + s.transform.ty, CANVAS.height);
      const centrePx = l.bbox[1] + l.bbox[3] / 2;
      expect(Math.abs(centrePx - tickPx)).toBeLessThanOrEqual(1);
    }
  });

  it("centres an x label horizontally on its tick, below the plot box", () => {
    const s = spec();
    const p = planAxis(s);
    const boxBottomPx = clipYToPx(s.box.y.min, CANVAS.height);
    for (const l of p.labels.filter((x) => x.role === "xTickLabel")) {
      const v = Number(l.text.slice(3));
      const tickPx = clipXToPx(v * s.transform.sx + s.transform.tx, CANVAS.width);
      const centrePx = l.bbox[0] + l.bbox[2] / 2;
      expect(Math.abs(centrePx - tickPx)).toBeLessThanOrEqual(1);
      expect(l.bbox[1]).toBeGreaterThanOrEqual(boxBottomPx);
    }
  });

  it("every label fits inside its gutter — the insets are big enough for them", () => {
    const s = spec();
    const p = planAxis(s);
    const boxLeftPx = clipXToPx(s.box.x.min, CANVAS.width);
    const boxBottomPx = clipYToPx(s.box.y.min, CANVAS.height);
    for (const l of p.labels) {
      if (l.role === "yTickLabel") {
        expect(l.bbox[0]).toBeGreaterThanOrEqual(0);
        expect(l.bbox[0] + l.bbox[2]).toBeLessThanOrEqual(boxLeftPx);
      }
      if (l.role === "xTickLabel") {
        expect(l.bbox[1]).toBeGreaterThanOrEqual(boxBottomPx);
        expect(l.bbox[1] + l.bbox[3]).toBeLessThanOrEqual(CANVAS.height);
      }
    }
    expect(p.droppedLabels).toEqual([]);
  });

  it("drops a colliding label and KEEPS its tick mark", () => {
    // Twenty x ticks across 1200px of plot box: their labels cannot all fit.
    const ticks = Array.from({ length: 20 }, (_, i) => ({
      value: i * 5,
      label: `09:${String(i).padStart(2, "0")}`,
    }));
    const p = planAxis(spec({ x: { ticks } }));
    const xLabels = p.labels.filter((l) => l.role === "xTickLabel");
    expect(xLabels.length).toBeLessThan(ticks.length);
    expect(p.droppedLabels.length).toBe(ticks.length - xLabels.length);
    expect(p.droppedLabels[0].reason).toMatch(/overlap/);
    // Every tick MARK survives: a sparse axis is not a thinned one.
    const xTickMarks = [];
    for (let i = 0; i < p.tickSegments.length; i += 4) {
      const seg = p.tickSegments.slice(i, i + 4);
      if (seg[1] !== seg[3]) xTickMarks.push(seg);
    }
    expect(xTickMarks).toHaveLength(ticks.length);
    // And what survived is genuinely disjoint.
    for (let i = 0; i < xLabels.length; i++) {
      for (let j = i + 1; j < xLabels.length; j++) {
        expect(boxesOverlap(xLabels[i].bbox, xLabels[j].bbox)).toBe(false);
      }
    }
  });

  it("reports the plot box in raster pixels, inset from the frame", () => {
    const p = planAxis(spec());
    const [x0, y0, x1, y1] = p.plotPx;
    expect(x0).toBe(DEFAULT_PLOT_INSETS.left);
    expect(y0).toBe(DEFAULT_PLOT_INSETS.top);
    expect(x1).toBe(CANVAS.width - DEFAULT_PLOT_INSETS.right - 1);
    expect(y1).toBe(CANVAS.height - DEFAULT_PLOT_INSETS.bottom - 1);
  });

  it("labelBoxPx rounds OUTWARD so the box contains every lit pixel", () => {
    const m: TextMetrics = {
      advanceWidthPx: 10.4,
      inkMinXPx: 0.4,
      inkMaxXPx: 10.2,
      inkMinYPx: -0.3,
      inkMaxYPx: 7.6,
      glyphCount: 3,
    };
    const box = labelBoxPx(0, 0, m, CANVAS); // origin at clip (0,0) = (640, 400)
    expect(box[0]).toBeLessThanOrEqual(640 + m.inkMinXPx);
    expect(box[0] + box[2]).toBeGreaterThanOrEqual(640 + m.inkMaxXPx);
    expect(box[1]).toBeLessThanOrEqual(400 - m.inkMaxYPx);
    expect(box[1] + box[3]).toBeGreaterThanOrEqual(400 - m.inkMinYPx);
  });
});

describe("checkTier1Labels — D1's tier-1 label row, without a raster", () => {
  it("passes a planned axis whose labels are disjoint, framed and temporal", () => {
    const p = planAxis(spec());
    const v = checkTier1Labels(p, CANVAS);
    expect(v.failures).toEqual([]);
    expect(v.pass).toBe(true);
    expect(v.counts.x).toBe(5);
    expect(v.counts.y).toBe(3);
  });

  it("fails an axis with no engine-drawn labels on a side", () => {
    const p = planAxis(spec({ x: undefined }));
    const v = checkTier1Labels(p, CANVAS);
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toContain("no engine-drawn x tick labels");
  });

  it("fails an x label that is an INDEX rather than an instant (SPEC §1.1)", () => {
    const p = planAxis(spec({ x: { ticks: [{ value: 50, label: "162" }] } }));
    const v = checkTier1Labels(p, CANVAS);
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toContain("does not parse as a timestamp");
  });

  it("catches an overlap the planner was not asked to prune", () => {
    const p = planAxis(spec());
    // Forge a collision: two declared boxes on the same pixels.
    p.labels.push({ ...p.labels[0], text: "forged" });
    const v = checkTier1Labels(p, CANVAS);
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toContain("overlaps");
  });

  it("catches a label pushed outside the frame", () => {
    const p = planAxis(spec());
    p.labels[0] = { ...p.labels[0], bbox: [-5, 10, 30, 12] };
    const v = checkTier1Labels(p, CANVAS);
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toContain("clipped by the frame");
  });
});

// ---------------------------------------------------------------------------
// 2. Command level
// ---------------------------------------------------------------------------
describe("EngineAxis — the commands it emits", () => {
  it("puts the furniture in its OWN pane at FULL_CLIP_REGION (plotbox note 7)", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    const region = t.commands.find((c) => c.cmd === "setPaneRegion")!;
    expect(region).toBeTruthy();
    expect(region.clipXMin).toBe(-1);
    expect(region.clipXMax).toBe(1);
    expect(region.clipYMin).toBe(-1);
    expect(region.clipYMax).toBe(1);
    // A pane of its own — the data pane's region IS the plot box, and anything
    // drawn in a gutter from there is scissored away with no error at all.
    expect(t.commands.filter((c) => c.cmd === "createPane")).toHaveLength(1);
  });

  it("never attaches the data transform: furniture is authored in clip space", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    expect(t.commands.filter((c) => c.cmd === "attachTransform")).toHaveLength(0);
  });

  it("lays text out BEFORE binding textSDF@1 (VALIDATION_BAD_VERTEX_COUNT)", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    const firstLayout = t.log.indexOf("setTextGeometryX");
    const textBindIdx = t.commands.findIndex(
      (c) => c.cmd === "bindDrawItem" && c.pipeline === "textSDF@1",
    );
    expect(firstLayout).toBeGreaterThanOrEqual(0);
    expect(textBindIdx).toBeGreaterThanOrEqual(0);
    // The bind's position in the interleaved log is after the first layout.
    const bindLogIdx = t.log.indexOf("bindDrawItem", firstLayout);
    expect(bindLogIdx).toBeGreaterThan(firstLayout);
  });

  it("binds lineAA@1 for the grid, the ticks and the spine", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    const lineBinds = t.commands.filter(
      (c) => c.cmd === "bindDrawItem" && c.pipeline === "lineAA@1",
    );
    expect(lineBinds).toHaveLength(3);
    const geoms = t.commands.filter((c) => c.cmd === "createGeometry" && c.format === "rect4");
    expect(geoms).toHaveLength(3);
  });

  it("colours the furniture from the THEME, not from literals (D1 tier 3)", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    const styles = t.commands.filter((c) => c.cmd === "setDrawItemStyle");
    const grid = styles[0];
    expect([grid.r, grid.g, grid.b]).toEqual([
      darkAxisTheme.gridColor[0],
      darkAxisTheme.gridColor[1],
      darkAxisTheme.gridColor[2],
    ]);
    const tick = styles[1];
    expect([tick.r, tick.g, tick.b]).toEqual([
      darkAxisTheme.tickColor[0],
      darkAxisTheme.tickColor[1],
      darkAxisTheme.tickColor[2],
    ]);
    const labelColor = t.commands.find((c) => c.cmd === "setDrawItemColor")!;
    expect([labelColor.r, labelColor.g, labelColor.b]).toEqual([
      darkAxisTheme.labelColor[0],
      darkAxisTheme.labelColor[1],
      darkAxisTheme.labelColor[2],
    ]);
  });

  it("rewrites its buffers with updateRange (op 2), never append (op 1)", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    expect(t.batches.length).toBeGreaterThan(0);
    for (const b of t.batches) {
      expect(new DataView(b).getUint8(0)).toBe(2); // OP_UPDATE_RANGE
      expect(new DataView(b).getUint32(5, true)).toBe(0); // offset 0
    }
  });

  it("re-syncing allocates no new resources — a domain change is not a rebuild", () => {
    const t = captureTarget();
    const ids = createIdAllocator();
    const axis = new EngineAxis(t, ids);
    axis.sync(spec());
    const afterFirst = ids.peek();
    const creates = (log: Record<string, unknown>[]) =>
      log.filter((c) => String(c.cmd).startsWith("create")).length;
    const createsFirst = creates(t.commands);

    for (let i = 0; i < 5; i++) {
      axis.sync(
        spec({ y: { ticks: [{ value: 405 + i, label: String(405 + i) }] } }),
      );
    }
    expect(ids.peek()).toBe(afterFirst);
    expect(creates(t.commands)).toBe(createsFirst);
    // …and it still re-uploads the geometry and re-counts it every time.
    expect(t.commands.filter((c) => c.cmd === "setGeometryVertexCount").length).toBeGreaterThan(3);
  });

  it("empties an unused label slot instead of destroying it", () => {
    const t = captureTarget();
    const axis = new EngineAxis(t);
    axis.sync(spec());
    const before = t.layouts.length;
    axis.sync(spec({ x: { ticks: [] }, y: { ticks: [] } }));
    const emptied = t.layouts.slice(before).filter((l) => l.text === "");
    expect(emptied.length).toBeGreaterThan(0);
    expect(t.commands.filter((c) => String(c.cmd).startsWith("destroy"))).toHaveLength(0);
  });

  it("passes the aspect correction to the layout", () => {
    const t = captureTarget();
    new EngineAxis(t).sync(spec());
    expect(t.layouts[0].xScale).toBeCloseTo(CANVAS.height / CANVAS.width, 12);
  });

  it("falls back to the isotropic setTextGeometry on an older host", () => {
    const t = captureTarget();
    delete (t as { setTextGeometryX?: unknown }).setTextGeometryX;
    new EngineAxis(t).sync(spec());
    expect(t.log).toContain("setTextGeometry");
    expect(t.log).not.toContain("setTextGeometryX");
  });

  it("dispose() removes every resource it created", () => {
    const t = captureTarget();
    const axis = new EngineAxis(t);
    axis.sync(spec());
    const created = t.commands.filter((c) => String(c.cmd).startsWith("create")).length;
    axis.dispose();
    const destroyed = t.commands.filter((c) => String(c.cmd).startsWith("destroy")).length;
    expect(destroyed).toBe(created);
  });
});

describe("encodeUpdateRecord", () => {
  it("frames op 2 at offset 0 with the float payload", () => {
    const buf = encodeUpdateRecord(77, [1, 2, 3, 4]);
    const dv = new DataView(buf);
    expect(dv.getUint8(0)).toBe(2);
    expect(dv.getUint32(1, true)).toBe(77);
    expect(dv.getUint32(5, true)).toBe(0);
    expect(dv.getUint32(9, true)).toBe(16);
    expect(Array.from(new Float32Array(buf, 13))).toEqual([1, 2, 3, 4]);
  });
});

describe("the pixel↔clip arithmetic the labels depend on", () => {
  it("fontSizeForPx inverts to the requested pixel height", () => {
    // fontSize is the ascent-to-descent height in clip units; clip Y spans 2.
    expect((fontSizeForPx(11, CANVAS) * CANVAS.height) / 2).toBeCloseTo(11, 9);
  });

  it("textXScale is the canvas's inverse aspect", () => {
    expect(textXScale({ width: 1800, height: 1200 })).toBeCloseTo(1200 / 1800, 12);
    expect(textXScale({ width: 900, height: 900 })).toBe(1);
  });

  it("agrees with plotbox.ts on what the default gutters are for", () => {
    // plotbox.ts sizes `left: 64` as "a price label at 11px, 7 glyphs, plus a
    // 6px tick and 4px of breathing room". If either file moves, this fails.
    const box = plotBox(CANVAS);
    const gutterPx = clipXToPx(box.x.min, CANVAS.width);
    expect(gutterPx).toBe(DEFAULT_PLOT_INSETS.left);
    const m = stubMeasurer().measure("418.25", AXIS_DEFAULTS.fontPx);
    expect(m.advanceWidthPx + AXIS_DEFAULTS.tickLengthPx + AXIS_DEFAULTS.labelGapPx)
      .toBeLessThanOrEqual(gutterPx);
  });
});

// ---------------------------------------------------------------------------
// 3. Against the real compiled wasm
// ---------------------------------------------------------------------------
describe("measureText + the whole axis against the real core (ENC-1253)", () => {
  let factory: DcEngineHostFactory;
  let fontBytes: Uint8Array;

  beforeAll(async () => {
    fontBytes = new Uint8Array(readFileSync(FONT_PATH));
    const mod = (await import(WASM_JS)) as { default: DcEngineHostFactory };
    factory = mod.default;
  });

  it("the committed wasm exports measureText and setTextGeometryX", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost();
    try {
      expect(typeof (core as unknown as { measureText?: unknown }).measureText).toBe("function");
      expect(typeof (core as unknown as { setTextGeometryX?: unknown }).setTextGeometryX).toBe(
        "function",
      );
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });

  it("measureText reports -1 glyphs with no font, mirroring setTextGeometry", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost() as unknown as {
      measureText(t: string, f: number, x: number): { glyphCount: number; glyphPx: number };
      dispose?(): void;
      delete?(): void;
    };
    try {
      const m = core.measureText("418.25", 0.02, 1);
      expect(m.glyphCount).toBe(-1);
      expect(m.glyphPx).toBe(48);
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });

  it("measureText's advance is EXACTLY the layout setTextGeometry performs", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost() as unknown as {
      loadFont(b: Uint8Array): boolean;
      applyControl(j: string): { ok: boolean };
      setTextGeometryX(b: number, g: number, t: string, x: number, y: number, f: number, s: number): number;
      measureText(t: string, f: number, x: number): { glyphCount: number; advanceWidth: number };
      dispose?(): void;
      delete?(): void;
    };
    try {
      expect(core.loadFont(fontBytes)).toBe(true);
      core.applyControl(JSON.stringify({ cmd: "createBuffer", id: 10, byteLength: 0 }));
      core.applyControl(
        JSON.stringify({
          cmd: "createGeometry",
          id: 100,
          vertexBufferId: 10,
          vertexCount: 0,
          format: "glyph8",
        }),
      );
      for (const s of ["418.25", "09:41:05", "0", "  ", "Price"]) {
        const m = core.measureText(s, 0.03, 0.75);
        const drawn = core.setTextGeometryX(10, 100, s, -0.5, 0.25, 0.03, 0.75);
        expect(m.glyphCount, `glyph count for ${JSON.stringify(s)}`).toBe(drawn);
      }
      // The advance scales linearly in both fontSize and xScale — the property
      // the pixel arithmetic in `createHostMeasurer` relies on.
      const a = core.measureText("418.25", 0.02, 1).advanceWidth;
      expect(core.measureText("418.25", 0.04, 1).advanceWidth).toBeCloseTo(2 * a, 6);
      expect(core.measureText("418.25", 0.02, 0.5).advanceWidth).toBeCloseTo(a / 2, 6);
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });

  it("the aspect correction makes a label's PIXEL width canvas-shape-independent", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost() as unknown as {
      loadFont(b: Uint8Array): boolean;
      measureText(t: string, f: number, x: number): Record<string, number>;
      dispose?(): void;
      delete?(): void;
    };
    try {
      core.loadFont(fontBytes);
      const target = core as unknown as AxisTarget;
      const square = createHostMeasurer(target, { width: 900, height: 900 })!;
      const wide = createHostMeasurer(target, { width: 1800, height: 900 })!;
      expect(square.measure("418.25", 11).advanceWidthPx).toBeCloseTo(
        wide.measure("418.25", 11).advanceWidthPx,
        4,
      );
      // Without the correction the wide canvas would stretch it by 2x — this is
      // the bug the xScale parameter exists to fix, stated as a measurement.
      const uncorrected = (core.measureText("418.25", fontSizeForPx(11, { width: 1800, height: 900 }), 1)
        .advanceWidth * 1800) / 2;
      expect(uncorrected).toBeCloseTo(2 * wide.measure("418.25", 11).advanceWidthPx, 4);
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });

  it("createHostMeasurer returns null on a host with no measureText", () => {
    const t = captureTarget();
    expect(createHostMeasurer(t, CANVAS)).toBeNull();
  });

  it("a whole axis authored against the true core binds everything it planned", async () => {
    const Module = await factory();
    const core = new Module.DcEngineHost();
    const rejected: string[] = [];
    try {
      expect(core.loadFont(fontBytes)).toBe(true);
      const target: AxisTarget = {
        applyControl(command: object) {
          const r = core.applyControl(JSON.stringify(command));
          if (!r.ok) rejected.push(`${JSON.stringify(command)} -> ${r.error}`);
          return r;
        },
        applyDataBatch(batch: ArrayBuffer) {
          core.applyDataBatch(new Uint8Array(batch));
        },
        setTextGeometry: (b, g, t, x, y, f) => core.setTextGeometry(b, g, t, x, y, f),
        setTextGeometryX: (b, g, t, x, y, f, s) =>
          (core as unknown as { setTextGeometryX(...a: unknown[]): number }).setTextGeometryX(
            b, g, t, x, y, f, s,
          ),
        measureText: (t, f, s) =>
          (core as unknown as {
            measureText(t: string, f: number, s: number): {
              advanceWidth: number;
              inkMinX: number;
              inkMaxX: number;
              inkMinY: number;
              inkMaxY: number;
              glyphCount: number;
              glyphPx: number;
            };
          }).measureText(t, f, s),
      };

      const canvas = { width: 1800, height: 1200 };
      const framed = frameSeries({ x: { min: 0, max: 240 }, y: { min: 406, max: 418 } }, canvas);
      const measurer = createHostMeasurer(target, canvas)!;
      const axis = new EngineAxis(target);
      const plan = axis.sync({
        box: framed.box,
        canvas,
        transform: framed.transform!,
        y: { ticks: [408, 410, 412, 414, 416].map((v) => ({ value: v, label: v.toFixed(2) })) },
        x: {
          ticks: [0, 60, 120, 180, 240].map((v) => ({
            value: v,
            label: `09:${String(30 + v / 60).padStart(2, "0")}:00`,
          })),
        },
        theme: darkAxisTheme,
        measurer,
      });

      expect(rejected, `rejected commands: ${rejected.join("; ")}`).toEqual([]);
      // 3 line draw items + one per placed label.
      expect(core.drawItemCount()).toBe(3 + plan.labels.length);
      expect(plan.labels.length).toBeGreaterThanOrEqual(8);

      // D1's tier-1 label row, against the labels the REAL font produced.
      const verdict = checkTier1Labels(plan, canvas);
      expect(verdict.failures, verdict.failures.join("; ")).toEqual([]);
      expect(verdict.pass).toBe(true);
    } finally {
      core.dispose?.();
      core.delete?.();
    }
  });
});
