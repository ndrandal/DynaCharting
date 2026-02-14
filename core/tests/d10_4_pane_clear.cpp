// D10.4 — Per-Pane Clear Color test (OSMesa)
// Tests: two panes with different clear colors, default pane stays black.

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

int main() {
  constexpr int W = 100;
  constexpr int H = 100;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  // --- Test 1: Two panes with different clear colors ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    // Top pane: dark blue clear color
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Top"})"), "createPane top");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.0,"clipYMax":1.0,"clipXMin":-1.0,"clipXMax":1.0})"),
      "setPaneRegion top");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneClearColor","id":1,"r":0.0,"g":0.0,"b":0.5,"a":1.0})"),
      "setPaneClearColor top blue");

    // Bottom pane: dark gray clear color
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Bottom"})"), "createPane bottom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":2,"clipYMin":-1.0,"clipYMax":0.0,"clipXMin":-1.0,"clipXMax":1.0})"),
      "setPaneRegion bottom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setPaneClearColor","id":2,"r":0.3,"g":0.3,"b":0.3,"a":1.0})"),
      "setPaneClearColor bottom gray");

    // No draw items — just clear colors

    dc::GpuBufferManager gpuBufs;
    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");

    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();

    // Top half (y=75, center): should be dark blue
    std::size_t topIdx = static_cast<std::size_t>((75 * W + 50) * 4);
    std::printf("  Top pixel: R=%d G=%d B=%d\n", pixels[topIdx], pixels[topIdx+1], pixels[topIdx+2]);
    requireTrue(pixels[topIdx] < 10, "top R near 0");
    requireTrue(pixels[topIdx + 1] < 10, "top G near 0");
    requireTrue(pixels[topIdx + 2] > 100, "top B > 100 (dark blue)");

    // Bottom half (y=25, center): should be dark gray
    std::size_t botIdx = static_cast<std::size_t>((25 * W + 50) * 4);
    std::printf("  Bottom pixel: R=%d G=%d B=%d\n", pixels[botIdx], pixels[botIdx+1], pixels[botIdx+2]);
    requireTrue(pixels[botIdx] > 60 && pixels[botIdx] < 100, "bottom R ~76 (gray)");
    requireTrue(pixels[botIdx + 1] > 60 && pixels[botIdx + 1] < 100, "bottom G ~76 (gray)");
    requireTrue(pixels[botIdx + 2] > 60 && pixels[botIdx + 2] < 100, "bottom B ~76 (gray)");

    std::printf("  Test 1 (two panes different clear colors) PASS\n");
  }

  // --- Test 2: Default pane (no clear color) stays black ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Default"})"), "createPane default");

    dc::GpuBufferManager gpuBufs;
    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");

    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();

    // Center should be black (global clear)
    std::size_t centerIdx = static_cast<std::size_t>((50 * W + 50) * 4);
    requireTrue(pixels[centerIdx] < 5, "default pane R ~0");
    requireTrue(pixels[centerIdx + 1] < 5, "default pane G ~0");
    requireTrue(pixels[centerIdx + 2] < 5, "default pane B ~0");

    std::printf("  Test 2 (default pane stays black) PASS\n");
  }

  std::printf("D10.4 pane_clear: ALL PASS\n");
  return 0;
}
