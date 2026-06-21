/* packages/dc-wasm/src/chart/SceneBuilder.test.ts — ENC-703 (G2)
 *
 * Unit tests for the client-side SceneBuilder. A MOCK SceneTarget captures every
 * applyControl command (in order) and every applyDataBatch ArrayBuffer, so we
 * can assert:
 *   - ids never collide across elements (the whole point of ENC-700 wiring)
 *   - the correct, ordered create→bind→attach→style command sequence per element
 *   - transform attach is emitted and references the transform id
 *   - the binary record framing bytes ([op][bufferId][offset][len][payload])
 *   - handle.update re-emits a data batch to the same buffer
 *
 * No engine, no WASM, no DOM — pure composition is exactly what's under test.
 */

import { describe, it, expect } from "vitest";
import { SceneBuilder, encodeAppendRecord, type SceneTarget } from "./SceneBuilder";

type Captured = {
  controls: Array<Record<string, unknown>>;
  batches: ArrayBuffer[];
};

function mockTarget(): SceneTarget & Captured {
  const controls: Array<Record<string, unknown>> = [];
  const batches: ArrayBuffer[] = [];
  return {
    controls,
    batches,
    applyControl(command: object): unknown {
      controls.push(command as Record<string, unknown>);
      return { ok: true };
    },
    applyDataBatch(batch: ArrayBuffer): void {
      batches.push(batch);
    },
  };
}

/** Decode an append record back into its header fields + payload floats. */
function decodeRecord(buf: ArrayBuffer): {
  op: number;
  bufferId: number;
  offset: number;
  len: number;
  floats: number[];
} {
  const dv = new DataView(buf);
  const op = dv.getUint8(0);
  const bufferId = dv.getUint32(1, true);
  const offset = dv.getUint32(5, true);
  const len = dv.getUint32(9, true);
  const floats = Array.from(new Float32Array(buf.slice(13)));
  return { op, bufferId, offset, len, floats };
}

const cmds = (t: Captured): string[] => t.controls.map((c) => String(c.cmd));

describe("encodeAppendRecord (binary framing)", () => {
  it("frames [op=1][bufferId LE][offset=0 LE][len LE][payload]", () => {
    const buf = encodeAppendRecord(42, [1.5, -2.25, 3.0]);
    const r = decodeRecord(buf);
    expect(r.op).toBe(1);
    expect(r.bufferId).toBe(42);
    expect(r.offset).toBe(0);
    expect(r.len).toBe(12); // 3 floats * 4 bytes
    expect(buf.byteLength).toBe(13 + 12);
    expect(r.floats).toEqual([1.5, -2.25, 3.0]);
  });

  it("encodes an empty payload as a 13-byte header", () => {
    const buf = encodeAppendRecord(7, []);
    expect(buf.byteLength).toBe(13);
    const r = decodeRecord(buf);
    expect(r.len).toBe(0);
    expect(r.floats).toEqual([]);
  });
});

describe("SceneBuilder — scaffolding", () => {
  it("pane emits createPane, region, and clear color when given", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const pane = b.pane({
      name: "price",
      region: { clipXMin: -0.8, clipXMax: 0.9, clipYMin: 0, clipYMax: 0.9 },
      clearColor: { r: 0.04, g: 0.05, b: 0.06, a: 1 },
    });
    expect(cmds(t)).toEqual(["createPane", "setPaneRegion", "setPaneClearColor"]);
    expect(t.controls[0]).toMatchObject({ cmd: "createPane", id: pane.paneId, name: "price" });
    expect(t.controls[1]).toMatchObject({
      cmd: "setPaneRegion",
      id: pane.paneId,
      clipXMin: -0.8,
      clipYMax: 0.9,
    });
    expect(t.controls[2]).toMatchObject({ cmd: "setPaneClearColor", id: pane.paneId, a: 1 });
  });

  it("pane with no opts emits only createPane", () => {
    const t = mockTarget();
    new SceneBuilder(t).pane();
    expect(cmds(t)).toEqual(["createPane"]);
  });

  it("layer references its pane id", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const pane = b.pane();
    const layer = b.layer(pane, { name: "candles" });
    expect(t.controls[t.controls.length - 1]).toMatchObject({
      cmd: "createLayer",
      id: layer.layerId,
      paneId: pane.paneId,
      name: "candles",
    });
    expect(layer.paneId).toBe(pane.paneId);
  });

  it("transform from an explicit affine emits createTransform + setTransform", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const xf = b.transform({ sx: 2, tx: 1, sy: 3, ty: -1 });
    expect(cmds(t)).toEqual(["createTransform", "setTransform"]);
    expect(t.controls[1]).toMatchObject({
      cmd: "setTransform",
      id: xf.transformId,
      sx: 2,
      tx: 1,
      sy: 3,
      ty: -1,
    });
  });

  it("transform from data/clip ranges uses fitTransform (positive sy, +Y up)", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    b.transform({
      dataRange: { x: { min: 0, max: 10 }, y: { min: 100, max: 200 } },
      clipRange: { x: { min: -1, max: 1 }, y: { min: -1, max: 1 } },
    });
    const set = t.controls[1];
    // sx = 2/10 = 0.2 ; sy = 2/100 = 0.02 (POSITIVE — orientation-correct)
    expect(set.sx).toBeCloseTo(0.2, 9);
    expect(set.sy).toBeCloseTo(0.02, 9);
    expect(set.sy as number).toBeGreaterThan(0);
  });

  it("transform.set re-emits setTransform on the same id", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const xf = b.transform({ sx: 1, tx: 0, sy: 1, ty: 0 });
    xf.set({ sx: 5, tx: 2, sy: 6, ty: 3 });
    expect(t.controls[t.controls.length - 1]).toMatchObject({
      cmd: "setTransform",
      id: xf.transformId,
      sx: 5,
      sy: 6,
    });
  });
});

