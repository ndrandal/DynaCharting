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

  // Overlay colors (8 slots)
  float lc[][4] = {
    {0.2f, 0.4f, 0.9f, 1.0f}, {0.9f, 0.5f, 0.0f, 1.0f},
    {0.0f, 0.6f, 0.6f, 1.0f}, {0.8f, 0.2f, 0.5f, 1.0f},
    {0.3f, 0.7f, 0.2f, 1.0f}, {0.6f, 0.3f, 0.8f, 1.0f},
    {0.8f, 0.7f, 0.1f, 1.0f}, {0.2f, 0.7f, 0.5f, 1.0f}
  };
  for (int i = 0; i < Theme::kMaxOverlayColors; ++i)
    for (int j = 0; j < 4; ++j)
      t.overlayColors[i][j] = lc[i][j];

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

  t.paneBorderColor[0] = 0.8f; t.paneBorderColor[1] = 0.8f;
  t.paneBorderColor[2] = 0.82f; t.paneBorderColor[3] = 1.0f;
  t.separatorColor[0] = 0.75f; t.separatorColor[1] = 0.75f;
  t.separatorColor[2] = 0.78f; t.separatorColor[3] = 1.0f;

  return t;
}

// -------------------- D78 new presets --------------------

Theme midnightTheme() {
  Theme t;
  t.name = "Midnight";

  // Deep navy background
  t.backgroundColor[0] = 0.04f; t.backgroundColor[1] = 0.055f;
  t.backgroundColor[2] = 0.1f;  t.backgroundColor[3] = 1.0f;

  // Teal/blue-green candles
  t.candleUp[0] = 0.0f;  t.candleUp[1] = 0.75f;
  t.candleUp[2] = 0.65f; t.candleUp[3] = 1.0f;
  t.candleDown[0] = 0.7f; t.candleDown[1] = 0.2f;
  t.candleDown[2] = 0.35f; t.candleDown[3] = 1.0f;

  // Subtle steel-blue grid
  t.gridColor[0] = 0.12f; t.gridColor[1] = 0.14f;
  t.gridColor[2] = 0.2f;  t.gridColor[3] = 0.6f;
  t.tickColor[0] = 0.25f; t.tickColor[1] = 0.3f;
  t.tickColor[2] = 0.4f;  t.tickColor[3] = 1.0f;
  t.labelColor[0] = 0.5f; t.labelColor[1] = 0.55f;
  t.labelColor[2] = 0.65f; t.labelColor[3] = 1.0f;
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;

  t.crosshairColor[0] = 0.4f; t.crosshairColor[1] = 0.5f;
  t.crosshairColor[2] = 0.7f; t.crosshairColor[3] = 0.6f;

  float mc[][4] = {
    {0.2f, 0.6f, 1.0f, 1.0f}, {0.9f, 0.55f, 0.15f, 1.0f},
    {0.0f, 0.8f, 0.7f, 1.0f}, {0.85f, 0.35f, 0.65f, 1.0f},
    {0.4f, 0.9f, 0.4f, 1.0f}, {0.7f, 0.4f, 1.0f, 1.0f},
    {1.0f, 0.8f, 0.25f, 1.0f}, {0.3f, 0.85f, 0.75f, 1.0f}
  };
  for (int i = 0; i < Theme::kMaxOverlayColors; ++i)
    for (int j = 0; j < 4; ++j)
      t.overlayColors[i][j] = mc[i][j];

  t.volumeUp[0] = 0.0f; t.volumeUp[1] = 0.5f;
  t.volumeUp[2] = 0.45f; t.volumeUp[3] = 0.5f;
  t.volumeDown[0] = 0.5f; t.volumeDown[1] = 0.15f;
  t.volumeDown[2] = 0.25f; t.volumeDown[3] = 0.5f;

  t.textColor[0] = 0.6f; t.textColor[1] = 0.65f;
  t.textColor[2] = 0.75f; t.textColor[3] = 1.0f;
  t.highlightColor[0] = 0.3f; t.highlightColor[1] = 0.7f;
  t.highlightColor[2] = 1.0f; t.highlightColor[3] = 0.5f;
  t.drawingColor[0] = 0.4f; t.drawingColor[1] = 0.7f;
  t.drawingColor[2] = 1.0f; t.drawingColor[3] = 1.0f;

  // Pane styling
  t.paneBorderColor[0] = 0.15f; t.paneBorderColor[1] = 0.18f;
  t.paneBorderColor[2] = 0.28f; t.paneBorderColor[3] = 1.0f;
  t.paneBorderWidth = 1.0f;
  t.separatorColor[0] = 0.12f; t.separatorColor[1] = 0.15f;
  t.separatorColor[2] = 0.25f; t.separatorColor[3] = 1.0f;
  t.separatorWidth = 1.0f;

  return t;
}

