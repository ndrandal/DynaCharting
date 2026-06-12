/* apps/showcase/src/engine/useReplay.ts
 *
 * The replay engine (T5.6 data source, CONTRACT-view-catalog.md). Replaces
 * useAgentStream for the gallery: instead of a live WS, it decodes a view's
 * captured `records.json` (frozen embassy dataplane frames) and schedules each
 * frame's binary payload into the engine's data plane (host.enqueueData) at the
 * frame's relative timestamp `t`. Supports play/pause/restart and reports
 * progress. No live backend needed → instant switching + static-deployable.
 *
 * Faithfulness: the captured frames ARE embassy's real binary records (13B
 * header + payload). Replaying them through enqueueData drives the WASM/Dawn
 * ingest path identically to the live slice — only the transport changes.
 *
 * MULTI-BUFFER GROWTH (ENC-568, supersedes the single-buffer rebind hack):
 * every frame is pushed verbatim through host.enqueueData. Each record already
 * carries its own bufferId, so a multi-series view (candles + volume + SMA)
 * grows ALL of its buffers as the timeline advances. For each growing geometry
 * the replay counts the records that land on its buffer and issues ONE
 * `setGeometryVertexCount(geometryId, count)` so the renderer draws every
 * streamed instance/vertex. ENC-558 (instanced) + ENC-569 (line/vertex) made the
 * backends re-read a grown buffer once its vertexCount advances, so this single
 * vertexCount bump per series replaces the old throttled createGeometry /
 * bindDrawItem / deleteGeometry geometry-rebind churn (which could only advance
 * ONE buffer per view).
 *
 * X-ANCHOR: when the view sets xAnchor, the X part of its transform is
 * re-derived from the FIRST replayed record's recordIndex (read off the primary
 * growth buffer's x field) so the chart frames from the left regardless of
 * embassy's absolute index at capture time.
 *
 * TEXTURE TIMELINE (ENC-568): a view may also carry an animated TEXTURE track —
 * `records.textures`, a list of frames `{ t, textureId, width, height,
 * pixelsB64, format? }`. Each is scheduled at its `t` and applied via
 * host.setTexturePixels, so a texturedQuad view can SWAP its texture over time
 * (e.g. a rolling-window correlation heatmap). The texture track loops with the
 * binary timeline. See TextureFrame below for the per-view schema.
 */

import { useEffect, useRef } from 'react';
import type { EngineHost } from '@repo/dc-wasm';

/**
 * One frame of a view's animated texture track. A view carrying a `textures`
 * array on its Records will, during replay, have each frame applied via
 * host.setTexturePixels(textureId, decode(pixelsB64), width, height, format) at
 * its relative timestamp `t` (looping with the binary timeline).
 *
 * SCHEMA (per-view authoring contract):
 *   - t          relative ms offset into the timeline (0 = first frame)
 *   - textureId  logical texture id the manifest's texturedQuad drawItem binds
 *                (via setDrawItemTexture); the same id is reused across frames
 *   - width      texture width  in pixels
 *   - height     texture height in pixels
 *   - pixelsB64  base64 of the tightly-packed pixel bytes (RGBA8 = w*h*4 bytes,
 *                row-major from row 0; R8 = w*h bytes)
 *   - format     optional TextureFormat code: 0 = R8, 1 = RGBA8 (default 1)
 */
export interface TextureFrame {
  t: number;
  textureId: number;
  width: number;
  height: number;
  /** Base64 of the tightly-packed pixel bytes for this frame. */
  pixelsB64: string;
  /** Texture format: 0 = R8, 1 = RGBA8 (default). */
  format?: number;
}

/** A captured view's frozen dataplane output (apps/showcase/views/<id>/records.json). */
export interface Records {
  meta: {
    viewId: string;
    durationMs: number;
    frameCount: number;
    cadenceMs: number;
  };
  /** Binary frames with relative timestamps. `b64` = base64 of one dataplane batch. */
  frames: { t: number; b64: string }[];
  /**
   * Optional animated TEXTURE track (ENC-568). When present, each frame is
   * applied via host.setTexturePixels at its `t`, looping with the timeline — so
   * a texturedQuad view can animate (swap its texture over time). Absent for
   * non-texture views (the common case). See TextureFrame for the schema.
   */
  textures?: TextureFrame[];
}

