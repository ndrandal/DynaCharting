// D5.1 — Viewport coordinate mapping, pan/zoom tests (pure C++)

#include "dc/viewport/Viewport.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

static void requireNear(double a, double b, double eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL [%s]: %.8f != %.8f (eps=%.8f)\n",
                 msg, a, b, eps);
    std::exit(1);
  }
}

int main() {
  constexpr double EPS = 1e-6;

  // --- Round-trip: pixel → clip → data → clip → pixel ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    double cx, cy, dx, dy, cx2, cy2;

    // Center pixel
    vp.pixelToClip(400, 300, cx, cy);
    requireNear(cx, 0.0, EPS, "center pixel→clip X");
    requireNear(cy, 0.0, EPS, "center pixel→clip Y");

    vp.clipToData(cx, cy, dx, dy);
    requireNear(dx, 50.0, EPS, "center clip→data X");
    requireNear(dy, 25.0, EPS, "center clip→data Y");

    vp.dataToClip(dx, dy, cx2, cy2);
    requireNear(cx2, cx, EPS, "roundtrip clip X");
    requireNear(cy2, cy, EPS, "roundtrip clip Y");

    // Top-left pixel (0,0) → clip(-1, 1) (Y flipped)
    vp.pixelToClip(0, 0, cx, cy);
    requireNear(cx, -1.0, EPS, "topleft pixel→clip X");
    requireNear(cy, 1.0, EPS, "topleft pixel→clip Y");

    // Bottom-right pixel (800,600) → clip(1, -1)
    vp.pixelToClip(800, 600, cx, cy);
    requireNear(cx, 1.0, EPS, "botright pixel→clip X");
    requireNear(cy, -1.0, EPS, "botright pixel→clip Y");

    std::printf("  round-trip PASS\n");
  }

  // --- pixelToData ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(400, 400);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(10.0, 20.0, 0.0, 100.0);

    double dx, dy;
    vp.pixelToData(200, 200, dx, dy); // center
    requireNear(dx, 15.0, EPS, "pixelToData center X");
    requireNear(dy, 50.0, EPS, "pixelToData center Y");

    std::printf("  pixelToData PASS\n");
  }

  // --- Pan shifts data range ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    double oldXMin = vp.dataRange().xMin;
    double oldXMax = vp.dataRange().xMax;

    // Pan right by 80 pixels (10% of 800)
    vp.pan(80, 0);

    // Data range should shift left (dragging right = move data left)
    double shift = (vp.dataRange().xMin - oldXMin);
    requireTrue(shift < -1e-6, "pan right shifts xMin left");
    requireNear(vp.dataRange().xMax - vp.dataRange().xMin,
                oldXMax - oldXMin, EPS, "pan preserves range width");

    std::printf("  pan PASS\n");
  }

  // --- Zoom at center halves range ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    // Zoom in at center (factor=1.0 → range shrinks by half)
    vp.zoom(1.0, 400, 300);

    double rangeX = vp.dataRange().xMax - vp.dataRange().xMin;
    requireNear(rangeX, 50.0, 0.01, "zoom halves X range");

    double rangeY = vp.dataRange().yMax - vp.dataRange().yMin;
    requireNear(rangeY, 25.0, 0.01, "zoom halves Y range");

    // Center should remain at data (50, 25)
    requireNear((vp.dataRange().xMin + vp.dataRange().xMax) / 2.0, 50.0, 0.01,
                "zoom center preserved X");
    requireNear((vp.dataRange().yMin + vp.dataRange().yMax) / 2.0, 25.0, 0.01,
                "zoom center preserved Y");

    std::printf("  zoom center PASS\n");
  }

  // --- Zoom at off-center pivot keeps pivot fixed ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    // Pivot at pixel (200, 150) = clip(-0.5, 0.5) = data(25, 37.5)
    double pivotDxBefore, pivotDyBefore;
    vp.pixelToData(200, 150, pivotDxBefore, pivotDyBefore);

    vp.zoom(1.0, 200, 150);

    double pivotDxAfter, pivotDyAfter;
    vp.pixelToData(200, 150, pivotDxAfter, pivotDyAfter);

    requireNear(pivotDxAfter, pivotDxBefore, 0.01, "pivot fixed X");
    requireNear(pivotDyAfter, pivotDyBefore, 0.01, "pivot fixed Y");

    std::printf("  zoom pivot PASS\n");
  }

  // --- containsPixel ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-0.9f, 0.9f, -0.3f, 0.9f}); // partial pane

    // Center of clip region: pixel ~ (400, ~150)
    requireTrue(vp.containsPixel(400, 150), "center pixel inside pane");

    // Bottom pixel (0,600) → clip(-1, -1) which is outside clip region
    requireTrue(!vp.containsPixel(0, 600), "bottom-left outside pane");

    std::printf("  containsPixel PASS\n");
  }

  // --- computeTransformParams ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-0.9f, 0.9f, -0.3f, 0.9f});
    vp.setDataRange(0.0, 100.0, 80.0, 120.0);

    dc::TransformParams tp = vp.computeTransformParams();

    // PaneRegion fields: {clipYMin, clipYMax, clipXMin, clipXMax}
    // So {-0.9f, 0.9f, -0.3f, 0.9f} → clipYMin=-0.9, clipYMax=0.9, clipXMin=-0.3, clipXMax=0.9
    // clipW = 0.9 - (-0.3) = 1.2, clipH = 0.9 - (-0.9) = 1.8
    // sx = clipW / dataW = 1.2 / 100 = 0.012
    requireNear(tp.sx, 0.012, 1e-4, "tp.sx");
    // sy = clipH / dataH = 1.8 / 40 = 0.045
    requireNear(tp.sy, 0.045, 1e-4, "tp.sy");
    // tx = clipXMin - xMin * sx = -0.3 - 0*0.012 = -0.3
    requireNear(tp.tx, -0.3, 1e-4, "tp.tx");
    // ty = clipYMin - yMin * sy = -0.9 - 80*0.045 = -0.9 - 3.6 = -4.5
    requireNear(tp.ty, -4.5, 1e-4, "tp.ty");

    std::printf("  computeTransformParams PASS\n");
  }

  // --- Clip region smaller than full viewport ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    // PaneRegion: {clipYMin, clipYMax, clipXMin, clipXMax}
    vp.setClipRegion({-0.8f, 0.8f, -0.5f, 0.5f});
    vp.setDataRange(10.0, 50.0, 0.0, 200.0);

    double dx, dy;
    // data(10,0) should map to clip(clipXMin, clipYMin) = (-0.5, -0.8)
    double cx, cy;
    vp.dataToClip(10.0, 0.0, cx, cy);
    requireNear(cx, -0.5, EPS, "subregion data→clip X min");
    requireNear(cy, -0.8, EPS, "subregion data→clip Y min");

    vp.dataToClip(50.0, 200.0, cx, cy);
    requireNear(cx, 0.5, EPS, "subregion data→clip X max");
    requireNear(cy, 0.8, EPS, "subregion data→clip Y max");

    // Round trip
    vp.clipToData(cx, cy, dx, dy);
    requireNear(dx, 50.0, EPS, "subregion roundtrip X");
    requireNear(dy, 200.0, EPS, "subregion roundtrip Y");

    std::printf("  clip subregion PASS\n");
  }

  std::printf("\nD5.1 viewport PASS\n");
  return 0;
}
