#include "dc/style/Theme.hpp"

#include <cstdio>
#include <string>

namespace dc {

// -------------------- preset authoring helpers --------------------
//
// The preset authors use these to populate palette slots with conventional
// chart meaning without depending on core/include/dc/recipe/ChartTheme.hpp
// (which is a recipe-layer convenience header). Slot assignments must match
// dc::chart_theme::kSlot* constants in ChartTheme.hpp.

static void setColor4(float dst[4], float r, float g, float b, float a) {
  dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
}

static void setCandlePair(Theme& t,
                          float ur, float ug, float ub, float ua,
                          float dr, float dg, float db, float da) {
  setColor4(t.palette[0],    ur, ug, ub, ua);
  setColor4(t.paletteAlt[0], dr, dg, db, da);
}

static void setVolumePair(Theme& t,
                          float ur, float ug, float ub, float ua,
                          float dr, float dg, float db, float da) {
  setColor4(t.palette[1],    ur, ug, ub, ua);
  setColor4(t.paletteAlt[1], dr, dg, db, da);
}

static void setOverlays(Theme& t, const float colors[8][4]) {
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 4; ++j) t.palette[2 + i][j] = colors[i][j];
  }
}

static void setCrosshair(Theme& t, float r, float g, float b, float a) {
  setColor4(t.palette[10], r, g, b, a);
}

// -------------------- built-in presets --------------------

Theme darkTheme() {
  Theme t;
  t.name = "Dark";
  // Palette defaults in the struct initializer already carry the dark scheme.
  return t;
}

Theme lightTheme() {
  Theme t;
  t.name = "Light";

  setColor4(t.backgroundColor, 0.95f, 0.95f, 0.96f, 1.0f);
  setCandlePair(t, 0.1f, 0.7f, 0.3f, 1.0f,
                   0.85f, 0.15f, 0.15f, 1.0f);
  setColor4(t.gridColor, 0.85f, 0.85f, 0.87f, 1.0f);
  setColor4(t.tickColor, 0.5f, 0.5f, 0.55f, 1.0f);
  setColor4(t.labelColor, 0.2f, 0.2f, 0.25f, 1.0f);
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;

  setCrosshair(t, 0.3f, 0.3f, 0.35f, 0.7f);

  float lc[8][4] = {
    {0.2f, 0.4f, 0.9f, 1.0f}, {0.9f, 0.5f, 0.0f, 1.0f},
    {0.0f, 0.6f, 0.6f, 1.0f}, {0.8f, 0.2f, 0.5f, 1.0f},
    {0.3f, 0.7f, 0.2f, 1.0f}, {0.6f, 0.3f, 0.8f, 1.0f},
    {0.8f, 0.7f, 0.1f, 1.0f}, {0.2f, 0.7f, 0.5f, 1.0f}
  };
  setOverlays(t, lc);

  setVolumePair(t, 0.1f, 0.6f, 0.3f, 0.5f,
                   0.7f, 0.15f, 0.15f, 0.5f);

  setColor4(t.textColor, 0.15f, 0.15f, 0.2f, 1.0f);
  setColor4(t.highlightColor, 0.9f, 0.7f, 0.0f, 0.5f);
  setColor4(t.drawingColor, 0.0f, 0.0f, 0.8f, 1.0f);
  setColor4(t.paneBorderColor, 0.8f, 0.8f, 0.82f, 1.0f);
  setColor4(t.separatorColor, 0.75f, 0.75f, 0.78f, 1.0f);
  return t;
}

