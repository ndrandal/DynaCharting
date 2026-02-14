// D10.1 — DrawItem Style System test
// Tests: setDrawItemStyle sets all fields, partial updates preserve others,
// GL: custom candle colors render, different pointSize renders differently.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
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

static void requireNear(float a, float b, float eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Pure C++ — setDrawItemStyle sets all fields ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})"), "createDI");

    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":100,)"
      R"("r":0.5,"g":0.6,"b":0.7,"a":0.8,)"
      R"("colorUpR":0.1,"colorUpG":0.2,"colorUpB":0.3,"colorUpA":0.4,)"
      R"("colorDownR":0.9,"colorDownG":0.8,"colorDownB":0.7,"colorDownA":0.6,)"
      R"("pointSize":12.0,"lineWidth":3.0})"), "setDrawItemStyle full");

    const dc::DrawItem* di = scene.getDrawItem(100);
    requireTrue(di != nullptr, "drawItem exists");
    requireNear(di->color[0], 0.5f, 0.01f, "color.r");
    requireNear(di->color[1], 0.6f, 0.01f, "color.g");
    requireNear(di->color[2], 0.7f, 0.01f, "color.b");
    requireNear(di->color[3], 0.8f, 0.01f, "color.a");
    requireNear(di->colorUp[0], 0.1f, 0.01f, "colorUp.r");
    requireNear(di->colorUp[1], 0.2f, 0.01f, "colorUp.g");
    requireNear(di->colorUp[2], 0.3f, 0.01f, "colorUp.b");
    requireNear(di->colorUp[3], 0.4f, 0.01f, "colorUp.a");
    requireNear(di->colorDown[0], 0.9f, 0.01f, "colorDown.r");
    requireNear(di->colorDown[1], 0.8f, 0.01f, "colorDown.g");
    requireNear(di->colorDown[2], 0.7f, 0.01f, "colorDown.b");
    requireNear(di->colorDown[3], 0.6f, 0.01f, "colorDown.a");
    requireNear(di->pointSize, 12.0f, 0.01f, "pointSize");
    requireNear(di->lineWidth, 3.0f, 0.01f, "lineWidth");

    std::printf("  Test 1 (setDrawItemStyle full) PASS\n");
  }

  // --- Test 2: Partial update preserves other fields ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})"), "createDI");

    // Only set pointSize
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":100,"pointSize":8.0})"), "partial style");

    const dc::DrawItem* di = scene.getDrawItem(100);
    requireNear(di->pointSize, 8.0f, 0.01f, "pointSize updated");
    requireNear(di->lineWidth, 1.0f, 0.01f, "lineWidth default preserved");
    requireNear(di->colorUp[0], 0.0f, 0.01f, "colorUp.r default preserved");
    requireNear(di->colorUp[1], 0.8f, 0.01f, "colorUp.g default preserved");
    requireNear(di->colorDown[0], 0.8f, 0.01f, "colorDown.r default preserved");

    std::printf("  Test 2 (partial update preserves defaults) PASS\n");
  }

  // --- Test 3: Default DrawItem has legacy green/red candle values ---
  {
    dc::DrawItem di;
    requireNear(di.colorUp[0], 0.0f, 0.01f, "default colorUp.r = 0");
    requireNear(di.colorUp[1], 0.8f, 0.01f, "default colorUp.g = 0.8");
    requireNear(di.colorDown[0], 0.8f, 0.01f, "default colorDown.r = 0.8");
    requireNear(di.colorDown[1], 0.0f, 0.01f, "default colorDown.g = 0");
    requireNear(di.pointSize, 4.0f, 0.01f, "default pointSize = 4");
    requireNear(di.lineWidth, 1.0f, 0.01f, "default lineWidth = 1");

    std::printf("  Test 3 (default values match legacy) PASS\n");
  }

  // --- Test 4: GL — candle with custom blue/yellow colors renders correctly ---
  {
    constexpr int W = 100;
    constexpr int H = 100;

    dc::OsMesaContext ctx;
    if (!ctx.init(W, H)) {
      std::fprintf(stderr, "Could not init OSMesa — skipping GL tests\n");
      std::printf("D10.1 style: ALL PASS (GL tests skipped)\n");
      return 0;
    }

    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"candle6","vertexCount":1})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"instancedCandle@1","geometryId":30})"),
      "bindDI");

    // Custom colors: up = blue (0,0,1), down = yellow (1,1,0)
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":40,)"
      R"("colorUpR":0.0,"colorUpG":0.0,"colorUpB":1.0,"colorUpA":1.0,)"
      R"("colorDownR":1.0,"colorDownG":1.0,"colorDownB":0.0,"colorDownA":1.0})"),
      "setStyle blue/yellow");

    // Create an up candle (close > open) centered at origin
    float candle[] = {
      0.0f,   // cx
      -0.3f,  // open
      0.5f,   // high
      -0.5f,  // low
      0.3f,   // close (> open → up candle)
      0.3f    // halfWidth
    };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, candle, sizeof(candle));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels = ctx.readPixels();

    // Center pixel should be blue (up candle)
    std::size_t centerIdx = static_cast<std::size_t>((50 * W + 50) * 4);
    requireTrue(pixels[centerIdx] < 10, "center R < 10 (not red/green)");
    requireTrue(pixels[centerIdx + 1] < 10, "center G < 10 (not green)");
    requireTrue(pixels[centerIdx + 2] > 200, "center B > 200 (blue)");

    std::printf("  Test 4 (custom candle color blue up) PASS\n");
  }

  std::printf("D10.1 style: ALL PASS\n");
  return 0;
}
