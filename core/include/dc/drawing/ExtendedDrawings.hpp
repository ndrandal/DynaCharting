#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// D69: Extended drawing types for multi-point drawings.
// This is a SEPARATE system from DrawingStore/DrawingInteraction (which handles
// 1-2 point drawings). ExtDrawingInteraction supports variable-point drawings
// (3+ clicks, or N-click with double-click to finalize).

enum class ExtDrawingType : std::uint8_t {
  Pitchfork = 10,        // 3 points: pivot + two endpoints
  ParallelChannel = 11,  // 3 points: line + width
  Polygon = 12,          // N points, closed shape
  Polyline = 13,         // N points, open path
  Ray = 14,              // 2 points, extends infinitely in one direction
  ExtendedLine = 15,     // 2 points, extends infinitely both directions
  FibExtension = 16,     // 3 points: A,B,C for fib projection
  Arrow = 17,            // 2 points with arrowhead
  AnchoredNote = 18      // 1 point + text
};

struct ExtDrawing {
  std::uint32_t id{0};
  ExtDrawingType type{};
  std::vector<double> pointsX;  // variable number of points
  std::vector<double> pointsY;
  float color[4] = {1.0f, 1.0f, 0.0f, 1.0f};
  float lineWidth{2.0f};
  std::string text;  // for AnchoredNote

  std::size_t pointCount() const { return pointsX.size(); }
};

// How many clicks does each type require? (0 = variable, finalize with double-click)
inline int requiredClicks(ExtDrawingType type) {
  switch (type) {
    case ExtDrawingType::AnchoredNote: return 1;
    case ExtDrawingType::Ray:
    case ExtDrawingType::ExtendedLine:
    case ExtDrawingType::Arrow: return 2;
    case ExtDrawingType::Pitchfork:
    case ExtDrawingType::ParallelChannel:
    case ExtDrawingType::FibExtension: return 3;
    case ExtDrawingType::Polygon:
    case ExtDrawingType::Polyline: return 0; // variable
    default: return 2;
  }
}

class ExtDrawingInteraction {
public:
  void begin(ExtDrawingType type);
  void cancel();

  // Click adds a point. Returns drawing ID when complete (>0) or 0 if more points needed.
  std::uint32_t onClick(double dataX, double dataY);

  // Double-click finalizes variable-point drawings (Polygon, Polyline).
  // Adds the final point and creates the drawing. Returns drawing ID or 0.
  std::uint32_t onDoubleClick(double dataX, double dataY);

  bool isActive() const;
  ExtDrawingType activeType() const;
  std::size_t currentPointCount() const;

  // Preview: current mouse position for rubber-band rendering
  void updatePreview(double dataX, double dataY);
  double previewX() const;
  double previewY() const;

  // Access completed drawings
  const std::vector<ExtDrawing>& drawings() const;
  const ExtDrawing* get(std::uint32_t id) const;
  void remove(std::uint32_t id);
  void clear();

private:
  ExtDrawingType activeType_{};
  bool active_{false};
  std::vector<double> tempX_, tempY_;
  double previewX_{0}, previewY_{0};
  std::vector<ExtDrawing> drawings_;
  std::uint32_t nextId_{1};

  // Helper: finalize the current temp points into a drawing
  std::uint32_t finalize();
};

} // namespace dc
