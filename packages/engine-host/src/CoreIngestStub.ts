export type IngestResult = {
  touchedBufferIds: number[];
  payloadBytes: number;     // bytes carried in the batch (semantic “ingest” bytes)
  droppedBytes: number;     // bytes dropped due to cap
};

type CpuBuffer = {
  id: number;
  cpu: Uint8Array;
};

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

export class CoreIngestStub {
  // Prevent runaway memory (ring buffer comes later). This is a simple hard cap.
  // Keep it modest so you can see stability (tune as you like).
  readonly MAX_BUFFER_BYTES = 4 * 1024 * 1024; // 4 MiB per buffer

  private buffers = new Map<number, CpuBuffer>();

  private maxBytes = new Map<number, number>(); // per-buffer cap (bytes)

  setMaxBytes(bufferId: number, bytes: number) {
    const b = Math.max(0, bytes | 0);
    this.maxBytes.set(bufferId, b);
    this.enforceCap(bufferId);
  }

  getMaxBytes(bufferId: number): number {
    return this.maxBytes.get(bufferId) ?? this.MAX_BUFFER_BYTES;
  }

  evictFront(bufferId: number, bytes: number) {
    const b = this.buffers.get(bufferId);
    if (!b) return;
    const drop = Math.max(0, bytes | 0);
    if (drop <= 0) return;
    if (drop >= b.cpu.byteLength) {
      b.cpu = new Uint8Array(0);
      return;
    }
    b.cpu = b.cpu.subarray(drop); // view into old buffer; ok for now
  }

  keepLast(bufferId: number, bytes: number) {
    const b = this.buffers.get(bufferId);
    if (!b) return;
    const keep = Math.max(0, bytes | 0);
    if (keep <= 0) {
      b.cpu = new Uint8Array(0);
      return;
    }
    if (b.cpu.byteLength <= keep) return;
    b.cpu = b.cpu.subarray(b.cpu.byteLength - keep);
  }

  private enforceCap(bufferId: number) {
    const b = this.buffers.get(bufferId);
    if (!b) return;
    const cap = this.getMaxBytes(bufferId);
    if (cap <= 0) { b.cpu = new Uint8Array(0); return; }
    if (b.cpu.byteLength <= cap) return;
    // rolling window: keep last cap bytes
    b.cpu = b.cpu.subarray(b.cpu.byteLength - cap);
  }


  getBufferBytes(bufferId: number): Uint8Array {
    const b = this.buffers.get(bufferId);
    return b ? b.cpu : new Uint8Array(0);
  }

  getActiveBufferCount(): number {
    return this.buffers.size;
  }

  ensureBuffer(bufferId: number) {
    if (this.buffers.has(bufferId)) return;
    this.buffers.set(bufferId, { id: bufferId, cpu: new Uint8Array(0) });
  }

  deleteBuffer(bufferId: number) {
    this.buffers.delete(bufferId);
  }


  ingestData(batch: ArrayBuffer): IngestResult {
    const dv = new DataView(batch);
    let p = 0;

    const touched = new Set<number>();
    let payloadBytes = 0;
    let droppedBytes = 0;

    while (p < dv.byteLength) {
      if (p + 1 + 4 + 4 + 4 > dv.byteLength) {
        throw new Error("CoreIngestStub: truncated record header");
      }

      const op = dv.getUint8(p); p += 1;
      const bufferId = dv.getUint32(p, true); p += 4;
      const offsetBytes = dv.getUint32(p, true); p += 4;
      const len = dv.getUint32(p, true); p += 4;

      if (p + len > dv.byteLength) {
        throw new Error("CoreIngestStub: truncated payload");
      }

      const payload = new Uint8Array(batch, p, len);
      p += len;

      payloadBytes += len;

      this.ensureBuffer(bufferId);
      const b = this.buffers.get(bufferId)!;

      if (op === OP_APPEND) {
        const cap = this.getMaxBytes(bufferId);
        if (cap <= 0) {
          droppedBytes += len;
          continue;
        }

        // Always accept, but keep only the last `cap` bytes (rolling window).
        // If incoming payload itself exceeds cap, keep only the tail.
        const incoming = (len > cap) ? payload.subarray(len - cap) : payload;
        const incomingLen = incoming.byteLength;

        // Fast path: append without realloc if empty
        if (b.cpu.byteLength === 0) {
          b.cpu = new Uint8Array(incomingLen);
          b.cpu.set(incoming, 0);
          touched.add(bufferId);
          continue;
        }

        // Append
        const next = new Uint8Array(b.cpu.byteLength + incomingLen);
        next.set(b.cpu, 0);
        next.set(incoming, b.cpu.byteLength);
        b.cpu = next;

        // Enforce cap via rolling keep-last
        if (b.cpu.byteLength > cap) {
          b.cpu = b.cpu.subarray(b.cpu.byteLength - cap);
        }

        // We only count drops when input had to be truncated
        if (len > incomingLen) droppedBytes += (len - incomingLen);

        touched.add(bufferId);
        continue;
      }

      if (op === OP_UPDATE_RANGE) {
        // updateRange can grow, but still cap
        const end = offsetBytes + len;
        if (end > this.MAX_BUFFER_BYTES) {
          // clamp
          const allowed = Math.max(0, this.MAX_BUFFER_BYTES - offsetBytes);
          if (allowed <= 0) {
            droppedBytes += len;
            continue;
          }
          const clipped = payload.subarray(0, allowed);

          if (end > b.cpu.byteLength) {
            const grown = new Uint8Array(Math.min(this.MAX_BUFFER_BYTES, end));
            grown.set(b.cpu, 0);
            b.cpu = grown;
          }
          b.cpu.set(clipped, offsetBytes);
          droppedBytes += (len - allowed);
          touched.add(bufferId);
          continue;
        }

        if (end > b.cpu.byteLength) {
          const grown = new Uint8Array(end);
          grown.set(b.cpu, 0);
          b.cpu = grown;
        }
        b.cpu.set(payload, offsetBytes);
        touched.add(bufferId);
        continue;
      }

      throw new Error(`CoreIngestStub: unknown op ${op}`);
    }

    return {
      touchedBufferIds: [...touched],
      payloadBytes,
      droppedBytes
    };
  }
}
