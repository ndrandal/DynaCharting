/* apps/showcase/src/scene/commands.ts
 *
 * Lightweight types for the imperative DynaCharting control plane. A
 * SceneDocument manifest is expressed as an ORDERED list of these commands; the
 * showcase wraps them in beginFrame/commitFrame and routes each through
 * EngineHost.applyControl (which forwards to the WASM core's CommandProcessor).
 *
 * These mirror the verbs the C++ CommandProcessor accepts (see
 * core/src/commands/CommandProcessor.cpp). We keep the type open (the `cmd`
 * discriminant + arbitrary fields) rather than exhaustively modelling every
 * command, because the manifest is authored data and the core is the
 * authoritative validator.
 */

export type SceneCommand = { cmd: string } & Record<string, unknown>;

/**
 * Binary buffer payloads referenced by a manifest. Vertex/instance data does
 * NOT travel as JSON — it is uploaded through the data plane (enqueueData) as
 * packed little-endian records:
 *
 *   [1B op][4B bufferId LE][4B offsetBytes LE][4B payloadBytes LE][payload...]
 *
 * op 1 = APPEND (grow the buffer), op 2 = UPDATE_RANGE (overwrite in place).
 * See core/src/ingest/IngestProcessor.cpp.
 */
export interface BufferUpload {
  bufferId: number;
  /** Float32 values to pack as the payload (interpreted per the geometry format). */
  floats: number[];
  /** APPEND (default) or UPDATE_RANGE. */
  op?: 'append' | 'updateRange';
  /** Byte offset for UPDATE_RANGE (ignored for APPEND). */
  offsetBytes?: number;
}

/**
 * A producer-rasterized texture supplied to a `texturedQuad@1` view (heatmaps,
 * spectrograms, weather radar). The pipeline can't compute per-pixel colour from
 * a JSON manifest (the frontier wall), so the COLORMAP is rasterized upstream
 * (at manifest-build time) into RGBA8 bytes and uploaded via
 * EngineHost.setTexturePixels. The manifest's drawItem references it by
 * textureId (setDrawItemTexture). Purity preserved — the engine only blits.
 */
export interface ViewTexture {
  textureId: number;
  width: number;
  height: number;
  /** RGBA8 pixel bytes (width*height*4), base64-encoded. */
  pixelsB64: string;
  /** Texture format: 0 = R8, otherwise RGBA8 (default). */
  format?: number;
}

/** A full manifest: structural/style commands + the binary data that feeds them. */
export interface SceneManifest {
  /** Human-readable label (shown in the title bar). */
  label: string;
  /** Ordered control commands (applied inside one frame). */
  commands: SceneCommand[];
  /**
   * Optional producer-rasterized textures for `texturedQuad@1` views — applied
   * via EngineHost.setTexturePixels when the manifest loads (the walled-tier
   * escape hatch, ENC-532). Absent for non-texture views.
   */
  textures?: ViewTexture[];
  /**
   * Binary buffer uploads applied via the data plane after the frame. OPTIONAL:
   * catalog views (capture/replay model, CONTRACT-view-catalog.md) carry NO
   * uploads — their data arrives at runtime via useReplay over enqueueData. Only
   * the legacy in-app/static-data path (sampleManifest) sets uploads. Treated as
   * `[]` when absent.
   */
  uploads?: BufferUpload[];
}

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;
const HEADER_SIZE = 13;

/**
 * Pack one BufferUpload into the engine's binary record format. Returns an
 * ArrayBuffer ready for EngineHost.enqueueData.
 */
export function encodeUpload(u: BufferUpload): ArrayBuffer {
  const payloadBytes = u.floats.length * 4;
  const buf = new ArrayBuffer(HEADER_SIZE + payloadBytes);
  const view = new DataView(buf);
  view.setUint8(0, u.op === 'updateRange' ? OP_UPDATE_RANGE : OP_APPEND);
  view.setUint32(1, u.bufferId, true);
  view.setUint32(5, u.op === 'updateRange' ? (u.offsetBytes ?? 0) : 0, true);
  view.setUint32(9, payloadBytes, true);
  let o = HEADER_SIZE;
  for (const f of u.floats) {
    view.setFloat32(o, f, true);
    o += 4;
  }
  return buf;
}
