#include "dc/ingest/IngestProcessor.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

static void appendRecord(std::vector<std::uint8_t>& batch,
                          std::uint8_t op, std::uint32_t bufferId,
                          std::uint32_t offset, const void* payload, std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

int main() {
  dc::IngestProcessor ingest;

  constexpr dc::Id BUF = 1;

  // Set cap to 48 bytes
  ingest.setMaxBytes(BUF, 48);
  requireTrue(ingest.getMaxBytes(BUF) == 48, "maxBytes==48");

  // Ingest 72 bytes (three 24-byte chunks)
  std::uint8_t data24[24];
  for (int i = 0; i < 24; i++) data24[i] = static_cast<std::uint8_t>(i);

  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, BUF, 0, data24, 24);
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  std::printf("After first append: size=%u\n", ingest.getBufferSize(BUF));
  requireTrue(ingest.getBufferSize(BUF) == 24, "size==24 after first");

  // Second 24-byte append
  for (int i = 0; i < 24; i++) data24[i] = static_cast<std::uint8_t>(100 + i);
  batch.clear();
  appendRecord(batch, 1, BUF, 0, data24, 24);
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  std::printf("After second append: size=%u\n", ingest.getBufferSize(BUF));
  requireTrue(ingest.getBufferSize(BUF) == 48, "size==48 after second");

  // Third 24-byte append → should evict front to stay at 48
  for (int i = 0; i < 24; i++) data24[i] = static_cast<std::uint8_t>(200 + i);
  batch.clear();
  appendRecord(batch, 1, BUF, 0, data24, 24);
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  std::printf("After third append (cap enforced): size=%u\n", ingest.getBufferSize(BUF));
  requireTrue(ingest.getBufferSize(BUF) == 48, "size==48 after cap enforcement");

  // First byte should be from second chunk (100), not first chunk (0)
  const auto* buf = ingest.getBufferData(BUF);
  std::printf("First byte after eviction: %u (expected 100)\n", buf[0]);
  requireTrue(buf[0] == 100, "front eviction kept correct data");

  // evictFront(24) → 24 bytes remain
  ingest.evictFront(BUF, 24);
  std::printf("After evictFront(24): size=%u\n", ingest.getBufferSize(BUF));
  requireTrue(ingest.getBufferSize(BUF) == 24, "size==24 after evictFront");
  requireTrue(ingest.getBufferData(BUF)[0] == 200, "evictFront kept last chunk");

  // keepLast(12) → 12 bytes remain
  ingest.keepLast(BUF, 12);
  std::printf("After keepLast(12): size=%u\n", ingest.getBufferSize(BUF));
  requireTrue(ingest.getBufferSize(BUF) == 12, "size==12 after keepLast");

  std::printf("\nD2.5 cache PASS\n");
  return 0;
}
