// D28.1 — Dashed lines (lineAA@1 with dash pattern)
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
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
  std::printf("=== D28.1 Dashed Lines ===\n");

  constexpr int W = 128;
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

  // -- Test 1: Dashed horizontal line has gaps --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // Horizontal line from (-0.9, 0) to (0.9, 0) in data space
    float line[] = {-0.9f, 0.0f, 0.9f, 0.0f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    gpuBufs.setCpuData(10, line, sizeof(line));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})"),
      "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"lineAA@1","geometryId":100})"), "bind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":0,"a":1})"), "color");
    // Set dash pattern: 16px dash, 16px gap, 4px wide line
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"lineWidth":4,"dashLength":16,"gapLength":16})"), "style");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Count colored pixels along the center row (y = H/2)
    int coloredCount = 0;
    int blackCount = 0;
    int y = H / 2;
    for (int x = 10; x < W - 10; x++) {
      int idx = (y * W + x) * 4;
      if (fb[idx + 1] > 100) { // green channel > 100
        coloredCount++;
      } else {
        blackCount++;
      }
    }

    check(coloredCount > 10, "dashed line: has colored pixels");
    check(blackCount > 10, "dashed line: has gap pixels");
    check(coloredCount < (W - 20), "dashed line: not fully solid");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup buf");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup geom");
  }

  // -- Test 2: Solid line (dashLength=0) has no gaps --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    float line[] = {-0.9f, 0.0f, 0.9f, 0.0f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":16})"), "buf2");
    gpuBufs.setCpuData(30, line, sizeof(line));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":1,"format":"rect4"})"),
      "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"lineAA@1","geometryId":200})"), "bind2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":22,"r":0,"g":1,"b":0,"a":1})"), "color2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":22,"lineWidth":4,"dashLength":0,"gapLength":0})"), "style2");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Count colored pixels along center row — should be all colored in line area
    int coloredCount = 0;
    int y = H / 2;
    for (int x = 10; x < W - 10; x++) {
      int idx = (y * W + x) * 4;
      if (fb[idx + 1] > 100) coloredCount++;
    }

    // Solid line should have more colored pixels than the dashed line
    check(coloredCount > (W - 30), "solid line: mostly colored along center");
  }

  // -- Test 3: setDrawItemStyle correctly sets dash fields --
  {
    dc::Scene s2;
    dc::ResourceRegistry r2;
    dc::CommandProcessor cp2(s2, r2);

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp2.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp2.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"dashLength":10,"gapLength":5})"), "style");

    const auto* di = s2.getDrawItem(3);
    check(di != nullptr, "drawItem exists");
    if (di) {
      check(di->dashLength == 10.0f, "dashLength = 10");
      check(di->gapLength == 5.0f, "gapLength = 5");
      check(di->cornerRadius == 0.0f, "cornerRadius default = 0");
    }
  }

  std::printf("=== D28.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
