#include "dc/geometry/CurveTessellator.hpp"

#include <cmath>

namespace dc {

// ---------- Quadratic Bezier ----------
// B(t) = (1-t)^2 * P0 + 2*(1-t)*t * P1 + t^2 * P2

std::vector<Vec2> CurveTessellator::quadraticBezier(Vec2 p0, Vec2 p1, Vec2 p2,
                                                    int segments) {
  if (segments < 1) segments = 1;

  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(segments) + 1);

  for (int i = 0; i <= segments; ++i) {
    double t = static_cast<double>(i) / static_cast<double>(segments);
    double u = 1.0 - t;

    double x = u * u * p0.x + 2.0 * u * t * p1.x + t * t * p2.x;
    double y = u * u * p0.y + 2.0 * u * t * p1.y + t * t * p2.y;
    result.push_back({x, y});
  }

  return result;
}

// ---------- Cubic Bezier ----------
// B(t) = (1-t)^3 * P0 + 3*(1-t)^2*t * P1 + 3*(1-t)*t^2 * P2 + t^3 * P3

std::vector<Vec2> CurveTessellator::cubicBezier(Vec2 p0, Vec2 p1, Vec2 p2,
                                                Vec2 p3, int segments) {
  if (segments < 1) segments = 1;

  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(segments) + 1);

  for (int i = 0; i <= segments; ++i) {
    double t = static_cast<double>(i) / static_cast<double>(segments);
    double u = 1.0 - t;
    double u2 = u * u;
    double u3 = u2 * u;
    double t2 = t * t;
    double t3 = t2 * t;

    double x = u3 * p0.x + 3.0 * u2 * t * p1.x + 3.0 * u * t2 * p2.x +
               t3 * p3.x;
    double y = u3 * p0.y + 3.0 * u2 * t * p1.y + 3.0 * u * t2 * p2.y +
               t3 * p3.y;
    result.push_back({x, y});
  }

  return result;
}

// ---------- Circular Arc ----------

std::vector<Vec2> CurveTessellator::arc(Vec2 center, double radius,
                                        double startAngle, double endAngle,
                                        int segments) {
  if (segments < 1) segments = 1;

  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(segments) + 1);

  double sweep = endAngle - startAngle;

  for (int i = 0; i <= segments; ++i) {
    double t = static_cast<double>(i) / static_cast<double>(segments);
    double angle = startAngle + t * sweep;

    double x = center.x + radius * std::cos(angle);
    double y = center.y + radius * std::sin(angle);
    result.push_back({x, y});
  }

  return result;
}

// ---------- Ellipse ----------
// Parametric form with rotation:
//   x = cx + rx*cos(t)*cos(rot) - ry*sin(t)*sin(rot)
//   y = cy + rx*cos(t)*sin(rot) + ry*sin(t)*cos(rot)

std::vector<Vec2> CurveTessellator::ellipse(Vec2 center, double rx, double ry,
                                            double rotation,
                                            double startAngle,
                                            double endAngle, int segments) {
  if (segments < 1) segments = 1;

  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(segments) + 1);

  double cosRot = std::cos(rotation);
  double sinRot = std::sin(rotation);
  double sweep = endAngle - startAngle;

  for (int i = 0; i <= segments; ++i) {
    double t = static_cast<double>(i) / static_cast<double>(segments);
    double angle = startAngle + t * sweep;

    double cosA = std::cos(angle);
    double sinA = std::sin(angle);

    double x = center.x + rx * cosA * cosRot - ry * sinA * sinRot;
    double y = center.y + rx * cosA * sinRot + ry * sinA * cosRot;
    result.push_back({x, y});
  }

  return result;
}

// ---------- Adaptive Cubic Bezier ----------
// Uses De Casteljau subdivision. Flatness is measured as the max distance
// of the control points from the baseline (chord from p0 to p3).

static double pointToLineDistance(Vec2 p, Vec2 a, Vec2 b) {
  double dx = b.x - a.x;
  double dy = b.y - a.y;
  double lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-15) {
    // a and b are the same point
    double ex = p.x - a.x;
    double ey = p.y - a.y;
    return std::sqrt(ex * ex + ey * ey);
  }
  // Perpendicular distance using cross product
  double cross = std::fabs((p.x - a.x) * dy - (p.y - a.y) * dx);
  return cross / std::sqrt(lenSq);
}