Theme neonTheme() {
  Theme t;
  t.name = "Neon";

  // Near-black background
  t.backgroundColor[0] = 0.02f; t.backgroundColor[1] = 0.02f;
  t.backgroundColor[2] = 0.04f; t.backgroundColor[3] = 1.0f;

  // Electric green / magenta candles
  t.candleUp[0] = 0.0f;  t.candleUp[1] = 1.0f;
  t.candleUp[2] = 0.4f;  t.candleUp[3] = 1.0f;
  t.candleDown[0] = 1.0f; t.candleDown[1] = 0.0f;
  t.candleDown[2] = 0.4f; t.candleDown[3] = 1.0f;

  // Dim cyan grid
  t.gridColor[0] = 0.0f;  t.gridColor[1] = 0.15f;
  t.gridColor[2] = 0.2f;  t.gridColor[3] = 0.4f;
  t.tickColor[0] = 0.0f;  t.tickColor[1] = 0.5f;
  t.tickColor[2] = 0.6f;  t.tickColor[3] = 1.0f;
  t.labelColor[0] = 0.4f; t.labelColor[1] = 0.8f;
  t.labelColor[2] = 0.9f; t.labelColor[3] = 1.0f;
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;
  t.gridDashLength = 4.0f;
  t.gridGapLength = 4.0f;

  t.crosshairColor[0] = 0.0f; t.crosshairColor[1] = 1.0f;
  t.crosshairColor[2] = 1.0f; t.crosshairColor[3] = 0.6f;

  float nc[][4] = {
    {0.0f, 0.8f, 1.0f, 1.0f}, {1.0f, 0.4f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.6f, 1.0f}, {1.0f, 0.0f, 0.8f, 1.0f},
    {0.6f, 1.0f, 0.0f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}
  };
  for (int i = 0; i < Theme::kMaxOverlayColors; ++i)
    for (int j = 0; j < 4; ++j)
      t.overlayColors[i][j] = nc[i][j];

  t.volumeUp[0] = 0.0f; t.volumeUp[1] = 0.8f;
  t.volumeUp[2] = 0.3f; t.volumeUp[3] = 0.5f;
  t.volumeDown[0] = 0.8f; t.volumeDown[1] = 0.0f;
  t.volumeDown[2] = 0.3f; t.volumeDown[3] = 0.5f;

  t.textColor[0] = 0.5f; t.textColor[1] = 0.9f;
  t.textColor[2] = 1.0f; t.textColor[3] = 1.0f;
  t.highlightColor[0] = 1.0f; t.highlightColor[1] = 1.0f;
  t.highlightColor[2] = 0.0f; t.highlightColor[3] = 0.6f;
  t.drawingColor[0] = 0.0f; t.drawingColor[1] = 1.0f;
  t.drawingColor[2] = 1.0f; t.drawingColor[3] = 1.0f;

  t.paneBorderColor[0] = 0.0f; t.paneBorderColor[1] = 0.3f;
  t.paneBorderColor[2] = 0.4f; t.paneBorderColor[3] = 1.0f;
  t.paneBorderWidth = 1.0f;
  t.separatorColor[0] = 0.0f; t.separatorColor[1] = 0.4f;
  t.separatorColor[2] = 0.5f; t.separatorColor[3] = 1.0f;
  t.separatorWidth = 2.0f;

  return t;
}

