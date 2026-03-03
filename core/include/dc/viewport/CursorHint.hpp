#pragma once
#include <cstdint>

namespace dc {

enum class CursorHint : std::uint8_t {
  Default, Pointer, Crosshair, Grab, Grabbing,
  ResizeH, ResizeV, ResizeNESW, ResizeNWSE, Text
};

struct CursorHintContext {
  bool isOverDrawItem{false};
  bool isDragging{false};
  bool isOverLayoutSplitter{false};
  bool isDrawingMode{false};
  bool isMeasuring{false};
  bool splitterVertical{false};
};

class CursorHintProvider {
public:
  static CursorHint resolve(const CursorHintContext& ctx);
};

} // namespace dc
