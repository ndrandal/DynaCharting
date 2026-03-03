// D29.3 — GPU picking (color-ID offscreen render pass)
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>

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
  std::printf("=== D29.3 GPU Picking ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("OSMesa not available — skipping\n");
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

  // -- Test 1: Pick a triangle, verify correct DrawItem ID --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di");

    // Full-screen triangle
    float tri[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf");
    gpuBufs.setCpuData(10, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})"), "bind");

    gpuBufs.uploadDirty();

    // Pick center of viewport
    auto result = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    check(result.drawItemId == 5, "pick center: drawItemId = 5");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup");
  }

  // -- Test 2: Pick background, verify ID = 0 --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    // Small triangle in center only
    float tri[] = {-0.2f, -0.2f, 0.2f, -0.2f, 0.0f, 0.2f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":24})"), "buf2");
    gpuBufs.setCpuData(30, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":3,"format":"pos2_clip"})"), "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"triSolid@1","geometryId":200})"), "bind2");

    gpuBufs.uploadDirty();

    // Pick corner (background)
    auto result = renderer.renderPick(scene, gpuBufs, W, H, 2, 2);
    check(result.drawItemId == 0, "pick background: drawItemId = 0");

    // Pick center (should find the triangle)
    auto result2 = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    check(result2.drawItemId == 22, "pick small tri center: drawItemId = 22");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":20})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":30})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":200})"), "cleanup");
  }

  // -- Test 3: Overlapping items, topmost wins --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":40,"name":"P3"})"), "pane3");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":41,"paneId":40})"), "layer3");

    // Bottom triangle (drawn first) — full screen, ID=50
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":41})"), "diBottom");
    float big[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":51,"byteLength":24})"), "bufBot");
    gpuBufs.setCpuData(51, big, sizeof(big));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":500,"vertexBufferId":51,"vertexCount":3,"format":"pos2_clip"})"), "geomBot");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triSolid@1","geometryId":500})"), "bindBot");

    // Top triangle (drawn second) — full screen, ID=60
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":60,"layerId":41})"), "diTop");
    float big2[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":61,"byteLength":24})"), "bufTop");
    gpuBufs.setCpuData(61, big2, sizeof(big2));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":600,"vertexBufferId":61,"vertexCount":3,"format":"pos2_clip"})"), "geomTop");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":60,"pipeline":"triSolid@1","geometryId":600})"), "bindTop");

    gpuBufs.uploadDirty();

    // Pick center — topmost (last drawn) should win
    auto result = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    check(result.drawItemId == 60, "overlapping: topmost drawItemId = 60");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":40})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":51})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":61})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":500})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":600})"), "cleanup");
  }

  // -- Test 4: Pick instancedRect --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":70,"name":"P4"})"), "pane4");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":71,"paneId":70})"), "layer4");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":72,"layerId":71})"), "di4");

    float rect[] = {-0.5f, -0.5f, 0.5f, 0.5f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":73,"byteLength":16})"), "buf4");
    gpuBufs.setCpuData(73, rect, sizeof(rect));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":700,"vertexBufferId":73,"vertexCount":1,"format":"rect4"})"), "geom4");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":72,"pipeline":"instancedRect@1","geometryId":700})"), "bind4");

    gpuBufs.uploadDirty();

    auto result = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    check(result.drawItemId == 72, "pick instRect center: drawItemId = 72");

    // Pick corner (outside rect)
    auto result2 = renderer.renderPick(scene, gpuBufs, W, H, 2, 2);
    check(result2.drawItemId == 0, "pick instRect corner: background");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":70})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":73})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":700})"), "cleanup");
  }

  // -- Test 5: PickResult struct defaults --
  {
    dc::PickResult pr;
    check(pr.drawItemId == 0, "PickResult default drawItemId = 0");
  }

  std::printf("=== D29.3 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
