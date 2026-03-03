#pragma once
#include <cstdint>
#include <cmath>
#include <vector>

namespace dc {

// D65: Variable-point-count drawings — freehand strokes and polylines.

struct StrokePoint {
  double x{0};
  double y{0};
  double pressure{1.0};   // 0-1, default 1.0
  double timestamp{0};     // seconds, for velocity-based simplification
};

struct Stroke {
  std::uint32_t id{0};
  std::vector<StrokePoint> points;
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float lineWidth{2.0f};
  bool closed{false};      // true = polygon, false = polyline
};

class FreehandCapture {
public:
  // Start capturing a new stroke
  void begin(double x, double y, double pressure = 1.0);

  // Add a point during mouse/touch move
  void addPoint(double x, double y, double pressure = 1.0);

  // Finish and return the completed stroke
  Stroke finish();

  // Cancel current capture
  void cancel();

  bool isCapturing() const;
  const std::vector<StrokePoint>& currentPoints() const;

  // Simplify a stroke using Ramer-Douglas-Peucker algorithm
  static std::vector<StrokePoint> simplify(const std::vector<StrokePoint>& points, double epsilon);

  // Smooth a stroke using moving average
  static std::vector<StrokePoint> smooth(const std::vector<StrokePoint>& points, int windowSize = 3);

private:
  bool capturing_{false};
  std::vector<StrokePoint> points_;
  std::uint32_t nextId_{1};
};

class StrokeStore {
public:
  std::uint32_t add(Stroke stroke);
  void remove(std::uint32_t id);
  void clear();
  const Stroke* get(std::uint32_t id) const;
  const std::vector<Stroke>& strokes() const;
  std::size_t count() const;

private:
  std::vector<Stroke> strokes_;
};

} // namespace dc