Theme pastelTheme() {
  Theme t;
  t.name = "Pastel";

  // Warm cream background
  t.backgroundColor[0] = 0.97f; t.backgroundColor[1] = 0.95f;
  t.backgroundColor[2] = 0.92f; t.backgroundColor[3] = 1.0f;

  // Muted sage green / soft rose candles
  t.candleUp[0] = 0.4f;  t.candleUp[1] = 0.72f;
  t.candleUp[2] = 0.55f; t.candleUp[3] = 1.0f;
  t.candleDown[0] = 0.82f; t.candleDown[1] = 0.42f;
  t.candleDown[2] = 0.45f; t.candleDown[3] = 1.0f;

  // Soft warm grey grid
  t.gridColor[0] = 0.85f; t.gridColor[1] = 0.82f;
  t.gridColor[2] = 0.78f; t.gridColor[3] = 0.5f;
  t.tickColor[0] = 0.6f;  t.tickColor[1] = 0.55f;
  t.tickColor[2] = 0.5f;  t.tickColor[3] = 1.0f;
  t.labelColor[0] = 0.4f; t.labelColor[1] = 0.37f;
  t.labelColor[2] = 0.35f; t.labelColor[3] = 1.0f;
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;
  t.gridOpacity = 0.5f;

  t.crosshairColor[0] = 0.5f; t.crosshairColor[1] = 0.45f;
  t.crosshairColor[2] = 0.4f; t.crosshairColor[3] = 0.5f;

  float pc[][4] = {
    {0.45f, 0.6f, 0.85f, 1.0f}, {0.9f, 0.65f, 0.4f, 1.0f},
    {0.4f, 0.75f, 0.7f, 1.0f},  {0.8f, 0.5f, 0.65f, 1.0f},
    {0.55f, 0.8f, 0.45f, 1.0f}, {0.7f, 0.5f, 0.8f, 1.0f},
    {0.85f, 0.75f, 0.4f, 1.0f}, {0.5f, 0.8f, 0.7f, 1.0f}
  };
  for (int i = 0; i < Theme::kMaxOverlayColors; ++i)
    for (int j = 0; j < 4; ++j)
      t.overlayColors[i][j] = pc[i][j];

  t.volumeUp[0] = 0.4f; t.volumeUp[1] = 0.65f;
  t.volumeUp[2] = 0.5f; t.volumeUp[3] = 0.4f;
  t.volumeDown[0] = 0.7f; t.volumeDown[1] = 0.4f;
  t.volumeDown[2] = 0.4f; t.volumeDown[3] = 0.4f;

  t.textColor[0] = 0.35f; t.textColor[1] = 0.32f;
  t.textColor[2] = 0.3f;  t.textColor[3] = 1.0f;
  t.highlightColor[0] = 0.85f; t.highlightColor[1] = 0.7f;
  t.highlightColor[2] = 0.3f;  t.highlightColor[3] = 0.5f;
  t.drawingColor[0] = 0.5f; t.drawingColor[1] = 0.55f;
  t.drawingColor[2] = 0.8f; t.drawingColor[3] = 1.0f;

  t.paneBorderColor[0] = 0.8f; t.paneBorderColor[1] = 0.77f;
  t.paneBorderColor[2] = 0.72f; t.paneBorderColor[3] = 1.0f;
  t.separatorColor[0] = 0.82f; t.separatorColor[1] = 0.78f;
  t.separatorColor[2] = 0.73f; t.separatorColor[3] = 1.0f;
  t.separatorWidth = 1.0f;

  return t;
}

