// D65.2 — Polyline simplification (RDP), smoothing, StrokeStore

#include "dc/drawing/FreehandCapture.hpp"

#include <cstdio>
#include <cmath>

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
  std::printf("=== D65.2 Polyline Simplify / StrokeStore Tests ===\n");

  // ============================================================
  // RDP Simplification
  // ============================================================

  // Test 1: Straight line collapses to 2 points
  {
    std::vector<dc::StrokePoint> pts;
    for (int i = 0; i <= 10; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = static_cast<double>(i) * 2.0;
      pts.push_back(p);
    }

    auto simplified = dc::FreehandCapture::simplify(pts, 0.01);
    check(simplified.size() == 2, "RDP straight line: 2 endpoints");
    check(simplified.front().x == 0.0, "RDP straight: first x");
    check(simplified.back().x == 10.0, "RDP straight: last x");
  }

  // Test 2: Triangle shape (3 key points) — mid-point deviation should be kept
  {
    std::vector<dc::StrokePoint> pts;
    // Build: (0,0) -> ramp to (5,10) -> ramp to (10,0)
    for (int i = 0; i <= 5; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = static_cast<double>(i) * 2.0;
      pts.push_back(p);
    }
    for (int i = 6; i <= 10; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = (10.0 - static_cast<double>(i)) * 2.0;
      pts.push_back(p);
    }

    auto simplified = dc::FreehandCapture::simplify(pts, 0.5);
    // The apex at (5,10) deviates significantly from the (0,0)-(10,0) line
    check(simplified.size() >= 3, "RDP triangle: at least 3 points kept");
    check(simplified.front().x == 0.0, "RDP triangle: starts at 0");
    check(simplified.back().x == 10.0, "RDP triangle: ends at 10");
  }

  // Test 3: Empty and tiny inputs are returned as-is
  {
    std::vector<dc::StrokePoint> empty;
    auto r0 = dc::FreehandCapture::simplify(empty, 1.0);
    check(r0.empty(), "RDP empty: returns empty");

    dc::StrokePoint p;
    p.x = 5; p.y = 5;
    std::vector<dc::StrokePoint> single = {p};
    auto r1 = dc::FreehandCapture::simplify(single, 1.0);
    check(r1.size() == 1, "RDP single point: returned as-is");

    dc::StrokePoint p2;
    p2.x = 10; p2.y = 10;
    std::vector<dc::StrokePoint> two = {p, p2};
    auto r2 = dc::FreehandCapture::simplify(two, 1.0);
    check(r2.size() == 2, "RDP two points: returned as-is");
  }

  // Test 4: Large epsilon collapses everything to 2 endpoints
  {
    std::vector<dc::StrokePoint> pts;
    for (int i = 0; i < 50; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = std::sin(static_cast<double>(i) * 0.5) * 5.0;
      pts.push_back(p);
    }
    auto simplified = dc::FreehandCapture::simplify(pts, 1000.0);
    check(simplified.size() == 2, "RDP large epsilon: only 2 points");
  }

  // ============================================================
  // Smoothing
  // ============================================================

  // Test 5: Smoothing reduces jitter
  {
    std::vector<dc::StrokePoint> pts;
    // Zigzag around y=10
    for (int i = 0; i < 20; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = 10.0 + ((i % 2 == 0) ? 2.0 : -2.0);
      pts.push_back(p);
    }

    auto smoothed = dc::FreehandCapture::smooth(pts, 5);
    check(smoothed.size() == pts.size(), "smooth: preserves point count");

    // Interior smoothed points should be closer to 10.0 than original zigzag
    double maxDevOriginal = 0;
    double maxDevSmoothed = 0;
    for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
      double dOrig = std::abs(pts[i].y - 10.0);
      double dSmooth = std::abs(smoothed[i].y - 10.0);
      if (dOrig > maxDevOriginal) maxDevOriginal = dOrig;
      if (dSmooth > maxDevSmoothed) maxDevSmoothed = dSmooth;
    }
    check(maxDevSmoothed < maxDevOriginal, "smooth: reduces max deviation");
  }

  // Test 6: Smoothing preserves endpoints
  {
    std::vector<dc::StrokePoint> pts;
    for (int i = 0; i < 10; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = static_cast<double>(i * i);
      pts.push_back(p);
    }

    auto smoothed = dc::FreehandCapture::smooth(pts, 3);
    check(smoothed.front().x == pts.front().x, "smooth: first x preserved");
    check(smoothed.front().y == pts.front().y, "smooth: first y preserved");
    check(smoothed.back().x == pts.back().x, "smooth: last x preserved");
    check(smoothed.back().y == pts.back().y, "smooth: last y preserved");
  }

  // Test 7: Smoothing with tiny input returns as-is
  {
    std::vector<dc::StrokePoint> two;
    dc::StrokePoint a, b;
    a.x = 0; a.y = 0; b.x = 1; b.y = 1;
    two = {a, b};
    auto r = dc::FreehandCapture::smooth(two, 5);
    check(r.size() == 2, "smooth 2 points: unchanged");

    std::vector<dc::StrokePoint> empty;
    auto r2 = dc::FreehandCapture::smooth(empty, 5);
    check(r2.empty(), "smooth empty: returns empty");
  }

  // Test 8: Smoothing with windowSize < 2 returns as-is
  {
    std::vector<dc::StrokePoint> pts;
    for (int i = 0; i < 5; ++i) {
      dc::StrokePoint p;
      p.x = static_cast<double>(i);
      p.y = static_cast<double>(i);
      pts.push_back(p);
    }
    auto r = dc::FreehandCapture::smooth(pts, 1);
    check(r.size() == pts.size(), "smooth window<2: returns same size");
    check(r[2].x == pts[2].x, "smooth window<2: values unchanged");
  }

  // ============================================================
  // StrokeStore
  // ============================================================

  // Test 9: Add and retrieve
  {
    dc::StrokeStore store;
    dc::Stroke s;
    s.id = 42;
    dc::StrokePoint p;
    p.x = 1; p.y = 2;
    s.points.push_back(p);

    auto id = store.add(std::move(s));
    check(id == 42, "store add: returns stroke id");
    check(store.count() == 1, "store: count is 1");

    const dc::Stroke* got = store.get(42);
    check(got != nullptr, "store get: found");
    check(got->points.size() == 1, "store get: correct points");
    check(got->points[0].x == 1.0, "store get: point x");
  }

  // Test 10: Remove
  {
    dc::StrokeStore store;

    dc::Stroke s1; s1.id = 10;
    dc::Stroke s2; s2.id = 20;
    dc::Stroke s3; s3.id = 30;
    store.add(std::move(s1));
    store.add(std::move(s2));
    store.add(std::move(s3));
    check(store.count() == 3, "store: 3 strokes");

    store.remove(20);
    check(store.count() == 2, "store after remove: 2 strokes");
    check(store.get(20) == nullptr, "store: removed stroke gone");
    check(store.get(10) != nullptr, "store: other stroke remains");
    check(store.get(30) != nullptr, "store: other stroke remains (2)");
  }

  // Test 11: Clear
  {
    dc::StrokeStore store;
    dc::Stroke s1; s1.id = 1;
    dc::Stroke s2; s2.id = 2;
    store.add(std::move(s1));
    store.add(std::move(s2));
    store.clear();
    check(store.count() == 0, "store clear: empty");
    check(store.strokes().empty(), "store clear: strokes() empty");
  }

  // Test 12: Get non-existent returns nullptr
  {
    dc::StrokeStore store;
    check(store.get(999) == nullptr, "store get missing: nullptr");
  }

  // Test 13: Integration — capture, simplify, store
  {
    dc::FreehandCapture cap;
    cap.begin(0, 0);
    for (int i = 1; i <= 20; ++i) {
      cap.addPoint(static_cast<double>(i), static_cast<double>(i));
    }
    dc::Stroke raw = cap.finish();
    check(raw.points.size() == 21, "integration: raw has 21 points");

    auto simplified = dc::FreehandCapture::simplify(raw.points, 0.01);
    check(simplified.size() == 2, "integration: straight line simplified to 2");

    raw.points = simplified;
    dc::StrokeStore store;
    auto id = store.add(std::move(raw));
    check(id > 0, "integration: stored with valid id");
    check(store.get(id)->points.size() == 2, "integration: stored simplified stroke");
  }

  std::printf("=== D65.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
