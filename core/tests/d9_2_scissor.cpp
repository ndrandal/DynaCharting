// D9.2 — GL scissor per pane test (OSMesa)
// Tests: two panes with colored triangles, scissor isolation, backward compat.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

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

static int countNonBlackInRegion(const std::vector<std::uint8_t>& pixels,
                                  int W, int H, int yMin, int yMax) {
  int count = 0;
  for (int y = yMin; y < yMax; y++) {
    for (int x = 0; x < W; x++) {
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (pixels[idx] > 5 || pixels[idx + 1] > 5 || pixels[idx + 2] > 5)
        count++;
    }
  }
  return count;
}

int main() {
  constexpr int W = 100;
  constexpr int H = 100;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  // --- Test 1: Two panes, top and bottom half, each with a colored triangle ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    // Top pane (upper half: clipY 0 to 1)
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Top"})"), "createPane top");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.0,"clipYMax":1.0,"clipXMin":-1.0,"clipXMax":1.0})"),
      "setPaneRegion top");

    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer top");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "createBuffer top");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":3})"),
      "createGeometry top");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI top");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"triSolid@1","geometryId":30})"),
      "bindDI top");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":40,"r":1.0,"g":0.0,"b":0.0,"a":1.0})"),
      "setColor top red");

    // Bottom pane (lower half: clipY -1 to 0)
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Bottom"})"), "createPane bottom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":2,"clipYMin":-1.0,"clipYMax":0.0,"clipXMin":-1.0,"clipXMax":1.0})"),
      "setPaneRegion bottom");

    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":2})"), "createLayer bottom");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":21,"byteLength":24})"), "createBuffer bottom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":31,"vertexBufferId":21,"format":"pos2_clip","vertexCount":3})"),
      "createGeometry bottom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":41,"layerId":11})"), "createDI bottom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":41,"pipeline":"triSolid@1","geometryId":31})"),
      "bindDI bottom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":41,"r":0.0,"g":0.0,"b":1.0,"a":1.0})"),
      "setColor bottom blue");

    // Full-screen triangle for both (covers the whole clip space)
    float verts[] = {
      -1.0f, -1.0f,
       1.0f, -1.0f,
       0.0f,  1.0f
    };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));
    gpuBufs.setCpuData(21, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();

    // Top half = pixels y=50..99 (OpenGL origin bottom-left)
    int topNonBlack = countNonBlackInRegion(pixels, W, H, 50, 100);
    // Bottom half = pixels y=0..49
    int bottomNonBlack = countNonBlackInRegion(pixels, W, H, 0, 50);

    std::printf("  Top non-black: %d, Bottom non-black: %d\n", topNonBlack, bottomNonBlack);
    requireTrue(topNonBlack > 100, "top pane has rendered content");
    requireTrue(bottomNonBlack > 100, "bottom pane has rendered content");

    // Check top has red, bottom has blue
    // Sample middle of top half (x=50, y=75)
    std::size_t topIdx = static_cast<std::size_t>((75 * W + 50) * 4);
    requireTrue(pixels[topIdx] > 200, "top pane red channel");
    requireTrue(pixels[topIdx + 2] < 10, "top pane no blue");

    // Sample middle of bottom half (x=50, y=25)
    std::size_t botIdx = static_cast<std::size_t>((25 * W + 50) * 4);
    requireTrue(pixels[botIdx + 2] > 200, "bottom pane blue channel");
    requireTrue(pixels[botIdx] < 10, "bottom pane no red");

    std::printf("  Test 1 (two panes scissor isolation) PASS\n");
  }

  // --- Test 2: Single pane with default region = backward compat ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Full"})"), "createPane full");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer full");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "createBuffer full");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":3})"),
      "createGeometry full");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI full");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"triSolid@1","geometryId":30})"),
      "bindDI full");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":40,"r":0.0,"g":1.0,"b":0.0,"a":1.0})"),
      "setColor green");

    float verts[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    auto stats = renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();
    std::size_t centerIdx = static_cast<std::size_t>((50 * W + 50) * 4);
    requireTrue(pixels[centerIdx + 1] > 200, "center green channel");
    requireTrue(stats.drawCalls == 1, "1 draw call");

    std::printf("  Test 2 (default region backward compat) PASS\n");
  }

  // --- Test 3: Draw item extending beyond pane bounds is clipped ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    // Small pane in center
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Small"})"), "createPane small");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":-0.3,"clipYMax":0.3,"clipXMin":-0.3,"clipXMax":0.3})"),
      "setPaneRegion small");

    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer small");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "createBuffer small");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":3})"),
      "createGeometry small");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI small");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"triSolid@1","geometryId":30})"),
      "bindDI small");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":40,"r":1.0,"g":1.0,"b":1.0,"a":1.0})"),
      "setColor white");

    // Large triangle that extends beyond the pane
    float verts[] = { -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 1.0f };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();

    // Corner (0,0) should be black — outside the pane region
    std::size_t cornerIdx = 0;
    requireTrue(pixels[cornerIdx] < 5 && pixels[cornerIdx + 1] < 5 && pixels[cornerIdx + 2] < 5,
                "corner is black (clipped)");

    // Center (50,50) should be white — inside the pane
    std::size_t centerIdx = static_cast<std::size_t>((50 * W + 50) * 4);
    requireTrue(pixels[centerIdx] > 200 && pixels[centerIdx + 1] > 200 && pixels[centerIdx + 2] > 200,
                "center is white (inside pane)");

    std::printf("  Test 3 (draw item clipped at pane boundary) PASS\n");
  }

  std::printf("D9.2 scissor: ALL PASS\n");
  return 0;
}
