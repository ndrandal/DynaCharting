#pragma once
#include "dc/scene/Scene.hpp"
#include <string>
#include <vector>

namespace dc {

struct DebugOverlayConfig {
  bool showBounds{true};
  bool showTransformAxes{true};
  bool showPaneRegions{true};
  std::uint32_t debugIdBase{900000};
  float boundsColor[4] = {0.0f, 1.0f, 0.0f, 0.5f};
  float paneColor[4] = {1.0f, 1.0f, 0.0f, 0.5f};
  float axisColor[4] = {1.0f, 0.0f, 0.0f, 0.5f};
};

class DebugOverlay {
public:
  std::vector<std::string> generateCommands(const Scene& scene,
                                            const DebugOverlayConfig& config,
                                            int viewW, int viewH);
  std::vector<std::string> disposeCommands();
  std::size_t debugItemCount() const;

private:
  std::vector<std::uint32_t> createdIds_;
  std::uint32_t nextOffset_{0};
};

} // namespace dc
