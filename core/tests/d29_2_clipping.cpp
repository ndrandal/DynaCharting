// D29.2 — Clipping masks (stencil-based geometric clipping)
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
  std::printf("=== D29.2 Clipping Masks ===\n");

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

  // -- Test 1: Clip source masks a full-screen triangle --
  // Clip source: small triangle in center
  // Masked item: full-screen green triangle
  // Result: green only in the clip region, black elsewhere
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // Clip source: small triangle covering center region
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "clip_di");
    float clipTri[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "clipBuf");
    gpuBufs.setCpuData(10, clipTri, sizeof(clipTri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "clipGeom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"), "clipBind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"isClipSource":true})"), "clipStyle");

    // Masked item: full-screen green triangle
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "masked_di");
    float bigTri[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "bigBuf");
    gpuBufs.setCpuData(11, bigTri, sizeof(bigTri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"), "bigGeom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"triSolid@1","geometryId":101})"), "bigBind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":1})"), "bigColor");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":4,"useClipMask":true})"), "maskStyle");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Center of viewport should be green (inside clip triangle)
    {
      int idx = (H / 2 * W + W / 2) * 4;
      check(fb[idx] < 30, "clipped center: red is low");
      check(fb[idx + 1] > 200, "clipped center: green is high");
      check(fb[idx + 2] < 30, "clipped center: blue is low");
    }

    // Corner should be black (outside clip triangle)
    {
      int idx = (2 * W + 2) * 4;
      check(fb[idx] < 15, "clipped corner: red is 0");
      check(fb[idx + 1] < 15, "clipped corner: green is 0");
      check(fb[idx + 2] < 15, "clipped corner: blue is 0");
    }

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":11})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":101})"), "cleanup");
  }

  // -- Test 2: Without clip mask, full-screen triangle renders everywhere --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");

    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":30,"layerId":21})"), "di");
    float tri[] = {-1,-1, 3,-1, -1,3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":40,"byteLength":24})"), "buf");
    gpuBufs.setCpuData(40, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":300,"vertexBufferId":40,"vertexCount":3,"format":"pos2_clip"})"), "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":30,"pipeline":"triSolid@1","geometryId":300})"), "bind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":30,"r":1,"g":0,"b":0,"a":1})"), "color");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Corner should be red (no clipping)
    int idx = (2 * W + 2) * 4;
    check(fb[idx] > 240, "unclipped corner: red is 255");
    check(fb[idx + 1] < 15, "unclipped corner: green is 0");
  }

  // -- Test 3: Command parsing for clip fields --
  {
    dc::Scene s2;
    dc::ResourceRegistry r2;
    dc::CommandProcessor cp2(s2, r2);

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp2.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp2.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // Default values
    const auto* di = s2.getDrawItem(3);
    check(di && !di->isClipSource, "default isClipSource = false");
    check(di && !di->useClipMask, "default useClipMask = false");

    // Set clip source
    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"isClipSource":true})"), "clipStyle");
    di = s2.getDrawItem(3);
    check(di && di->isClipSource, "isClipSource = true");
    check(di && !di->useClipMask, "useClipMask still false");

    // Set use clip mask (and unset clip source)
    requireOk(cp2.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"isClipSource":false,"useClipMask":true})"), "maskStyle");
    di = s2.getDrawItem(3);
    check(di && !di->isClipSource, "isClipSource = false");
    check(di && di->useClipMask, "useClipMask = true");
  }

  std::printf("=== D29.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
