// D64.1 -- Bezier tessellation tests
#include "dc/geometry/CurveTessellator.hpp"

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

static bool near(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D64.1 Bezier Tessellation ===\n");

  // --- Quadratic Bezier ---

  // Test 1: Quadratic bezier start point matches p0
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.5, 1.0}, p2{1.0, 0.0};
    auto pts = dc::CurveTessellator::quadraticBezier(p0, p1, p2, 16);
    check(near(pts.front().x, 0.0) && near(pts.front().y, 0.0),
          "quadratic: start matches p0");
  }

  // Test 2: Quadratic bezier end point matches p2
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.5, 1.0}, p2{1.0, 0.0};
    auto pts = dc::CurveTessellator::quadraticBezier(p0, p1, p2, 16);
    check(near(pts.back().x, 1.0) && near(pts.back().y, 0.0),
          "quadratic: end matches p2");
  }

  // Test 3: Quadratic bezier produces correct number of points
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.5, 1.0}, p2{1.0, 0.0};
    auto pts = dc::CurveTessellator::quadraticBezier(p0, p1, p2, 20);
    check(pts.size() == 21, "quadratic: 20 segments -> 21 points");
  }

  // Test 4: Quadratic bezier midpoint for symmetric curve
  // For symmetric control points p0=(0,0), p1=(0.5,1), p2=(1,0),
  // midpoint at t=0.5: x = 0.5, y = 0.5
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.5, 1.0}, p2{1.0, 0.0};
    auto pts = dc::CurveTessellator::quadraticBezier(p0, p1, p2, 2);
    // pts[1] is at t=0.5
    check(near(pts[1].x, 0.5) && near(pts[1].y, 0.5),
          "quadratic: midpoint at t=0.5");
  }

  // Test 5: Quadratic bezier with collinear control points produces straight line
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.5, 0.5}, p2{1.0, 1.0};
    auto pts = dc::CurveTessellator::quadraticBezier(p0, p1, p2, 10);
    bool allOnLine = true;
    for (auto& p : pts) {
      if (!near(p.x, p.y, 1e-9)) {
        allOnLine = false;
        break;
      }
    }
    check(allOnLine, "quadratic: collinear points -> straight line");
  }

  // --- Cubic Bezier ---

  // Test 6: Cubic bezier start point matches p0
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.25, 1.0}, p2{0.75, 1.0}, p3{1.0, 0.0};
    auto pts = dc::CurveTessellator::cubicBezier(p0, p1, p2, p3, 32);
    check(near(pts.front().x, 0.0) && near(pts.front().y, 0.0),
          "cubic: start matches p0");
  }

  // Test 7: Cubic bezier end point matches p3
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.25, 1.0}, p2{0.75, 1.0}, p3{1.0, 0.0};
    auto pts = dc::CurveTessellator::cubicBezier(p0, p1, p2, p3, 32);
    check(near(pts.back().x, 1.0) && near(pts.back().y, 0.0),
          "cubic: end matches p3");
  }

  // Test 8: Cubic bezier produces correct number of points
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.25, 1.0}, p2{0.75, 1.0}, p3{1.0, 0.0};
    auto pts = dc::CurveTessellator::cubicBezier(p0, p1, p2, p3, 50);
    check(pts.size() == 51, "cubic: 50 segments -> 51 points");
  }

  // Test 9: Cubic bezier midpoint for symmetric S-curve
  // p0=(0,0), p1=(0,1), p2=(1,0), p3=(1,1): at t=0.5
  // B(0.5) = 0.125*P0 + 0.375*P1 + 0.375*P2 + 0.125*P3
  //        = (0.375*0 + 0.375*1 + 0.125*1, 0.375*1 + 0.375*0 + 0.125*1)
  //        = (0.5, 0.5)
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.0, 1.0}, p2{1.0, 0.0}, p3{1.0, 1.0};
    auto pts = dc::CurveTessellator::cubicBezier(p0, p1, p2, p3, 2);
    check(near(pts[1].x, 0.5) && near(pts[1].y, 0.5),
          "cubic: symmetric S-curve midpoint at (0.5, 0.5)");
  }

  // Test 10: Cubic bezier with collinear control points
  {
    dc::Vec2 p0{0.0, 0.0}, p1{1.0, 1.0}, p2{2.0, 2.0}, p3{3.0, 3.0};
    auto pts = dc::CurveTessellator::cubicBezier(p0, p1, p2, p3, 10);
    bool allOnLine = true;
    for (auto& p : pts) {
      if (!near(p.y, p.x, 1e-9)) {
        allOnLine = false;
        break;
      }
    }
    check(allOnLine, "cubic: collinear points -> straight line");
  }

  // --- Adaptive Cubic Bezier ---

  // Test 11: Adaptive bezier start and end match control points
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.25, 1.0}, p2{0.75, 1.0}, p3{1.0, 0.0};
    auto pts = dc::CurveTessellator::cubicBezierAdaptive(p0, p1, p2, p3, 0.1);
    check(near(pts.front().x, 0.0) && near(pts.front().y, 0.0),
          "adaptive: start matches p0");
    check(near(pts.back().x, 1.0) && near(pts.back().y, 0.0),
          "adaptive: end matches p3");
  }

  // Test 12: Adaptive bezier produces fewer points for a straight line
  {
    dc::Vec2 p0{0.0, 0.0}, p1{1.0, 1.0}, p2{2.0, 2.0}, p3{3.0, 3.0};
    auto straight = dc::CurveTessellator::cubicBezierAdaptive(p0, p1, p2, p3, 0.1);

    dc::Vec2 q0{0.0, 0.0}, q1{0.0, 2.0}, q2{3.0, -1.0}, q3{3.0, 1.0};
    auto curved = dc::CurveTessellator::cubicBezierAdaptive(q0, q1, q2, q3, 0.1);

    check(straight.size() < curved.size(),
          "adaptive: straight line uses fewer points than curve");
  }

  // Test 13: Adaptive bezier with tight tolerance produces more points
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.0, 2.0}, p2{2.0, -1.0}, p3{2.0, 1.0};
    auto coarse = dc::CurveTessellator::cubicBezierAdaptive(p0, p1, p2, p3, 2.0);
    auto fine = dc::CurveTessellator::cubicBezierAdaptive(p0, p1, p2, p3, 0.01);
    check(fine.size() > coarse.size(),
          "adaptive: tighter tolerance -> more points");
  }

  // Test 14: Adaptive bezier for straight line produces minimal points
  {
    dc::Vec2 p0{0.0, 0.0}, p1{1.0, 0.0}, p2{2.0, 0.0}, p3{3.0, 0.0};
    auto pts = dc::CurveTessellator::cubicBezierAdaptive(p0, p1, p2, p3, 0.5);
    // Should be 2 points: start + end (one subdivision level)
    check(pts.size() == 2, "adaptive: perfectly straight -> 2 points");
  }

  // Test 15: Minimum segment count clamped to 1
  {
    dc::Vec2 p0{0.0, 0.0}, p1{0.5, 1.0}, p2{1.0, 0.0};
    auto pts = dc::CurveTessellator::quadraticBezier(p0, p1, p2, 0);
    check(pts.size() >= 2, "quadratic: segments=0 clamped to at least 1 -> 2+ points");
  }

  std::printf("=== D64.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