Theme bloombergTheme() {
  Theme t;
  t.name = "Bloomberg";

  // Classic terminal black
  t.backgroundColor[0] = 0.0f; t.backgroundColor[1] = 0.0f;
  t.backgroundColor[2] = 0.0f; t.backgroundColor[3] = 1.0f;

  // Amber up / white down
  t.candleUp[0] = 0.0f;  t.candleUp[1] = 0.8f;
  t.candleUp[2] = 0.0f;  t.candleUp[3] = 1.0f;
  t.candleDown[0] = 0.9f; t.candleDown[1] = 0.0f;
  t.candleDown[2] = 0.0f; t.candleDown[3] = 1.0f;

  // Medium grey dashed grid
  t.gridColor[0] = 0.2f;  t.gridColor[1] = 0.2f;
  t.gridColor[2] = 0.2f;  t.gridColor[3] = 0.8f;
  t.tickColor[0] = 0.45f; t.tickColor[1] = 0.45f;
  t.tickColor[2] = 0.45f; t.tickColor[3] = 1.0f;
  t.labelColor[0] = 1.0f; t.labelColor[1] = 0.6f;
  t.labelColor[2] = 0.0f; t.labelColor[3] = 1.0f;
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;
  t.gridDashLength = 3.0f;
  t.gridGapLength = 3.0f;

  t.crosshairColor[0] = 1.0f; t.crosshairColor[1] = 1.0f;
  t.crosshairColor[2] = 1.0f; t.crosshairColor[3] = 0.5f;

  float bc[][4] = {
    {1.0f, 0.6f, 0.0f, 1.0f}, {0.3f, 0.6f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.9f, 0.5f, 1.0f},
    {1.0f, 0.3f, 0.3f, 1.0f}, {0.6f, 0.4f, 1.0f, 1.0f},
    {0.0f, 0.8f, 0.8f, 1.0f}, {1.0f, 0.8f, 0.5f, 1.0f}
  };
  for (int i = 0; i < Theme::kMaxOverlayColors; ++i)
    for (int j = 0; j < 4; ++j)
      t.overlayColors[i][j] = bc[i][j];

  t.volumeUp[0] = 0.0f; t.volumeUp[1] = 0.6f;
  t.volumeUp[2] = 0.0f; t.volumeUp[3] = 0.5f;
  t.volumeDown[0] = 0.6f; t.volumeDown[1] = 0.0f;
  t.volumeDown[2] = 0.0f; t.volumeDown[3] = 0.5f;

  t.textColor[0] = 1.0f; t.textColor[1] = 0.6f;
  t.textColor[2] = 0.0f; t.textColor[3] = 1.0f;
  t.highlightColor[0] = 1.0f; t.highlightColor[1] = 1.0f;
  t.highlightColor[2] = 0.0f; t.highlightColor[3] = 0.5f;
  t.drawingColor[0] = 1.0f; t.drawingColor[1] = 0.6f;
  t.drawingColor[2] = 0.0f; t.drawingColor[3] = 1.0f;

  t.paneBorderColor[0] = 0.3f; t.paneBorderColor[1] = 0.3f;
  t.paneBorderColor[2] = 0.3f; t.paneBorderColor[3] = 1.0f;
  t.paneBorderWidth = 1.0f;
  t.separatorColor[0] = 0.4f; t.separatorColor[1] = 0.4f;
  t.separatorColor[2] = 0.4f; t.separatorColor[3] = 1.0f;
  t.separatorWidth = 1.0f;

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

  // Overlay drawItems: round-robin through overlayColors (8 slots)
  for (std::size_t i = 0; i < target.overlayDrawItemIds.size(); ++i) {
    std::size_t colorIdx = i % static_cast<std::size_t>(Theme::kMaxOverlayColors);
    cmds.push_back(makeDrawItemColorCmd(target.overlayDrawItemIds[i],
                                        theme.overlayColors[colorIdx]));
  }

  // Grid drawItems: color + lineWidth + dash/gap/opacity
  for (Id id : target.gridDrawItemIds) {
    // Apply grid opacity to color alpha
    float gc[4] = {theme.gridColor[0], theme.gridColor[1],
                   theme.gridColor[2], theme.gridColor[3] * theme.gridOpacity};
    cmds.push_back(makeLineStyleCmd(id, gc, theme.gridLineWidth));
    // Emit dash/gap if non-zero
    if (theme.gridDashLength > 0.0f) {
      char buf[512];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setDrawItemStyle","drawItemId":%llu,"dashLength":%.9g,"gapLength":%.9g})",
        static_cast<unsigned long long>(id), theme.gridDashLength, theme.gridGapLength);
      cmds.push_back(buf);
    }
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
