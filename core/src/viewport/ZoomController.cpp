#include "dc/viewport/ZoomController.hpp"
#include <cmath>

namespace dc {

bool ZoomController::processKey(KeyCode key, Viewport& vp) const {
  switch (key) {
    case KeyCode::Left:
      panByFraction(vp, -config_.panFraction, 0.0);
      return true;
    case KeyCode::Right:
      panByFraction(vp, config_.panFraction, 0.0);
      return true;
    case KeyCode::Up:
      panByFraction(vp, 0.0, config_.panFraction);
      return true;
    case KeyCode::Down:
      panByFraction(vp, 0.0, -config_.panFraction);
      return true;
    case KeyCode::Home:
      // Zoom in (handled by caller providing zoom-to-fit)
      zoomByFraction(vp, config_.zoomFraction);
      return true;
    case KeyCode::End:
      // Zoom out
      zoomByFraction(vp, -config_.zoomFraction);
      return true;
    case KeyCode::None:
    default:
      return false;
  }
}

void ZoomController::zoomToFit(Viewport& vp,
                                double dataXMin, double dataXMax,
                                double dataYMin, double dataYMax) const {
  double xRange = dataXMax - dataXMin;
  double yRange = dataYMax - dataYMin;

  if (xRange <= 0.0) xRange = 1.0;
  if (yRange <= 0.0) yRange = 1.0;

  double mx = xRange * config_.fitMargin;
  double my = yRange * config_.fitMargin;

  vp.setDataRange(dataXMin - mx, dataXMax + mx,
                   dataYMin - my, dataYMax + my);
}

void ZoomController::panByFraction(Viewport& vp, double fracX, double fracY) {
  const auto& dr = vp.dataRange();
  double dx = (dr.xMax - dr.xMin) * fracX;
  double dy = (dr.yMax - dr.yMin) * fracY;
  vp.setDataRange(dr.xMin + dx, dr.xMax + dx,
                   dr.yMin + dy, dr.yMax + dy);
}

void ZoomController::zoomByFraction(Viewport& vp, double fraction) {
  const auto& dr = vp.dataRange();
  double cx = (dr.xMin + dr.xMax) * 0.5;
  double cy = (dr.yMin + dr.yMax) * 0.5;

  double scale = std::exp(-fraction);
  vp.setDataRange(
    cx + (dr.xMin - cx) * scale,
    cx + (dr.xMax - cx) * scale,
    cy + (dr.yMin - cy) * scale,
    cy + (dr.yMax - cy) * scale);
}

} // namespace dc
