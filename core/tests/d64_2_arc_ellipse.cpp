// D64.2 -- Arc, ellipse, and Catmull-Rom tessellation tests
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

static const double PI = 3.14159265358979323846;
static const double TWO_PI = 2.0 * PI;

int main() {
  std::printf("=== D64.2 Arc, Ellipse & Catmull-Rom ===\n");

  // --- Arc ---

  // Test 1: Arc start point matches (center + radius * cos/sin(startAngle))
  {
    dc::Vec2 center{0.0, 0.0};
    double radius = 5.0;
    double startAngle = 0.0;
    double endAngle = PI;
    auto pts = dc::CurveTessellator::arc(center, radius, startAngle, endAngle, 32);
    check(near(pts.front().x, 5.0) && near(pts.front().y, 0.0),
          "arc: start at angle 0 -> (5, 0)");
  }

  // Test 2: Arc end point matches (center + radius * cos/sin(endAngle))
  {
    dc::Vec2 center{0.0, 0.0};
    double radius = 5.0;
    double startAngle = 0.0;
    double endAngle = PI;
    auto pts = dc::CurveTessellator::arc(center, radius, startAngle, endAngle, 32);
    check(near(pts.back().x, -5.0, 1e-9) && near(pts.back().y, 0.0, 1e-9),
          "arc: end at angle PI -> (-5, 0)");
  }

  // Test 3: Arc produces correct number of points
  {
    auto pts = dc::CurveTessellator::arc({0, 0}, 1.0, 0.0, TWO_PI, 64);
    check(pts.size() == 65, "arc: 64 segments -> 65 points");
  }

  // Test 4: Arc midpoint for semicircle
  {
    auto pts = dc::CurveTessellator::arc({0, 0}, 1.0, 0.0, PI, 2);
    // pts[1] is at angle PI/2 -> (0, 1)
    check(near(pts[1].x, 0.0, 1e-9) && near(pts[1].y, 1.0, 1e-9),
          "arc: midpoint of semicircle at (0, 1)");
  }

  // Test 5: All arc points are on the circle (distance from center == radius)
  {
    dc::Vec2 center{3.0, 4.0};
    double radius = 7.0;
    auto pts = dc::CurveTessellator::arc(center, radius, 0.0, TWO_PI, 100);
    bool allOnCircle = true;
    for (auto& p : pts) {
      double dx = p.x - center.x;
      double dy = p.y - center.y;
      double dist = std::sqrt(dx * dx + dy * dy);
      if (!near(dist, radius, 1e-9)) {
        allOnCircle = false;
        break;
      }
    }
    check(allOnCircle, "arc: all points lie on circle");
  }

  // Test 6: Arc with non-zero center offset
  {
    dc::Vec2 center{10.0, 20.0};
    double radius = 3.0;
    auto pts = dc::CurveTessellator::arc(center, radius, 0.0, PI * 0.5, 8);
    check(near(pts.front().x, 13.0) && near(pts.front().y, 20.0),
          "arc: offset center start");
    check(near(pts.back().x, 10.0, 1e-9) && near(pts.back().y, 23.0, 1e-9),
          "arc: offset center end at PI/2");
  }

  // --- Ellipse ---

  // Test 7: Ellipse with equal radii matches arc output
  {
    dc::Vec2 center{0.0, 0.0};
    double r = 5.0;
    int segs = 32;

    auto arcPts = dc::CurveTessellator::arc(center, r, 0.0, TWO_PI, segs);
    auto ellPts = dc::CurveTessellator::ellipse(center, r, r, 0.0, 0.0, TWO_PI, segs);

    check(arcPts.size() == ellPts.size(), "ellipse: circle matches arc point count");

    bool allMatch = true;
    for (std::size_t i = 0; i < arcPts.size() && i < ellPts.size(); ++i) {
      if (!near(arcPts[i].x, ellPts[i].x, 1e-9) ||
          !near(arcPts[i].y, ellPts[i].y, 1e-9)) {
        allMatch = false;
        break;
      }
    }
    check(allMatch, "ellipse: equal radii -> matches arc output exactly");
  }

  // Test 8: Ellipse start and end for full revolution
  {
    dc::Vec2 center{0.0, 0.0};
    auto pts = dc::CurveTessellator::ellipse(center, 3.0, 2.0, 0.0, 0.0, TWO_PI, 64);
    // Start: (3, 0), End: should be back near (3, 0)
    check(near(pts.front().x, 3.0) && near(pts.front().y, 0.0),
          "ellipse: start at (rx, 0)");
    check(near(pts.back().x, 3.0, 1e-8) && near(pts.back().y, 0.0, 1e-8),
          "ellipse: end of full revolution near start");
  }

  // Test 9: Ellipse with rotation
  {
    dc::Vec2 center{0.0, 0.0};
    // 90 degree rotation: rx along Y, ry along X
    auto pts = dc::CurveTessellator::ellipse(center, 3.0, 2.0, PI * 0.5, 0.0, TWO_PI, 64);
    // At angle=0, cos(0)=1, sin(0)=0
    // x = 3*cos(0)*cos(PI/2) - 2*sin(0)*sin(PI/2) = 3*0 - 0 = 0
    // y = 3*cos(0)*sin(PI/2) + 2*sin(0)*cos(PI/2) = 3*1 + 0 = 3
    check(near(pts.front().x, 0.0, 1e-9) && near(pts.front().y, 3.0, 1e-9),
          "ellipse: 90-degree rotation start at (0, rx)");
  }

  // Test 10: Ellipse produces correct number of points
  {
    auto pts = dc::CurveTessellator::ellipse({0, 0}, 1.0, 2.0, 0.0, 0.0, TWO_PI, 100);
    check(pts.size() == 101, "ellipse: 100 segments -> 101 points");
  }

  // Test 11: Ellipse points lie on elliptical curve (un-rotated)
  {
    dc::Vec2 center{0.0, 0.0};
    double rx = 4.0, ry = 2.0;
    auto pts = dc::CurveTessellator::ellipse(center, rx, ry, 0.0, 0.0, TWO_PI, 200);
    bool allOnEllipse = true;
    for (auto& p : pts) {
      // (x/rx)^2 + (y/ry)^2 should equal 1
      double val = (p.x / rx) * (p.x / rx) + (p.y / ry) * (p.y / ry);
      if (!near(val, 1.0, 1e-9)) {
        allOnEllipse = false;
        break;
      }
    }
    check(allOnEllipse, "ellipse: all points satisfy ellipse equation");
  }

  // --- Catmull-Rom ---

  // Test 12: Catmull-Rom passes through all input points
  {
    std::vector<dc::Vec2> control = {
        {0.0, 0.0}, {1.0, 2.0}, {3.0, 1.0}, {5.0, 3.0}, {7.0, 0.0}};
    int segsPerSpan = 16;
    auto pts = dc::CurveTessellator::catmullRom(control, segsPerSpan);

    // The first point should match control[0]
    check(near(pts[0].x, 0.0) && near(pts[0].y, 0.0),
          "catmull-rom: passes through first point");

    // Each subsequent control point is at index (spanIndex * segsPerSpan)
    bool allPass = true;
    for (std::size_t i = 1; i < control.size(); ++i) {
      std::size_t idx = i * static_cast<std::size_t>(segsPerSpan);
      if (idx >= pts.size() ||
          !near(pts[idx].x, control[i].x, 1e-9) ||
          !near(pts[idx].y, control[i].y, 1e-9)) {
        allPass = false;
        break;
      }
    }
    check(allPass, "catmull-rom: passes through all control points");
  }

  // Test 13: Catmull-Rom output size is correct
  {
    std::vector<dc::Vec2> control = {{0, 0}, {1, 1}, {2, 0}, {3, 1}};
    int segsPerSpan = 8;
    auto pts = dc::CurveTessellator::catmullRom(control, segsPerSpan);
    // (N-1) spans * segsPerSpan + 1 = 3*8 + 1 = 25
    check(pts.size() == 25, "catmull-rom: 4 points, 8 segs/span -> 25 points");
  }

  // Test 14: Catmull-Rom with 2 points produces a straight line
  {
    std::vector<dc::Vec2> control = {{0.0, 0.0}, {4.0, 4.0}};
    auto pts = dc::CurveTessellator::catmullRom(control, 8);
    bool allOnLine = true;
    for (auto& p : pts) {
      if (!near(p.x, p.y, 1e-9)) {
        allOnLine = false;
        break;
      }
    }
    check(allOnLine, "catmull-rom: 2 points -> straight line");
  }

  // Test 15: Catmull-Rom with single point returns it
  {
    std::vector<dc::Vec2> control = {{5.0, 7.0}};
    auto pts = dc::CurveTessellator::catmullRom(control, 16);
    check(pts.size() == 1 && near(pts[0].x, 5.0) && near(pts[0].y, 7.0),
          "catmull-rom: single point returned as-is");
  }

  // Test 16: Catmull-Rom with collinear points produces straight segments
  {
    std::vector<dc::Vec2> control = {{0, 0}, {1, 1}, {2, 2}, {3, 3}};
    auto pts = dc::CurveTessellator::catmullRom(control, 10);
    bool allOnLine = true;
    for (auto& p : pts) {
      if (!near(p.x, p.y, 1e-6)) {
        allOnLine = false;
        break;
      }
    }
    check(allOnLine, "catmull-rom: collinear points -> straight line");
  }

  // Test 17: Arc with negative sweep (clockwise)
  {
    dc::Vec2 center{0.0, 0.0};
    double radius = 1.0;
    auto pts = dc::CurveTessellator::arc(center, radius, PI, 0.0, 32);
    // Start at angle PI -> (-1, 0)
    check(near(pts.front().x, -1.0, 1e-9) && near(pts.front().y, 0.0, 1e-9),
          "arc: negative sweep start at (-1, 0)");
    // End at angle 0 -> (1, 0)
    check(near(pts.back().x, 1.0, 1e-9) && near(pts.back().y, 0.0, 1e-9),
          "arc: negative sweep end at (1, 0)");
  }

  std::printf("=== D64.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
