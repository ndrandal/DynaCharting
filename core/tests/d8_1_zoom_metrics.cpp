// D8.1 — Viewport zoom metrics test (pure C++, no GL)
// Tests: visibleDataWidth, pixelsPerDataUnitX/Y at various zoom levels.

#include "dc/viewport/Viewport.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireClose(double a, double b, double eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Full clip, 800x600 fb ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f}); // full
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    requireClose(vp.visibleDataWidth(), 100.0, 1e-9, "visibleDataWidth 100");

    // pixelsPerDataUnitX: clipW=2, pixelW = 2/2 * 800 = 800, ppdu = 800/100 = 8
    requireClose(vp.pixelsPerDataUnitX(), 8.0, 1e-9, "ppduX full clip");

    // pixelsPerDataUnitY: clipH=2, pixelH = 2/2 * 600 = 600, ppdu = 600/50 = 12
    requireClose(vp.pixelsPerDataUnitY(), 12.0, 1e-9, "ppduY full clip");

    std::printf("  Full clip PASS\n");
  }

  // --- Test 2: Zoom in ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(40.0, 60.0, 0.0, 50.0);

    requireClose(vp.visibleDataWidth(), 20.0, 1e-9, "visibleDataWidth zoomed in");
    // ppduX = 800 / 20 = 40
    requireClose(vp.pixelsPerDataUnitX(), 40.0, 1e-9, "ppduX zoomed in");

    std::printf("  Zoom in PASS\n");
  }

  // --- Test 3: Sub-pane clip region ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    // Half-width clip region: clipXMin=-1, clipXMax=0 → clipW = 1
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 0.0f});
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    // pixelW = 1/2 * 800 = 400, ppdu = 400/100 = 4
    requireClose(vp.pixelsPerDataUnitX(), 4.0, 1e-9, "ppduX sub-pane");

    std::printf("  Sub-pane PASS\n");
  }

  // --- Test 4: Zero data width ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(50.0, 50.0, 10.0, 10.0);

    requireClose(vp.visibleDataWidth(), 0.0, 1e-9, "visibleDataWidth zero");
    requireClose(vp.pixelsPerDataUnitX(), 0.0, 1e-9, "ppduX zero range");
    requireClose(vp.pixelsPerDataUnitY(), 0.0, 1e-9, "ppduY zero range");

    std::printf("  Zero range PASS\n");
  }

  std::printf("\nD8.1 zoom metrics PASS\n");
  return 0;
}