Theme midnightTheme() {
  Theme t;
  t.name = "Midnight";

  setColor4(t.backgroundColor, 0.04f, 0.055f, 0.1f, 1.0f);
  setCandlePair(t, 0.0f, 0.75f, 0.65f, 1.0f,
                   0.7f, 0.2f, 0.35f, 1.0f);

  setColor4(t.gridColor, 0.12f, 0.14f, 0.2f, 0.6f);
  setColor4(t.tickColor, 0.25f, 0.3f, 0.4f, 1.0f);
  setColor4(t.labelColor, 0.5f, 0.55f, 0.65f, 1.0f);
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;

  setCrosshair(t, 0.4f, 0.5f, 0.7f, 0.6f);

  float mc[8][4] = {
    {0.2f, 0.6f, 1.0f, 1.0f}, {0.9f, 0.55f, 0.15f, 1.0f},
    {0.0f, 0.8f, 0.7f, 1.0f}, {0.85f, 0.35f, 0.65f, 1.0f},
    {0.4f, 0.9f, 0.4f, 1.0f}, {0.7f, 0.4f, 1.0f, 1.0f},
    {1.0f, 0.8f, 0.25f, 1.0f}, {0.3f, 0.85f, 0.75f, 1.0f}
  };
  setOverlays(t, mc);

  setVolumePair(t, 0.0f, 0.5f, 0.45f, 0.5f,
                   0.5f, 0.15f, 0.25f, 0.5f);

  setColor4(t.textColor, 0.6f, 0.65f, 0.75f, 1.0f);
  setColor4(t.highlightColor, 0.3f, 0.7f, 1.0f, 0.5f);
  setColor4(t.drawingColor, 0.4f, 0.7f, 1.0f, 1.0f);
  setColor4(t.paneBorderColor, 0.15f, 0.18f, 0.28f, 1.0f);
  t.paneBorderWidth = 1.0f;
  setColor4(t.separatorColor, 0.12f, 0.15f, 0.25f, 1.0f);
  t.separatorWidth = 1.0f;
  return t;
}

Theme neonTheme() {
  Theme t;
  t.name = "Neon";

  setColor4(t.backgroundColor, 0.02f, 0.02f, 0.04f, 1.0f);
  setCandlePair(t, 0.0f, 1.0f, 0.4f, 1.0f,
                   1.0f, 0.0f, 0.4f, 1.0f);

  setColor4(t.gridColor, 0.0f, 0.15f, 0.2f, 0.4f);
  setColor4(t.tickColor, 0.0f, 0.5f, 0.6f, 1.0f);
  setColor4(t.labelColor, 0.4f, 0.8f, 0.9f, 1.0f);
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;
  t.gridDashLength = 4.0f;
  t.gridGapLength = 4.0f;

  setCrosshair(t, 0.0f, 1.0f, 1.0f, 0.6f);

  float nc[8][4] = {
    {0.0f, 0.8f, 1.0f, 1.0f}, {1.0f, 0.4f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.6f, 1.0f}, {1.0f, 0.0f, 0.8f, 1.0f},
    {0.6f, 1.0f, 0.0f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}
  };
  setOverlays(t, nc);

  setVolumePair(t, 0.0f, 0.8f, 0.3f, 0.5f,
                   0.8f, 0.0f, 0.3f, 0.5f);

  setColor4(t.textColor, 0.5f, 0.9f, 1.0f, 1.0f);
  setColor4(t.highlightColor, 1.0f, 1.0f, 0.0f, 0.6f);
  setColor4(t.drawingColor, 0.0f, 1.0f, 1.0f, 1.0f);
  setColor4(t.paneBorderColor, 0.0f, 0.3f, 0.4f, 1.0f);
  t.paneBorderWidth = 1.0f;
  setColor4(t.separatorColor, 0.0f, 0.4f, 0.5f, 1.0f);
  t.separatorWidth = 2.0f;
  return t;
}

