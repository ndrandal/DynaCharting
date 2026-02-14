// D20.1-D20.3 — Theme struct, presets, and command generation
// Tests:
//   1. darkTheme() and lightTheme() return valid themes with different bg colors
//   2. generateThemeCommands produces correct number of commands for targets
//   3. Apply commands through CommandProcessor — verify scene state changes
//   4. Empty target produces no commands

#include "dc/style/Theme.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void requireNear(float a, float b, float eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: darkTheme() and lightTheme() return valid themes with different bg colors ---
  {
    dc::Theme dark = dc::darkTheme();
    dc::Theme light = dc::lightTheme();

    requireTrue(dark.name == "Dark", "dark theme name");
    requireTrue(light.name == "Light", "light theme name");

    // Dark bg should be near (0.1, 0.1, 0.12)
    requireNear(dark.backgroundColor[0], 0.1f, 0.01f, "dark bg R");
    requireNear(dark.backgroundColor[1], 0.1f, 0.01f, "dark bg G");
    requireNear(dark.backgroundColor[2], 0.12f, 0.01f, "dark bg B");
    requireNear(dark.backgroundColor[3], 1.0f, 0.01f, "dark bg A");

    // Light bg should be near (0.95, 0.95, 0.96)
    requireNear(light.backgroundColor[0], 0.95f, 0.01f, "light bg R");
    requireNear(light.backgroundColor[1], 0.95f, 0.01f, "light bg G");
    requireNear(light.backgroundColor[2], 0.96f, 0.01f, "light bg B");
    requireNear(light.backgroundColor[3], 1.0f, 0.01f, "light bg A");

    // Backgrounds must differ
    requireTrue(std::fabs(dark.backgroundColor[0] - light.backgroundColor[0]) > 0.5f,
                "dark/light bg R differ significantly");

    // Candle colors should differ
    requireTrue(std::fabs(dark.candleUp[1] - light.candleUp[1]) > 0.05f,
                "dark/light candleUp G differ");

    // Label colors should differ (dark = bright, light = dark)
    requireTrue(dark.labelColor[0] > 0.5f, "dark label bright");
    requireTrue(light.labelColor[0] < 0.5f, "light label dark");

    // Overlay colors present (4 slots)
    requireNear(dark.overlayColors[0][2], 1.0f, 0.01f, "dark overlay[0] B");
    requireNear(dark.overlayColors[1][0], 1.0f, 0.01f, "dark overlay[1] R (orange)");

    std::printf("  Test 1 (darkTheme/lightTheme presets): PASS\n");
  }

  // --- Test 2: generateThemeCommands produces correct number of commands ---
  {
    dc::Theme theme = dc::darkTheme();
    dc::ThemeTarget target;
    target.paneIds = {1, 2};                       // 2 pane commands
    target.candleDrawItemIds = {100};               // 1 candle command
    target.volumeDrawItemIds = {200, 201};          // 2 volume commands
    target.overlayDrawItemIds = {300, 301, 302};    // 3 overlay commands
    target.gridDrawItemIds = {400};                 // 1 grid command
    target.tickDrawItemIds = {500, 501};            // 2 tick commands
    target.textDrawItemIds = {600};                 // 1 text command
    target.highlightDrawItemIds = {700};            // 1 highlight command
    target.drawingDrawItemIds = {800};              // 1 drawing command
    target.crosshairDrawItemIds = {900};            // 1 crosshair command
    // Total: 2 + 1 + 2 + 3 + 1 + 2 + 1 + 1 + 1 + 1 = 15

    std::vector<std::string> cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.size() == 15, "expected 15 commands");

    // First two should be setPaneClearColor
    requireTrue(cmds[0].find("setPaneClearColor") != std::string::npos,
                "cmd[0] is setPaneClearColor");
    requireTrue(cmds[1].find("setPaneClearColor") != std::string::npos,
                "cmd[1] is setPaneClearColor");

    // Third should be candle style (setDrawItemStyle with colorUp/colorDown)
    requireTrue(cmds[2].find("setDrawItemStyle") != std::string::npos,
                "cmd[2] is setDrawItemStyle");
    requireTrue(cmds[2].find("colorUpR") != std::string::npos,
                "cmd[2] has colorUpR");
    requireTrue(cmds[2].find("colorDownR") != std::string::npos,
                "cmd[2] has colorDownR");

    // Overlay commands should be setDrawItemColor
    requireTrue(cmds[5].find("setDrawItemColor") != std::string::npos,
                "cmd[5] is setDrawItemColor (overlay)");

    // Grid commands should have lineWidth
    requireTrue(cmds[8].find("lineWidth") != std::string::npos,
                "cmd[8] has lineWidth (grid)");

    std::printf("  Test 2 (command count and types): PASS\n");
  }

  // --- Test 3: Apply commands through CommandProcessor — verify scene state ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    // Create scene structure: 1 pane, 1 layer, 3 drawItems
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})"), "candleDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":101,"layerId":10})"), "gridDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10})"), "textDI");

    // Generate theme commands targeting these resources
    dc::Theme theme = dc::lightTheme();
    dc::ThemeTarget target;
    target.paneIds = {1};
    target.candleDrawItemIds = {100};
    target.gridDrawItemIds = {101};
    target.textDrawItemIds = {102};

    std::vector<std::string> cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.size() == 4, "expected 4 commands (1 pane + 1 candle + 1 grid + 1 text)");

    // Apply all commands
    for (const auto& cmd : cmds) {
      requireOk(cp.applyJsonText(cmd), cmd.c_str());
    }

    // Verify pane clear color matches light theme background
    const dc::Pane* pane = scene.getPane(1);
    requireTrue(pane != nullptr, "pane exists");
    requireTrue(pane->hasClearColor, "pane has clear color after theme apply");
    requireNear(pane->clearColor[0], theme.backgroundColor[0], 0.01f, "pane clearColor R");
    requireNear(pane->clearColor[1], theme.backgroundColor[1], 0.01f, "pane clearColor G");
    requireNear(pane->clearColor[2], theme.backgroundColor[2], 0.01f, "pane clearColor B");
    requireNear(pane->clearColor[3], theme.backgroundColor[3], 0.01f, "pane clearColor A");

    // Verify candle drawItem has light theme candle colors
    const dc::DrawItem* candleDi = scene.getDrawItem(100);
    requireTrue(candleDi != nullptr, "candle drawItem exists");
    requireNear(candleDi->colorUp[0], theme.candleUp[0], 0.01f, "candle colorUp R");
    requireNear(candleDi->colorUp[1], theme.candleUp[1], 0.01f, "candle colorUp G");
    requireNear(candleDi->colorUp[2], theme.candleUp[2], 0.01f, "candle colorUp B");
    requireNear(candleDi->colorDown[0], theme.candleDown[0], 0.01f, "candle colorDown R");
    requireNear(candleDi->colorDown[1], theme.candleDown[1], 0.01f, "candle colorDown G");
    requireNear(candleDi->colorDown[2], theme.candleDown[2], 0.01f, "candle colorDown B");

    // Verify grid drawItem has light theme grid color + lineWidth
    const dc::DrawItem* gridDi = scene.getDrawItem(101);
    requireTrue(gridDi != nullptr, "grid drawItem exists");
    requireNear(gridDi->color[0], theme.gridColor[0], 0.01f, "grid color R");
    requireNear(gridDi->color[1], theme.gridColor[1], 0.01f, "grid color G");
    requireNear(gridDi->color[2], theme.gridColor[2], 0.01f, "grid color B");
    requireNear(gridDi->lineWidth, theme.gridLineWidth, 0.01f, "grid lineWidth");

    // Verify text drawItem has light theme label color
    const dc::DrawItem* textDi = scene.getDrawItem(102);
    requireTrue(textDi != nullptr, "text drawItem exists");
    requireNear(textDi->color[0], theme.labelColor[0], 0.01f, "text color R");
    requireNear(textDi->color[1], theme.labelColor[1], 0.01f, "text color G");
    requireNear(textDi->color[2], theme.labelColor[2], 0.01f, "text color B");

    std::printf("  Test 3 (apply commands via CommandProcessor): PASS\n");
  }

  // --- Test 4: Empty target produces no commands ---
  {
    dc::Theme theme = dc::darkTheme();
    dc::ThemeTarget target;  // all vectors empty

    std::vector<std::string> cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.empty(), "empty target produces zero commands");

    std::printf("  Test 4 (empty target): PASS\n");
  }

  // --- Test 5: Overlay round-robin colors ---
  {
    dc::Theme theme = dc::darkTheme();
    dc::ThemeTarget target;
    // 6 overlays: should cycle through 4 colors, wrapping at index 4 and 5
    target.overlayDrawItemIds = {300, 301, 302, 303, 304, 305};

    std::vector<std::string> cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.size() == 6, "6 overlay commands");

    // cmd[0] and cmd[4] should both use overlayColors[0] (blue)
    // Just check they both contain the same blue R value
    char needle[64];
    std::snprintf(needle, sizeof(needle), "\"r\":%.9g", theme.overlayColors[0][0]);
    requireTrue(cmds[0].find(needle) != std::string::npos, "cmd[0] has overlay[0] R");
    requireTrue(cmds[4].find(needle) != std::string::npos, "cmd[4] has overlay[0] R (wrap)");

    // cmd[1] should use overlayColors[1] (orange R=1.0)
    std::snprintf(needle, sizeof(needle), "\"r\":%.9g", theme.overlayColors[1][0]);
    requireTrue(cmds[1].find(needle) != std::string::npos, "cmd[1] has overlay[1] R");

    std::printf("  Test 5 (overlay round-robin): PASS\n");
  }

  // --- Test 6: Theme switch — apply dark then light, verify state updates ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})"), "createDI");

    dc::ThemeTarget target;
    target.paneIds = {1};
    target.candleDrawItemIds = {100};

    // Apply dark theme
    dc::Theme dark = dc::darkTheme();
    for (const auto& cmd : dc::generateThemeCommands(dark, target)) {
      requireOk(cp.applyJsonText(cmd), "apply dark");
    }

    const dc::Pane* pane = scene.getPane(1);
    requireNear(pane->clearColor[0], dark.backgroundColor[0], 0.01f, "dark bg applied R");

    const dc::DrawItem* di = scene.getDrawItem(100);
    requireNear(di->colorUp[1], dark.candleUp[1], 0.01f, "dark candleUp G applied");

    // Now switch to light theme
    dc::Theme light = dc::lightTheme();
    for (const auto& cmd : dc::generateThemeCommands(light, target)) {
      requireOk(cp.applyJsonText(cmd), "apply light");
    }

    pane = scene.getPane(1);
    requireNear(pane->clearColor[0], light.backgroundColor[0], 0.01f, "light bg applied R");

    di = scene.getDrawItem(100);
    requireNear(di->colorUp[1], light.candleUp[1], 0.01f, "light candleUp G applied");
    requireNear(di->colorDown[0], light.candleDown[0], 0.01f, "light candleDown R applied");

    std::printf("  Test 6 (theme switch dark->light): PASS\n");
  }

  std::printf("D20.1-D20.3 theme: ALL PASS\n");
  return 0;
}
