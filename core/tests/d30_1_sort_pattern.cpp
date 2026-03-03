// D30.1 — Sort pattern via index buffers (no new engine code needed)
// Demonstrates that writing sorted indices controls logical draw order.
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/Geometry.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

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
  std::printf("=== D30.1 Sort Pattern via Index Buffers ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Setup scene
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");

  // Create vertex buffer with 4 rects in arbitrary order
  // Rect4 format: x0, y0, x1, y1 (16 bytes per rect)
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "createBuffer vertex");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":0})"), "createBuffer index");

  ingest.ensureBuffer(10);
  ingest.ensureBuffer(11);

  // 4 rects: unsorted X values: 3.0, 1.0, 4.0, 2.0
  float rects[] = {
    3.0f, 0.0f, 3.5f, 1.0f,  // rect 0: x=3
    1.0f, 0.0f, 1.5f, 1.0f,  // rect 1: x=1
    4.0f, 0.0f, 4.5f, 1.0f,  // rect 2: x=4
    2.0f, 0.0f, 2.5f, 1.0f,  // rect 3: x=2
  };
  ingest.setBufferData(10, reinterpret_cast<const std::uint8_t*>(rects), sizeof(rects));

  // Sorted indices: 1, 3, 0, 2 → x=1, x=2, x=3, x=4
  std::uint32_t sortedIndices[] = {1, 3, 0, 2};
  ingest.setBufferData(11, reinterpret_cast<const std::uint8_t*>(sortedIndices), sizeof(sortedIndices));

  // Create geometry with index buffer
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"rect4","indexBufferId":11,"indexCount":4})"),
    "createGeometry");

  // Test 1: Geometry has index buffer attached
  const dc::Geometry* g = scene.getGeometry(100);
  check(g != nullptr, "geometry exists");
  check(g->indexBufferId == 11, "indexBufferId set");
  check(g->indexCount == 4, "indexCount is 4");

  // Test 2: Verify index buffer contains sorted order
  const std::uint8_t* idxData = ingest.getBufferData(11);
  check(idxData != nullptr, "index buffer has data");
  std::uint32_t readIndices[4];
  std::memcpy(readIndices, idxData, sizeof(readIndices));
  check(readIndices[0] == 1, "sorted index 0 is 1 (x=1)");
  check(readIndices[1] == 3, "sorted index 1 is 3 (x=2)");
  check(readIndices[2] == 0, "sorted index 2 is 0 (x=3)");
  check(readIndices[3] == 2, "sorted index 3 is 2 (x=4)");

  // Test 3: Verify we can read sorted vertex data via indices
  const std::uint8_t* vtxData = ingest.getBufferData(10);
  float firstSortedX;
  std::memcpy(&firstSortedX, vtxData + readIndices[0] * 16, sizeof(float));
  check(firstSortedX == 1.0f, "first sorted rect x=1.0");

  float lastSortedX;
  std::memcpy(&lastSortedX, vtxData + readIndices[3] * 16, sizeof(float));
  check(lastSortedX == 4.0f, "last sorted rect x=4.0");

  // Test 4: Changing index order re-sorts without touching vertex data
  std::uint32_t reverseIndices[] = {2, 0, 3, 1};
  ingest.setBufferData(11, reinterpret_cast<const std::uint8_t*>(reverseIndices), sizeof(reverseIndices));
  const std::uint8_t* idxData2 = ingest.getBufferData(11);
  std::uint32_t readReverse[4];
  std::memcpy(readReverse, idxData2, sizeof(readReverse));
  float revFirstX;
  std::memcpy(&revFirstX, vtxData + readReverse[0] * 16, sizeof(float));
  check(revFirstX == 4.0f, "reverse order first rect x=4.0");

  std::printf("=== D30.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
