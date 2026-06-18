// ENC-633 (D3) — BrushGesture implementation. See header.
#include "dc/interaction/BrushGesture.hpp"

namespace dc {

void BrushGesture::begin(double dataX, double dataY) {
  active_ = true;
  x0_ = x1_ = dataX;
  y0_ = y1_ = dataY;
}

bool BrushGesture::update(double dataX, double dataY) {
  if (!active_) return false;
  x1_ = dataX;
  y1_ = dataY;
  return live_ ? writeSignal() : false;
}

bool BrushGesture::end(double dataX, double dataY) {
  if (!active_) return false;
  x1_ = dataX;
  y1_ = dataY;
  const bool mutated = writeSignal();
  active_ = false;
  return mutated;
}

void BrushGesture::cancel() {
  active_ = false;
  if (signals_) signals_->clear(signalId_);
}

bool BrushGesture::writeSignal() {
  if (!signals_ || signalId_ == kInvalidId) return false;
  SignalValue v;
  switch (mode_) {
    case Mode::XInterval:
      v = IntervalSelection{field_, minX(), maxX()};
      break;
    case Mode::YInterval:
      v = IntervalSelection{field_, minY(), maxY()};
      break;
    case Mode::Rect:
      v = BrushRect{x0_, y0_, x1_, y1_};
      break;
  }
  // Define on first use, set thereafter — so callers need not pre-define.
  if (signals_->has(signalId_)) {
    signals_->set(signalId_, std::move(v));
  } else {
    signals_->define(signalId_, std::move(v));
  }
  return true;
}

}  // namespace dc
