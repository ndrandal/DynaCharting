// D10.6 — Anti-Aliased Lines (lineAA@1 pipeline) test (OSMesa)
// Tests: lineAA@1 renders, edge pixels have intermediate values (AA),
// AA line covers more pixels than line2d@1.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static int countNonBlack(const std::vector<std::uint8_t>& pixels, int W, int H) {
  int count = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (pixels[idx] > 5 || pixels[idx + 1] > 5 || pixels[idx + 2] > 5)
        count++;
    }
  }
  return count;
}

static int countIntermediateAlpha(const std::vector<std::uint8_t>& pixels, int W, int H) {
  // Count pixels where any color channel is between 10 and 240 (intermediate = AA edge)
  int count = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      std::uint8_t maxC = std::max({static_cast<std::uint8_t>(pixels[idx]),
                                    static_cast<std::uint8_t>(pixels[idx+1]),
                                    static_cast<std::uint8_t>(pixels[idx+2])});
      if (maxC > 10 && maxC < 240)
        count++;
    }
  }
  return count;
}

int main() {
  constexpr int W = 200;
  constexpr int H = 200;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  // --- Test 1: lineAA@1 renders a diagonal line with AA ---
  int aaPixelCount = 0;
  int aaIntermediateCount = 0;
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"rect4","vertexCount":1})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"lineAA@1","geometryId":30})"),
      "bindDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":40,"r":1.0,"g":1.0,"b":1.0,"a":1.0})"),
      "setColor white");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":40,"lineWidth":4.0})"),
      "setLineWidth");

    // Diagonal line segment from bottom-left to top-right (clip coords)
    float seg[] = { -0.8f, -0.8f, 0.8f, 0.8f };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, seg, sizeof(seg));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    auto stats = renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();
    requireTrue(stats.drawCalls == 1, "1 draw call for lineAA");

    auto pixels = ctx.readPixels();
    aaPixelCount = countNonBlack(pixels, W, H);
    aaIntermediateCount = countIntermediateAlpha(pixels, W, H);

    std::printf("  AA line: non-black=%d, intermediate=%d\n", aaPixelCount, aaIntermediateCount);
    requireTrue(aaPixelCount > 100, "AA line renders visible pixels");
    requireTrue(aaIntermediateCount > 0, "AA line has intermediate (antialiased) edge pixels");

    std::printf("  Test 1 (lineAA@1 renders with AA edges) PASS\n");
  }

  // --- Test 2: Compare with line2d@1 — AA version covers more pixels ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":2})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"line2d@1","geometryId":30})"),
      "bindDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":40,"r":1.0,"g":1.0,"b":1.0,"a":1.0})"),
      "setColor white");

    // Same diagonal line but as two vertices for line2d
    float verts[] = { -0.8f, -0.8f, 0.8f, 0.8f };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();
    int linePixelCount = countNonBlack(pixels, W, H);

    std::printf("  line2d: non-black=%d, lineAA: non-black=%d\n", linePixelCount, aaPixelCount);
    requireTrue(aaPixelCount > linePixelCount, "AA line covers more pixels (quad expansion)");

    std::printf("  Test 2 (AA covers more pixels than line2d) PASS\n");
  }

  std::printf("D10.6 aa_lines: ALL PASS\n");
  return 0;
}
