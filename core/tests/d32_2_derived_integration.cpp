// D32.2 — DerivedBuffer integration with IngestProcessor
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
  std::printf("=== D32.2 DerivedBuffer Integration Tests ===\n");

  dc::IngestProcessor ingest;
  dc::DerivedBufferManager mgr;

  // Setup: source buffer with Pos2_Clip data (8 bytes per vertex: x, y)
  dc::Id srcBuf = 10;
  dc::Id outBuf = 20;
  ingest.ensureBuffer(srcBuf);

  dc::DerivedBufferConfig cfg;
  cfg.sourceBufferId = srcBuf;
  cfg.outputBufferId = outBuf;
  cfg.recordStride = 8; // 2 floats
  cfg.mode = dc::DeriveMode::IndexedFilter;
  mgr.add(1, cfg);
  mgr.setIndices(1, {0, 2});

  // Test 1: Source starts empty, output should be empty
  {
    auto touched = mgr.rebuild({srcBuf}, ingest);
    check(touched.size() == 1, "rebuild even on empty source");
    check(ingest.getBufferSize(outBuf) == 0, "output empty when source empty");
  }

  // Test 2: Append data to source, rebuild
  {
    float verts[] = {
      0.0f, 0.0f,  // vertex 0
      1.0f, 1.0f,  // vertex 1
      2.0f, 2.0f,  // vertex 2
      3.0f, 3.0f,  // vertex 3
    };
    ingest.setBufferData(srcBuf, reinterpret_cast<const std::uint8_t*>(verts), sizeof(verts));

    mgr.rebuild({srcBuf}, ingest);

    std::uint32_t outSize = ingest.getBufferSize(outBuf);
    check(outSize == 16, "output has 2 vertices (16 bytes)");

    const std::uint8_t* data = ingest.getBufferData(outBuf);
    float out[4];
    std::memcpy(out, data, sizeof(out));
    check(out[0] == 0.0f && out[1] == 0.0f, "output[0] = vertex 0");
    check(out[2] == 2.0f && out[3] == 2.0f, "output[1] = vertex 2");
  }

  // Test 3: Change indices, rebuild without source change
  {
    mgr.setIndices(1, {3, 1});
    mgr.rebuild({srcBuf}, ingest);

    const std::uint8_t* data = ingest.getBufferData(outBuf);
    float out[4];
    std::memcpy(out, data, sizeof(out));
    check(out[0] == 3.0f && out[1] == 3.0f, "new indices: output[0] = vertex 3");
    check(out[2] == 1.0f && out[3] == 1.0f, "new indices: output[1] = vertex 1");
  }

  // Test 4: Range mode integration
  {
    dc::Id outBuf2 = 30;
    dc::DerivedBufferConfig cfg2;
    cfg2.sourceBufferId = srcBuf;
    cfg2.outputBufferId = outBuf2;
    cfg2.recordStride = 8;
    cfg2.mode = dc::DeriveMode::Range;
    mgr.add(2, cfg2);
    mgr.setRange(2, 1, 3);

    mgr.rebuild({srcBuf}, ingest);

    std::uint32_t outSize = ingest.getBufferSize(outBuf2);
    check(outSize == 16, "range output has 2 vertices");

    const std::uint8_t* data = ingest.getBufferData(outBuf2);
    float out[4];
    std::memcpy(out, data, sizeof(out));
    check(out[0] == 1.0f && out[1] == 1.0f, "range output[0] = vertex 1");
    check(out[2] == 2.0f && out[3] == 2.0f, "range output[1] = vertex 2");
  }

  // Test 5: Remove derived config
  {
    mgr.remove(1);
    check(mgr.count() == 1, "count after remove");
    mgr.remove(2);
    check(mgr.count() == 0, "count after removing all");
  }

  std::printf("=== D32.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
