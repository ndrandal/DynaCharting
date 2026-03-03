#pragma once
#include "dc/ids/Id.hpp"

#include <string>
#include <vector>

namespace dc {

struct Theme {
  std::string name;

  // Background
  float backgroundColor[4] = {0.1f, 0.1f, 0.12f, 1.0f};

  // Candle colors
  float candleUp[4] = {0.0f, 0.8f, 0.4f, 1.0f};
  float candleDown[4] = {0.9f, 0.2f, 0.2f, 1.0f};

  // Grid/axis
  float gridColor[4] = {0.2f, 0.2f, 0.25f, 1.0f};
  float tickColor[4] = {0.4f, 0.4f, 0.45f, 1.0f};
  float labelColor[4] = {0.7f, 0.7f, 0.75f, 1.0f};
  float gridLineWidth{1.0f};
  float tickLineWidth{1.0f};

  // D78: Grid dash pattern (0 = solid)
  float gridDashLength{0.0f};
  float gridGapLength{0.0f};
  float gridOpacity{1.0f};  // multiplied into gridColor alpha

  // Crosshair / interactive
  float crosshairColor[4] = {0.8f, 0.8f, 0.85f, 0.7f};

  // Line overlays (SMA, Bollinger, etc.) — 8 slots
  static constexpr int kMaxOverlayColors = 8;
  float overlayColors[kMaxOverlayColors][4] = {
    {0.3f, 0.5f, 1.0f, 1.0f},  // blue
    {1.0f, 0.6f, 0.0f, 1.0f},  // orange
    {0.0f, 0.8f, 0.8f, 1.0f},  // cyan
    {1.0f, 0.3f, 0.7f, 1.0f},  // pink
    {0.5f, 1.0f, 0.3f, 1.0f},  // lime
    {0.9f, 0.4f, 0.9f, 1.0f},  // violet
    {1.0f, 0.85f, 0.2f, 1.0f}, // gold
    {0.3f, 0.9f, 0.7f, 1.0f}   // mint
  };

  // Volume
  float volumeUp[4] = {0.0f, 0.6f, 0.3f, 0.6f};
  float volumeDown[4] = {0.7f, 0.15f, 0.15f, 0.6f};

  // Text
  float textColor[4] = {0.8f, 0.8f, 0.85f, 1.0f};

  // Selection/highlight
  float highlightColor[4] = {1.0f, 1.0f, 0.3f, 0.5f};

  // Drawing tools
  float drawingColor[4] = {1.0f, 1.0f, 0.0f, 1.0f};

  // D78: Pane borders
  float paneBorderColor[4] = {0.3f, 0.3f, 0.35f, 1.0f};
  float paneBorderWidth{0.0f};  // pixels, 0 = no border

  // D78: Pane separators
  float separatorColor[4] = {0.25f, 0.25f, 0.3f, 1.0f};
  float separatorWidth{0.0f};  // pixels, 0 = no separator
};

// Built-in presets
Theme darkTheme();
Theme lightTheme();
Theme midnightTheme();
Theme neonTheme();
Theme pastelTheme();
Theme bloombergTheme();

// ---------- ThemeApplier ----------

// Describes which scene resources each theme category should target.
struct ThemeTarget {
  std::vector<Id> paneIds;             // for background color
  std::vector<Id> candleDrawItemIds;   // for candle up/down colors
  std::vector<Id> volumeDrawItemIds;   // for volume up/down colors
  std::vector<Id> overlayDrawItemIds;  // for overlay line colors (round-robin)
  std::vector<Id> gridDrawItemIds;     // for grid style
  std::vector<Id> tickDrawItemIds;     // for tick style
  std::vector<Id> textDrawItemIds;     // for text/label color
  std::vector<Id> highlightDrawItemIds;
  std::vector<Id> drawingDrawItemIds;
  std::vector<Id> crosshairDrawItemIds;
};

// Returns a list of JSON command strings to apply the theme to the given targets.
// Commands are suitable for CommandProcessor::applyJsonText().
std::vector<std::string> generateThemeCommands(const Theme& theme, const ThemeTarget& target);

} // namespace dc
