#pragma once
#include <algorithm>
#include <cmath>

namespace dc {

struct MinimapConfig {
  double fullDataXMin{0};
  double fullDataXMax{1000};
  double fullDataYMin{0};
  double fullDataYMax{100};
};

struct MinimapViewWindow {
  // Normalized coordinates [0,1] within the minimap
  float x0{0}, y0{0}, x1{1}, y1{1};
};

class MinimapState {
public:
  void setFullRange(double xMin, double xMax, double yMin, double yMax);
  void setViewport(double viewXMin, double viewXMax, double viewYMin, double viewYMax);

  MinimapViewWindow viewWindow() const;

  // Hit test: is a normalized point inside the view window?
  bool hitTest(float nx, float ny) const;

  // Drag the viewport window to center on a normalized position
  void dragTo(float nx, float ny, double& outViewXMin, double& outViewXMax,
              double& outViewYMin, double& outViewYMax) const;

  const MinimapConfig& config() const { return config_; }

private:
  MinimapConfig config_;
  double viewXMin_{0}, viewXMax_{100};
  double viewYMin_{0}, viewYMax_{100};
};

} // namespace dc