/**
 * A live-growing geometry the replay advances: as records land on `bufferId`,
 * the replay issues `setGeometryVertexCount(geometryId, count)` so the renderer
 * draws every streamed instance/vertex. One entry per growing series in a view
 * (e.g. candle-overlays has three: candles, volume, SMA).
 */
export interface GrowthSeries {
  /** Buffer the records stream into (matches the manifest's createBuffer id). */
  bufferId: number;
  /** Geometry whose vertexCount tracks the buffer's record count. */
  geometryId: number;
  /** Bytes per record on this buffer (candle6 = 24, rect4 = 16, pos2 = 8). */
  stride: number;
}

/**
 * Growth/X-anchor descriptor for a view's PRIMARY live-growing instanced
 * geometry. Provided by a view's manifest module; the replay uses it to locate
 * the X-anchor field on the primary growth buffer and (when no explicit
 * `growthSeries` list is given) as the single series to advance. The chrome
 * overlay + view authoring metadata also read it. Multi-series views additionally
 * export `growthSeries` (see ViewManifestModule).
 */
export interface GrowthSync {
  bufferId: number;
  geometryId: number;
  drawItemId: number;
  layerId: number;
  /** Bytes per record (e.g. candle6 = 24). */
  stride: number;
  /** Vertex format of the live buffer (e.g. "candle6"). */
  format: string;
  /** Pipeline the drawItem binds (e.g. "instancedCandle@1"). */
  pipeline: string;
  /** Transform to live-anchor on X from the first replayed record. */
  transformId: number;
  /** Byte offset of the x field (recordIndex) within a record. */
  xField: number;
}

/** X-anchor framing the replay derives from the first record when view.xAnchor is set. */
export interface XAnchorSpec {
  /** Width (in record-index units) of the visible X window. */
  xWindow: number;
  /** Clip-space X extents the window maps onto. */
  clipMin: number;
  clipMax: number;
  /** Baked Y mapping (preserved across the live X setTransform). */
  sy: number;
  ty: number;
}

export interface ReplayOptions {
  /** When false, the replay is paused (no frames scheduled). Default true. */
  playing?: boolean;
  /** Called with [0..1] replay progress as frames are scheduled. */
  onProgress?: (fraction: number) => void;
  /**
   * Called once when the timeline's last frame has been pushed. The switching
   * controller uses this to drive looping (resetScene → applyManifest →
   * restart) so a loop replays the SAME data instead of re-appending it onto
   * the ever-growing buffer. Omit for a one-shot replay.
   */
  onComplete?: () => void;
  /** Primary growth descriptor (locates the X-anchor field); omit for fixed-size views. */
  growth?: GrowthSync;
  /**
   * Every growing series in the view (candles + volume + SMA …). The replay
   * advances each geometry's vertexCount as its buffer grows. When omitted but
   * `growth` is set, the primary `growth` series alone is advanced (back-compat
   * for single-series views).
   */
  growthSeries?: GrowthSeries[];
  /** X-anchor framing (omit unless the view sets xAnchor). */
  xAnchor?: XAnchorSpec;
}

const OP_APPEND = 1;
const RECORD_HEADER_SIZE = 13; // [1B op][4B bufferId][4B offset][4B payloadBytes]
/** Min interval between vertexCount bumps (ms); bounds control-plane churn. */
const VCOUNT_SYNC_INTERVAL_MS = 200;

function b64ToArrayBuffer(b64: string): ArrayBuffer {
  const bin = atob(b64);
  const len = bin.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) bytes[i] = bin.charCodeAt(i);
  return bytes.buffer;
}

