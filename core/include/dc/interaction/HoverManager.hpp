#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dc {

struct TooltipField {
  std::string label;    // e.g. "Open", "High", "Volume"
  std::string value;    // e.g. "42,150.50"
  float color[4] = {1, 1, 1, 1};  // color for this field (e.g., match series color)
};

struct TooltipData {
  bool visible{false};
  double dataX{0}, dataY{0};       // data-space position
  double screenX{0}, screenY{0};   // screen-space position for rendering
  std::uint32_t drawItemId{0};     // what's being hovered
  std::string title;                // e.g. "AAPL" or series name
  std::vector<TooltipField> fields;
};

// Callback for hover events
using HoverCallback = std::function<void(std::uint32_t drawItemId, double dataX, double dataY)>;
using TooltipProvider = std::function<TooltipData(std::uint32_t drawItemId, double dataX, double dataY)>;

class HoverManager {
public:
  // Update hover position (call on mouse move)
  // drawItemId comes from GPU picking or hit testing (0 = nothing)
  void update(std::uint32_t drawItemId, double dataX, double dataY,
              double screenX, double screenY);

  // Clear hover state (mouse left chart area)
  void clear();

  // Current hover state
  bool isHovering() const;
  std::uint32_t hoveredDrawItemId() const;
  double hoverDataX() const;
  double hoverDataY() const;

  // Tooltip
  const TooltipData& tooltip() const;

  // Register callback for hover enter/exit
  void setOnHoverEnter(HoverCallback cb);
  void setOnHoverExit(HoverCallback cb);

  // Register tooltip data provider
  // When a drawItem is hovered, the provider is called to get tooltip content
  void setTooltipProvider(TooltipProvider provider);

  // Configure hover behavior
  void setHoverDelay(double milliseconds);   // delay before tooltip shows (default 0)
  double hoverDelay() const;

  // Tooltip positioning
  enum class TooltipAnchor : std::uint8_t {
    CursorFollow = 0,   // tooltip follows cursor
    DataPoint = 1,      // tooltip anchored to data point
    Fixed = 2           // tooltip at fixed position (e.g., top-left)
  };
  void setTooltipAnchor(TooltipAnchor anchor);
  TooltipAnchor tooltipAnchor() const;

private:
  std::uint32_t currentDrawItemId_{0};
  double dataX_{0}, dataY_{0};
  double screenX_{0}, screenY_{0};
  TooltipData tooltip_;
  HoverCallback onEnter_, onExit_;
  TooltipProvider provider_;
  double hoverDelay_{0};
  TooltipAnchor anchor_{TooltipAnchor::CursorFollow};
  bool hovering_{false};
};

} // namespace dc
