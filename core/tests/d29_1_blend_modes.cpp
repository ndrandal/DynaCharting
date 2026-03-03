// D29.1 — Blend modes (per-DrawItem GL blend switching)
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
  std::printf("=== D29.1 Blend Modes ===\n");

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

  // -- Test 1: Additive blend (two overlapping semi-transparent triangles) --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // Triangle 1: covers full viewport, red, 0.5 alpha
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di1");
    float tri1[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf1");
    gpuBufs.setCpuData(10, tri1, sizeof(tri1));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom1");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"), "bind1");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":0.5})"), "color1");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"blendMode":"additive"})"), "style1");

    // Triangle 2: covers full viewport, green, 0.5 alpha, additive
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "di2");
    float tri2[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "buf2");
    gpuBufs.setCpuData(11, tri2, sizeof(tri2));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"), "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"triSolid@1","geometryId":101})"), "bind2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":0.5})"), "color2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":4,"blendMode":"additive"})"), "style2");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // With additive blend: both triangles add their color
    // First tri (red 0.5): 0 + 0.5*255 = 127 on red channel
    // Second tri (green 0.5): adds 0.5*255 = 127 on green channel
    // Result: ~127 red, ~127 green, 0 blue
    int idx = (H / 2 * W + W / 2) * 4;
    check(fb[idx] > 90 && fb[idx] < 200, "additive: red channel has contribution");
    check(fb[idx + 1] > 90 && fb[idx + 1] < 200, "additive: green channel has contribution");
    check(fb[idx + 2] < 30, "additive: blue channel is low");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":11})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":101})"), "cleanup");
  }

  // -- Test 2: Normal blend (second triangle occludes first) --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");

    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":30,"layerId":21})"), "di1");
    float tri[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":40,"byteLength":24})"), "buf");
    gpuBufs.setCpuData(40, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":300,"vertexBufferId":40,"vertexCount":3,"format":"pos2_clip"})"), "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":30,"pipeline":"triSolid@1","geometryId":300})"), "bind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":30,"r":0,"g":0,"b":1,"a":1})"), "color");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":30,"blendMode":"normal"})"), "style");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    int idx = (H / 2 * W + W / 2) * 4;
    check(fb[idx] < 15, "normal: red channel is 0");
    check(fb[idx + 1] < 15, "normal: green channel is 0");
    check(fb[idx + 2] > 240, "normal: blue channel is 255");
  }

  // -- Test 3: Command parsing roundtrip --
  {
    dc::Scene s2;
    dc::ResourceRegistry r2;
    dc::CommandProcessor cp2(s2, r2);

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp2.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp2.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"blendMode":"multiply"})"), "style");

    const auto* di = s2.getDrawItem(3);
    check(di != nullptr, "drawItem exists");
    if (di) {
      check(di->blendMode == dc::BlendMode::Multiply, "blendMode = Multiply");
    }

    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"blendMode":"screen"})"), "style2");
    di = s2.getDrawItem(3);
    if (di) {
      check(di->blendMode == dc::BlendMode::Screen, "blendMode = Screen");
    }

    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"blendMode":"additive"})"), "style3");
    di = s2.getDrawItem(3);
    if (di) {
      check(di->blendMode == dc::BlendMode::Additive, "blendMode = Additive");
    }
  }

  std::printf("=== D29.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
