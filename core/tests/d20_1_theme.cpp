// D20.1-D20.3 — Theme struct, presets, and command generation (D81: palette/
// PaletteGroup based API).
#include "dc/recipe/ChartTheme.hpp"
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
  namespace ct = dc::chart_theme;

  // --- Test 1: presets differ in background + palette slot 0 (candle up) ---
  {
    dc::Theme dark = dc::darkTheme();
    dc::Theme light = dc::lightTheme();

    requireTrue(dark.name == "Dark", "dark theme name");
    requireTrue(light.name == "Light", "light theme name");

    requireNear(dark.backgroundColor[0], 0.1f, 0.01f, "dark bg R");
    requireNear(light.backgroundColor[0], 0.95f, 0.01f, "light bg R");
    requireTrue(std::fabs(dark.backgroundColor[0] - light.backgroundColor[0]) > 0.5f,
                "dark/light bg R differ significantly");

    // Candle up (palette slot 0) differs between presets.
    requireTrue(std::fabs(ct::candleUp(dark)[1] - ct::candleUp(light)[1]) > 0.05f,
                "dark/light candle up G differ");

    requireTrue(dark.labelColor[0] > 0.5f, "dark label bright");
    requireTrue(light.labelColor[0] < 0.5f, "light label dark");

    // Overlay colors live at palette slots 2..9.
    requireNear(ct::overlay(dark, 0)[2], 1.0f, 0.01f, "dark overlay[0] B");
    requireNear(ct::overlay(dark, 1)[0], 1.0f, 0.01f, "dark overlay[1] R (orange)");

    std::printf("  Test 1 (presets): PASS\n");
  }

  // --- Test 2: generateThemeCommands groups + counts ---
  {
    dc::Theme theme = dc::darkTheme();
    dc::ThemeTarget target;
    target.groups.push_back(ct::paneBackgroundGroup({1, 2}));   // 2 cmds
    target.groups.push_back(ct::candleGroup({100}));            // 1 cmd
    target.groups.push_back(ct::volumeGroup({200, 201}));       // 2 cmds
    target.groups.push_back(ct::overlayGroup({300, 301, 302})); // 3 cmds
    target.groups.push_back(ct::gridGroup({400}));              // 1 cmd (no dash on dark)
    target.groups.push_back(ct::tickGroup({500, 501}));         // 2 cmds
    target.groups.push_back(ct::textGroup({600}));              // 1 cmd
    target.groups.push_back(ct::highlightGroup({700}));         // 1 cmd
    target.groups.push_back(ct::drawingGroup({800}));           // 1 cmd
    target.groups.push_back(ct::crosshairGroup({900}));         // 1 cmd
    // Total: 2 + 1 + 2 + 3 + 1 + 2 + 1 + 1 + 1 + 1 = 15

    auto cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.size() == 15, "expected 15 commands");

    requireTrue(cmds[0].find("setPaneClearColor") != std::string::npos, "cmd[0] pane");
    requireTrue(cmds[1].find("setPaneClearColor") != std::string::npos, "cmd[1] pane");

    requireTrue(cmds[2].find("setDrawItemStyle") != std::string::npos, "cmd[2] style");
    requireTrue(cmds[2].find("colorUpR") != std::string::npos, "cmd[2] colorUpR");
    requireTrue(cmds[2].find("colorDownR") != std::string::npos, "cmd[2] colorDownR");

    requireTrue(cmds[5].find("setDrawItemColor") != std::string::npos, "cmd[5] overlay");
    requireTrue(cmds[8].find("lineWidth") != std::string::npos, "cmd[8] grid lineWidth");

    std::printf("  Test 2 (command count and types): PASS\n");
  }

  // --- Test 3: end-to-end apply via CommandProcessor ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})"), "candleDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":101,"layerId":10})"), "gridDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10})"), "textDI");

    dc::Theme theme = dc::lightTheme();
    dc::ThemeTarget target;
    target.groups.push_back(ct::paneBackgroundGroup({1}));
    target.groups.push_back(ct::candleGroup({100}));
    target.groups.push_back(ct::gridGroup({101}));
    target.groups.push_back(ct::labelGroup({102}));

    auto cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.size() == 4, "expected 4 commands");
    for (const auto& cmd : cmds) requireOk(cp.applyJsonText(cmd), cmd.c_str());

    const auto* pane = scene.getPane(1);
    requireTrue(pane && pane->hasClearColor, "pane has clear color");
    requireNear(pane->clearColor[0], theme.backgroundColor[0], 0.01f, "pane clearColor R");

    const auto* candleDi = scene.getDrawItem(100);
    requireNear(candleDi->colorUp[0], ct::candleUp(theme)[0], 0.01f, "candle up R");
    requireNear(candleDi->colorDown[0], ct::candleDown(theme)[0], 0.01f, "candle down R");

    const auto* gridDi = scene.getDrawItem(101);
    requireNear(gridDi->color[0], theme.gridColor[0], 0.01f, "grid color R");
    requireNear(gridDi->lineWidth, theme.gridLineWidth, 0.01f, "grid lineWidth");

    const auto* textDi = scene.getDrawItem(102);
    requireNear(textDi->color[0], theme.labelColor[0], 0.01f, "label color R");

    std::printf("  Test 3 (apply via CommandProcessor): PASS\n");
  }

  // --- Test 4: empty target emits zero commands ---
  {
    dc::Theme theme = dc::darkTheme();
    dc::ThemeTarget target;
    auto cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.empty(), "empty target => zero commands");
    std::printf("  Test 4 (empty target): PASS\n");
  }

  // --- Test 5: overlay rotation across 10 items (cycles slots 2..9) ---
  {
    dc::Theme theme = dc::darkTheme();
    dc::ThemeTarget target;
    target.groups.push_back(ct::overlayGroup({300, 301, 302, 303, 304, 305, 306, 307, 308, 309}));

    auto cmds = dc::generateThemeCommands(theme, target);
    requireTrue(cmds.size() == 10, "10 overlay commands");

    char needle[64];
    // cmd[0] uses overlay(0) == palette[2]; cmd[8] wraps back to overlay(0).
    std::snprintf(needle, sizeof(needle), "\"r\":%.9g", ct::overlay(theme, 0)[0]);
    requireTrue(cmds[0].find(needle) != std::string::npos, "cmd[0] overlay[0] R");
    requireTrue(cmds[8].find(needle) != std::string::npos, "cmd[8] overlay[0] R wraps");

    std::snprintf(needle, sizeof(needle), "\"r\":%.9g", ct::overlay(theme, 1)[0]);
    requireTrue(cmds[1].find(needle) != std::string::npos, "cmd[1] overlay[1] R");

    std::printf("  Test 5 (overlay rotation): PASS\n");
  }

  // --- Test 6: theme switch dark -> light ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})"), "createDI");

    dc::ThemeTarget target;
    target.groups.push_back(ct::paneBackgroundGroup({1}));
    target.groups.push_back(ct::candleGroup({100}));

    dc::Theme dark = dc::darkTheme();
    for (const auto& cmd : dc::generateThemeCommands(dark, target))
      requireOk(cp.applyJsonText(cmd), "apply dark");
    const auto* pane = scene.getPane(1);
    requireNear(pane->clearColor[0], dark.backgroundColor[0], 0.01f, "dark bg applied R");
    const auto* di = scene.getDrawItem(100);
    requireNear(di->colorUp[1], ct::candleUp(dark)[1], 0.01f, "dark candle up G applied");

    dc::Theme light = dc::lightTheme();
    for (const auto& cmd : dc::generateThemeCommands(light, target))
      requireOk(cp.applyJsonText(cmd), "apply light");
    pane = scene.getPane(1);
    requireNear(pane->clearColor[0], light.backgroundColor[0], 0.01f, "light bg applied R");
    di = scene.getDrawItem(100);
    requireNear(di->colorUp[1], ct::candleUp(light)[1], 0.01f, "light candle up G applied");
    requireNear(di->colorDown[0], ct::candleDown(light)[0], 0.01f, "light candle down R applied");

    std::printf("  Test 6 (theme switch): PASS\n");
  }

  std::printf("D20.1-D20.3 theme: ALL PASS\n");
  return 0;
}