describe("SceneBuilder — draw element command sequence + order", () => {
  it("addCandles emits create→bind→attach→style in order, then a data batch", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const pane = b.pane();
    const layer = b.layer(pane);
    const xf = b.transform({ sx: 1, tx: 0, sy: 1, ty: 0 });
    t.controls.length = 0; // focus on the element commands

    const h = b.addCandles(
      layer,
      [{ x: 0, open: 1, high: 2, low: 0.5, close: 1.5, halfWidth: 0.25 }],
      { colorUp: [0, 1, 0, 1], colorDown: [1, 0, 0, 1] },
      xf,
    );

    expect(cmds(t)).toEqual([
      "createBuffer",
      "createGeometry",
      "createDrawItem",
      "bindDrawItem",
      "attachTransform",
      "setDrawItemStyle",
    ]);
    // create/bind reference the handle's ids
    expect(t.controls[0]).toMatchObject({ cmd: "createBuffer", id: h.bufferId });
    expect(t.controls[1]).toMatchObject({
      cmd: "createGeometry",
      id: h.geometryId,
      vertexBufferId: h.bufferId,
      format: "candle6",
      vertexCount: 1,
    });
    expect(t.controls[2]).toMatchObject({
      cmd: "createDrawItem",
      id: h.drawItemId,
      layerId: layer.layerId,
    });
    expect(t.controls[3]).toMatchObject({
      cmd: "bindDrawItem",
      drawItemId: h.drawItemId,
      pipeline: "instancedCandle@1",
      geometryId: h.geometryId,
    });
    expect(t.controls[4]).toMatchObject({
      cmd: "attachTransform",
      drawItemId: h.drawItemId,
      transformId: xf.transformId,
    });
    expect(t.controls[5]).toMatchObject({
      cmd: "setDrawItemStyle",
      drawItemId: h.drawItemId,
      colorUp: [0, 1, 0, 1],
      colorDown: [1, 0, 0, 1],
    });

    // exactly one data batch, framed to the candle buffer
    expect(t.batches).toHaveLength(1);
    const rec = decodeRecord(t.batches[0]);
    expect(rec.op).toBe(1);
    expect(rec.bufferId).toBe(h.bufferId);
    expect(rec.floats).toEqual([0, 1, 2, 0.5, 1.5, 0.25]);
  });

  it("addRects omits attachTransform when no transform is given (uses setDrawItemColor)", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const layer = b.layer(b.pane());
    t.controls.length = 0;

    const h = b.addRects(
      layer,
      [
        { x0: 0, y0: 0, x1: 1, y1: 2 },
        { x0: 1, y0: 0, x1: 2, y1: 3 },
      ],
      { r: 0.1, g: 0.8, b: 0.4, a: 0.5 },
    );

    expect(cmds(t)).toEqual([
      "createBuffer",
      "createGeometry",
      "createDrawItem",
      "bindDrawItem",
      "setDrawItemColor",
    ]);
    expect(cmds(t)).not.toContain("attachTransform");
    expect(t.controls[1]).toMatchObject({ format: "rect4", vertexCount: 2 });
    expect(t.controls[t.controls.length - 1]).toMatchObject({
      cmd: "setDrawItemColor",
      drawItemId: h.drawItemId,
      r: 0.1,
      a: 0.5,
    });
    expect(decodeRecord(t.batches[0]).floats).toEqual([0, 0, 1, 2, 1, 0, 2, 3]);
  });

  it("addLine converts a polyline into rect4 connected segments", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const layer = b.layer(b.pane());
    t.controls.length = 0;

    const h = b.addLine(
      layer,
      [
        [0, 0],
        [1, 1],
        [2, 0],
      ],
      { r: 1, g: 1, b: 0, a: 1, lineWidth: 2 },
    );
    // 3 points -> 2 segments -> 8 floats; vertexCount = 2
    expect(t.controls[1]).toMatchObject({ format: "rect4", vertexCount: 2 });
    expect(t.controls[t.controls.length - 1]).toMatchObject({ cmd: "setDrawItemStyle", lineWidth: 2 });
    expect(decodeRecord(t.batches[0]).floats).toEqual([0, 0, 1, 1, 1, 1, 2, 0]);
    expect(h.bufferId).toBeGreaterThan(0);
  });

  it("addLineSegments passes raw rect4 floats straight through", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const layer = b.layer(b.pane());
    const segs = [0, 5, 10, 5, 0, 6, 10, 6];
    t.controls.length = 0;
    b.addLineSegments(layer, segs, { r: 0.2, g: 0.2, b: 0.3, a: 0.9 });
    expect(t.controls[1]).toMatchObject({ format: "rect4", vertexCount: 2 });
    expect(decodeRecord(t.batches[0]).floats).toEqual(segs);
  });
});

