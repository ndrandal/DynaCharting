#pragma once
#include <cmath>

namespace dc {

struct MeasureResult {
  double x0{0}, y0{0};     // start point (data space)
  double x1{0}, y1{0};     // end point (data space)
  double dx{0}, dy{0};     // deltas
  double distance{0};      // Euclidean distance in data space
  double percentChange{0}; // (y1-y0)/y0 * 100
  bool valid{false};
};

class MeasureState {
public:
  // Begin measurement from a data-space point
  void begin(double dataX, double dataY);

  // Update the second point (while dragging / hovering)
  void update(double dataX, double dataY);

  // Finish measurement
  MeasureResult finish(double dataX, double dataY);

  // Cancel current measurement
  void cancel();

  // Query state
  bool isActive() const { return active_; }
  MeasureResult current() const; // returns current measurement (valid if active and has 2nd point)

private:
  bool active_{false};
  bool hasSecond_{false};
  double x0_{0}, y0_{0};
  double x1_{0}, y1_{0};
};

} // namespace dc
