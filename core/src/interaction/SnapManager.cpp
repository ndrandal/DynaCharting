#include "dc/interaction/SnapManager.hpp"

#include <cmath>
#include <limits>

namespace dc {

void SnapManager::setMode(SnapMode mode) { mode_ = mode; }
SnapMode SnapManager::mode() const { return mode_; }

void SnapManager::setMagnetRadius(double radius) { magnetRadius_ = radius; }
double SnapManager::magnetRadius() const { return magnetRadius_; }

void SnapManager::setGridInterval(double intervalX, double intervalY) {
  gridIntervalX_ = intervalX;
  gridIntervalY_ = intervalY;
}
double SnapManager::gridIntervalX() const { return gridIntervalX_; }
double SnapManager::gridIntervalY() const { return gridIntervalY_; }

void SnapManager::setOHLCTargets(const double* data, std::size_t candleCount,
                                  std::size_t stride) {
  ohlcTargets_.clear();
  if (!data || candleCount == 0 || stride < 5) return;

  ohlcTargets_.reserve(candleCount * 4);
  for (std::size_t i = 0; i < candleCount; ++i) {
    const double* c = data + i * stride;
    double x     = c[0];
    double open  = c[1];
    double high  = c[2];
    double low   = c[3];
    double close = c[4];

    // Register 4 targets per candle: O, H, L, C
    ohlcTargets_.push_back({x, open,  0, 0});
    ohlcTargets_.push_back({x, high,  0, 0});
    ohlcTargets_.push_back({x, low,   0, 0});
    ohlcTargets_.push_back({x, close, 0, 0});
  }
}

void SnapManager::addTarget(SnapTarget target) {
  customTargets_.push_back(target);
}

void SnapManager::clearTargets() { customTargets_.clear(); }
void SnapManager::clearOHLCTargets() { ohlcTargets_.clear(); }

SnapResult SnapManager::snap(double dataX, double dataY,
                              double pixelPerDataX,
                              double pixelPerDataY) const {
  SnapResult unsnapped;
  unsnapped.x = dataX;
  unsnapped.y = dataY;
  unsnapped.snapped = false;
  unsnapped.distance = 0;
  unsnapped.targetSourceId = 0;

  if (mode_ == SnapMode::None) {
    return unsnapped;
  }

  SnapResult magnetResult = unsnapped;
  SnapResult gridResult   = unsnapped;
  bool hasMagnet = false;
  bool hasGrid   = false;

  if (mode_ == SnapMode::Magnet || mode_ == SnapMode::Both) {
    magnetResult = snapToTargets(dataX, dataY, pixelPerDataX, pixelPerDataY);
    hasMagnet = magnetResult.snapped;
  }

  if (mode_ == SnapMode::Grid || mode_ == SnapMode::Both) {
    gridResult = snapToGrid(dataX, dataY, pixelPerDataX, pixelPerDataY);
    hasGrid = gridResult.snapped;
  }

  if (mode_ == SnapMode::Both) {
    if (hasMagnet && hasGrid) {
      return (magnetResult.distance <= gridResult.distance)
               ? magnetResult
               : gridResult;
    }
    if (hasMagnet) return magnetResult;
    if (hasGrid) return gridResult;
    return unsnapped;
  }

  if (mode_ == SnapMode::Magnet) return magnetResult;
  if (mode_ == SnapMode::Grid) return gridResult;

  return unsnapped;
}

SnapResult SnapManager::snapToTargets(double dataX, double dataY,
                                       double pixelPerDataX,
                                       double pixelPerDataY) const {
  SnapResult result;
  result.x = dataX;
  result.y = dataY;
  result.snapped = false;
  result.distance = std::numeric_limits<double>::max();
  result.targetSourceId = 0;

  double bestDistSq = std::numeric_limits<double>::max();
  double radiusSq = magnetRadius_ * magnetRadius_;

  auto consider = [&](const SnapTarget& t) {
    // Convert data-space delta to pixel-space delta
    double dxPx = (t.x - dataX) * pixelPerDataX;
    double dyPx = (t.y - dataY) * pixelPerDataY;
    double distSq = dxPx * dxPx + dyPx * dyPx;
    if (distSq < bestDistSq && distSq <= radiusSq) {
      bestDistSq = distSq;
      result.x = t.x;
      result.y = t.y;
      result.snapped = true;
      result.distance = std::sqrt(distSq);
      result.targetSourceId = t.sourceId;
    }
  };

  for (const auto& t : ohlcTargets_) consider(t);
  for (const auto& t : customTargets_) consider(t);

  return result;
}

SnapResult SnapManager::snapToGrid(double dataX, double dataY,
                                    double pixelPerDataX,
                                    double pixelPerDataY) const {
  SnapResult result;
  result.x = dataX;
  result.y = dataY;
  result.snapped = false;
  result.distance = 0;
  result.targetSourceId = 0;

  if (gridIntervalX_ <= 0 && gridIntervalY_ <= 0) {
    return result;
  }

  double snappedX = dataX;
  double snappedY = dataY;

  if (gridIntervalX_ > 0) {
    snappedX = std::round(dataX / gridIntervalX_) * gridIntervalX_;
  }
  if (gridIntervalY_ > 0) {
    snappedY = std::round(dataY / gridIntervalY_) * gridIntervalY_;
  }

  double dxPx = (snappedX - dataX) * pixelPerDataX;
  double dyPx = (snappedY - dataY) * pixelPerDataY;
  double distPx = std::sqrt(dxPx * dxPx + dyPx * dyPx);

  // Grid snapping always succeeds (within radius check in Both mode is
  // handled by the caller comparing distances). For pure Grid mode we
  // always snap, regardless of radius.
  result.x = snappedX;
  result.y = snappedY;
  result.snapped = true;
  result.distance = distPx;

  return result;
}

} // namespace dc