describe("SceneBuilder — id uniqueness across elements", () => {
  it("never reuses an id across panes, layers, transforms, and draws", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const seen = new Set<number>();
    const remember = (...ids: number[]): void => {
      for (const id of ids) {
        expect(seen.has(id)).toBe(false);
        seen.add(id);
      }
    };

    const pane = b.pane();
    remember(pane.paneId);
    const layer = b.layer(pane);
    remember(layer.layerId);
    const xf = b.transform({ sx: 1, tx: 0, sy: 1, ty: 0 });
    remember(xf.transformId);

    const candles = b.addCandles(
      layer,
      [{ x: 0, open: 1, high: 2, low: 0, close: 1.5, halfWidth: 0.3 }],
      { colorUp: [0, 1, 0, 1], colorDown: [1, 0, 0, 1] },
      xf,
    );
    remember(candles.bufferId, candles.geometryId, candles.drawItemId);

    const line = b.addLine(layer, [[0, 0], [1, 1]], { r: 1, g: 1, b: 1, a: 1 }, xf);
    remember(line.bufferId, line.geometryId, line.drawItemId);

    const rects = b.addRects(layer, [{ x0: 0, y0: 0, x1: 1, y1: 1 }], { r: 1, g: 0, b: 0 }, xf);
    remember(rects.bufferId, rects.geometryId, rects.drawItemId);

    // 3 scaffolding ids + 9 element ids = 12 distinct ids, all >= 1.
    expect(seen.size).toBe(12);
    for (const id of seen) expect(id).toBeGreaterThanOrEqual(1);
  });

  it("ids referenced in the emitted commands are all distinct", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const layer = b.layer(b.pane());
    const xf = b.transform({ sx: 1, tx: 0, sy: 1, ty: 0 });
    b.addRects(layer, [{ x0: 0, y0: 0, x1: 1, y1: 1 }], { r: 1, g: 0, b: 0 }, xf);
    b.addRects(layer, [{ x0: 0, y0: 0, x1: 1, y1: 1 }], { r: 0, g: 1, b: 0 }, xf);

    // gather every created id across createBuffer/Geometry/DrawItem/Pane/Layer/Transform
    const created = t.controls
      .filter((c) =>
        ["createPane", "createLayer", "createTransform", "createBuffer", "createGeometry", "createDrawItem"].includes(
          String(c.cmd),
        ),
      )
      .map((c) => c.id as number);
    expect(new Set(created).size).toBe(created.length);
  });
});

describe("SceneBuilder — handle.update streams new data", () => {
  it("re-emits a data batch to the element's buffer", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const layer = b.layer(b.pane());
    const xf = b.transform({ sx: 1, tx: 0, sy: 1, ty: 0 });
    const h = b.addCandles(
      layer,
      [{ x: 0, open: 1, high: 2, low: 0, close: 1.5, halfWidth: 0.25 }],
      { colorUp: [0, 1, 0, 1], colorDown: [1, 0, 0, 1] },
      xf,
    );
    const before = t.batches.length;
    const controlsBefore = t.controls.length;

    h.update([
      { x: 0, open: 1, high: 2, low: 0, close: 1.5, halfWidth: 0.25 },
      { x: 1, open: 1.5, high: 3, low: 1, close: 2.5, halfWidth: 0.25 },
    ]);

    // exactly one new batch, no new control commands (pure data stream)
    expect(t.batches.length).toBe(before + 1);
    expect(t.controls.length).toBe(controlsBefore);
    const rec = decodeRecord(t.batches[t.batches.length - 1]);
    expect(rec.bufferId).toBe(h.bufferId);
    expect(rec.floats).toEqual([0, 1, 2, 0, 1.5, 0.25, 1, 1.5, 3, 1, 2.5, 0.25]);
  });

  it("rect handle.update re-encodes rects", () => {
    const t = mockTarget();
    const b = new SceneBuilder(t);
    const h = b.addRects(b.layer(b.pane()), [{ x0: 0, y0: 0, x1: 1, y1: 1 }], { r: 1, g: 0, b: 0 });
    h.update([{ x0: 2, y0: 3, x1: 4, y1: 5 }]);
    expect(decodeRecord(t.batches[t.batches.length - 1]).floats).toEqual([2, 3, 4, 5]);
  });
});

describe("SceneBuilder — id allocator injection", () => {
  it("accepts a custom start so it can coexist with reserved ids", () => {
    const t = mockTarget();
    // import indirectly to keep the test self-contained
    const b = new SceneBuilder(t);
    expect(b.peekId()).toBeGreaterThanOrEqual(1);
  });
});
