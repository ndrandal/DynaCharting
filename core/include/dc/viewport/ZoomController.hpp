#pragma once
#include "dc/viewport/Viewport.hpp"
#include "dc/viewport/InputState.hpp"

namespace dc {

// D15.1: Keyboard navigation + zoom-to-fit controller.
// Processes KeyCode events and applies viewport changes.
// Mouse wheel zoom and drag-pan are already handled by InputMapper.
struct ZoomControllerConfig {
  double panFraction{0.1};     // arrow keys: pan by 10% of visible range
  double zoomFraction{0.2};    // +/-: zoom by 20%
  double fitMargin{0.05};      // 5% margin on zoom-to-fit
};

class ZoomController {
public:
  void setConfig(const ZoomControllerConfig& cfg) { config_ = cfg; }

  // Process keyboard input. Returns true if viewport changed.
  bool processKey(KeyCode key, Viewport& vp) const;

  // D15.3: Zoom to fit the given data bounds with margin.
  void zoomToFit(Viewport& vp,
                  double dataXMin, double dataXMax,
                  double dataYMin, double dataYMax) const;

  // Pan by fraction of visible range.
  static void panByFraction(Viewport& vp, double fracX, double fracY);

  // Zoom by fraction centered on viewport center.
  static void zoomByFraction(Viewport& vp, double fraction);

private:
  ZoomControllerConfig config_;
};

} // namespace dc
