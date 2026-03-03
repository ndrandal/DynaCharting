#pragma once
#include "dc/ids/Id.hpp"
#include <cmath>
#include <cstdint>

namespace dc {

enum class DragDropPhase : std::uint8_t {
  Idle, Detecting, Dragging, Dropped
};

struct DragPayload {
  Id sourceDrawItemId{0};
  std::uint32_t recordIndex{0};
  double startDataX{0}, startDataY{0};
};

struct DropResult {
  DragPayload payload;
  Id targetDrawItemId{0};
  double dropDataX{0}, dropDataY{0};
  bool accepted{false};
};

class DragDropState {
public:
  void setThreshold(double px) { threshold_ = px; }
  double threshold() const { return threshold_; }

  void beginDetect(Id sourceDrawItemId, std::uint32_t recordIndex,
                   double px, double py, double dataX, double dataY);
  bool update(double px, double py, double dataX, double dataY);
  DropResult drop(Id targetDrawItemId, double dataX, double dataY);
  void cancel();

  DragDropPhase phase() const { return phase_; }
  const DragPayload& payload() const { return payload_; }

private:
  DragDropPhase phase_{DragDropPhase::Idle};
  DragPayload payload_;
  double threshold_{5.0};
  double startPx_{0}, startPy_{0};
  double currentDataX_{0}, currentDataY_{0};
};

} // namespace dc
