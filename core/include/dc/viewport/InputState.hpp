#pragma once
#include <cstdint>

namespace dc {

enum class KeyCode : std::uint8_t {
  None = 0, Left, Right, Up, Down, Home, End
};

// Generic input snapshot — NOT GLFW-specific
struct ViewportInputState {
  double cursorX{0}, cursorY{0};  // pixels, 0=left/top
  double dragDx{0}, dragDy{0};   // pixel deltas this frame
  double scrollDelta{0};          // positive = zoom in
  bool dragging{false};
  bool clicked{false};                // one-frame true on mouse release without significant drag
  KeyCode keyPressed{KeyCode::None};  // key event this frame
};

struct InputMapperConfig {
  double zoomSensitivity{0.1};
  bool linkXAxis{false};  // pan/zoom X applies to all viewports
};

} // namespace dc
