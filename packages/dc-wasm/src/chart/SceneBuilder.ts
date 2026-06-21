/* packages/dc-wasm/src/chart/SceneBuilder.ts — ENC-703 (G2)
 *
 * A thin, framework-agnostic, dependency-free CLIENT-SIDE scene/chart builder
 * over the engine's imperative `applyControl` surface. It encapsulates the
 * verbose, order-sensitive
 *
 *   createBuffer → createGeometry → createDrawItem → bindDrawItem →
 *   attachTransform → setDrawItemStyle/Color → applyDataBatch(record)
 *
 * sequence that the rich-chart research (specs/2026-06-20-rich-chart-research)
 * showed every visual element requires when hand-authored (see
 * harness/rich_chart.html `addItem`, and examples/engine_host_demo.html). Each
 * draw element is 6–8 ordered commands plus hand-managed, non-colliding ids;
 * this builder owns the id allocation (via `createIdAllocator`, ENC-700) and
 * emits the commands in the right order, returning a handle the caller can use
 * to stream updates.
 *
 * NO new engine features — this is PURE composition over the existing control
 * surface. It does not render, does not touch the DOM, and holds no engine
 * state beyond the ids it has handed out.
 *
 * ORIENTATION: author data in the mathematically correct convention (higher
 * value → higher clip-Y), matching scale.ts / embassy's RangeTracker. The
 * engine Y-inversion is fixed centrally in the blit (ENC-696) — do NOT
 * compensate here. When given data/clip ranges, transform() uses `fitTransform`
 * from scale.ts so the algebra lives in one tested place (ENC-699).
 */

import { createIdAllocator, type IdAllocator } from "./ids";
import { fitTransform, type Range, type Transform2D } from "./scale";

// ---- Binary record framing (the data plane wire format) --------------------
// Per-record: [1B op][4B bufferId LE][4B offset LE][4B len LE][payload].
// Matches engine_host_demo.html / rich_chart.html `record()` and the engine's
// documented ingestion format (op 1 = append, op 2 = updateRange).
const OP_APPEND = 1;
const RECORD_HEADER_BYTES = 13;

/**
 * Encode a binary append record for `bufferId` from a flat float array. Returns
 * a standalone `ArrayBuffer` ready for `applyDataBatch`. Exported so callers /
 * tests can build records without an engine.
 */
export function encodeAppendRecord(bufferId: number, floats: ArrayLike<number>): ArrayBuffer {
  const payload = new Float32Array(floats);
  const payloadBytes = payload.byteLength;
  const out = new ArrayBuffer(RECORD_HEADER_BYTES + payloadBytes);
  const dv = new DataView(out);
  dv.setUint8(0, OP_APPEND);
  dv.setUint32(1, bufferId, true);
  dv.setUint32(5, 0, true); // offset
  dv.setUint32(9, payloadBytes, true);
  new Uint8Array(out, RECORD_HEADER_BYTES).set(new Uint8Array(payload.buffer));
  return out;
}

// ---- The minimal engine surface the builder drives -------------------------
/**
 * The subset of `EngineHost` the builder needs. Anything with these two methods
 * works — the real `EngineHost`, or a mock that captures the calls (the tests
 * do exactly that). Keeps the builder decoupled from the full host.
 */
export interface SceneTarget {
  applyControl(command: object): unknown;
  applyDataBatch(batch: ArrayBuffer): void;
}

// ---- Styles ----------------------------------------------------------------
/** RGBA color, components in [0,1]. */
export type Rgba = { r: number; g: number; b: number; a?: number };

/** Style for `lineAA@1` lines (gridlines, polylines, SMA overlays). */
export type LineStyle = Rgba & {
  lineWidth?: number;
  dashLength?: number;
  gapLength?: number;
};

/** Style for `instancedRect@1` rectangles (volume bars, bands). */
export type RectStyle = Rgba;

/** Style for `instancedCandle@1` candlesticks: separate up/down colors. */
export type CandleStyle = {
  colorUp: [number, number, number, number];
  colorDown: [number, number, number, number];
};

// ---- Element shapes --------------------------------------------------------
/** One OHLC candle (candle6 layout: x,open,high,low,close,halfWidth). */
export type Candle = {
  x: number;
  open: number;
  high: number;
  low: number;
  close: number;
  halfWidth: number;
};

