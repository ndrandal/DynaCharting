// D15.1-D15.3 — ZoomController: keyboard nav + zoom-to-fit

#include "dc/viewport/ZoomController.hpp"
#include "dc/viewport/Viewport.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

static void requireClose(double a, double b, double tol, const char* msg) {
  if (std::fabs(a - b) > tol) {
    std::fprintf(stderr, "ASSERT FAIL: %s (%.6f vs %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // ---- Test 1: Keyboard pan (arrow keys) ----
  {
    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(800, 600);

    dc::ZoomController zc;
    dc::ZoomControllerConfig cfg;
    cfg.panFraction = 0.1;
    zc.setConfig(cfg);

    requireTrue(zc.processKey(dc::KeyCode::Right, vp), "right key changed vp");
    requireClose(vp.dataRange().xMin, 10.0, 0.01, "right pan xMin");
    requireClose(vp.dataRange().xMax, 110.0, 0.01, "right pan xMax");

    requireTrue(zc.processKey(dc::KeyCode::Left, vp), "left key changed vp");
    requireClose(vp.dataRange().xMin, 0.0, 0.01, "left pan restores xMin");

    requireTrue(zc.processKey(dc::KeyCode::Up, vp), "up key changed vp");
    requireClose(vp.dataRange().yMin, 10.0, 0.01, "up pan yMin");

    requireTrue(zc.processKey(dc::KeyCode::Down, vp), "down key changed vp");
    requireClose(vp.dataRange().yMin, 0.0, 0.01, "down pan restores yMin");

    requireTrue(!zc.processKey(dc::KeyCode::None, vp), "None key no change");
    std::printf("  Test 1 (keyboard pan): PASS\n");
  }

  // ---- Test 2: Zoom by fraction (Home/End) ----
  {
    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(800, 600);

    dc::ZoomController zc;
    dc::ZoomControllerConfig cfg;
    cfg.zoomFraction = 0.2;
    zc.setConfig(cfg);

    double origWidth = vp.dataRange().xMax - vp.dataRange().xMin;
    zc.processKey(dc::KeyCode::Home, vp); // zoom in
    double newWidth = vp.dataRange().xMax - vp.dataRange().xMin;
    requireTrue(newWidth < origWidth, "Home zooms in");

    zc.processKey(dc::KeyCode::End, vp); // zoom out
    double restoredWidth = vp.dataRange().xMax - vp.dataRange().xMin;
    requireClose(restoredWidth, origWidth, 0.5, "End zooms out approx");

    std::printf("  Test 2 (zoom in/out): PASS\n");
  }

  // ---- Test 3: Zoom to fit ----
  {
    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(800, 600);

    dc::ZoomController zc;
    dc::ZoomControllerConfig cfg;
    cfg.fitMargin = 0.05;
    zc.setConfig(cfg);

    zc.zoomToFit(vp, 10.0, 50.0, 20.0, 80.0);

    double xRange = 50.0 - 10.0;
    double margin = xRange * 0.05;
    requireClose(vp.dataRange().xMin, 10.0 - margin, 0.01, "fit xMin with margin");
    requireClose(vp.dataRange().xMax, 50.0 + margin, 0.01, "fit xMax with margin");
    requireClose(vp.dataRange().yMin, 20.0 - 80.0 * 0.05 + 20.0 * 0.05, 0.5, "fit yMin approx");

    std::printf("  Test 3 (zoom to fit): PASS\n");
  }

  // ---- Test 4: Pan by fraction static method ----
  {
    dc::Viewport vp;
    vp.setDataRange(100, 200, 50, 150);
    vp.setPixelViewport(800, 600);

    dc::ZoomController::panByFraction(vp, 0.5, 0.0);
    requireClose(vp.dataRange().xMin, 150.0, 0.01, "panByFraction x");
    requireClose(vp.dataRange().xMax, 250.0, 0.01, "panByFraction x max");

    std::printf("  Test 4 (panByFraction): PASS\n");
  }

  std::printf("D15.1-D15.3 zoom_controller: ALL PASS\n");
  return 0;
}
