#include "dc/drawing/FreehandCapture.hpp"
#include <algorithm>
#include <cmath>

namespace dc {

// ============================================================
// FreehandCapture
// ============================================================

void FreehandCapture::begin(double x, double y, double pressure) {
  capturing_ = true;
  points_.clear();
  StrokePoint pt;
  pt.x = x;
  pt.y = y;
  pt.pressure = pressure;
  pt.timestamp = 0.0;
  points_.push_back(pt);
}

void FreehandCapture::addPoint(double x, double y, double pressure) {
  if (!capturing_) return;
  StrokePoint pt;
  pt.x = x;
  pt.y = y;
  pt.pressure = pressure;
  pt.timestamp = 0.0;
  points_.push_back(pt);
}

Stroke FreehandCapture::finish() {
  Stroke s;
  if (capturing_) {
    s.id = nextId_++;
    s.points = std::move(points_);
  }
  capturing_ = false;
  points_.clear();
  return s;
}

void FreehandCapture::cancel() {
  capturing_ = false;
  points_.clear();
}

bool FreehandCapture::isCapturing() const {
  return capturing_;
}

const std::vector<StrokePoint>& FreehandCapture::currentPoints() const {
  return points_;
}

// ============================================================
// Ramer-Douglas-Peucker simplification
// ============================================================

// Perpendicular distance from point p to line segment (a, b).
static double perpendicularDistance(const StrokePoint& p,
                                   const StrokePoint& a,
                                   const StrokePoint& b) {
  double dx = b.x - a.x;
  double dy = b.y - a.y;
  double lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-18) {
    // a and b are essentially the same point
    double ex = p.x - a.x;
    double ey = p.y - a.y;
    return std::sqrt(ex * ex + ey * ey);
  }
  // Signed area of the parallelogram = |cross product|
  double cross = std::abs(dy * (p.x - a.x) - dx * (p.y - a.y));
  return cross / std::sqrt(lenSq);
}

// Recursive RDP helper. Marks which indices to keep.
static void rdpRecurse(const std::vector<StrokePoint>& pts,
                       std::size_t first, std::size_t last,
                       double epsilon,
                       std::vector<bool>& keep) {
  if (last <= first + 1) return;

  double maxDist = 0.0;
  std::size_t maxIdx = first;

  for (std::size_t i = first + 1; i < last; ++i) {
    double d = perpendicularDistance(pts[i], pts[first], pts[last]);
    if (d > maxDist) {
      maxDist = d;
      maxIdx = i;
    }
  }

  if (maxDist > epsilon) {
    keep[maxIdx] = true;
    rdpRecurse(pts, first, maxIdx, epsilon, keep);
    rdpRecurse(pts, maxIdx, last, epsilon, keep);
  }
}

std::vector<StrokePoint> FreehandCapture::simplify(
    const std::vector<StrokePoint>& points, double epsilon) {
  if (points.size() <= 2) return points;

  std::vector<bool> keep(points.size(), false);
  keep.front() = true;
  keep.back() = true;

  rdpRecurse(points, 0, points.size() - 1, epsilon, keep);

  std::vector<StrokePoint> result;
  result.reserve(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    if (keep[i]) result.push_back(points[i]);
  }
  return result;
}

// ============================================================
// Moving-average smoothing
// ============================================================

std::vector<StrokePoint> FreehandCapture::smooth(
    const std::vector<StrokePoint>& points, int windowSize) {
  if (points.size() <= 2 || windowSize < 2) return points;

  std::vector<StrokePoint> result;
  result.reserve(points.size());

  int halfW = windowSize / 2;
  int n = static_cast<int>(points.size());

  for (int i = 0; i < n; ++i) {
    // Keep first and last points as anchors
    if (i == 0 || i == n - 1) {
      result.push_back(points[static_cast<std::size_t>(i)]);
      continue;
    }

    int lo = std::max(0, i - halfW);
    int hi = std::min(n - 1, i + halfW);
    double sumX = 0, sumY = 0, sumP = 0;
    int count = 0;
    for (int j = lo; j <= hi; ++j) {
      sumX += points[static_cast<std::size_t>(j)].x;
      sumY += points[static_cast<std::size_t>(j)].y;
      sumP += points[static_cast<std::size_t>(j)].pressure;
      ++count;
    }
    StrokePoint sp;
    sp.x = sumX / count;
    sp.y = sumY / count;
    sp.pressure = sumP / count;
    sp.timestamp = points[static_cast<std::size_t>(i)].timestamp;
    result.push_back(sp);
  }

  return result;
}

// ============================================================
// StrokeStore
// ============================================================

std::uint32_t StrokeStore::add(Stroke stroke) {
  // If the stroke already has an id > 0 use it; otherwise it is uninitialized
  // (the caller obtained it from FreehandCapture::finish which assigns an id).
  strokes_.push_back(std::move(stroke));
  return strokes_.back().id;
}

void StrokeStore::remove(std::uint32_t id) {
  strokes_.erase(
    std::remove_if(strokes_.begin(), strokes_.end(),
      [id](const Stroke& s) { return s.id == id; }),
    strokes_.end());
}

void StrokeStore::clear() {
  strokes_.clear();
}

const Stroke* StrokeStore::get(std::uint32_t id) const {
  for (const auto& s : strokes_) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

const std::vector<Stroke>& StrokeStore::strokes() const {
  return strokes_;
}

std::size_t StrokeStore::count() const {
  return strokes_.size();
}

} // namespace dc