/** One axis-aligned rectangle (rect4 layout: x0,y0,x1,y1). */
export type Rect = { x0: number; y0: number; x1: number; y1: number };

// ---- Handles ---------------------------------------------------------------
/** Handle for a pane. */
export interface PaneHandle {
  readonly paneId: number;
}

/** Handle for a layer (belongs to a pane). */
export interface LayerHandle {
  readonly layerId: number;
  readonly paneId: number;
}

/** Handle for a transform (the data→clip affine attached to draw items). */
export interface TransformHandle {
  readonly transformId: number;
  /** Re-emit the transform affine (e.g. on pan/zoom). */
  set(params: Transform2D): void;
}

/**
 * Handle for a drawn element (line/candle/rect). Bundles the three ids the
 * element owns and an `update(data)` that re-pushes geometry bytes to its
 * buffer so the caller can stream updates without re-wiring the element.
 */
export interface DrawHandle<TData> {
  readonly drawItemId: number;
  readonly bufferId: number;
  readonly geometryId: number;
  /** Re-encode `data` for this element and push it to the engine. */
  update(data: TData): void;
}

// ---- Options ---------------------------------------------------------------
export type PaneOptions = {
  name?: string;
  /** Pane region in clip space; emits setPaneRegion when provided. */
  region?: { clipXMin: number; clipXMax: number; clipYMin: number; clipYMax: number };
  /** Pane clear color; emits setPaneClearColor when provided. */
  clearColor?: Rgba;
};

export type LayerOptions = { name?: string };

/**
 * Transform params: either a ready affine `{sx,tx,sy,ty}`, OR data/clip ranges
 * fitted via `fitTransform` (scale.ts). Ranges are the ergonomic path — give the
 * data extent and (optionally) the clip target rect and the builder computes the
 * affine for you.
 */
export type TransformParams =
  | Transform2D
  | {
      dataRange: { x: Range; y: Range };
      clipRange?: { x: Range; y: Range };
    };

function isAffine(p: TransformParams): p is Transform2D {
  return (
    typeof (p as Transform2D).sx === "number" &&
    typeof (p as Transform2D).sy === "number" &&
    typeof (p as Transform2D).tx === "number" &&
    typeof (p as Transform2D).ty === "number"
  );
}

// ---- Geometry encoders (flat float arrays, engine layouts) -----------------
function linePointsToFloats(points: ReadonlyArray<readonly [number, number]>): number[] {
  // lineAA@1 consumes rect4 segments [x0,y0,x1,y1]; a polyline of N points is
  // N-1 connected segments (matches harness maSegs / gridlines).
  const out: number[] = [];
  for (let i = 0; i < points.length - 1; i++) {
    const a = points[i];
    const b = points[i + 1];
    out.push(a[0], a[1], b[0], b[1]);
  }
  return out;
}

function candlesToFloats(candles: ReadonlyArray<Candle>): number[] {
  const out: number[] = [];
  for (const c of candles) out.push(c.x, c.open, c.high, c.low, c.close, c.halfWidth);
  return out;
}

function rectsToFloats(rects: ReadonlyArray<Rect>): number[] {
  const out: number[] = [];
  for (const r of rects) out.push(r.x0, r.y0, r.x1, r.y1);
  return out;
}

/**
 * The client-side scene/chart builder. Construct with an `EngineHost` (or any
 * {@link SceneTarget}); it owns a fresh {@link IdAllocator} so callers never
 * hand-manage ids. Each `pane`/`layer`/`transform`/`addX` method emits the
 * ordered control commands (and, for draws, the geometry bytes) and returns a
 * handle.
 */
export class SceneBuilder {
  private readonly target: SceneTarget;
  private readonly ids: IdAllocator;

  /**
   * @param target the engine surface (an `EngineHost`, or a mock with
   *   `applyControl` + `applyDataBatch`).
   * @param ids    optional id allocator (defaults to a fresh `createIdAllocator()`).
   *   Inject one to coexist with externally-allocated ids.
   */
  constructor(target: SceneTarget, ids: IdAllocator = createIdAllocator()) {
    this.target = target;
    this.ids = ids;
  }

  /** The id that will be handed out next (does not consume it). */
  peekId(): number {
    return this.ids.peek();
  }

