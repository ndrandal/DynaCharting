// D65.1 — FreehandCapture: begin/addPoint/finish flow, cancel

#include "dc/drawing/FreehandCapture.hpp"

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
  std::printf("=== D65.1 FreehandCapture Tests ===\n");

  // Test 1: Initial state — not capturing
  {
    dc::FreehandCapture cap;
    check(!cap.isCapturing(), "initial state: not capturing");
    check(cap.currentPoints().empty(), "initial state: no points");
  }

  // Test 2: Begin starts capture
  {
    dc::FreehandCapture cap;
    cap.begin(10.0, 20.0);
    check(cap.isCapturing(), "begin: now capturing");
    check(cap.currentPoints().size() == 1, "begin: has 1 point");
    check(cap.currentPoints()[0].x == 10.0, "begin: x correct");
    check(cap.currentPoints()[0].y == 20.0, "begin: y correct");
    check(cap.currentPoints()[0].pressure == 1.0, "begin: default pressure");
  }

  // Test 3: addPoint accumulates points
  {
    dc::FreehandCapture cap;
    cap.begin(0.0, 0.0);
    cap.addPoint(1.0, 1.0);
    cap.addPoint(2.0, 2.0);
    cap.addPoint(3.0, 3.0);
    check(cap.currentPoints().size() == 4, "addPoint: 4 points total");
    check(cap.currentPoints()[3].x == 3.0, "addPoint: last point x");
  }

  // Test 4: addPoint without begin is ignored
  {
    dc::FreehandCapture cap;
    cap.addPoint(5.0, 5.0);
    check(cap.currentPoints().empty(), "addPoint without begin: ignored");
    check(!cap.isCapturing(), "addPoint without begin: still not capturing");
  }

  // Test 5: finish returns stroke and stops capture
  {
    dc::FreehandCapture cap;
    cap.begin(0.0, 0.0, 0.5);
    cap.addPoint(10.0, 10.0, 0.7);
    cap.addPoint(20.0, 5.0, 0.9);

    dc::Stroke s = cap.finish();
    check(s.id == 1, "finish: first stroke id is 1");
    check(s.points.size() == 3, "finish: stroke has 3 points");
    check(s.points[0].pressure == 0.5, "finish: first pressure preserved");
    check(s.points[1].x == 10.0, "finish: second point x");
    check(!cap.isCapturing(), "finish: no longer capturing");
    check(cap.currentPoints().empty(), "finish: points cleared");
  }

  // Test 6: Multiple strokes get incrementing IDs
  {
    dc::FreehandCapture cap;

    cap.begin(0, 0);
    dc::Stroke s1 = cap.finish();

    cap.begin(1, 1);
    dc::Stroke s2 = cap.finish();

    cap.begin(2, 2);
    dc::Stroke s3 = cap.finish();

    check(s1.id == 1, "sequential id: first is 1");
    check(s2.id == 2, "sequential id: second is 2");
    check(s3.id == 3, "sequential id: third is 3");
  }

  // Test 7: Cancel clears state
  {
    dc::FreehandCapture cap;
    cap.begin(5.0, 5.0);
    cap.addPoint(10.0, 10.0);
    cap.addPoint(15.0, 15.0);
    check(cap.isCapturing(), "cancel: was capturing");

    cap.cancel();
    check(!cap.isCapturing(), "cancel: no longer capturing");
    check(cap.currentPoints().empty(), "cancel: points cleared");
  }

  // Test 8: Finish after cancel returns empty stroke with id 0
  {
    dc::FreehandCapture cap;
    cap.begin(1.0, 1.0);
    cap.cancel();

    dc::Stroke s = cap.finish();
    check(s.id == 0, "finish after cancel: id is 0");
    check(s.points.empty(), "finish after cancel: no points");
  }

  // Test 9: Stroke default values
  {
    dc::FreehandCapture cap;
    cap.begin(0, 0);
    dc::Stroke s = cap.finish();
    check(s.color[0] == 1.0f, "default color R");
    check(s.color[1] == 1.0f, "default color G");
    check(s.color[2] == 1.0f, "default color B");
    check(s.color[3] == 1.0f, "default color A");
    check(s.lineWidth == 2.0f, "default lineWidth");
    check(!s.closed, "default closed is false");
  }

  // Test 10: Begin while already capturing restarts
  {
    dc::FreehandCapture cap;
    cap.begin(0, 0);
    cap.addPoint(1, 1);
    cap.addPoint(2, 2);
    check(cap.currentPoints().size() == 3, "pre-restart: 3 points");

    cap.begin(100, 200);
    check(cap.isCapturing(), "restart: still capturing");
    check(cap.currentPoints().size() == 1, "restart: reset to 1 point");
    check(cap.currentPoints()[0].x == 100.0, "restart: new x");
    check(cap.currentPoints()[0].y == 200.0, "restart: new y");
  }

  std::printf("=== D65.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
