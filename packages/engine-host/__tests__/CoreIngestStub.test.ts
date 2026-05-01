import { describe, it, expect } from "vitest";
import { CoreIngestStub } from "../src/CoreIngestStub";

const HEADER = 1 + 4 + 4 + 4;

function makeRecord(op: number, bufferId: number, offset: number, payload: Uint8Array): ArrayBuffer {
  const buf = new ArrayBuffer(HEADER + payload.byteLength);
  const v = new DataView(buf);
  v.setUint8(0, op);
  v.setUint32(1, bufferId, true);
  v.setUint32(5, offset, true);
  v.setUint32(9, payload.byteLength, true);
  new Uint8Array(buf, HEADER).set(payload);
  return buf;
}

describe("CoreIngestStub", () => {
  it("appends bytes via OP=1", () => {
    const s = new CoreIngestStub();
    const r = s.ingestData(makeRecord(1, 7, 0, new Uint8Array([1, 2, 3, 4])));
    expect(r.touchedBufferIds).toEqual([7]);
    expect(r.payloadBytes).toBe(4);
    expect(r.droppedBytes).toBe(0);
    expect(Array.from(s.getBufferBytes(7))).toEqual([1, 2, 3, 4]);
  });

  it("updateRange (OP=2) writes at offset", () => {
    const s = new CoreIngestStub();
    s.ingestData(makeRecord(1, 1, 0, new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0])));
    s.ingestData(makeRecord(2, 1, 4, new Uint8Array([9, 9, 9, 9])));
    expect(Array.from(s.getBufferBytes(1))).toEqual([0, 0, 0, 0, 9, 9, 9, 9]);
  });

  it("rejects truncated header", () => {
    const small = new ArrayBuffer(HEADER - 1);
    const s = new CoreIngestStub();
    expect(() => s.ingestData(small)).toThrow(/truncated record header/);
  });

  it("rejects truncated payload", () => {
    const buf = new ArrayBuffer(HEADER + 2);
    const v = new DataView(buf);
    v.setUint8(0, 1);
    v.setUint32(1, 5, true);
    v.setUint32(5, 0, true);
    v.setUint32(9, 8, true);  // claims 8 bytes payload
    const s = new CoreIngestStub();
    expect(() => s.ingestData(buf)).toThrow(/truncated payload/);
  });

  it("rejects unknown op", () => {
    const s = new CoreIngestStub();
    expect(() => s.ingestData(makeRecord(99, 1, 0, new Uint8Array([1])))).toThrow(/unknown op/);
  });

  it("setMaxBytes enforces a rolling window on append", () => {
    const s = new CoreIngestStub();
    s.setMaxBytes(2, 4);
    s.ingestData(makeRecord(1, 2, 0, new Uint8Array([1, 2, 3, 4, 5, 6])));
    // Initial append exceeds cap → keep last 4 bytes.
    expect(Array.from(s.getBufferBytes(2))).toEqual([3, 4, 5, 6]);
  });

  it("setMaxBytes(0) drops every append for that buffer", () => {
    const s = new CoreIngestStub();
    s.setMaxBytes(3, 0);
    const r = s.ingestData(makeRecord(1, 3, 0, new Uint8Array([1, 2, 3])));
    expect(r.droppedBytes).toBe(3);
    expect(s.getBufferBytes(3).byteLength).toBe(0);
  });

  it("evictFront drops bytes from the head", () => {
    const s = new CoreIngestStub();
    s.ingestData(makeRecord(1, 8, 0, new Uint8Array([10, 20, 30, 40])));
    s.evictFront(8, 2);
    expect(Array.from(s.getBufferBytes(8))).toEqual([30, 40]);
  });

  it("keepLast clamps a buffer to its tail", () => {
    const s = new CoreIngestStub();
    s.ingestData(makeRecord(1, 9, 0, new Uint8Array([1, 2, 3, 4, 5])));
    s.keepLast(9, 2);
    expect(Array.from(s.getBufferBytes(9))).toEqual([4, 5]);
  });

  it("multiple records in a single batch all get processed", () => {
    const a = makeRecord(1, 1, 0, new Uint8Array([0xaa]));
    const b = makeRecord(1, 2, 0, new Uint8Array([0xbb, 0xcc]));
    const merged = new Uint8Array(a.byteLength + b.byteLength);
    merged.set(new Uint8Array(a), 0);
    merged.set(new Uint8Array(b), a.byteLength);
    const s = new CoreIngestStub();
    const r = s.ingestData(merged.buffer);
    expect(r.touchedBufferIds.sort()).toEqual([1, 2]);
    expect(s.getBufferBytes(1)[0]).toBe(0xaa);
    expect(s.getBufferBytes(2)[1]).toBe(0xcc);
  });

  it("fuzz: 100 randomly-sized appends keep the buffer well-formed", () => {
    const s = new CoreIngestStub();
    s.setMaxBytes(0, 1024);
    let total = 0;
    for (let i = 0; i < 100; ++i) {
      const len = 1 + Math.floor(Math.random() * 32);
      const payload = new Uint8Array(len);
      for (let j = 0; j < len; ++j) payload[j] = (i + j) & 0xff;
      s.ingestData(makeRecord(1, 0, 0, payload));
      total += len;
    }
    const got = s.getBufferBytes(0);
    expect(got.byteLength).toBeLessThanOrEqual(1024);
    // Sanity: the tail bytes should be from the last record(s).
    expect(got.byteLength).toBeGreaterThan(0);
  });
});