/** Decode base64 pixel bytes for a texture frame. */
function b64ToBytes(b64: string): Uint8Array {
  const bin = atob(b64);
  const len = bin.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) bytes[i] = bin.charCodeAt(i);
  return bytes;
}

/** Count whole `stride`-byte APPEND records targeting `bufferId` in one batch. */
function countRecordsForBuffer(batch: ArrayBuffer, bufferId: number, stride: number): number {
  const dv = new DataView(batch);
  let o = 0;
  let records = 0;
  while (o + RECORD_HEADER_SIZE <= dv.byteLength) {
    const op = dv.getUint8(o);
    const bufId = dv.getUint32(o + 1, true);
    const payloadBytes = dv.getUint32(o + 9, true);
    o += RECORD_HEADER_SIZE;
    if (o + payloadBytes > dv.byteLength) break;
    if (op === OP_APPEND && bufId === bufferId) records += Math.floor(payloadBytes / stride);
    o += payloadBytes;
  }
  return records;
}

/** Read the first APPEND record's x value for `bufferId` from a batch, or null. */
function firstRecordX(batch: ArrayBuffer, bufferId: number, xField: number): number | null {
  const dv = new DataView(batch);
  let o = 0;
  while (o + RECORD_HEADER_SIZE <= dv.byteLength) {
    const op = dv.getUint8(o);
    const bufId = dv.getUint32(o + 1, true);
    const payloadBytes = dv.getUint32(o + 9, true);
    o += RECORD_HEADER_SIZE;
    if (o + payloadBytes > dv.byteLength) break;
    if (op === OP_APPEND && bufId === bufferId && payloadBytes >= xField + 4) {
      return dv.getFloat32(o + xField, true);
    }
    o += payloadBytes;
  }
  return null;
}

/**
 * Replay a view's captured records into the engine's data plane on the recorded
 * timeline. Re-runs (restarts the timeline) whenever `host`, `records`, or the
 * play/loop options change. Pausing (`playing:false`) freezes the current frame
 * and resumes the schedule from where it left off on the next play.
 *
 * Every record (across every bufferId) is delivered verbatim via enqueueData, so
 * multi-series views grow ALL of their buffers; each growing geometry's
 * vertexCount is advanced as its buffer grows (ENC-558/ENC-569 re-read). An
 * optional texture track is applied alongside the binary timeline (ENC-568) so
 * texturedQuad views can animate.
 */
