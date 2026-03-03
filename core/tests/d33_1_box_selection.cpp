// D33.1 — BoxSelection: known vertex positions, verify hits/misses
#include "dc/selection/BoxSelection.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstring>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    ++failed;
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
  std::printf("=== D33.1 BoxSelection Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Setup: pane + layer + draw item + buffer + geometry
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "createBuffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":6,"format":"pos2_clip"})"),
    "createGeometry");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"),
    "bindDrawItem");

  // Vertex data: 6 vertices (Pos2_Clip = 8 bytes each)
  ingest.ensureBuffer(10);
  float verts[] = {
    0.0f, 0.0f,   // vertex 0: origin
    1.0f, 1.0f,   // vertex 1: inside box
    2.0f, 2.0f,   // vertex 2: inside box
    5.0f, 5.0f,   // vertex 3: outside box
    1.5f, 0.5f,   // vertex 4: inside box
    -1.0f, -1.0f, // vertex 5: outside box
  };
  ingest.setBufferData(10, reinterpret_cast<const std::uint8_t*>(verts), sizeof(verts));

  dc::BoxSelection boxSel;

  // Test 1: Lifecycle: begin -> update -> finish
  {
    check(!boxSel.isActive(), "initially not active");

    boxSel.begin(0.0, 0.0);
    check(boxSel.isActive(), "active after begin");

    boxSel.update(3.0, 3.0);
    auto rect = boxSel.currentRect();
    check(rect.x0 == 0.0 && rect.y0 == 0.0, "start corner preserved");
    check(rect.x1 == 3.0 && rect.y1 == 3.0, "end corner updated");

    auto result = boxSel.finish(3.0, 3.0, scene, ingest);
    check(!boxSel.isActive(), "not active after finish");

    // Box [0,0]-[3,3] should contain vertices 0,1,2,4 (not 3=5,5 or 5=-1,-1)
    check(result.hits.size() == 4, "4 hits in box [0,0]-[3,3]");
  }

  // Test 2: Cancel
  {
    boxSel.begin(0.0, 0.0);
    check(boxSel.isActive(), "active after begin");
    boxSel.cancel();
    check(!boxSel.isActive(), "not active after cancel");
  }

  // Test 3: Zero-area box -> no hits
  {
    boxSel.begin(1.0, 1.0);
    auto result = boxSel.finish(1.0, 1.0, scene, ingest);
    check(result.hits.empty(), "zero-area box -> no hits");
  }

  // Test 4: Inverted box (finish before start) still works
  {
    boxSel.begin(3.0, 3.0);
    auto result = boxSel.finish(0.0, 0.0, scene, ingest);
    check(result.hits.size() == 4, "inverted box works (same result)");
  }

  // Test 5: Small box that misses all
  {
    boxSel.begin(10.0, 10.0);
    auto result = boxSel.finish(11.0, 11.0, scene, ingest);
    check(result.hits.empty(), "box far away -> no hits");
  }

  // Test 6: Finish without begin returns empty
  {
    auto result = boxSel.finish(0.0, 0.0, scene, ingest);
    check(result.hits.empty(), "finish without begin -> empty");
  }

  std::printf("=== D33.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
