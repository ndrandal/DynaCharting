// D81.3: Partial GPU uploads — GpuBufferManager coalesced dirty ranges +
// IngestProcessor IngestWrite emission.
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireEq(std::uint64_t got, std::uint64_t want, const char* ctx) {
  if (got != want) {
    std::fprintf(stderr, "ASSERT FAIL [%s]: got=%llu want=%llu\n",
                 ctx, (unsigned long long)got, (unsigned long long)want);
    std::exit(1);
  }
}

// Build a single binary ingest record.
static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(v & 0xFF);
  out.push_back((v >> 8) & 0xFF);
  out.push_back((v >> 16) & 0xFF);
  out.push_back((v >> 24) & 0xFF);
}
static void appendRecord(std::vector<std::uint8_t>& batch,
                          std::uint8_t op, std::uint32_t bufferId,
                          std::uint32_t offset, const void* payload,
                          std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const std::uint8_t* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

// Case 1: IngestProcessor emits correct IngestWrite entries for APPEND +
// UPDATE_RANGE, including post-eviction full-range rewrites.
static void testIngestWrites() {
  dc::IngestProcessor ingest;
  ingest.setMaxBytes(100, 16);       // tiny cap so we can trigger eviction

  // First append: 8 bytes into empty buffer.
  std::uint8_t p1[8] = {1,2,3,4,5,6,7,8};
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, p1, 8);
  auto r = ingest.processBatch(batch.data(), (std::uint32_t)batch.size());

  requireEq(r.writes.size(), 1, "one write from one APPEND record");
  requireEq(r.writes[0].bufferId, 100, "bufferId=100");
  requireEq(r.writes[0].offset, 0, "offset=0 on first append into empty buffer");
  requireEq(r.writes[0].length, 8, "length=payload size");

  // Second append: 12 more bytes. Post-insert size = 20, exceeds cap=16, so
  // eviction fires; expected write is full-range [0, 16).
  batch.clear();
  std::uint8_t p2[12] = {9,10,11,12,13,14,15,16,17,18,19,20};
  appendRecord(batch, 1, 100, 0, p2, 12);
  r = ingest.processBatch(batch.data(), (std::uint32_t)batch.size());
  requireEq(r.writes.size(), 1, "one write from one APPEND with eviction");
  requireEq(r.writes[0].offset, 0, "eviction write starts at 0");
  requireEq(r.writes[0].length, 16, "eviction write spans full buffer");
  requireEq(ingest.getBufferSize(100), 16, "buffer capped at max=16");

  // Case 3: UPDATE_RANGE mid-buffer — dirty range equals update range.
  batch.clear();
  std::uint8_t p3[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  appendRecord(batch, 2, 100, 5, p3, 4);         // op=2 UPDATE_RANGE, offset=5
  r = ingest.processBatch(batch.data(), (std::uint32_t)batch.size());
  requireEq(r.writes.size(), 1, "one write from UPDATE_RANGE");
  requireEq(r.writes[0].offset, 5, "update offset preserved");
  requireEq(r.writes[0].length, 4, "update length preserved");
}

// Case 2: GpuBufferManager coalesces adjacent writeRange() calls into one
// glBufferSubData per ingest tick (requires a GL context).
static void testCoalescedUpload(dc::OsMesaContext& ctx) {
  (void)ctx;
  dc::GpuBufferManager gpu;

  const std::uint32_t bufId = 42;
  const std::uint32_t initialSize = 1024;
  std::vector<std::uint8_t> zero(initialSize, 0);
  // Initial upload: full reupload path.
  gpu.setCpuData(bufId, zero.data(), initialSize);
  gpu.uploadDirty();
  auto s = gpu.lastUploadStats();
  requireEq(s.fullUploads, 1, "initial upload is a full reupload");
  requireEq(s.subUploads, 0, "no subUploads on initial full upload");

  // Simulate 10 sequential tail-append writes of 8 bytes each, all before
  // the next uploadDirty(). They should coalesce into a single subUpload.
  std::uint8_t payload[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
  for (int i = 0; i < 10; ++i) {
    gpu.writeRange(bufId, 100 + i * 8, payload, 8);  // contiguous 100..180
  }
  gpu.uploadDirty();
  s = gpu.lastUploadStats();
  requireEq(s.fullUploads, 0, "no full reupload when size unchanged");
  requireEq(s.subUploads, 1, "10 contiguous writes coalesce into 1 subUpload");
  requireEq(s.bytesUploaded, 80, "exactly 80 bytes uploaded, not full buffer");

  // Two disjoint writes should result in two subUploads.
  gpu.writeRange(bufId, 0, payload, 8);
  gpu.writeRange(bufId, 512, payload, 8);
  gpu.uploadDirty();
  s = gpu.lastUploadStats();
  requireEq(s.subUploads, 2, "two disjoint writes = two subUploads");
  requireEq(s.bytesUploaded, 16, "two writes of 8 bytes each");

  // Growing past current capacity forces a full reupload.
  gpu.writeRange(bufId, initialSize, payload, 8);     // extends to 1032
  gpu.uploadDirty();
  s = gpu.lastUploadStats();
  requireEq(s.fullUploads, 1, "grow past gpuCapacity forces full reupload");
  requireEq(s.bytesUploaded, initialSize + 8, "full reupload = entire new size");

  // Subsequent tail write within new capacity should go back to subUpload.
  gpu.writeRange(bufId, 1020, payload, 8);
  gpu.uploadDirty();
  s = gpu.lastUploadStats();
  requireEq(s.fullUploads, 0, "within-capacity write stays in subUpload path");
  requireEq(s.subUploads, 1, "single subUpload for single range");
}

int main() {
  testIngestWrites();

  dc::OsMesaContext ctx;
  if (!ctx.init(64, 64)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping GL portion\n");
    std::fprintf(stdout, "D81.3 range upload (cpu-only): OK\n");
    return 0;
  }
  testCoalescedUpload(ctx);

  std::fprintf(stdout, "D81.3 range upload: OK\n");
  return 0;
}
