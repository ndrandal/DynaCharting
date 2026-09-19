/* packages/dc-wasm/src/chart/domain.ts — ENC-1252 (chart-quality-bar SPEC D7)
 *
 * THE VISIBLE DOMAIN, DERIVED FROM THE STREAMED DATA.
 *
 * "The axis is a measurement or it is not an axis" (D7). Before this module the
 * only thing a client could say about its own domain was whatever a human had
 * typed into a view file — `"y": { "min": 408, "max": 418 }` — which is a
 * caption, not a measurement: if the geometry drifted, the number would not
 * move (SPEC §1.3).
 *
 * This is the client-side counterpart of embassy's `RangeTracker`
 * (`embassy/internal/pipeline/range_tracker.go`), and deliberately mirrors its
 * shape so the two cannot disagree about what "the domain" means:
 *
 *   - AXIS GROUPS. embassy registers a set of bufferIDs per axis group and
 *     silently ignores a value for a buffer in no group. So does this: a
 *     `DomainTracker` folds ONLY the buffers it was constructed with. A volume
 *     sub-pane's buffer must not widen the price axis, and the only way to know
 *     that is for the caller to declare it.
 *   - RUNNING (min, max), O(Δ). Each batch folds only the bytes in that batch —
 *     never a rescan of the buffer. Same complexity class as embassy's Observe
 *     and as the C++ `RunningDomain` (`dc/scale/Scale.hpp`, ENC-596).
 *   - COLLAPSE FLOOR. A degenerate range (a stalled feed; one record) is widened
 *     to ±max(mid·frac, abs) around its midpoint, so a downstream `fitAxis` can
 *     never produce s = Infinity. Same defaults as embassy's
 *     MinHalfWidthFrac 0.005 / MinHalfWidthAbs 0.01.
 *
 * Two things it deliberately does NOT do:
 *
 *   - It does not pad by default. embassy's RangeTracker adds 10% headroom
 *     because it is computing a *transform*; this module reports a *domain*,
 *     and an axis that claims a 10%-wider range than the data has is the same
 *     species of lie D7 exists to kill. `paddingFrac` is available for the
 *     framing caller (fitting the series to the viewport is ENC-1256).
 *   - It does not decide framing, ticks or marks. Ticks/gridlines/spine are
 *     ENC-1253; this module's only job is to produce a number the chart can
 *     state.
 *
 * WIRE FORMAT. It folds the real dataplane record stream — the same bytes
 * embassy emits and `EngineHost.enqueueData` consumes — so there is no second
 * copy of the data and no shim:
 *
 *   [1B op][4B bufferId u32 LE][4B offsetBytes u32 LE][4B payloadBytes u32 LE][payload]
 *
 * op 1 = append, op 2 = updateRange. `offsetBytes` is the destination offset
 * within the target buffer, so `offsetBytes % stride` gives the record phase of
 * the payload; a partial leading record is skipped rather than misread.
 */

import type { Range } from "./scale";

/**
 * Where the domain-bearing f32 fields sit inside one record of a vertex format.
 * Offsets are BYTES from the start of the record, matching the engine's vertex
 * layouts (`core/include/dc/scene/Geometry.hpp`).
 */
export interface RecordLayout {
  /** Bytes per record (the engine's `strideOf(VertexFormat)`). */
  stride: number;
  /** Byte offsets of f32 fields contributing to the X domain. */
  x: number[];
  /** Byte offsets of f32 fields contributing to the Y domain. */
  y: number[];
  /**
   * Byte offset of a half-extent field that widens X by ±value (candle6's
   * `halfWidth`). The domain of a bar is the bar, not its centre.
   */
  xHalfWidth?: number;
}

/**
 * Domain-bearing field layout per engine vertex format. Keyed by the format
 * strings `createGeometry` accepts (`toString(VertexFormat)`), plus the
 * shorthand `pos2` some manifests use for `pos2_clip`.
 *
 * NOTE `pos2_clip` records are already in CLIP space, so a domain taken over one
 * is a clip-space domain. That is the caller's call to make: as with embassy,
 * nothing is folded unless the caller names the buffer.
 */
export const RECORD_LAYOUTS: Readonly<Record<string, RecordLayout>> = {
  // [x, open, high, low, close, halfWidth]
  candle6: { stride: 24, x: [0], y: [4, 8, 12, 16], xHalfWidth: 20 },
  // [x0, y0, x1, y1]
  rect4: { stride: 16, x: [0, 8], y: [4, 12] },
  // rect4 (16) + rgba8 (4) + scalar lane (4)
  rect4_color: { stride: 24, x: [0, 8], y: [4, 12] },
  // [x, y]
  pos2: { stride: 8, x: [0], y: [4] },
  pos2_clip: { stride: 8, x: [0], y: [4] },
  // [x, y, alpha]
  pos2_alpha: { stride: 12, x: [0], y: [4] },
  // [x, y, r, g, b, a]
  pos2_color4: { stride: 24, x: [0], y: [4] },
  // [x, y, u, v]
  pos2_uv4: { stride: 16, x: [0], y: [4] },
  // pos2 (8) + rgba8 (4) + size px (4)
  point4_color: { stride: 16, x: [0], y: [4] },
  // [x0, y0, x1, y1, u0, v0, u1, v1]
  glyph8: { stride: 32, x: [0, 8], y: [4, 12] },
};

