#pragma once
#include "dc/drawing/DrawingStore.hpp"
#include <cstdint>

namespace dc {

// D16.5: Drawing interaction state machine.
// States: Idle -> PlacingFirst -> PlacingSecond -> Idle
enum class DrawingMode : std::uint8_t {
  Idle = 0,
  PlacingTrendlineFirst,
  PlacingTrendlineSecond,
  PlacingHorizontalLevel,
  PlacingVerticalLine,       // D21.2
  PlacingRectangleFirst,     // D21.2
  PlacingRectangleSecond,    // D21.2
  PlacingFibFirst,           // D21.2
  PlacingFibSecond           // D21.2
};

class DrawingInteraction {
public:
  // Begin a new drawing creation.
  void beginTrendline();
  void beginHorizontalLevel();
  void beginVerticalLine();      // D21.2
  void beginRectangle();         // D21.2
  void beginFibRetracement();    // D21.2
  void cancel();

  // Process a click in data coordinates.
  // Returns the drawing ID if a drawing was completed, or 0.
  std::uint32_t onClick(double dataX, double dataY, DrawingStore& store);

  DrawingMode mode() const { return mode_; }
  bool isActive() const { return mode_ != DrawingMode::Idle; }

  // Preview point (for rendering feedback during placement)
  double previewX() const { return previewX_; }
  double previewY() const { return previewY_; }

private:
  DrawingMode mode_{DrawingMode::Idle};
  double firstX_{0}, firstY_{0};
  double previewX_{0}, previewY_{0};
};

} // namespace dc
