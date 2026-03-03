// D67.1 — SnapManager: grid snapping and None mode
#include "dc/interaction/SnapManager.hpp"

#include <cmath>
#include <cstdio>

static int passed = 0;
static int failed = 0;

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
  std::printf("=== D67.1 SnapManager Grid Tests ===\n");

  // Test 1: Default mode is None
  {
    dc::SnapManager sm;
    check(sm.mode() == dc::SnapMode::None, "default mode is None");
  }

  // Test 2: None mode returns unsnapped position
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::None);
    dc::SnapResult r = sm.snap(3.7, 8.2, 1.0, 1.0);
    check(!r.snapped, "None mode: not snapped");
    check(std::fabs(r.x - 3.7) < 1e-9, "None mode: x unchanged");
    check(std::fabs(r.y - 8.2) < 1e-9, "None mode: y unchanged");
  }

  // Test 3: Grid mode snaps to nearest grid intersection
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(5.0, 10.0);
    // 3.7 rounds to 5.0 (gridX=5), 8.2 rounds to 10.0 (gridY=10)
    dc::SnapResult r = sm.snap(3.7, 8.2, 1.0, 1.0);
    check(r.snapped, "Grid mode: snapped");
    check(std::fabs(r.x - 5.0) < 1e-9, "Grid mode: x snapped to 5.0");
    check(std::fabs(r.y - 10.0) < 1e-9, "Grid mode: y snapped to 10.0");
  }

  // Test 4: Grid mode rounds correctly at midpoint
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(10.0, 10.0);
    // 15.0 is exactly on grid, should stay
    dc::SnapResult r = sm.snap(15.0, 25.0, 1.0, 1.0);
    check(r.snapped, "Grid exact: snapped");
    check(std::fabs(r.x - 20.0) < 1e-9, "Grid exact: x stays 20.0");
    check(std::fabs(r.y - 30.0) < 1e-9, "Grid exact: y stays 30.0");
  }

  // Test 5: Grid with only X interval set (Y interval = 0 means no Y snap)
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(5.0, 0.0);
    dc::SnapResult r = sm.snap(7.3, 8.2, 1.0, 1.0);
    check(r.snapped, "Grid X-only: snapped");
    check(std::fabs(r.x - 5.0) < 1e-9, "Grid X-only: x snapped to 5.0");
    check(std::fabs(r.y - 8.2) < 1e-9, "Grid X-only: y unchanged");
  }

  // Test 6: Grid with only Y interval set
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(0.0, 10.0);
    dc::SnapResult r = sm.snap(3.7, 14.0, 1.0, 1.0);
    check(r.snapped, "Grid Y-only: snapped");
    check(std::fabs(r.x - 3.7) < 1e-9, "Grid Y-only: x unchanged");
    check(std::fabs(r.y - 10.0) < 1e-9, "Grid Y-only: y snapped to 10.0");
  }

  // Test 7: Grid with zero intervals means no snapping (both 0)
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(0.0, 0.0);
    dc::SnapResult r = sm.snap(3.7, 8.2, 1.0, 1.0);
    check(!r.snapped, "Grid zero intervals: not snapped");
  }

  // Test 8: Grid distance is computed in pixel-space
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(10.0, 10.0);
    // cursor at (12.0, 13.0) snaps to (10.0, 10.0)
    // pixelPerDataX=2, pixelPerDataY=3
    // dxPx = (10 - 12) * 2 = -4, dyPx = (10 - 13) * 3 = -9
    // dist = sqrt(16 + 81) = sqrt(97)
    dc::SnapResult r = sm.snap(12.0, 13.0, 2.0, 3.0);
    check(r.snapped, "Grid pixel distance: snapped");
    check(std::fabs(r.distance - std::sqrt(97.0)) < 1e-6,
          "Grid pixel distance: correct");
  }

  // Test 9: Grid with negative coordinates
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Grid);
    sm.setGridInterval(5.0, 5.0);
    dc::SnapResult r = sm.snap(-3.7, -8.2, 1.0, 1.0);
    check(r.snapped, "Grid negative: snapped");
    check(std::fabs(r.x - (-5.0)) < 1e-9, "Grid negative: x snapped to -5.0");
    check(std::fabs(r.y - (-10.0)) < 1e-9, "Grid negative: y snapped to -10.0");
  }

  // Test 10: setMode / mode round-trip
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    check(sm.mode() == dc::SnapMode::Magnet, "mode set to Magnet");
    sm.setMode(dc::SnapMode::Both);
    check(sm.mode() == dc::SnapMode::Both, "mode set to Both");
  }

  // Test 11: Default magnet radius is 10
  {
    dc::SnapManager sm;
    check(std::fabs(sm.magnetRadius() - 10.0) < 1e-9, "default magnet radius is 10");
  }

  // Test 12: setMagnetRadius / magnetRadius round-trip
  {
    dc::SnapManager sm;
    sm.setMagnetRadius(25.0);
    check(std::fabs(sm.magnetRadius() - 25.0) < 1e-9, "magnet radius set to 25");
  }

  std::printf("=== D67.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
