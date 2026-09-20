/* apps/showcase/src/engine/useAgentStream.ts
 *
 * WebSocket data-plane hook. When VITE_SHOWCASE_AGENT_URL is set, it opens the
 * agent (embassy) data plane, sets binaryType=arraybuffer, and forwards each
 * binary message straight into the engine's data plane (host.enqueueData).
 * When the env var is unset, it's inert.
 *
 * TEXT FRAMES: THE MANIFEST IS STILL IGNORED, THE CLOCK IS NOT (ENC-1282).
 * embassy's first text frame is its own scene-init envelope
 * (`{type:'scene-init',commands:[…]}`). The showcase still does NOT apply those
 * commands — it owns its manifest (CONTRACT-buffer-id.md) — but since ENC-1281
 * that envelope is also the ONLY place the producer's clock appears:
 * `createBuffer.timeBasis = {baseMs, periodMs, epochKnown}`, declared once per
 * buffer (SPEC D6). Reading it is not "applying the manifest": it is one
 * per-stream declaration lifted out of a frame whose imperative half is
 * discarded, and it is what turns `t = baseMs + x·periodMs` from a client fit
 * into the producer's measurement. A buffer whose stream has no uniform bar
 * period carries no `timeBasis` at all and the axis is DROPPED (SPEC D7
 * corollary) — which is why `onTimeBasis` is also called with `null`.
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
import { transmittedBasisFromSceneInit } from '@repo/dc-wasm';
import type { EngineHost, TimeBasis } from '@repo/dc-wasm';

const AGENT_URL = import.meta.env.VITE_SHOWCASE_AGENT_URL as string | undefined;

/**
 * Which dataplane session to subscribe to, default `showcase` (the `sessionId`
 * the showcase's own `instruction.json` fixtures declare).
 *
 * embassy's `/data` socket hands a connection NOTHING until it names a session:
 * the handler's only inbound message is `{"type":"subscribe","sessionId":"…"}`
 * and the scene-init envelope plus every buffer snapshot are enqueued inside
 * that call (embassy `internal/dataplane/server.go` `subscribe`). A connection
 * that never subscribes stays open and silent — which looks exactly like a feed
 * with no data, and is why this hook went unexercised.
 */
const AGENT_SESSION =
  (import.meta.env.VITE_SHOWCASE_AGENT_SESSION as string | undefined) || 'showcase';

/**
 * True when this build is pointed at a live agent data plane.
 *
 * The caller needs this to decide what drives the engine: with a live socket
 * attached, replaying a captured tape onto the same buffers would interleave two
 * unrelated record streams on one buffer id.
 */
export function agentStreamEnabled(): boolean {
  return typeof AGENT_URL === 'string' && AGENT_URL.length > 0;
}

/** Callbacks the live stream reports into. All optional. */
export interface AgentStreamCallbacks {
  /**
   * Every binary batch, with the instant it was observed — the same contract
   * `useReplay` uses, so one `DomainTracker`/`IndexTimeTracker` pair serves both
   * inputs. `Date.now()` here is a real wall clock, unlike a tape's `t`.
   */
  onBatch?: (batch: ArrayBuffer, observedAtMs: number) => void;
  /**
   * The basis the producer declared for the growth buffer, or `null` when it
   * declared none (drop the axis). Fires on every scene-init frame, because
   * embassy re-publishes the envelope once it learns a buffer's `baseMs`
   * mid-stream (embassy `republishTimeBasis`): the FIRST envelope a late joiner
   * sees may legitimately carry `epochKnown: false`, and the correction arrives
   * as a second frame rather than as a new connection.
   */
  onTimeBasis?: (basis: TimeBasis | null) => void;
}

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
export function useAgentStream(
  host: EngineHost | null,
  growth?: GrowthSync,
  callbacks?: AgentStreamCallbacks,
): void {
  const onBatch = callbacks?.onBatch;
  const onTimeBasis = callbacks?.onTimeBasis;
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
      ws.onopen = () => {
        ws?.send(JSON.stringify({ type: 'subscribe', sessionId: AGENT_SESSION }));
      };
      ws.onmessage = (ev: MessageEvent) => {
        if (typeof ev.data === 'string') {
          // The manifest half of the envelope is deliberately discarded; only
          // the per-buffer clock is read (see the module docstring). Matched by
          // buffer id, never by position — embassy emits createBuffer commands
          // in sorted-id order, not in the order the manifest declares them.
          if (onTimeBasis) {
            onTimeBasis(transmittedBasisFromSceneInit(ev.data, growth?.bufferId));
          }
          return;
        }
        if (ev.data instanceof ArrayBuffer) {
          if (growth) {
            anchorX(ev.data);
            recordTotal += countRecordsForBuffer(ev.data, growth.bufferId, growth.stride);
          }
          host.enqueueData(ev.data);
          // Date.now(), not performance.now(): a live socket's arrival time is
          // the only wall clock a FITTED basis can have, and the two clocks are
          // not on the same origin. A transmitted basis ignores this entirely.
          onBatch?.(ev.data, Date.now());
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
  }, [host, growth, onBatch, onTimeBasis]);
}
