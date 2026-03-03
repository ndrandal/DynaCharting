// D28.2 — Rounded rectangles (instancedRect@1 with cornerRadius)
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

static bool pixelIs(const std::vector<std::uint8_t>& fb, int w,
                     int x, int y, std::uint8_t r, std::uint8_t g,
                     std::uint8_t b, int tolerance = 15) {
  int idx = (y * w + x) * 4;
  return std::abs(fb[idx] - r) <= tolerance &&
         std::abs(fb[idx+1] - g) <= tolerance &&
         std::abs(fb[idx+2] - b) <= tolerance;
}

int main() {
  std::printf("=== D28.2 Rounded Rectangles ===\n");

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

  // -- Test 1: Rounded rect has transparent corners --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // Large rect covering most of viewport
    float rect[] = {-0.8f, -0.8f, 0.8f, 0.8f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    gpuBufs.setCpuData(10, rect, sizeof(rect));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})"),
      "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})"), "bind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})"), "color");
    // Large corner radius
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":15})"), "style");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Center should be red
    check(pixelIs(fb, W, W/2, H/2, 255, 0, 0), "rounded rect: center is red");

    // Extreme corner (2,2) should be black (rounded off)
    // The rect spans from about pixel 6 to 57 (0.8 * 32 = 25.6 offset from center)
    // Corner radius = 15px, so ~15px from each corner is rounded
    check(pixelIs(fb, W, 7, 7, 0, 0, 0, 20), "rounded rect: bottom-left corner is black");
    check(pixelIs(fb, W, 56, 56, 0, 0, 0, 20), "rounded rect: top-right corner is black");

    // Mid-edge should still be red (halfway along bottom edge)
    check(pixelIs(fb, W, W/2, 8, 255, 0, 0, 30), "rounded rect: bottom mid-edge is red");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup buf");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup geom");
  }

  // -- Test 2: Sharp corners (cornerRadius=0) fills entire rect --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    float rect[] = {-0.8f, -0.8f, 0.8f, 0.8f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":16})"), "buf2");
    gpuBufs.setCpuData(30, rect, sizeof(rect));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":1,"format":"rect4"})"),
      "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"instancedRect@1","geometryId":200})"), "bind2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":22,"r":0,"g":0,"b":1,"a":1})"), "color2");
    // No corner radius
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":22,"cornerRadius":0})"), "style2");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Corner should be filled (blue), not rounded
    check(pixelIs(fb, W, 8, 8, 0, 0, 255), "sharp rect: bottom-left corner is blue");
    check(pixelIs(fb, W, 55, 55, 0, 0, 255), "sharp rect: top-right corner is blue");
    check(pixelIs(fb, W, W/2, H/2, 0, 0, 255), "sharp rect: center is blue");
  }

  // -- Test 3: setDrawItemStyle sets cornerRadius field --
  {
    dc::Scene s2;
    dc::ResourceRegistry r2;
    dc::CommandProcessor cp2(s2, r2);

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp2.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp2.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":8.5})"), "style");

    const auto* di = s2.getDrawItem(3);
    check(di != nullptr, "drawItem exists");
    if (di) {
      check(std::fabs(di->cornerRadius - 8.5f) < 0.01f, "cornerRadius = 8.5");
    }
  }

  std::printf("=== D28.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