void CurveTessellator::subdivide(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                                 double tolerance, std::vector<Vec2>& out,
                                 int depth) {
  // Safety guard against infinite recursion
  if (depth > 20) {
    out.push_back(p3);
    return;
  }

  // Flatness check: max distance of control points p1, p2 from chord p0->p3
  double d1 = pointToLineDistance(p1, p0, p3);
  double d2 = pointToLineDistance(p2, p0, p3);

  if (d1 <= tolerance && d2 <= tolerance) {
    // Flat enough — emit endpoint
    out.push_back(p3);
    return;
  }

  // De Casteljau split at t=0.5
  Vec2 m01 = {(p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5};
  Vec2 m12 = {(p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5};
  Vec2 m23 = {(p2.x + p3.x) * 0.5, (p2.y + p3.y) * 0.5};

  Vec2 m012 = {(m01.x + m12.x) * 0.5, (m01.y + m12.y) * 0.5};
  Vec2 m123 = {(m12.x + m23.x) * 0.5, (m12.y + m23.y) * 0.5};

  Vec2 mid = {(m012.x + m123.x) * 0.5, (m012.y + m123.y) * 0.5};

  subdivide(p0, m01, m012, mid, tolerance, out, depth + 1);
  subdivide(mid, m123, m23, p3, tolerance, out, depth + 1);
}

std::vector<Vec2> CurveTessellator::cubicBezierAdaptive(Vec2 p0, Vec2 p1,
                                                        Vec2 p2, Vec2 p3,
                                                        double tolerance) {
  std::vector<Vec2> result;
  result.push_back(p0);
  subdivide(p0, p1, p2, p3, tolerance, result, 0);
  return result;
}

// ---------- Catmull-Rom Spline ----------
// Standard Catmull-Rom using 4-point basis per span.
// For N input points there are (N-1) spans, but the outer two half-spans use
// reflected control points so the spline passes through all points.

std::vector<Vec2> CurveTessellator::catmullRom(const std::vector<Vec2>& points,
                                               int segmentsPerSpan) {
  if (points.size() < 2) return points;
  if (segmentsPerSpan < 1) segmentsPerSpan = 1;

  std::vector<Vec2> result;

  // Build extended point list with reflected endpoints for open spline
  // p[-1] = 2*p[0] - p[1], p[n] = 2*p[n-1] - p[n-2]
  std::size_t n = points.size();
  std::vector<Vec2> ext;
  ext.reserve(n + 2);

  // Reflected point before start
  ext.push_back({2.0 * points[0].x - points[1].x,
                 2.0 * points[0].y - points[1].y});

  for (std::size_t i = 0; i < n; ++i) {
    ext.push_back(points[i]);
  }

  // Reflected point after end
  ext.push_back({2.0 * points[n - 1].x - points[n - 2].x,
                 2.0 * points[n - 1].y - points[n - 2].y});

  // Now ext[1..n] are the original points, ext[0] and ext[n+1] are reflected.
  // For each span between ext[i] and ext[i+1] (i in [1, n-1]), use
  // ext[i-1], ext[i], ext[i+1], ext[i+2] as the four control points.

  result.push_back(points[0]);

  for (std::size_t i = 1; i < n; ++i) {
    Vec2 pm1 = ext[i - 1];
    Vec2 p0i = ext[i];
    Vec2 p1i = ext[i + 1];
    Vec2 p2i = ext[i + 2];

    for (int s = 1; s <= segmentsPerSpan; ++s) {
      double t = static_cast<double>(s) / static_cast<double>(segmentsPerSpan);
      double t2 = t * t;
      double t3 = t2 * t;

      // Catmull-Rom basis:
      // q(t) = 0.5 * ((2*P1) +
      //                (-P0 + P2) * t +
      //                (2*P0 - 5*P1 + 4*P2 - P3) * t^2 +
      //                (-P0 + 3*P1 - 3*P2 + P3) * t^3)
      double x = 0.5 * ((2.0 * p0i.x) + (-pm1.x + p1i.x) * t +
                         (2.0 * pm1.x - 5.0 * p0i.x + 4.0 * p1i.x -
                          p2i.x) *
                             t2 +
                         (-pm1.x + 3.0 * p0i.x - 3.0 * p1i.x + p2i.x) * t3);
      double y = 0.5 * ((2.0 * p0i.y) + (-pm1.y + p1i.y) * t +
                         (2.0 * pm1.y - 5.0 * p0i.y + 4.0 * p1i.y -
                          p2i.y) *
                             t2 +
                         (-pm1.y + 3.0 * p0i.y - 3.0 * p1i.y + p2i.y) * t3);
      result.push_back({x, y});
    }
  }

  return result;
}

} // namespace dc
