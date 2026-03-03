// D57.2 — MessageCodec: compress/decompress round-trip, isCompressed check
#include "dc/data/MessageCodec.hpp"

#include <cstdio>
#include <cstring>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D57.2 MessageCodec Tests ===\n");

  // Test 1: Round-trip with simple data
  {
    const std::uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    auto compressed = dc::MessageCodec::compress(data, sizeof(data));

    // Compressed should be 8 bytes header + 8 bytes data = 16 bytes
    check(compressed.size() == 16, "compressed size = header + data");

    // Should be detected as compressed
    check(dc::MessageCodec::isCompressed(compressed.data(), compressed.size()),
          "isCompressed returns true for compressed data");

    // Decompress and verify
    auto decompressed = dc::MessageCodec::decompress(compressed.data(), compressed.size());
    check(decompressed.size() == sizeof(data), "decompressed size matches original");
    check(std::memcmp(decompressed.data(), data, sizeof(data)) == 0,
          "decompressed data matches original");
  }

  // Test 2: isCompressed on raw (uncompressed) data
  {
    const std::uint8_t raw[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    check(!dc::MessageCodec::isCompressed(raw, sizeof(raw)),
          "isCompressed returns false for raw data");
  }

  // Test 3: isCompressed on too-short data
  {
    const std::uint8_t tiny[] = {0xDC, 0x43};
    check(!dc::MessageCodec::isCompressed(tiny, sizeof(tiny)),
          "isCompressed returns false for data < header size");
  }

  // Test 4: isCompressed on null
  {
    check(!dc::MessageCodec::isCompressed(nullptr, 0),
          "isCompressed returns false for null");
  }

  // Test 5: Empty payload round-trip
  {
    auto compressed = dc::MessageCodec::compress(nullptr, 0);
    check(compressed.size() == 8, "empty compress produces 8-byte header");
    check(dc::MessageCodec::isCompressed(compressed.data(), compressed.size()),
          "empty compressed is detected");

    auto decompressed = dc::MessageCodec::decompress(compressed.data(), compressed.size());
    check(decompressed.empty(), "empty decompress yields empty vector");
  }

  // Test 6: Larger payload round-trip
  {
    std::vector<std::uint8_t> data(1024);
    for (std::size_t i = 0; i < data.size(); ++i) {
      data[i] = static_cast<std::uint8_t>(i & 0xFF);
    }

    auto compressed = dc::MessageCodec::compress(data.data(), data.size());
    check(compressed.size() == 8 + 1024, "1KB compressed size correct");

    auto decompressed = dc::MessageCodec::decompress(compressed.data(), compressed.size());
    check(decompressed.size() == 1024, "1KB decompressed size correct");
    check(std::memcmp(decompressed.data(), data.data(), data.size()) == 0,
          "1KB round-trip data intact");
  }

  // Test 7: Magic bytes verification
  {
    const std::uint8_t data[] = {42};
    auto compressed = dc::MessageCodec::compress(data, 1);
    check(compressed[0] == 0xDC, "magic byte 0 = 0xDC");
    check(compressed[1] == 0x43, "magic byte 1 = 0x43");
    check(compressed[2] == 0x4D, "magic byte 2 = 0x4D");
    check(compressed[3] == 0x50, "magic byte 3 = 0x50");

    // Original size stored as LE u32 = 1
    check(compressed[4] == 1, "size byte 0 = 1");
    check(compressed[5] == 0, "size byte 1 = 0");
    check(compressed[6] == 0, "size byte 2 = 0");
    check(compressed[7] == 0, "size byte 3 = 0");

    // Payload
    check(compressed[8] == 42, "payload byte = 42");
  }

  // Test 8: Decompress corrupted data returns empty
  {
    // Valid magic but wrong size field
    std::uint8_t bad[] = {0xDC, 0x43, 0x4D, 0x50, 0xFF, 0x00, 0x00, 0x00, 0x01};
    auto result = dc::MessageCodec::decompress(bad, sizeof(bad));
    check(result.empty(), "decompress with mismatched size returns empty");
  }

  std::printf("=== D57.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
