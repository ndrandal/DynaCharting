#pragma once
#include "dc/ids/Id.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------- Theme ----------
// The engine's Theme is an agnostic set of styling knobs: a background color,
// generic grid/axis styling, and an indexed color palette. There are NO
// chart-domain field names here (no candleUp, no volumeDown, no crosshairColor).
// Chart-domain meaning is assigned by the caller — typically via the recipe
// layer helpers in `dc/recipe/ChartTheme.hpp`.
//
// Palette layout:
//   palette[N]     — primary color per slot (RGBA)
//   paletteAlt[N]  — "secondary" color paired with the primary (RGBA).
//                    Used when a style group has a pair of colors (e.g. up/down
//                    on OHLC bars). For Solid groups, paletteAlt is ignored.
struct Theme {
  std::string name;

  // Engine-generic background / axis styling.
  float backgroundColor[4] = {0.1f, 0.1f, 0.12f, 1.0f};
  float textColor[4]       = {0.8f, 0.8f, 0.85f, 1.0f};
  float highlightColor[4]  = {1.0f, 1.0f, 0.3f, 0.5f};
  float drawingColor[4]    = {1.0f, 1.0f, 0.0f, 1.0f};

  float gridColor[4]       = {0.2f, 0.2f, 0.25f, 1.0f};
  float tickColor[4]       = {0.4f, 0.4f, 0.45f, 1.0f};
  float labelColor[4]      = {0.7f, 0.7f, 0.75f, 1.0f};
  float gridLineWidth{1.0f};
  float tickLineWidth{1.0f};
  float gridDashLength{0.0f};
  float gridGapLength{0.0f};
  float gridOpacity{1.0f};

  float paneBorderColor[4] = {0.3f, 0.3f, 0.35f, 1.0f};
  float paneBorderWidth{0.0f};
  float separatorColor[4]  = {0.25f, 0.25f, 0.3f, 1.0f};
  float separatorWidth{0.0f};

  // Indexed color palette. Slot assignment is a chart-layer concern.
  static constexpr int kPaletteSize = 16;
  float palette[kPaletteSize][4] = {
    // Default dark palette (slot assignments are advisory; see ChartTheme.hpp).
    {0.0f, 0.8f, 0.4f, 1.0f},   // 0
    {0.0f, 0.6f, 0.3f, 0.6f},   // 1
    {0.3f, 0.5f, 1.0f, 1.0f},   // 2
    {1.0f, 0.6f, 0.0f, 1.0f},   // 3
    {0.0f, 0.8f, 0.8f, 1.0f},   // 4
    {1.0f, 0.3f, 0.7f, 1.0f},   // 5
    {0.5f, 1.0f, 0.3f, 1.0f},   // 6
    {0.9f, 0.4f, 0.9f, 1.0f},   // 7
    {1.0f, 0.85f, 0.2f, 1.0f},  // 8
    {0.3f, 0.9f, 0.7f, 1.0f},   // 9
    {0.8f, 0.8f, 0.85f, 0.7f},  // 10
    {0.0f, 0.0f, 0.0f, 0.0f},   // 11
    {0.0f, 0.0f, 0.0f, 0.0f},   // 12
    {0.0f, 0.0f, 0.0f, 0.0f},   // 13
    {0.0f, 0.0f, 0.0f, 0.0f},   // 14
    {0.0f, 0.0f, 0.0f, 0.0f},   // 15
  };
  float paletteAlt[kPaletteSize][4] = {
    {0.9f, 0.2f, 0.2f, 1.0f},   // 0 (paired with slot 0 primary)
    {0.7f, 0.15f, 0.15f, 0.6f}, // 1
    {0.0f, 0.0f, 0.0f, 0.0f},   // 2..15: unused by default
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
  };
};

// Built-in presets.
Theme darkTheme();
Theme lightTheme();
Theme midnightTheme();
Theme neonTheme();
Theme pastelTheme();
Theme bloombergTheme();

// ---------- ThemeTarget ----------

// Describes how one palette slot (or the engine-generic background / axis
// styling) should be applied to a set of scene resources.
struct PaletteGroup {
  enum class Kind : std::uint8_t {
    Solid,             // palette[slot] -> setDrawItemColor
    UpDownPair,        // palette[slot] + paletteAlt[slot] -> setDrawItemStyle (colorUp/colorDown)
    LineStyle,         // palette[slot] + theme.tickLineWidth -> setDrawItemStyle (color + lineWidth)
    GridStyle,         // theme.gridColor/opacity/dash/gap -> setDrawItemStyle
    TickStyle,         // theme.tickColor/tickLineWidth -> setDrawItemStyle
    LabelColor,        // theme.labelColor -> setDrawItemColor
    TextColor,         // theme.textColor -> setDrawItemColor
    HighlightColor,    // theme.highlightColor -> setDrawItemColor
    DrawingColor,      // theme.drawingColor -> setDrawItemColor
    PaneBackground,    // theme.backgroundColor -> setPaneClearColor (ids are paneIds)
  };

  Kind kind{Kind::Solid};
  std::uint8_t slot{0};           // palette index for Solid / UpDownPair / LineStyle
  std::vector<Id> targetIds;      // drawItem IDs (or pane IDs for PaneBackground)
  bool rotateSlots{false};        // if true, slot is the base and each target uses (base + i) % kPaletteSize
};

struct ThemeTarget {
  std::vector<PaletteGroup> groups;
};

// Generates the sequence of JSON commands that apply `theme` to the targets.
// Commands are suitable for CommandProcessor::applyJsonText().
std::vector<std::string> generateThemeCommands(const Theme& theme,
                                                const ThemeTarget& target);

} // namespace dc
