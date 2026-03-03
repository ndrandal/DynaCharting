// D33.2 — BoxSelection GL: render scattered points, box-select subset
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/selection/BoxSelection.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

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
  std::printf("=== D33.2 BoxSelection GL Tests ===\n");

  dc::OsMesaContext ctx;
  if (!ctx.init(64, 64)) {
    std::fprintf(stderr, "SKIP: OSMesa init failed\n");
    return 0;
  }

  dc::Renderer renderer;
  check(renderer.init(), "renderer init");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  dc::GpuBufferManager gpuBufs;
  cp.setIngestProcessor(&ingest);

  // Setup scene with points
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "createBuffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"pos2_clip"})"),
    "createGeometry");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"points@1","geometryId":100})"),
    "bindDrawItem");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})"),
    "setColor");

  // 4 points in clip space
  ingest.ensureBuffer(10);
  float pts[] = {
    -0.5f, -0.5f,  // point 0: bottom-left quadrant
     0.5f,  0.5f,  // point 1: top-right quadrant
    -0.5f,  0.5f,  // point 2: top-left quadrant
     0.5f, -0.5f,  // point 3: bottom-right quadrant
  };
  ingest.setBufferData(10, reinterpret_cast<const std::uint8_t*>(pts), sizeof(pts));
  gpuBufs.setCpuData(10, ingest.getBufferData(10), ingest.getBufferSize(10));
  gpuBufs.uploadDirty();

  // Render to verify scene is valid
  auto stats = renderer.render(scene, gpuBufs, 64, 64);
  check(stats.drawCalls > 0, "rendered points");

  // Box-select top-right quadrant (data coords = clip coords here)
  dc::BoxSelection boxSel;
  boxSel.begin(0.0, 0.0);
  auto result = boxSel.finish(1.0, 1.0, scene, ingest);

  check(result.hits.size() == 1, "box [0,0]-[1,1] selects 1 point");
  if (!result.hits.empty()) {
    check(result.hits[0].recordIndex == 1, "selected point is index 1 (0.5, 0.5)");
  }

  // Box-select entire viewport
  boxSel.begin(-1.0, -1.0);
  auto result2 = boxSel.finish(1.0, 1.0, scene, ingest);
  check(result2.hits.size() == 4, "box [-1,-1]-[1,1] selects all 4 points");

  std::printf("=== D33.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
