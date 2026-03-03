// D56.2 — BatchBuilder GL: verify batched scene produces correct render output
#include "dc/gl/BatchBuilder.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdio>
#include <cstdint>
#include <vector>

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

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
  }
}

int main() {
  std::printf("=== D56.2 BatchBuilder GL Tests ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("SKIP: OSMesa not available\n");
    return 0;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  if (!renderer.init()) {
    std::fprintf(stderr, "Renderer init failed\n");
    return 1;
  }

  // Build scene: 2 overlapping triangles (red + green), same pipeline
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // Triangle 1: covers full viewport, red
  float tri1[] = {-1, -1, 3, -1, -1, 3};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf1");
  gpuBufs.setCpuData(10, tri1, sizeof(tri1));
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom1");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":2})"), "di1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triSolid@1","geometryId":100})"), "bind1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":50,"r":1,"g":0,"b":0,"a":1})"), "color1");

  // Triangle 2: covers full viewport, green
  float tri2[] = {-1, -1, 3, -1, -1, 3};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "buf2");
  gpuBufs.setCpuData(11, tri2, sizeof(tri2));
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"), "geom2");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":51,"layerId":2})"), "di2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":51,"pipeline":"triSolid@1","geometryId":101})"), "bind2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":51,"r":0,"g":1,"b":0,"a":1})"), "color2");

  gpuBufs.uploadDirty();

  // Test 1: Verify batching produces 1 batch for 2 same-pipeline items
  dc::BatchBuilder builder;
  dc::BatchedFrame frame = builder.build(scene);
  check(frame.panes.size() == 1, "1 pane");
  check(frame.panes[0].batches.size() == 1, "1 batch for 2 same-pipeline items");
  check(frame.panes[0].batches[0].items.size() == 2, "batch has 2 items");

  // Test 2: Render and verify last draw wins (green on top)
  renderer.render(scene, gpuBufs, W, H);
  auto fb = ctx.readPixels();

  int midIdx = (H / 2 * W + W / 2) * 4;
  // Green triangle is drawn second (id=51 > id=50), so green wins
  check(fb[midIdx] < 15, "red channel is 0 (green covers red)");
  check(fb[midIdx + 1] > 240, "green channel is 255");
  check(fb[midIdx + 2] < 15, "blue channel is 0");

  // Test 3: Batch count matches expected state changes
  // Add a draw item with different blend mode
  float tri3[] = {-1, -1, 3, -1, -1, 3};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":12,"byteLength":24})"), "buf3");
  gpuBufs.setCpuData(12, tri3, sizeof(tri3));
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":102,"vertexBufferId":12,"vertexCount":3,"format":"pos2_clip"})"), "geom3");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":52,"layerId":2})"), "di3");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":52,"pipeline":"triSolid@1","geometryId":102})"), "bind3");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":52,"r":0,"g":0,"b":1,"a":0.5})"), "color3");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemStyle","drawItemId":52,"blendMode":"additive"})"), "style3");

  frame = builder.build(scene);
  check(frame.panes[0].batches.size() == 2, "blend mode change creates new batch");

  // Test 4: Verify render still works correctly
  gpuBufs.uploadDirty();
  renderer.render(scene, gpuBufs, W, H);
  auto fb2 = ctx.readPixels();
  // Blue additive on top of green should produce cyan-ish
  int midIdx2 = (H / 2 * W + W / 2) * 4;
  check(fb2[midIdx2 + 1] > 100, "green channel present");
  check(fb2[midIdx2 + 2] > 50, "blue channel has contribution (additive)");

  std::printf("=== D56.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
