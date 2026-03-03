#include "dc/interaction/DragDropState.hpp"

namespace dc {

void DragDropState::beginDetect(Id sourceDrawItemId, std::uint32_t recordIndex,
                                 double px, double py,
                                 double dataX, double dataY) {
  phase_ = DragDropPhase::Detecting;
  payload_.sourceDrawItemId = sourceDrawItemId;
  payload_.recordIndex = recordIndex;
  payload_.startDataX = dataX;
  payload_.startDataY = dataY;
  startPx_ = px;
  startPy_ = py;
  currentDataX_ = dataX;
  currentDataY_ = dataY;
}

bool DragDropState::update(double px, double py, double dataX, double dataY) {
  currentDataX_ = dataX;
  currentDataY_ = dataY;

  if (phase_ == DragDropPhase::Detecting) {
    double dx = px - startPx_;
    double dy = py - startPy_;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist >= threshold_) {
      phase_ = DragDropPhase::Dragging;
      return true;
    }
  }
  return false;
}

DropResult DragDropState::drop(Id targetDrawItemId, double dataX, double dataY) {
  DropResult result;
  result.payload = payload_;
  result.targetDrawItemId = targetDrawItemId;
  result.dropDataX = dataX;
  result.dropDataY = dataY;
  result.accepted = (phase_ == DragDropPhase::Dragging);
  phase_ = DragDropPhase::Dropped;
  return result;
}

void DragDropState::cancel() {
  phase_ = DragDropPhase::Idle;
  payload_ = {};
}

} // namespace dc
