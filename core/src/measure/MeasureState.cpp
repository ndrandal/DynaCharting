#include "dc/measure/MeasureState.hpp"

namespace dc {

void MeasureState::begin(double dataX, double dataY) {
  active_ = true;
  hasSecond_ = false;
  x0_ = dataX;
  y0_ = dataY;
  x1_ = dataX;
  y1_ = dataY;
}

void MeasureState::update(double dataX, double dataY) {
  if (!active_) return;
  hasSecond_ = true;
  x1_ = dataX;
  y1_ = dataY;
}

MeasureResult MeasureState::finish(double dataX, double dataY) {
  if (!active_) return {};
  update(dataX, dataY);
  MeasureResult r;
  r.x0 = x0_;
  r.y0 = y0_;
  r.x1 = x1_;
  r.y1 = y1_;
  r.dx = x1_ - x0_;
  r.dy = y1_ - y0_;
  r.distance = std::sqrt(r.dx * r.dx + r.dy * r.dy);
  r.percentChange = (y0_ != 0.0) ? ((y1_ - y0_) / y0_ * 100.0) : 0.0;
  r.valid = true;
  active_ = false;
  hasSecond_ = false;
  return r;
}

void MeasureState::cancel() {
  active_ = false;
  hasSecond_ = false;
}

MeasureResult MeasureState::current() const {
  if (!active_ || !hasSecond_) return {};
  MeasureResult r;
  r.x0 = x0_;
  r.y0 = y0_;
  r.x1 = x1_;
  r.y1 = y1_;
  r.dx = x1_ - x0_;
  r.dy = y1_ - y0_;
  r.distance = std::sqrt(r.dx * r.dx + r.dy * r.dy);
  r.percentChange = (y0_ != 0.0) ? ((y1_ - y0_) / y0_ * 100.0) : 0.0;
  r.valid = true;
  return r;
}

} // namespace dc