  // -------------------- scene scaffolding --------------------
  /** Create a pane, optionally setting its region + clear color. */
  pane(opts: PaneOptions = {}): PaneHandle {
    const paneId = this.ids.nextFor("pane");
    this.ctrl({ cmd: "createPane", id: paneId, name: opts.name ?? `pane${paneId}` });
    if (opts.region) {
      this.ctrl({
        cmd: "setPaneRegion",
        id: paneId,
        clipXMin: opts.region.clipXMin,
        clipXMax: opts.region.clipXMax,
        clipYMin: opts.region.clipYMin,
        clipYMax: opts.region.clipYMax,
      });
    }
    if (opts.clearColor) {
      const c = opts.clearColor;
      this.ctrl({
        cmd: "setPaneClearColor",
        id: paneId,
        r: c.r,
        g: c.g,
        b: c.b,
        a: c.a ?? 1,
      });
    }
    return { paneId };
  }

  /** Create a layer inside `paneId`. Accepts a `PaneHandle` or a raw pane id. */
  layer(pane: PaneHandle | number, opts: LayerOptions = {}): LayerHandle {
    const paneId = typeof pane === "number" ? pane : pane.paneId;
    const layerId = this.ids.nextFor("layer");
    this.ctrl({
      cmd: "createLayer",
      id: layerId,
      paneId,
      name: opts.name ?? `layer${layerId}`,
    });
    return { layerId, paneId };
  }

  /**
   * Create a transform and set its affine. Pass a ready `{sx,tx,sy,ty}` OR
   * `{dataRange, clipRange?}` to fit data extent → clip via `fitTransform`.
   */
  transform(params: TransformParams): TransformHandle {
    const transformId = this.ids.nextFor("transform");
    this.ctrl({ cmd: "createTransform", id: transformId });
    const affine = isAffine(params) ? params : fitTransform(params.dataRange, params.clipRange);
    this.emitTransform(transformId, affine);
    return {
      transformId,
      set: (next: Transform2D): void => this.emitTransform(transformId, next),
    };
  }

  private emitTransform(transformId: number, a: Transform2D): void {
    this.ctrl({ cmd: "setTransform", id: transformId, sx: a.sx, sy: a.sy, tx: a.tx, ty: a.ty });
  }

  // -------------------- draw elements --------------------
  /**
   * Add an anti-aliased polyline (`lineAA@1`, rect4 connected segments) to
   * `layer`. `points` is an ordered list of `[x,y]` in data space (rendered
   * under `transform` if given). Gridlines: pass already-segmented points or use
   * the scale.ts grid helpers and `addLineSegments`.
   */
  addLine(
    layer: LayerHandle | number,
    points: ReadonlyArray<readonly [number, number]>,
    style: LineStyle,
    transform?: TransformHandle | number,
  ): DrawHandle<ReadonlyArray<readonly [number, number]>> {
    const handle = this.element(
      layer,
      "lineAA@1",
      "rect4",
      linePointsToFloats(points),
      transform,
      this.lineStyleCmd(style),
    );
    return this.wrap(handle, (pts: ReadonlyArray<readonly [number, number]>) =>
      linePointsToFloats(pts),
    );
  }

  /**
   * Add `lineAA@1` lines from raw rect4 segments `[x0,y0,x1,y1, …]` (data
   * space). Use for gridlines built by scale.ts `gridSegments` /
   * `horizontalGridSegments` / `verticalGridSegments`, which already emit this
   * flat layout.
   */
  addLineSegments(
    layer: LayerHandle | number,
    segments: ReadonlyArray<number>,
    style: LineStyle,
    transform?: TransformHandle | number,
  ): DrawHandle<ReadonlyArray<number>> {
    const handle = this.element(
      layer,
      "lineAA@1",
      "rect4",
      segments,
      transform,
      this.lineStyleCmd(style),
    );
    return this.wrap(handle, (segs: ReadonlyArray<number>) => segs.slice());
  }

  /**
   * Add candlesticks (`instancedCandle@1`, candle6 layout). `style` carries the
   * separate up/down colors.
   */
  addCandles(
    layer: LayerHandle | number,
    candles: ReadonlyArray<Candle>,
    style: CandleStyle,
    transform?: TransformHandle | number,
  ): DrawHandle<ReadonlyArray<Candle>> {
    const handle = this.element(
      layer,
      "instancedCandle@1",
      "candle6",
      candlesToFloats(candles),
      transform,
      {
        cmd: "setDrawItemStyle",
        drawItemId: 0, // patched in element()
        colorUp: style.colorUp,
        colorDown: style.colorDown,
      },
    );
    return this.wrap(handle, (cs: ReadonlyArray<Candle>) => candlesToFloats(cs));
  }

