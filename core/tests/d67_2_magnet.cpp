// D67.2 — SnapManager: magnet mode with OHLC targets, radius, custom targets, Both mode
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
  std::printf("=== D67.2 SnapManager Magnet Tests ===\n");

  // Test 1: Magnet mode with OHLC — cursor near high snaps to high
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(15.0);

    // One candle: x=100, open=50, high=80, low=20, close=60
    double candle[] = {100.0, 50.0, 80.0, 20.0, 60.0};
    sm.setOHLCTargets(candle, 1);

    // Cursor at (100, 78) — 2 data units from high=80
    // pixelPerDataX=1, pixelPerDataY=1 => 2 pixels away
    dc::SnapResult r = sm.snap(100.0, 78.0, 1.0, 1.0);
    check(r.snapped, "OHLC magnet: snapped");
    check(std::fabs(r.x - 100.0) < 1e-9, "OHLC magnet: x = 100");
    check(std::fabs(r.y - 80.0) < 1e-9, "OHLC magnet: y = high (80)");
    check(r.distance < 15.0, "OHLC magnet: distance within radius");
  }

  // Test 2: Magnet mode — cursor near close snaps to close
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(10.0);

    double candle[] = {100.0, 50.0, 80.0, 20.0, 60.0};
    sm.setOHLCTargets(candle, 1);

    // Cursor at (101, 59) — close to close=60
    dc::SnapResult r = sm.snap(101.0, 59.0, 1.0, 1.0);
    check(r.snapped, "OHLC close: snapped");
    check(std::fabs(r.y - 60.0) < 1e-9, "OHLC close: y = close (60)");
  }

  // Test 3: Cursor beyond magnet radius does NOT snap
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(5.0);

    double candle[] = {100.0, 50.0, 80.0, 20.0, 60.0};
    sm.setOHLCTargets(candle, 1);

    // Cursor at (100, 70) — 10 data units from high=80, radius=5px
    dc::SnapResult r = sm.snap(100.0, 70.0, 1.0, 1.0);
    check(!r.snapped, "beyond radius: not snapped");
    check(std::fabs(r.x - 100.0) < 1e-9, "beyond radius: x unchanged");
    check(std::fabs(r.y - 70.0) < 1e-9, "beyond radius: y unchanged");
  }

  // Test 4: Magnet radius respects pixel-per-data scaling
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(10.0);

    double candle[] = {100.0, 50.0, 80.0, 20.0, 60.0};
    sm.setOHLCTargets(candle, 1);

    // Cursor at (100, 75) — 5 data units from high=80
    // pixelPerDataY=3 => 15 pixels distance (>10px radius)
    dc::SnapResult r = sm.snap(100.0, 75.0, 1.0, 3.0);
    check(!r.snapped, "pixel scale beyond radius: not snapped");
  }

  // Test 5: Custom snap targets via addTarget
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(10.0);

    dc::SnapTarget t;
    t.x = 50.0;
    t.y = 50.0;
    t.sourceId = 42;
    t.kind = 1;  // drawing endpoint
    sm.addTarget(t);

    dc::SnapResult r = sm.snap(52.0, 48.0, 1.0, 1.0);
    check(r.snapped, "custom target: snapped");
    check(std::fabs(r.x - 50.0) < 1e-9, "custom target: x = 50");
    check(std::fabs(r.y - 50.0) < 1e-9, "custom target: y = 50");
    check(r.targetSourceId == 42, "custom target: sourceId = 42");
  }

  // Test 6: clearTargets removes custom targets
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(10.0);

    dc::SnapTarget t;
    t.x = 50.0;
    t.y = 50.0;
    sm.addTarget(t);
    sm.clearTargets();

    dc::SnapResult r = sm.snap(52.0, 48.0, 1.0, 1.0);
    check(!r.snapped, "after clearTargets: not snapped");
  }

  // Test 7: clearOHLCTargets removes OHLC targets
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(10.0);

    double candle[] = {100.0, 50.0, 80.0, 20.0, 60.0};
    sm.setOHLCTargets(candle, 1);
    sm.clearOHLCTargets();

    dc::SnapResult r = sm.snap(100.0, 79.0, 1.0, 1.0);
    check(!r.snapped, "after clearOHLCTargets: not snapped");
  }

  // Test 8: Multiple candles — nearest OHLC point wins
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(15.0);

    // candle 0: x=10, O=50, H=80, L=20, C=60
    // candle 1: x=20, O=55, H=85, L=25, C=65
    double candles[] = {
      10.0, 50.0, 80.0, 20.0, 60.0,
      20.0, 55.0, 85.0, 25.0, 65.0
    };
    sm.setOHLCTargets(candles, 2);

    // Cursor at (19, 84) — nearest is candle 1 high=85 at x=20
    dc::SnapResult r = sm.snap(19.0, 84.0, 1.0, 1.0);
    check(r.snapped, "multi candle: snapped");
    check(std::fabs(r.x - 20.0) < 1e-9, "multi candle: x = 20 (candle 1)");
    check(std::fabs(r.y - 85.0) < 1e-9, "multi candle: y = 85 (high of candle 1)");
  }

  // Test 9: Both mode — magnet closer than grid
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Both);
    sm.setMagnetRadius(20.0);
    sm.setGridInterval(10.0, 10.0);

    // Custom target at (52, 48)
    dc::SnapTarget t;
    t.x = 52.0;
    t.y = 48.0;
    t.sourceId = 7;
    sm.addTarget(t);

    // Cursor at (51, 47) — magnet target at (52,48) is sqrt(2)~1.41 px away
    // Grid snaps to (50,50) which is sqrt(1+9)~3.16 px away
    dc::SnapResult r = sm.snap(51.0, 47.0, 1.0, 1.0);
    check(r.snapped, "Both mode magnet closer: snapped");
    check(std::fabs(r.x - 52.0) < 1e-9, "Both mode magnet closer: x = 52 (magnet)");
    check(std::fabs(r.y - 48.0) < 1e-9, "Both mode magnet closer: y = 48 (magnet)");
  }

  // Test 10: Both mode — grid closer than magnet
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Both);
    sm.setMagnetRadius(20.0);
    sm.setGridInterval(10.0, 10.0);

    // Custom target at (57, 57)
    dc::SnapTarget t;
    t.x = 57.0;
    t.y = 57.0;
    sm.addTarget(t);

    // Cursor at (50.1, 50.1) — grid (50,50) is ~0.14 px away
    // magnet (57,57) is ~9.76 px away
    dc::SnapResult r = sm.snap(50.1, 50.1, 1.0, 1.0);
    check(r.snapped, "Both mode grid closer: snapped");
    check(std::fabs(r.x - 50.0) < 1e-9, "Both mode grid closer: x = 50 (grid)");
    check(std::fabs(r.y - 50.0) < 1e-9, "Both mode grid closer: y = 50 (grid)");
  }

  // Test 11: Both mode — no magnet targets, only grid
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Both);
    sm.setMagnetRadius(10.0);
    sm.setGridInterval(5.0, 5.0);

    dc::SnapResult r = sm.snap(7.3, 8.2, 1.0, 1.0);
    check(r.snapped, "Both mode grid only: snapped");
    check(std::fabs(r.x - 5.0) < 1e-9, "Both mode grid only: x = 5");
    check(std::fabs(r.y - 10.0) < 1e-9, "Both mode grid only: y = 10");
  }

  // Test 12: setOHLCTargets with stride > 5
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(10.0);

    // stride=7: x, o, h, l, c, vol, timestamp
    double data[] = {
      100.0, 50.0, 80.0, 20.0, 60.0, 1000.0, 12345.0
    };
    sm.setOHLCTargets(data, 1, 7);

    dc::SnapResult r = sm.snap(100.0, 79.0, 1.0, 1.0);
    check(r.snapped, "stride 7: snapped to high");
    check(std::fabs(r.y - 80.0) < 1e-9, "stride 7: y = 80 (high)");
  }

  // Test 13: Magnet picks closest among OHLC targets with no custom
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(20.0);

    double candle[] = {100.0, 50.0, 80.0, 20.0, 60.0};
    sm.setOHLCTargets(candle, 1);

    // Cursor at (100, 51) — closest to open=50 (1 unit away)
    dc::SnapResult r = sm.snap(100.0, 51.0, 1.0, 1.0);
    check(r.snapped, "nearest OHLC: snapped");
    check(std::fabs(r.y - 50.0) < 1e-9, "nearest OHLC: y = open (50)");
  }

  // Test 14: Snap distance is reported in pixels
  {
    dc::SnapManager sm;
    sm.setMode(dc::SnapMode::Magnet);
    sm.setMagnetRadius(20.0);

    dc::SnapTarget t;
    t.x = 10.0;
    t.y = 10.0;
    sm.addTarget(t);

    // Cursor at (13, 14) with pixel scale 1:1
    // distance = sqrt(9 + 16) = 5
    dc::SnapResult r = sm.snap(13.0, 14.0, 1.0, 1.0);
    check(r.snapped, "distance check: snapped");
    check(std::fabs(r.distance - 5.0) < 1e-9, "distance check: distance = 5 px");
  }

  std::printf("=== D67.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
