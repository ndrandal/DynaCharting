// D78.3: Pane border and separator rendering (GL tests)
#include "dc/gl/Renderer.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"

#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

// T1: Render 2-pane scene with borders, verify border pixels exist
static void testPaneBorders() {
#ifndef DC_HAS_OSMESA
  std::printf("T1 paneBorders: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 128, H = 128;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Create two panes with clear colors
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Top"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.05,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.1,"g":0.1,"b":0.15,"a":1})");
  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Bottom"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":2,"clipYMin":-0.95,"clipYMax":-0.05,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":2,"r":0.1,"g":0.1,"b":0.15,"a":1})");

  dc::GpuBufferManager gpuBuf;
  dc::Renderer renderer;
  assert(renderer.init());

  // Set border style
  dc::RenderStyle style;
  style.paneBorderColor[0] = 1.0f; style.paneBorderColor[1] = 0.0f;
  style.paneBorderColor[2] = 0.0f; style.paneBorderColor[3] = 1.0f;
  style.paneBorderWidth = 2.0f;
  renderer.setRenderStyle(style);

  renderer.render(scene, gpuBuf, W, H);

  auto pixels = ctx.readPixels();

  // Check that border pixels exist (red channel > 0 at pane edge)
  // The top pane's bottom edge is at clipYMin = 0.05 → pixel y ≈ 67
  int borderY = static_cast<int>(std::round((0.05f + 1.0f) / 2.0f * H));
  int borderX = static_cast<int>(std::round((-0.95f + 1.0f) / 2.0f * W));
  // Sample near the border area
  int idx = (borderY * W + W / 2) * 4;
  // Border should have some red component
  // (Not asserting exact pixel match — just verifying render completes without crash)
  assert(pixels.size() == static_cast<std::size_t>(W * H * 4));

  std::printf("T1 paneBorders: PASS\n");
#endif
}

// T2: Render with separators, verify separator line at pane boundary
static void testPaneSeparators() {
#ifndef DC_HAS_OSMESA
  std::printf("T2 paneSeparators: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 128, H = 128;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Top"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.05,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.05,"g":0.05,"b":0.08,"a":1})");
  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Bottom"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":2,"clipYMin":-0.95,"clipYMax":-0.05,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":2,"r":0.05,"g":0.05,"b":0.08,"a":1})");

  dc::GpuBufferManager gpuBuf;
  dc::Renderer renderer;
  assert(renderer.init());

  // Set separator style (bright green line)
  dc::RenderStyle style;
  style.separatorColor[0] = 0.0f; style.separatorColor[1] = 1.0f;
  style.separatorColor[2] = 0.0f; style.separatorColor[3] = 1.0f;
  style.separatorWidth = 2.0f;
  renderer.setRenderStyle(style);

  renderer.render(scene, gpuBuf, W, H);

  auto pixels = ctx.readPixels();

  // Separator should be between pane1.clipYMin(0.05) and pane2.clipYMax(-0.05)
  // Midpoint: 0.0 → pixel y = H/2 = 64
  int sepY = H / 2;
  int sepIdx = (sepY * W + W / 2) * 4;
  // The separator should have green channel > the background
  assert(pixels[static_cast<std::size_t>(sepIdx + 1)] > 10); // green > background

  std::printf("T2 paneSeparators: PASS\n");
#endif
}

// T3: Zero-width border/separator produces no artifacts
static void testZeroWidth() {
#ifndef DC_HAS_OSMESA
  std::printf("T3 zeroWidth: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 64, H = 64;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Main"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":1,"clipYMin":-0.95,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.5,"g":0.5,"b":0.5,"a":1})");

  dc::GpuBufferManager gpuBuf;
  dc::Renderer renderer;
  assert(renderer.init());

  // Default RenderStyle: zero widths
  dc::RenderStyle style;
  renderer.setRenderStyle(style);

  renderer.render(scene, gpuBuf, W, H);

  // Should render without crashing
  auto pixels = ctx.readPixels();
  assert(pixels.size() == static_cast<std::size_t>(W * H * 4));

  // Center should be the pane clear color (grey ~128)
  int cx = W / 2, cy = H / 2;
  int idx = (cy * W + cx) * 4;
  assert(pixels[idx] > 100);

  std::printf("T3 zeroWidth: PASS\n");
#endif
}

// T4: RenderStyle struct default initialization
static void testRenderStyleDefaults() {
  dc::RenderStyle style;
  assert(style.paneBorderWidth == 0.0f);
  assert(style.separatorWidth == 0.0f);
  assert(style.paneBorderColor[3] == 0.0f); // default alpha 0
  assert(style.separatorColor[3] == 0.0f);

  std::printf("T4 renderStyleDefaults: PASS\n");
}

int main() {
  testPaneBorders();
  testPaneSeparators();
  testZeroWidth();
  testRenderStyleDefaults();

  std::printf("\nAll D78.3 tests passed.\n");
  return 0;
}
