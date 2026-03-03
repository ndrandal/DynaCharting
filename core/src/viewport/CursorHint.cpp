#include "dc/viewport/CursorHint.hpp"

namespace dc {

CursorHint CursorHintProvider::resolve(const CursorHintContext& ctx) {
  if (ctx.isDragging) return CursorHint::Grabbing;
  if (ctx.isOverLayoutSplitter) {
    return ctx.splitterVertical ? CursorHint::ResizeH : CursorHint::ResizeV;
  }
  if (ctx.isDrawingMode) return CursorHint::Crosshair;
  if (ctx.isMeasuring) return CursorHint::Crosshair;
  if (ctx.isOverDrawItem) return CursorHint::Pointer;
  return CursorHint::Default;
}

} // namespace dc
