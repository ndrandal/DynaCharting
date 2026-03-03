#pragma once
#include <vector>

namespace dc {

struct Vec2 {
  double x, y;
};

class CurveTessellator {
public:
  // Quadratic Bezier (3 control points) -> polyline
  static std::vector<Vec2> quadraticBezier(Vec2 p0, Vec2 p1, Vec2 p2,
                                           int segments = 32);

  // Cubic Bezier (4 control points) -> polyline
  static std::vector<Vec2> cubicBezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                                       int segments = 32);

  // Circular arc (center, radius, startAngle, endAngle in radians) -> polyline
  static std::vector<Vec2> arc(Vec2 center, double radius, double startAngle,
                               double endAngle, int segments = 32);

  // Ellipse (center, radiusX, radiusY, rotation, startAngle, endAngle) -> polyline
  static std::vector<Vec2> ellipse(Vec2 center, double rx, double ry,
                                   double rotation = 0.0,
                                   double startAngle = 0.0,
                                   double endAngle = 6.283185307179586,
                                   int segments = 64);

  // Adaptive cubic bezier (subdivides based on flatness tolerance)
  static std::vector<Vec2> cubicBezierAdaptive(Vec2 p0, Vec2 p1, Vec2 p2,
                                               Vec2 p3,
                                               double tolerance = 0.5);

  // Catmull-Rom spline through points
  static std::vector<Vec2> catmullRom(const std::vector<Vec2>& points,
                                      int segmentsPerSpan = 16);

private:
  // Recursive subdivision helper for adaptive bezier
  static void subdivide(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double tolerance,
                        std::vector<Vec2>& out, int depth);
};

} // namespace dc