export function useReplay(host: EngineHost | null, records: Records | null, opts: ReplayOptions = {}): void {
  const { playing = true, onProgress, onComplete, growth, growthSeries, xAnchor } = opts;
  // Hold the latest callbacks without re-arming the timeline each render.
  const onProgressRef = useRef(onProgress);
  onProgressRef.current = onProgress;
  const onCompleteRef = useRef(onComplete);
  onCompleteRef.current = onComplete;

  useEffect(() => {
    if (!host || !records) return;
    const frames = records.frames;
    const textureFrames = records.textures ?? [];
    // A view may have a binary timeline, a texture timeline, or both. Nothing to
    // do only when BOTH are empty.
    if (!frames.length && !textureFrames.length) return;
    if (!playing) return;

    const total = frames.length;

    // The series to advance: an explicit growthSeries list, else the primary
    // `growth` (single-series back-compat), else none (fixed-size view).
    const series: GrowthSeries[] =
      growthSeries && growthSeries.length
        ? growthSeries
        : growth
          ? [{ bufferId: growth.bufferId, geometryId: growth.geometryId, stride: growth.stride }]
          : [];

    // Per-run replay state (reset each (re)start of the effect).
    let cancelled = false;
    let idx = 0;
    let xAnchored = false;
    let lastSync = 0;
    const counts = new Map<number, number>(); // bufferId -> cumulative records
    const synced = new Map<number, number>(); // geometryId -> last bumped count
    const timers: ReturnType<typeof setTimeout>[] = [];

    // Live X-anchor: on the first record, map [firstX, firstX+window]→clipX.
    const anchorXFor = (batch: ArrayBuffer) => {
      if (!growth || !xAnchor || xAnchored) return;
      const firstX = firstRecordX(batch, growth.bufferId, growth.xField);
      if (firstX === null) return;
      xAnchored = true;
      const sx = (xAnchor.clipMax - xAnchor.clipMin) / xAnchor.xWindow;
      const tx = xAnchor.clipMin - sx * firstX;
      host.applyControl({ cmd: 'setTransform', id: growth.transformId, sx, tx, sy: xAnchor.sy, ty: xAnchor.ty });
      host.markDirty();
    };

    // Advance each growing geometry's vertexCount to its buffer's record count.
    // Throttled so a burst of frames doesn't flood the control plane; `force`
    // (final flush) bypasses the throttle so the last records always render.
    const syncGrowth = (force = false) => {
      if (!series.length) return;
      const now = performance.now();
      if (!force && now - lastSync < VCOUNT_SYNC_INTERVAL_MS) return;
      lastSync = now;
      let bumped = false;
      for (const s of series) {
        const count = counts.get(s.bufferId) ?? 0;
        if (count <= 0 || synced.get(s.geometryId) === count) continue;
        synced.set(s.geometryId, count);
        host.applyControl({ cmd: 'setGeometryVertexCount', geometryId: s.geometryId, vertexCount: count });
        bumped = true;
      }
      if (bumped) host.markDirty();
    };

    // Progress + completion are driven by the BINARY timeline when it exists,
    // else by the TEXTURE timeline (texture-only views). One source of truth so a
    // texture-only view still reports progress + loops.
    const progressTotal = total > 0 ? total : textureFrames.length;
    let done = 0;
    const advance = () => {
      done += 1;
      onProgressRef.current?.(progressTotal > 0 ? done / progressTotal : 1);
      if (done >= progressTotal) {
        if (series.length) syncGrowth(true); // final flush of grown geometry
        onCompleteRef.current?.();
      }
    };
    const usesBinaryProgress = total > 0;

    // Push one binary frame into the data plane verbatim. Every record routes to
    // its own bufferId, so all of a multi-series view's buffers grow together;
    // we tally per-buffer counts and advance each geometry's vertexCount.
    const pushFrame = (i: number) => {
      if (cancelled) return;
      const ab = b64ToArrayBuffer(frames[i].b64);
      anchorXFor(ab);
      for (const s of series) {
        counts.set(s.bufferId, (counts.get(s.bufferId) ?? 0) + countRecordsForBuffer(ab, s.bufferId, s.stride));
      }
      host.enqueueData(ab);
      syncGrowth();
      idx = i + 1;
      advance();
    };

    // Apply one texture frame (animated TEXTURE track — ENC-568).
    const applyTextureFrame = (f: TextureFrame) => {
      if (cancelled) return;
      const pixels = b64ToBytes(f.pixelsB64);
      host.setTexturePixels(f.textureId, pixels, f.width, f.height, f.format ?? 1);
      host.markDirty();
      if (!usesBinaryProgress) advance(); // texture-only view: drive progress here
    };

    // Schedule every binary frame + every texture frame at its relative `t`,
    // anchored on whichever timeline starts first. One timeline pass; the
    // controller drives looping via onComplete.
    const scheduleRun = () => {
      if (cancelled) return;
      const firstT = Math.min(
        frames.length ? frames[0].t : Infinity,
        textureFrames.length ? textureFrames[0].t : Infinity,
      );
      const t0 = Number.isFinite(firstT) ? firstT : 0;
      for (let i = idx; i < total; i++) {
        const delay = Math.max(0, frames[i].t - t0);
        timers.push(setTimeout(() => pushFrame(i), delay));
      }
      for (const f of textureFrames) {
        const delay = Math.max(0, f.t - t0);
        timers.push(setTimeout(() => applyTextureFrame(f), delay));
      }
    };

    scheduleRun();

    return () => {
      cancelled = true;
      for (const t of timers) clearTimeout(t);
    };
    // growth/xAnchor are stable per-view objects; host+records+playing drive re-arm.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [host, records, playing]);
}
