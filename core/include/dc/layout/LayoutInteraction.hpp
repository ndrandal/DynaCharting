#pragma once
#include "dc/viewport/InputState.hpp"

namespace dc {

class LayoutManager;

struct LayoutInteractionConfig {
  float hitTolerancePx{8.0f};
};

class LayoutInteraction {
public:
  void setConfig(const LayoutInteractionConfig& cfg);
  void setLayoutManager(LayoutManager* lm);
  void setPixelViewport(int fbWidth, int fbHeight);

  bool processInput(const ViewportInputState& input);
  bool isDragging() const { return dragging_; }
  int hoveredDivider() const { return hoveredDivider_; }

private:
  LayoutInteractionConfig config_;
  LayoutManager* lm_{nullptr};
  int fbW_{800};
  int fbH_{600};
  bool dragging_{false};
  int hoveredDivider_{-1};
  int dragDivider_{-1};
};

} // namespace dc
