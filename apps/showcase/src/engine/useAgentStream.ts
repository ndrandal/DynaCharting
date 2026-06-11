/* apps/showcase/src/engine/useAgentStream.ts
 *
 * WebSocket data-plane hook. When VITE_SHOWCASE_AGENT_URL is set, it opens the
 * agent (embassy) data plane, sets binaryType=arraybuffer, and forwards each
 * binary message straight into the engine's data plane (host.enqueueData).
 * embassy's text frames (its own scene-init) are IGNORED — the showcase owns
 * its manifest (CONTRACT-buffer-id.md). When the env var is unset, it's inert.
 *
 * Optional growth sync (`growth`): the data plane only grows a buffer's BYTES;
 * the renderer draws geometry.vertexCount instances. For a live vertex/instance
 * buffer (e.g. the candle6 buffer) the browser must advance vertexCount as
 * records land. We count records straight off the wire — parsing each batch's
 * APPEND record headers (op 1, [1B op][4B bufferId][4B offset][4B payloadBytes])
 * and summing payloadBytes/stride for the target bufferId — then, throttled to
 * one update per animation frame, issue setGeometryVertexCount = total records.
 * Counting on the wire (not via a WASM read) keeps the sync off the ASYNCIFY
 * core entirely; setGeometryVertexCount goes through applyControl, which the
 * host defers while a render is in flight and replays after.
 */

const OP_APPEND = 1;
const RECORD_HEADER_SIZE = 13; // [1B op][4B bufferId][4B offset][4B payloadBytes]

/**
 * Count records targeting `bufferId` in one binary batch by walking its APPEND
 * record headers. Returns the number of whole `stride`-byte records appended.
 */
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
    if (op === OP_APPEND && bufId === bufferId) {
      records += Math.floor(payloadBytes / stride);
    }
    o += payloadBytes;
  }
  return records;
}

import { useEffect } from 'react';
import type { EngineHost } from '@repo/dc-wasm';

const AGENT_URL = import.meta.env.VITE_SHOWCASE_AGENT_URL as string | undefined;

/**
 * Drive a live-growing instanced geometry from a streamed buffer.
 *
 * The WASM/Dawn instanced backends cache their GPU instance buffer per
 * geometryId on first render and never re-read the CPU buffer as it grows
 * (DawnInstancedCandleBackend::ensureGeoBuffers). To surface newly-streamed
 * records the browser must hand the renderer a FRESH geometryId, which forces a
 * rebuild that uploads the buffer's current contents. We do this throttled
 * (~once/sec): create a new geometry over the same buffer with the up-to-date
 * vertexCount, rebind the drawItem to it, and delete the prior geometry.
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
  /** Transform to live-anchor on X from the first streamed record. */
  transformId: number;
  /** Byte offset of the x field within a record (recordIndex). */
  xField: number;
  /** Width (in record-index units) of the visible X window. */
  xWindow: number;
  /** Clip-space X extents the window maps onto. */
  clipMin: number;
  clipMax: number;
  /** Baked Y mapping (price → clip), preserved across the live setTransform. */
  sy: number;
  ty: number;
}

/** Min interval between geometry rebuilds (ms). Bounds rebuild churn/leak. */
const REBUILD_INTERVAL_MS = 750;

/** Read the first record's x value for `bufferId` from a batch, or null. */
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
 * When VITE_SHOWCASE_AGENT_URL is set, open a WS, set binaryType=arraybuffer,
 * forward each binary message into the engine's data plane, and (when `growth`
 * is provided) keep the geometry's vertexCount in step with the buffer's record
 * count. When the URL is unset (the default), do nothing. Reconnect logic is
 * deliberately omitted.
 */
export function useAgentStream(host: EngineHost | null, growth?: GrowthSync): void {
  useEffect(() => {
    if (!host || !AGENT_URL) return;

    let ws: WebSocket | null = null;
    let rafPending = false;
    let recordTotal = 0; // cumulative records counted off the wire
    let syncedCount = -1; // record count of the last geometry rebuild
    let xAnchored = false; // whether the live X transform has been set
    let curGeometryId = growth?.geometryId ?? 0; // geometry currently bound
    let geomSeq = 0; // monotonic suffix for fresh geometry ids
    let lastRebuild = 0; // timestamp of the last rebuild (throttle)

    // Live X-anchor: on the first record, map [firstX, firstX+window] → clipX so
    // candles frame from the left regardless of embassy's absolute recordIndex.
    const anchorX = (batch: ArrayBuffer) => {
      if (!growth || xAnchored) return;
      const firstX = firstRecordX(batch, growth.bufferId, growth.xField);
      if (firstX === null) return;
      xAnchored = true;
      const sx = (growth.clipMax - growth.clipMin) / growth.xWindow;
      const tx = growth.clipMin - sx * firstX;
      host.applyControl({
        cmd: 'setTransform',
        id: growth.transformId,
        sx,
        tx,
        sy: growth.sy,
        ty: growth.ty,
      });
      host.markDirty();
    };

    // Throttled geometry rebuild: hand the renderer a fresh geometryId so the
    // instanced backend re-uploads the grown buffer (see GrowthSync docstring).
    // One attempt per animation frame, rate-limited to REBUILD_INTERVAL_MS.
    const scheduleGrowthSync = () => {
      if (!growth || rafPending) return;
      rafPending = true;
      requestAnimationFrame(() => {
        rafPending = false;
        const g = growth;
        const now = performance.now();
        if (recordTotal <= 0 || recordTotal === syncedCount) return;
        if (now - lastRebuild < REBUILD_INTERVAL_MS) return;
        lastRebuild = now;
        syncedCount = recordTotal;

        // Fresh geometry id (kept inside the geometry block: base+1 .. base+98).
        // The backend caches its instance buffer per geometryId and is never
        // invalidated, so each rebuild needs a NOT-recently-used id. At
        // REBUILD_INTERVAL_MS this only wraps after ~98 rebuilds (>1min), past
        // the slice's render window; live auto-ranging / a backend-side dirty
        // hook is the real long-run fix (later phase).
        geomSeq += 1;
        const newGeom = g.geometryId + (geomSeq % 98) + 1;
        const prevGeom = curGeometryId;

        // Build the new geometry over the same live buffer with the current
        // record count, then atomically rebind the drawItem to it. applyControl
        // defers under a render-in-flight and replays in order, so the rebind
        // never sees a half-built geometry.
        host.applyControl({
          cmd: 'createGeometry',
          id: newGeom,
          vertexBufferId: g.bufferId,
          format: g.format,
          vertexCount: recordTotal,
        });
        host.applyControl({
          cmd: 'bindDrawItem',
          drawItemId: g.drawItemId,
          pipeline: g.pipeline,
          geometryId: newGeom,
        });
        if (prevGeom && prevGeom !== newGeom) {
          host.deleteGeometry(prevGeom);
        }
        curGeometryId = newGeom;
        host.markDirty();
      });
    };

    try {
      ws = new WebSocket(AGENT_URL);
      ws.binaryType = 'arraybuffer';
      ws.onmessage = (ev: MessageEvent) => {
        if (ev.data instanceof ArrayBuffer) {
          if (growth) {
            anchorX(ev.data);
            recordTotal += countRecordsForBuffer(ev.data, growth.bufferId, growth.stride);
          }
          host.enqueueData(ev.data);
          scheduleGrowthSync();
        }
      };
      ws.onerror = () => {
        console.warn('[showcase] agent stream error');
      };
    } catch (e) {
      console.warn('[showcase] agent stream failed to open:', (e as Error).message);
    }

    return () => {
      ws?.close();
    };
  }, [host, growth]);
}
