// D32.1 — DerivedBufferManager: IndexedFilter and Range modes
#include "dc/data/DerivedBuffer.hpp"
#include "dc/ingest/IngestProcessor.hpp"

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
  std::printf("=== D32.1 DerivedBuffer Tests ===\n");

  dc::IngestProcessor ingest;
  dc::DerivedBufferManager mgr;

  // Source buffer: 5 floats (stride=4 bytes each)
  dc::Id srcBuf = 100;
  dc::Id outBuf = 200;
  ingest.ensureBuffer(srcBuf);
  float srcData[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
  ingest.setBufferData(srcBuf, reinterpret_cast<const std::uint8_t*>(srcData), sizeof(srcData));

  // Test 1: IndexedFilter mode
  {
    dc::DerivedBufferConfig cfg;
    cfg.sourceBufferId = srcBuf;
    cfg.outputBufferId = outBuf;
    cfg.recordStride = 4; // sizeof(float)
    cfg.mode = dc::DeriveMode::IndexedFilter;

    mgr.add(1, cfg);
    mgr.setIndices(1, {2, 4, 0}); // pick indices 2, 4, 0 -> 30, 50, 10

    auto touched = mgr.rebuild({srcBuf}, ingest);
    check(touched.size() == 1, "rebuild returns 1 touched buffer");
    check(touched[0] == outBuf, "touched buffer is outputBufferId");

    const std::uint8_t* data = ingest.getBufferData(outBuf);
    std::uint32_t size = ingest.getBufferSize(outBuf);
    check(size == 12, "output is 3 floats (12 bytes)");

    float out[3];
    std::memcpy(out, data, sizeof(out));
    check(out[0] == 30.0f, "indexed[0] = 30");
    check(out[1] == 50.0f, "indexed[1] = 50");
    check(out[2] == 10.0f, "indexed[2] = 10");
  }

  // Test 2: Index change
  {
    mgr.setIndices(1, {1, 3}); // 20, 40
    mgr.rebuild({srcBuf}, ingest);

    const std::uint8_t* data = ingest.getBufferData(outBuf);
    std::uint32_t size = ingest.getBufferSize(outBuf);
    check(size == 8, "output is 2 floats after index change");

    float out[2];
    std::memcpy(out, data, sizeof(out));
    check(out[0] == 20.0f, "re-indexed[0] = 20");
    check(out[1] == 40.0f, "re-indexed[1] = 40");
  }

  // Test 3: Empty indices
  {
    mgr.setIndices(1, {});
    mgr.rebuild({srcBuf}, ingest);
    std::uint32_t size = ingest.getBufferSize(outBuf);
    check(size == 0, "empty indices -> empty output");
  }

  // Test 4: Out-of-range indices are skipped
  {
    mgr.setIndices(1, {0, 99, 2}); // 99 is out of range
    mgr.rebuild({srcBuf}, ingest);
    std::uint32_t size = ingest.getBufferSize(outBuf);
    check(size == 8, "out-of-range indices skipped (2 valid)");
  }

  mgr.remove(1);

  // Test 5: Range mode
  {
    dc::Id outBuf2 = 201;
    dc::DerivedBufferConfig cfg;
    cfg.sourceBufferId = srcBuf;
    cfg.outputBufferId = outBuf2;
    cfg.recordStride = 4;
    cfg.mode = dc::DeriveMode::Range;

    mgr.add(2, cfg);
    mgr.setRange(2, 1, 4); // records 1..4 -> 20, 30, 40

    mgr.rebuild({srcBuf}, ingest);

    const std::uint8_t* data = ingest.getBufferData(outBuf2);
    std::uint32_t size = ingest.getBufferSize(outBuf2);
    check(size == 12, "range output is 3 floats");

    float out[3];
    std::memcpy(out, data, sizeof(out));
    check(out[0] == 20.0f, "range[0] = 20");
    check(out[1] == 30.0f, "range[1] = 30");
    check(out[2] == 40.0f, "range[2] = 40");
  }

  // Test 6: Range clamping
  {
    mgr.setRange(2, 3, 100); // end clamped to totalRecords=5
    mgr.rebuild({srcBuf}, ingest);
    std::uint32_t size = ingest.getBufferSize(201);
    check(size == 8, "range clamped: 2 records");
  }

  // Test 7: Untouched source -> no rebuild
  {
    dc::Id otherBuf = 999;
    auto touched = mgr.rebuild({otherBuf}, ingest);
    check(touched.empty(), "untouched source -> no rebuild");
  }

  std::printf("=== D32.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
