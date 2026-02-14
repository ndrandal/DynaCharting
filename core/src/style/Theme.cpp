#include "dc/style/Theme.hpp"

#include <cstdio>
#include <string>

namespace dc {

// -------------------- Built-in presets --------------------

Theme darkTheme() {
  Theme t;
  t.name = "Dark";
  // All fields already carry the dark-theme defaults from the struct initializers.
  return t;
}

Theme lightTheme() {
  Theme t;
  t.name = "Light";

  t.backgroundColor[0] = 0.95f; t.backgroundColor[1] = 0.95f;
  t.backgroundColor[2] = 0.96f; t.backgroundColor[3] = 1.0f;

  t.candleUp[0] = 0.1f;  t.candleUp[1] = 0.7f;
  t.candleUp[2] = 0.3f;  t.candleUp[3] = 1.0f;

  t.candleDown[0] = 0.85f; t.candleDown[1] = 0.15f;
  t.candleDown[2] = 0.15f; t.candleDown[3] = 1.0f;

  t.gridColor[0] = 0.85f; t.gridColor[1] = 0.85f;
  t.gridColor[2] = 0.87f; t.gridColor[3] = 1.0f;

  t.tickColor[0] = 0.5f; t.tickColor[1] = 0.5f;
  t.tickColor[2] = 0.55f; t.tickColor[3] = 1.0f;

  t.labelColor[0] = 0.2f; t.labelColor[1] = 0.2f;
  t.labelColor[2] = 0.25f; t.labelColor[3] = 1.0f;

  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;

  t.crosshairColor[0] = 0.3f; t.crosshairColor[1] = 0.3f;
  t.crosshairColor[2] = 0.35f; t.crosshairColor[3] = 0.7f;

  t.overlayColors[0][0] = 0.2f; t.overlayColors[0][1] = 0.4f;
  t.overlayColors[0][2] = 0.9f; t.overlayColors[0][3] = 1.0f;

  t.overlayColors[1][0] = 0.9f; t.overlayColors[1][1] = 0.5f;
  t.overlayColors[1][2] = 0.0f; t.overlayColors[1][3] = 1.0f;

  t.overlayColors[2][0] = 0.0f; t.overlayColors[2][1] = 0.6f;
  t.overlayColors[2][2] = 0.6f; t.overlayColors[2][3] = 1.0f;

  t.overlayColors[3][0] = 0.8f; t.overlayColors[3][1] = 0.2f;
  t.overlayColors[3][2] = 0.5f; t.overlayColors[3][3] = 1.0f;

  t.volumeUp[0] = 0.1f; t.volumeUp[1] = 0.6f;
  t.volumeUp[2] = 0.3f; t.volumeUp[3] = 0.5f;

  t.volumeDown[0] = 0.7f; t.volumeDown[1] = 0.15f;
  t.volumeDown[2] = 0.15f; t.volumeDown[3] = 0.5f;

  t.textColor[0] = 0.15f; t.textColor[1] = 0.15f;
  t.textColor[2] = 0.2f;  t.textColor[3] = 1.0f;

  t.highlightColor[0] = 0.9f; t.highlightColor[1] = 0.7f;
  t.highlightColor[2] = 0.0f; t.highlightColor[3] = 0.5f;

  t.drawingColor[0] = 0.0f; t.drawingColor[1] = 0.0f;
  t.drawingColor[2] = 0.8f; t.drawingColor[3] = 1.0f;

  return t;
}

// -------------------- Command generation helpers --------------------

// Emit setPaneClearColor for one pane.
static std::string makePaneClearColorCmd(Id paneId, const float c[4]) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setPaneClearColor","id":%llu,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    static_cast<unsigned long long>(paneId), c[0], c[1], c[2], c[3]);
  return buf;
}

// Emit setDrawItemColor (base color only) for one drawItem.
static std::string makeDrawItemColorCmd(Id drawItemId, const float c[4]) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":%llu,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    static_cast<unsigned long long>(drawItemId), c[0], c[1], c[2], c[3]);
  return buf;
}

// Emit setDrawItemStyle with colorUp/colorDown for candle-like drawItems.
static std::string makeCandleStyleCmd(Id drawItemId, const float up[4], const float down[4]) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%llu,)"
    R"("colorUpR":%.9g,"colorUpG":%.9g,"colorUpB":%.9g,"colorUpA":%.9g,)"
    R"("colorDownR":%.9g,"colorDownG":%.9g,"colorDownB":%.9g,"colorDownA":%.9g})",
    static_cast<unsigned long long>(drawItemId),
    up[0], up[1], up[2], up[3],
    down[0], down[1], down[2], down[3]);
  return buf;
}

// Emit setDrawItemStyle with base color + lineWidth for grid/tick drawItems.
static std::string makeLineStyleCmd(Id drawItemId, const float c[4], float lineWidth) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%llu,)"
    R"("r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
    static_cast<unsigned long long>(drawItemId),
    c[0], c[1], c[2], c[3], lineWidth);
  return buf;
}

// -------------------- generateThemeCommands --------------------

std::vector<std::string> generateThemeCommands(const Theme& theme, const ThemeTarget& target) {
  std::vector<std::string> cmds;

  // Pane background colors
  for (Id id : target.paneIds) {
    cmds.push_back(makePaneClearColorCmd(id, theme.backgroundColor));
  }

  // Candle drawItems: set colorUp / colorDown
  for (Id id : target.candleDrawItemIds) {
    cmds.push_back(makeCandleStyleCmd(id, theme.candleUp, theme.candleDown));
  }

  // Volume drawItems: set colorUp / colorDown
  for (Id id : target.volumeDrawItemIds) {
    cmds.push_back(makeCandleStyleCmd(id, theme.volumeUp, theme.volumeDown));
  }

  // Overlay drawItems: round-robin through overlayColors
  for (std::size_t i = 0; i < target.overlayDrawItemIds.size(); ++i) {
    std::size_t colorIdx = i % 4u;
    cmds.push_back(makeDrawItemColorCmd(target.overlayDrawItemIds[i],
                                        theme.overlayColors[colorIdx]));
  }

  // Grid drawItems: color + lineWidth
  for (Id id : target.gridDrawItemIds) {
    cmds.push_back(makeLineStyleCmd(id, theme.gridColor, theme.gridLineWidth));
  }

  // Tick drawItems: color + lineWidth
  for (Id id : target.tickDrawItemIds) {
    cmds.push_back(makeLineStyleCmd(id, theme.tickColor, theme.tickLineWidth));
  }

  // Text / label drawItems: base color
  for (Id id : target.textDrawItemIds) {
    cmds.push_back(makeDrawItemColorCmd(id, theme.labelColor));
  }

  // Highlight drawItems: base color
  for (Id id : target.highlightDrawItemIds) {
    cmds.push_back(makeDrawItemColorCmd(id, theme.highlightColor));
  }

  // Drawing tool drawItems: base color
  for (Id id : target.drawingDrawItemIds) {
    cmds.push_back(makeDrawItemColorCmd(id, theme.drawingColor));
  }

  // Crosshair drawItems: base color
  for (Id id : target.crosshairDrawItemIds) {
    cmds.push_back(makeDrawItemColorCmd(id, theme.crosshairColor));
  }

  return cmds;
}

} // namespace dc
