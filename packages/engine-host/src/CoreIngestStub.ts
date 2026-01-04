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
        // Hard cap: drop bytes if over limit
        if (b.cpu.byteLength >= this.MAX_BUFFER_BYTES) {
          droppedBytes += len;
          continue;
        }
        const allowed = Math.min(len, this.MAX_BUFFER_BYTES - b.cpu.byteLength);
        if (allowed <= 0) {
          droppedBytes += len;
          continue;
        }

        const oldLen = b.cpu.byteLength;
        const next = new Uint8Array(oldLen + allowed);
        next.set(b.cpu, 0);
        next.set(payload.subarray(0, allowed), oldLen);
        b.cpu = next;

        if (allowed < len) droppedBytes += (len - allowed);
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