Theme pastelTheme() {
  Theme t;
  t.name = "Pastel";

  setColor4(t.backgroundColor, 0.97f, 0.95f, 0.92f, 1.0f);
  setCandlePair(t, 0.4f, 0.72f, 0.55f, 1.0f,
                   0.82f, 0.42f, 0.45f, 1.0f);

  setColor4(t.gridColor, 0.85f, 0.82f, 0.78f, 0.5f);
  setColor4(t.tickColor, 0.6f, 0.55f, 0.5f, 1.0f);
  setColor4(t.labelColor, 0.4f, 0.37f, 0.35f, 1.0f);
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;
  t.gridOpacity = 0.5f;

  setCrosshair(t, 0.5f, 0.45f, 0.4f, 0.5f);

  float pc[8][4] = {
    {0.45f, 0.6f, 0.85f, 1.0f}, {0.9f, 0.65f, 0.4f, 1.0f},
    {0.4f, 0.75f, 0.7f, 1.0f},  {0.8f, 0.5f, 0.65f, 1.0f},
    {0.55f, 0.8f, 0.45f, 1.0f}, {0.7f, 0.5f, 0.8f, 1.0f},
    {0.85f, 0.75f, 0.4f, 1.0f}, {0.5f, 0.8f, 0.7f, 1.0f}
  };
  setOverlays(t, pc);

  setVolumePair(t, 0.4f, 0.65f, 0.5f, 0.4f,
                   0.7f, 0.4f, 0.4f, 0.4f);

  setColor4(t.textColor, 0.35f, 0.32f, 0.3f, 1.0f);
  setColor4(t.highlightColor, 0.85f, 0.7f, 0.3f, 0.5f);
  setColor4(t.drawingColor, 0.5f, 0.55f, 0.8f, 1.0f);
  setColor4(t.paneBorderColor, 0.8f, 0.77f, 0.72f, 1.0f);
  setColor4(t.separatorColor, 0.82f, 0.78f, 0.73f, 1.0f);
  t.separatorWidth = 1.0f;
  return t;
}

Theme bloombergTheme() {
  Theme t;
  t.name = "Bloomberg";

  setColor4(t.backgroundColor, 0.0f, 0.0f, 0.0f, 1.0f);
  setCandlePair(t, 0.0f, 0.8f, 0.0f, 1.0f,
                   0.9f, 0.0f, 0.0f, 1.0f);

  setColor4(t.gridColor, 0.2f, 0.2f, 0.2f, 0.8f);
  setColor4(t.tickColor, 0.45f, 0.45f, 0.45f, 1.0f);
  setColor4(t.labelColor, 1.0f, 0.6f, 0.0f, 1.0f);
  t.gridLineWidth = 1.0f;
  t.tickLineWidth = 1.0f;
  t.gridDashLength = 3.0f;
  t.gridGapLength = 3.0f;

  setCrosshair(t, 1.0f, 1.0f, 1.0f, 0.5f);

  float bc[8][4] = {
    {1.0f, 0.6f, 0.0f, 1.0f}, {0.3f, 0.6f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.9f, 0.5f, 1.0f},
    {1.0f, 0.3f, 0.3f, 1.0f}, {0.6f, 0.4f, 1.0f, 1.0f},
    {0.0f, 0.8f, 0.8f, 1.0f}, {1.0f, 0.8f, 0.5f, 1.0f}
  };
  setOverlays(t, bc);

  setVolumePair(t, 0.0f, 0.6f, 0.0f, 0.5f,
                   0.6f, 0.0f, 0.0f, 0.5f);

  setColor4(t.textColor, 1.0f, 0.6f, 0.0f, 1.0f);
  setColor4(t.highlightColor, 1.0f, 1.0f, 0.0f, 0.5f);
  setColor4(t.drawingColor, 1.0f, 0.6f, 0.0f, 1.0f);
  setColor4(t.paneBorderColor, 0.3f, 0.3f, 0.3f, 1.0f);
  t.paneBorderWidth = 1.0f;
  setColor4(t.separatorColor, 0.4f, 0.4f, 0.4f, 1.0f);
  t.separatorWidth = 1.0f;
  return t;
}

// -------------------- command generation --------------------

static std::string cmdPaneClearColor(Id paneId, const float c[4]) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setPaneClearColor","id":%llu,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    static_cast<unsigned long long>(paneId), c[0], c[1], c[2], c[3]);
  return buf;
}