/**
 * One buffer registered into a tracker's axis group. Give either a `format`
 * (resolved through RECORD_LAYOUTS) or an explicit `layout`.
 */
export interface DomainSource {
  /** Dataplane buffer id these records land on. */
  bufferId: number;
  /** Engine vertex format name, e.g. 'candle6'. Ignored when `layout` is set. */
  format?: string;
  /** Explicit layout, for a format not in RECORD_LAYOUTS. */
  layout?: RecordLayout;
  /**
   * Restrict which axes this buffer feeds. Default both. A buffer drawn on a
   * shared X but its own Y (a volume sub-pane) registers with `axes: 'x'`.
   */
  axes?: "x" | "y" | "xy";
}

/**
 * Policy applied when turning a running (min,max) into a stated domain.
 * Defaults mirror embassy's RangeTrackerOpts, except `paddingFrac` — see the
 * module header for why a reported domain is unpadded by default.
 */
export interface DomainPolicy {
  /** Symmetric headroom as a fraction of the span. Default 0 (none). */
  paddingFrac?: number;
  /** Collapse floor as a fraction of |midpoint|. Default 0.005. */
  minHalfWidthFrac?: number;
  /** Absolute collapse floor. Default 0.01. */
  minHalfWidthAbs?: number;
}

const DEFAULT_POLICY: Required<DomainPolicy> = {
  paddingFrac: 0,
  minHalfWidthFrac: 0.005,
  minHalfWidthAbs: 0.01,
};

/** A tracker's report: the per-axis domain plus what it was measured from. */
export interface ObservedDomain {
  /** X domain, or null when nothing has been observed on the X axis. */
  x: Range | null;
  /** Y domain, or null when nothing has been observed on the Y axis. */
  y: Range | null;
  /** Records folded so far (the analogue of RangeTracker.Observations). */
  records: number;
  /** Field values folded so far, across both axes. */
  observations: number;
}

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;
const RECORD_HEADER_SIZE = 13; // [1B op][4B bufferId][4B offset][4B payloadBytes]

/**
 * Widen a collapsed range and (optionally) pad it. Never returns min > max.
 * Non-finite input is returned untouched — a caller that folded garbage should
 * see the garbage, not a laundered number.
 */
export function applyDomainPolicy(range: Range, policy: DomainPolicy = {}): Range {
  const p = { ...DEFAULT_POLICY, ...policy };
  let { min, max } = range;
  if (!Number.isFinite(min) || !Number.isFinite(max)) return { min, max };
  if (max < min) [min, max] = [max, min];

  const mid = (min + max) / 2;
  const floor = Math.max(Math.abs(mid) * p.minHalfWidthFrac, p.minHalfWidthAbs);
  if (max - min < floor * 2) {
    min = mid - floor;
    max = mid + floor;
  }
  if (p.paddingFrac > 0) {
    const pad = (max - min) * p.paddingFrac;
    min -= pad;
    max += pad;
  }
  return { min, max };
}

/** Mutable running extent for one axis. */
class Running {
  min = Number.POSITIVE_INFINITY;
  max = Number.NEGATIVE_INFINITY;
  count = 0;

  fold(v: number): void {
    if (!Number.isFinite(v)) return; // NaN/Inf never widens a domain
    if (v < this.min) this.min = v;
    if (v > this.max) this.max = v;
    this.count++;
  }

  reset(): void {
    this.min = Number.POSITIVE_INFINITY;
    this.max = Number.NEGATIVE_INFINITY;
    this.count = 0;
  }

  range(): Range | null {
    return this.count > 0 ? { min: this.min, max: this.max } : null;
  }
}

interface ResolvedSource {
  layout: RecordLayout;
  wantX: boolean;
  wantY: boolean;
}

function resolveLayout(src: DomainSource): RecordLayout {
  if (src.layout) return src.layout;
  const byName = src.format ? RECORD_LAYOUTS[src.format] : undefined;
  if (!byName) {
    throw new Error(
      `DomainTracker: buffer ${src.bufferId} declares no layout and format ` +
        `${JSON.stringify(src.format)} is not a known record layout ` +
        `(${Object.keys(RECORD_LAYOUTS).join(", ")})`,
    );
  }
  return byName;
}

