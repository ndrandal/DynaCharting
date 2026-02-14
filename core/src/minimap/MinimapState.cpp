#include "dc/minimap/MinimapState.hpp"

namespace dc {

void MinimapState::setFullRange(double xMin, double xMax, double yMin, double yMax) {
  config_.fullDataXMin = xMin;
  config_.fullDataXMax = xMax;
  config_.fullDataYMin = yMin;
  config_.fullDataYMax = yMax;
}

void MinimapState::setViewport(double vxMin, double vxMax, double vyMin, double vyMax) {
  viewXMin_ = vxMin;
  viewXMax_ = vxMax;
  viewYMin_ = vyMin;
  viewYMax_ = vyMax;
}

MinimapViewWindow MinimapState::viewWindow() const {
  double fullW = config_.fullDataXMax - config_.fullDataXMin;
  double fullH = config_.fullDataYMax - config_.fullDataYMin;
  if (fullW <= 0) fullW = 1;
  if (fullH <= 0) fullH = 1;

  MinimapViewWindow w;
  w.x0 = static_cast<float>((viewXMin_ - config_.fullDataXMin) / fullW);
  w.x1 = static_cast<float>((viewXMax_ - config_.fullDataXMin) / fullW);
  w.y0 = static_cast<float>((viewYMin_ - config_.fullDataYMin) / fullH);
  w.y1 = static_cast<float>((viewYMax_ - config_.fullDataYMin) / fullH);

  // Clamp to [0,1]
  w.x0 = std::max(0.0f, std::min(1.0f, w.x0));
  w.x1 = std::max(0.0f, std::min(1.0f, w.x1));
  w.y0 = std::max(0.0f, std::min(1.0f, w.y0));
  w.y1 = std::max(0.0f, std::min(1.0f, w.y1));
  return w;
}

bool MinimapState::hitTest(float nx, float ny) const {
  auto w = viewWindow();
  return nx >= w.x0 && nx <= w.x1 && ny >= w.y0 && ny <= w.y1;
}

void MinimapState::dragTo(float nx, float ny, double& outVXMin, double& outVXMax,
                           double& outVYMin, double& outVYMax) const {
  double fullW = config_.fullDataXMax - config_.fullDataXMin;
  double fullH = config_.fullDataYMax - config_.fullDataYMin;
  double viewW = viewXMax_ - viewXMin_;
  double viewH = viewYMax_ - viewYMin_;

  double centerX = config_.fullDataXMin + nx * fullW;
  double centerY = config_.fullDataYMin + ny * fullH;

  outVXMin = centerX - viewW * 0.5;
  outVXMax = centerX + viewW * 0.5;
  outVYMin = centerY - viewH * 0.5;
  outVYMax = centerY + viewH * 0.5;
}

} // namespace dc
