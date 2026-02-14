#pragma once
#include "dc/viewport/InputState.hpp"
#include "dc/viewport/Viewport.hpp"
#include <vector>

namespace dc {

class InputMapper {
public:
  void setConfig(const InputMapperConfig& cfg);
  void setViewports(std::vector<Viewport*> viewports);

  // Process input, apply to appropriate viewport. Returns true if any viewport changed.
  bool processInput(const ViewportInputState& input);

  // Currently active viewport (cursor is inside), or nullptr.
  Viewport* activeViewport() const { return active_; }

  // Cursor position in clip/data coords of active viewport.
  // Returns false if no active viewport.
  bool cursorClip(double& cx, double& cy) const;
  bool cursorData(double& dx, double& dy) const;

  // Click detection (D11.1)
  bool hasClick() const;
  bool clickData(double& dx, double& dy) const;
  bool clickPixel(double& px, double& py) const;

private:
  InputMapperConfig config_;
  std::vector<Viewport*> viewports_;
  Viewport* active_{nullptr};
  double lastCursorX_{0}, lastCursorY_{0};
  bool hasClick_{false};
  double clickPx_{0}, clickPy_{0};
};

} // namespace dc