/**
 * Streaming per-axis domain over the dataplane record stream.
 *
 * Construct it with the buffers that make up ONE axis group, feed it every
 * binary batch the view receives, and ask it for the domain. Folding is O(Δ):
 * each batch is walked once, nothing is rescanned.
 *
 * ```ts
 * const tracker = new DomainTracker([{ bufferId: 10100, format: 'candle6' }]);
 * ws.onmessage = (e) => { tracker.observe(e.data); host.enqueueData(e.data); };
 * tracker.domain(); // { x: {min,max}, y: {min,max}, records, observations }
 * ```
 */
export class DomainTracker {
  private readonly sources = new Map<number, ResolvedSource>();
  private readonly xs = new Running();
  private readonly ys = new Running();
  private recordCount = 0;

  constructor(
    sources: DomainSource[],
    private readonly policy: DomainPolicy = {},
  ) {
    for (const src of sources) {
      const axes = src.axes ?? "xy";
      this.sources.set(src.bufferId, {
        layout: resolveLayout(src),
        wantX: axes === "x" || axes === "xy",
        wantY: axes === "y" || axes === "xy",
      });
    }
  }

  /** Buffer ids this tracker folds (its axis group). */
  get bufferIds(): number[] {
    return [...this.sources.keys()];
  }

  /** Records folded so far. */
  get records(): number {
    return this.recordCount;
  }

  /**
   * Fold one dataplane batch. Records on buffers outside the axis group are
   * skipped (as in embassy's Observe), as are malformed/truncated records.
   * Returns the number of records folded from THIS batch.
   */
  observe(batch: ArrayBuffer | ArrayBufferView): number {
    const view =
      batch instanceof ArrayBuffer
        ? new DataView(batch)
        : new DataView(batch.buffer, batch.byteOffset, batch.byteLength);

    let o = 0;
    let folded = 0;
    while (o + RECORD_HEADER_SIZE <= view.byteLength) {
      const op = view.getUint8(o);
      const bufferId = view.getUint32(o + 1, true);
      const offsetBytes = view.getUint32(o + 5, true);
      const payloadBytes = view.getUint32(o + 9, true);
      o += RECORD_HEADER_SIZE;
      if (o + payloadBytes > view.byteLength) break; // truncated batch
      const src =
        op === OP_APPEND || op === OP_UPDATE_RANGE ? this.sources.get(bufferId) : undefined;
      if (src) folded += this.foldPayload(view, o, payloadBytes, offsetBytes, src);
      o += payloadBytes;
    }
    this.recordCount += folded;
    return folded;
  }

  /** Fold whole records out of one payload. Returns the count folded. */
  private foldPayload(
    view: DataView,
    payloadStart: number,
    payloadBytes: number,
    offsetBytes: number,
    src: ResolvedSource,
  ): number {
    const { stride } = src.layout;
    if (stride <= 0) return 0;
    // The payload lands at `offsetBytes` in the destination buffer, so the first
    // record boundary inside it is at this phase. An append is record-aligned
    // (phase 0); a partial updateRange is not, and the leading fragment is
    // skipped rather than decoded at the wrong offset.
    const phase = (stride - (offsetBytes % stride)) % stride;
    let folded = 0;
    for (let r = payloadStart + phase; r + stride <= payloadStart + payloadBytes; r += stride) {
      if (src.wantX) {
        const half = src.layout.xHalfWidth !== undefined
          ? Math.abs(view.getFloat32(r + src.layout.xHalfWidth, true))
          : 0;
        for (const off of src.layout.x) {
          const v = view.getFloat32(r + off, true);
          if (half > 0 && Number.isFinite(half)) {
            this.xs.fold(v - half);
            this.xs.fold(v + half);
          } else {
            this.xs.fold(v);
          }
        }
      }
      if (src.wantY) {
        for (const off of src.layout.y) this.ys.fold(view.getFloat32(r + off, true));
      }
      folded++;
    }
    return folded;
  }

  /** The raw running extents, with NO policy applied. */
  raw(): ObservedDomain {
    return {
      x: this.xs.range(),
      y: this.ys.range(),
      records: this.recordCount,
      observations: this.xs.count + this.ys.count,
    };
  }

  /**
   * The stated domain: the running extents with the collapse floor (and any
   * `paddingFrac`) applied. Null per axis until something has been observed —
   * a chart with no data states no domain rather than inventing one.
   */
  domain(policy: DomainPolicy = this.policy): ObservedDomain {
    const raw = this.raw();
    return {
      x: raw.x ? applyDomainPolicy(raw.x, policy) : null,
      y: raw.y ? applyDomainPolicy(raw.y, policy) : null,
      records: raw.records,
      observations: raw.observations,
    };
  }

  /** Forget everything observed (a replay loop restarting on a fresh buffer). */
  reset(): void {
    this.xs.reset();
    this.ys.reset();
    this.recordCount = 0;
  }
}
