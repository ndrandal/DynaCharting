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
 * Growth-sync + X-anchor (was in useAgentStream): for instanced geometry the
 * renderer draws geometry.vertexCount instances and the dataplane only grows
 * the buffer's bytes, so as records land we advance vertexCount via a
 * fresh-geometry rebind (ENC-558: the instanced backend caches its GPU buffer
 * per geometryId). When the view sets xAnchor, the X part of the transform is
 * re-derived from the first replayed record's recordIndex so the chart frames
 * from the left regardless of embassy's absolute index at capture time.
 */

import { useEffect, useRef } from 'react';
import type { EngineHost } from '@repo/dc-wasm';

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
}

/**
 * Growth/X-anchor descriptor for a live-growing instanced geometry. Provided by
 * a view's manifest module when its data feeds an instanced pipeline whose
 * vertexCount must track the streamed record count (e.g. candle6). Omit for
 * views whose geometry is fixed-size (the buffer is overwritten in place).
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
  /** Instanced-geometry growth descriptor (omit for fixed-size views). */
  growth?: GrowthSync;
  /** X-anchor framing (omit unless the view sets xAnchor). */
  xAnchor?: XAnchorSpec;
}

const OP_APPEND = 1;
const RECORD_HEADER_SIZE = 13; // [1B op][4B bufferId][4B offset][4B payloadBytes]
/** Min interval between instanced-geometry rebuilds (ms); bounds rebuild churn. */
const REBUILD_INTERVAL_MS = 250;

function b64ToArrayBuffer(b64: string): ArrayBuffer {
  const bin = atob(b64);
  const len = bin.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) bytes[i] = bin.charCodeAt(i);
  return bytes.buffer;
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
 */
export function useReplay(host: EngineHost | null, records: Records | null, opts: ReplayOptions = {}): void {
  const { playing = true, onProgress, onComplete, growth, xAnchor } = opts;
  // Hold the latest callbacks without re-arming the timeline each render.
  const onProgressRef = useRef(onProgress);
  onProgressRef.current = onProgress;
  const onCompleteRef = useRef(onComplete);
  onCompleteRef.current = onComplete;

  useEffect(() => {
    if (!host || !records || !records.frames.length) return;
    if (!playing) return;

    const frames = records.frames;
    const total = frames.length;

    // Per-run replay state (reset each (re)start of the effect).
    let cancelled = false;
    let idx = 0;
    let recordTotal = 0; // cumulative records counted off the replayed frames
    let syncedCount = -1; // record count at the last geometry rebuild
    let xAnchored = false;
    let curGeometryId = growth?.geometryId ?? 0;
    let geomSeq = 0;
    let lastRebuild = 0;
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

    // Throttled instanced-geometry rebuild so the backend re-uploads the grown
    // buffer (ENC-558). Hand a fresh geometryId, rebind, delete the prior one.
    const growthSync = () => {
      if (!growth) return;
      const now = performance.now();
      if (recordTotal <= 0 || recordTotal === syncedCount) return;
      if (now - lastRebuild < REBUILD_INTERVAL_MS) return;
      lastRebuild = now;
      syncedCount = recordTotal;
      geomSeq += 1;
      const newGeom = growth.geometryId + (geomSeq % 98) + 1;
      const prevGeom = curGeometryId;
      host.applyControl({
        cmd: 'createGeometry',
        id: newGeom,
        vertexBufferId: growth.bufferId,
        format: growth.format,
        vertexCount: recordTotal,
      });
      host.applyControl({ cmd: 'bindDrawItem', drawItemId: growth.drawItemId, pipeline: growth.pipeline, geometryId: newGeom });
      if (prevGeom && prevGeom !== newGeom) host.deleteGeometry(prevGeom);
      curGeometryId = newGeom;
      host.markDirty();
    };

    // Push one frame into the data plane + update growth/progress.
    const pushFrame = (i: number) => {
      if (cancelled) return;
      const ab = b64ToArrayBuffer(frames[i].b64);
      if (growth) {
        anchorXFor(ab);
        recordTotal += countRecordsForBuffer(ab, growth.bufferId, growth.stride);
      }
      host.enqueueData(ab);
      if (growth) growthSync();
      onProgressRef.current?.((i + 1) / total);
      idx = i + 1;
      if (idx >= total) {
        // Final flush of the grown geometry so the last records render even if
        // the rebuild throttle skipped them, then signal completion (the
        // controller drives looping via resetScene → applyManifest → restart).
        if (growth) {
          lastRebuild = 0;
          growthSync();
        }
        onCompleteRef.current?.();
      }
    };

    // Schedule every frame at its relative `t`. One timeline pass.
    const scheduleRun = () => {
      if (cancelled) return;
      const t0 = frames[0].t;
      for (let i = idx; i < total; i++) {
        const delay = Math.max(0, frames[i].t - t0);
        const timer = setTimeout(() => pushFrame(i), delay);
        timers.push(timer);
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
