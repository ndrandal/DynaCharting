// D26.2 — Indexed GL rendering (non-instanced + instanced)
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
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
                     std::uint8_t b, int tolerance = 10) {
  int idx = (y * w + x) * 4;
  return std::abs(fb[idx] - r) <= tolerance &&
         std::abs(fb[idx+1] - g) <= tolerance &&
         std::abs(fb[idx+2] - b) <= tolerance;
}

int main() {
  std::printf("=== D26.2 Indexed GL Rendering ===\n");

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

  // -- Test 1: Non-instanced triSolid@1 with index buffer --
  // Left quad (2 tris) + right quad (2 tris), index selects only left quad
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // 4 triangles = 12 vertices (2 floats each = 96 bytes)
    // Left quad: tri0 + tri1 cover [-1,-1] to [0,1]
    // Right quad: tri2 + tri3 cover [0,-1] to [1,1]
    float verts[] = {
      // Tri0: left-quad bottom-left triangle
      -1.0f, -1.0f,  0.0f, -1.0f,  -1.0f, 1.0f,
      // Tri1: left-quad top-right triangle
      -1.0f, 1.0f,   0.0f, -1.0f,   0.0f, 1.0f,
      // Tri2: right-quad bottom-left triangle
       0.0f, -1.0f,  1.0f, -1.0f,   0.0f, 1.0f,
      // Tri3: right-quad top-right triangle
       0.0f, 1.0f,   1.0f, -1.0f,   1.0f, 1.0f,
    };

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":96})"), "vbuf");
    gpuBufs.setCpuData(10, verts, sizeof(verts));

    // Index buffer: select only left quad (indices 0..5)
    std::uint32_t indices[] = {0, 1, 2, 3, 4, 5};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "ibuf");
    gpuBufs.setCpuData(11, indices, sizeof(indices));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":12,"format":"pos2_clip","indexBufferId":11,"indexCount":6})"),
      "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"), "bind");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":0,"a":1})"), "color");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);

    auto fb = ctx.readPixels();

    // Left half should be green, right half should be black (not selected)
    check(pixelIs(fb, W, W/4, H/2, 0, 255, 0), "triSolid indexed: left half green");
    check(pixelIs(fb, W, 3*W/4, H/2, 0, 0, 0, 5), "triSolid indexed: right half black (filtered)");

    // Clean up for next test
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup pane");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup vbuf");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":11})"), "cleanup ibuf");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup geom");
  }

  // -- Test 2: Instanced instancedRect@1 with index buffer --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    // 4 rects: each covers a quadrant
    float rects[] = {
      // Rect0: left half, bottom half  (x0, y0, x1, y1)
      -1.0f, -1.0f, 0.0f, 0.0f,
      // Rect1: right half, bottom half
       0.0f, -1.0f, 1.0f, 0.0f,
      // Rect2: left half, top half
      -1.0f, 0.0f, 0.0f, 1.0f,
      // Rect3: right half, top half
       0.0f, 0.0f, 1.0f, 1.0f,
    };

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":64})"), "vbuf2");
    gpuBufs.setCpuData(30, rects, sizeof(rects));

    // Index buffer: select only rect0 and rect3 (diagonal pair)
    std::uint32_t indices[] = {0, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":31,"byteLength":8})"), "ibuf2");
    gpuBufs.setCpuData(31, indices, sizeof(indices));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":4,"format":"rect4","indexBufferId":31,"indexCount":2})"),
      "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"instancedRect@1","geometryId":200})"), "bind2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":22,"r":1,"g":0,"b":0,"a":1})"), "color2");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);

    auto fb = ctx.readPixels();

    // Rect0 = bottom-left quadrant should be red
    check(pixelIs(fb, W, W/4, H/4, 255, 0, 0), "instRect indexed: bottom-left red");
    // Rect3 = top-right quadrant should be red
    check(pixelIs(fb, W, 3*W/4, 3*H/4, 255, 0, 0), "instRect indexed: top-right red");
    // Rect1 = bottom-right quadrant should be black (not selected)
    check(pixelIs(fb, W, 3*W/4, H/4, 0, 0, 0, 5), "instRect indexed: bottom-right black (filtered)");
    // Rect2 = top-left quadrant should be black (not selected)
    check(pixelIs(fb, W, W/4, 3*H/4, 0, 0, 0, 5), "instRect indexed: top-left black (filtered)");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":20})"), "cleanup pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":30})"), "cleanup vbuf2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":31})"), "cleanup ibuf2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":200})"), "cleanup geom2");
  }

  // -- Test 3: No index buffer = existing behavior (regression guard) --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":40,"name":"P3"})"), "pane3");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":41,"paneId":40})"), "layer3");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":42,"layerId":41})"), "di3");

    // Full-screen triangle
    float verts[] = {
      -1.0f, -1.0f,
       3.0f, -1.0f,
      -1.0f,  3.0f,
    };

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":50,"byteLength":24})"), "vbuf3");
    gpuBufs.setCpuData(50, verts, sizeof(verts));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":300,"vertexBufferId":50,"vertexCount":3,"format":"pos2_clip"})"),
      "geom3");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"triSolid@1","geometryId":300})"), "bind3");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":42,"r":0,"g":0,"b":1,"a":1})"), "color3");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);

    auto fb = ctx.readPixels();

    // Entire viewport should be blue (no index buffer, draws all vertices)
    check(pixelIs(fb, W, W/2, H/2, 0, 0, 255), "no-index: center blue");
    check(pixelIs(fb, W, 1, 1, 0, 0, 255), "no-index: corner blue");
  }

  std::printf("=== D26.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
