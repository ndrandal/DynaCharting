// D28.3 — Gradient fills (triGradient@1 with per-vertex color)
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
  std::printf("=== D28.3 Gradient Fills ===\n");

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

  // -- Test 1: Full-screen triangle with per-vertex RGB colors --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // Large triangle covering the viewport
    // Pos2Color4: x, y, r, g, b, a (6 floats per vertex = 24 bytes)
    // Bottom-left: RED, bottom-right: GREEN, top-center: BLUE
    float verts[] = {
      // v0: bottom-left — red
      -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
      // v1: bottom-right — green
       1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
      // v2: top-center — blue
       0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
    };

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":72})"), "buf");
    gpuBufs.setCpuData(10, verts, sizeof(verts));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_color4"})"),
      "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triGradient@1","geometryId":100})"), "bind");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Bottom-left corner (~pixel 5, 5) should be mostly red
    {
      int idx = (5 * W + 5) * 4;
      check(fb[idx] > 150, "gradient: bottom-left has strong red");
      check(fb[idx+1] < 100, "gradient: bottom-left has weak green");
      check(fb[idx+2] < 100, "gradient: bottom-left has weak blue");
    }

    // Bottom-right corner (~pixel 58, 5) should be mostly green
    {
      int idx = (5 * W + 58) * 4;
      check(fb[idx] < 100, "gradient: bottom-right has weak red");
      check(fb[idx+1] > 150, "gradient: bottom-right has strong green");
      check(fb[idx+2] < 100, "gradient: bottom-right has weak blue");
    }

    // Center pixel should be a blend of all three (roughly 85 each for RGB)
    {
      int idx = (H/3 * W + W/2) * 4;  // roughly centroid
      check(fb[idx] > 30 && fb[idx] < 200, "gradient: center has mixed red");
      check(fb[idx+1] > 30 && fb[idx+1] < 200, "gradient: center has mixed green");
      check(fb[idx+2] > 10, "gradient: center has some blue");
    }

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup buf");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup geom");
  }

  // -- Test 2: Uniform color triangle (all vertices same color) --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    // All vertices same yellow color
    float verts[] = {
      -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
       3.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
      -1.0f,  3.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    };

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":72})"), "buf2");
    gpuBufs.setCpuData(30, verts, sizeof(verts));

    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":3,"format":"pos2_color4"})"),
      "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"triGradient@1","geometryId":200})"), "bind2");

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    auto fb = ctx.readPixels();

    // Entire viewport should be yellow (255, 255, 0)
    int idx = (H/2 * W + W/2) * 4;
    check(fb[idx] > 240, "uniform gradient: center red=255");
    check(fb[idx+1] > 240, "uniform gradient: center green=255");
    check(fb[idx+2] < 15, "uniform gradient: center blue=0");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":20})"), "cleanup2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":30})"), "cleanup buf2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":200})"), "cleanup geom2");
  }

  // -- Test 3: Pipeline validation (pos2_color4 format) --
  {
    dc::Scene s2;
    dc::ResourceRegistry r2;
    dc::CommandProcessor cp2(s2, r2);

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp2.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp2.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
    requireOk(cp2.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "buf");
    requireOk(cp2.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_color4"})"),
      "geom");

    // Binding to triGradient@1 with pos2_color4 should succeed
    auto ok = cp2.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triGradient@1","geometryId":100})");
    check(ok.ok, "triGradient@1 with pos2_color4: bind OK");

    // Binding to triSolid@1 with pos2_color4 should fail (format mismatch)
    auto bad = cp2.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})");
    check(!bad.ok, "triSolid@1 with pos2_color4: rejected (format mismatch)");
  }

  // -- Test 4: Pos2Color4 vertex format properties --
  {
    check(dc::strideOf(dc::VertexFormat::Pos2Color4) == 24, "Pos2Color4 stride = 24 bytes");
    check(std::string(dc::toString(dc::VertexFormat::Pos2Color4)) == "pos2_color4",
          "Pos2Color4 toString = pos2_color4");
  }

  std::printf("=== D28.3 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