  /** Add rectangles (`instancedRect@1`, rect4 layout) — volume bars, bands. */
  addRects(
    layer: LayerHandle | number,
    rects: ReadonlyArray<Rect>,
    style: RectStyle,
    transform?: TransformHandle | number,
  ): DrawHandle<ReadonlyArray<Rect>> {
    const handle = this.element(
      layer,
      "instancedRect@1",
      "rect4",
      rectsToFloats(rects),
      transform,
      { cmd: "setDrawItemColor", drawItemId: 0, r: style.r, g: style.g, b: style.b, a: style.a ?? 1 },
    );
    return this.wrap(handle, (rs: ReadonlyArray<Rect>) => rectsToFloats(rs));
  }

  // -------------------- internals --------------------
  private lineStyleCmd(style: LineStyle): object {
    const cmd: Record<string, unknown> = {
      cmd: "setDrawItemStyle",
      drawItemId: 0, // patched in element()
      r: style.r,
      g: style.g,
      b: style.b,
      a: style.a ?? 1,
    };
    if (style.lineWidth != null) cmd.lineWidth = style.lineWidth;
    if (style.dashLength != null) cmd.dashLength = style.dashLength;
    if (style.gapLength != null) cmd.gapLength = style.gapLength;
    return cmd;
  }

  /**
   * The core composition: allocate buffer/geometry/drawItem ids, emit the
   * ordered create→bind→attach→style commands, then push the geometry bytes.
   * `vertexCount` is element count (segments / candles / rects), matching the
   * harness `count` for each pipeline's format.
   */
  private element(
    layer: LayerHandle | number,
    pipeline: string,
    format: string,
    floats: ReadonlyArray<number>,
    transform: TransformHandle | number | undefined,
    styleCmd: object,
  ): { drawItemId: number; bufferId: number; geometryId: number } {
    const layerId = typeof layer === "number" ? layer : layer.layerId;
    const bufferId = this.ids.nextFor("buffer");
    const geometryId = this.ids.nextFor("geometry");
    const drawItemId = this.ids.nextFor("drawItem");

    const vertexCount = vertexCountFor(format, floats.length);
    const data = new Float32Array(floats);

    this.ctrl({ cmd: "createBuffer", id: bufferId, byteLength: data.byteLength });
    this.ctrl({
      cmd: "createGeometry",
      id: geometryId,
      vertexBufferId: bufferId,
      format,
      vertexCount,
    });
    this.ctrl({ cmd: "createDrawItem", id: drawItemId, layerId });
    this.ctrl({ cmd: "bindDrawItem", drawItemId, pipeline, geometryId });
    if (transform !== undefined) {
      const transformId = typeof transform === "number" ? transform : transform.transformId;
      this.ctrl({ cmd: "attachTransform", drawItemId, transformId });
    }
    // Patch the placeholder drawItemId into the caller's style command and emit.
    this.ctrl({ ...styleCmd, drawItemId });

    this.target.applyDataBatch(encodeAppendRecord(bufferId, floats));

    return { drawItemId, bufferId, geometryId };
  }

  /** Wrap an element's ids with an `update(data)` that re-pushes geometry. */
  private wrap<TData>(
    h: { drawItemId: number; bufferId: number; geometryId: number },
    encode: (data: TData) => ArrayLike<number>,
  ): DrawHandle<TData> {
    return {
      drawItemId: h.drawItemId,
      bufferId: h.bufferId,
      geometryId: h.geometryId,
      update: (data: TData): void => {
        this.target.applyDataBatch(encodeAppendRecord(h.bufferId, encode(data)));
      },
    };
  }

  private ctrl(command: object): void {
    this.target.applyControl(command);
  }
}

/**
 * Element count for a geometry `format` given its total float length. The
 * engine's `vertexCount`/instance count is the number of elements, not floats:
 * rect4 = 4 floats/elem, candle6 = 6 floats/elem. Unknown formats default to a
 * 2-float (pos2) element.
 */
function vertexCountFor(format: string, floatLength: number): number {
  const floatsPer = format === "candle6" ? 6 : format === "rect4" ? 4 : 2;
  return Math.floor(floatLength / floatsPer);
}