static std::string cmdDrawItemColor(Id drawItemId, const float c[4]) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":%llu,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    static_cast<unsigned long long>(drawItemId), c[0], c[1], c[2], c[3]);
  return buf;
}

static std::string cmdUpDownStyle(Id drawItemId,
                                   const float up[4], const float down[4]) {
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

static std::string cmdLineStyle(Id drawItemId, const float c[4], float lineWidth) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%llu,)"
    R"("r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
    static_cast<unsigned long long>(drawItemId),
    c[0], c[1], c[2], c[3], lineWidth);
  return buf;
}

static std::string cmdDashStyle(Id drawItemId,
                                 float dashLength, float gapLength) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%llu,"dashLength":%.9g,"gapLength":%.9g})",
    static_cast<unsigned long long>(drawItemId), dashLength, gapLength);
  return buf;
}

std::vector<std::string> generateThemeCommands(const Theme& theme,
                                                const ThemeTarget& target) {
  std::vector<std::string> cmds;
  const auto clampSlot = [](std::uint8_t s) -> std::uint8_t {
    return (s < Theme::kPaletteSize) ? s : 0;
  };

  for (const auto& g : target.groups) {
    switch (g.kind) {
      case PaletteGroup::Kind::Solid: {
        for (std::size_t i = 0; i < g.targetIds.size(); ++i) {
          std::uint8_t slot = clampSlot(g.rotateSlots
              ? static_cast<std::uint8_t>(
                  (g.slot + (i % 8u)) % Theme::kPaletteSize)
              : g.slot);
          cmds.push_back(cmdDrawItemColor(g.targetIds[i], theme.palette[slot]));
        }
        break;
      }
      case PaletteGroup::Kind::UpDownPair: {
        std::uint8_t slot = clampSlot(g.slot);
        for (Id id : g.targetIds) {
          cmds.push_back(cmdUpDownStyle(id, theme.palette[slot],
                                             theme.paletteAlt[slot]));
        }
        break;
      }
      case PaletteGroup::Kind::LineStyle: {
        std::uint8_t slot = clampSlot(g.slot);
        for (Id id : g.targetIds) {
          cmds.push_back(cmdLineStyle(id, theme.palette[slot],
                                           theme.tickLineWidth));
        }
        break;
      }
      case PaletteGroup::Kind::GridStyle: {
        float gc[4] = {theme.gridColor[0], theme.gridColor[1],
                       theme.gridColor[2], theme.gridColor[3] * theme.gridOpacity};
        for (Id id : g.targetIds) {
          cmds.push_back(cmdLineStyle(id, gc, theme.gridLineWidth));
          if (theme.gridDashLength > 0.0f) {
            cmds.push_back(cmdDashStyle(id, theme.gridDashLength,
                                             theme.gridGapLength));
          }
        }
        break;
      }
      case PaletteGroup::Kind::TickStyle:
        for (Id id : g.targetIds) {
          cmds.push_back(cmdLineStyle(id, theme.tickColor, theme.tickLineWidth));
        }
        break;
      case PaletteGroup::Kind::LabelColor:
        for (Id id : g.targetIds) cmds.push_back(cmdDrawItemColor(id, theme.labelColor));
        break;
      case PaletteGroup::Kind::TextColor:
        for (Id id : g.targetIds) cmds.push_back(cmdDrawItemColor(id, theme.textColor));
        break;
      case PaletteGroup::Kind::HighlightColor:
        for (Id id : g.targetIds) cmds.push_back(cmdDrawItemColor(id, theme.highlightColor));
        break;
      case PaletteGroup::Kind::DrawingColor:
        for (Id id : g.targetIds) cmds.push_back(cmdDrawItemColor(id, theme.drawingColor));
        break;
      case PaletteGroup::Kind::PaneBackground:
        for (Id id : g.targetIds) cmds.push_back(cmdPaneClearColor(id, theme.backgroundColor));
        break;
    }
  }
  return cmds;
}

} // namespace dc
