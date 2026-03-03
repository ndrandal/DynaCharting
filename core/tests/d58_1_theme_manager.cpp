// D58.1 — ThemeManager: set/apply/callback/presets
#include "dc/style/ThemeManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

static bool near(float a, float b, float eps = 0.01f) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D58.1 ThemeManager Tests ===\n");

  // Test 1: Default state — starts with "Dark"
  {
    dc::ThemeManager tm;
    check(tm.themeName() == "Dark", "default theme name is Dark");
    check(near(tm.getTheme().backgroundColor[0], 0.1f), "default bg R matches dark theme");
  }

  // Test 2: registeredThemes contains both presets
  {
    dc::ThemeManager tm;
    auto names = tm.registeredThemes();
    check(names.size() == 6, "6 presets registered");
    check(std::find(names.begin(), names.end(), "Dark") != names.end(), "Dark is registered");
    check(std::find(names.begin(), names.end(), "Light") != names.end(), "Light is registered");
  }

  // Test 3: setTheme by name switches to Light
  {
    dc::ThemeManager tm;
    tm.setTheme("Light");
    check(tm.themeName() == "Light", "name updated to Light");
    check(near(tm.getTheme().backgroundColor[0], 0.95f), "bg R matches light theme");
  }

  // Test 4: setTheme by name with non-existent name — no change
  {
    dc::ThemeManager tm;
    tm.setTheme("NonExistent");
    check(tm.themeName() == "Dark", "name unchanged for non-existent theme");
  }

  // Test 5: setTheme by Theme object
  {
    dc::ThemeManager tm;
    dc::Theme custom;
    custom.name = "Custom";
    custom.backgroundColor[0] = 0.5f;
    custom.backgroundColor[1] = 0.5f;
    custom.backgroundColor[2] = 0.5f;
    custom.backgroundColor[3] = 1.0f;

    tm.setTheme(custom);
    check(tm.themeName() == "Custom", "name is Custom after setTheme(theme)");
    check(near(tm.getTheme().backgroundColor[0], 0.5f), "bg R matches custom theme");
  }

  // Test 6: Callback fires on setTheme
  {
    dc::ThemeManager tm;
    int callbackCount = 0;
    std::string receivedName;
    tm.setOnThemeChanged([&](const dc::Theme& t) {
      ++callbackCount;
      receivedName = t.name;
    });

    tm.setTheme("Light");
    check(callbackCount == 1, "callback fired once");
    check(receivedName == "Light", "callback received Light theme");

    tm.setTheme("Dark");
    check(callbackCount == 2, "callback fired twice");
    check(receivedName == "Dark", "callback received Dark theme");
  }

  // Test 7: Callback fires on setTheme(Theme)
  {
    dc::ThemeManager tm;
    int callbackCount = 0;
    tm.setOnThemeChanged([&](const dc::Theme&) { ++callbackCount; });

    dc::Theme custom;
    custom.name = "MyTheme";
    tm.setTheme(custom);
    check(callbackCount == 1, "callback fired on setTheme(Theme)");
  }

  // Test 8: registerTheme adds a new preset
  {
    dc::ThemeManager tm;
    dc::Theme custom;
    custom.name = "Solarized";
    tm.registerTheme("Solarized", custom);

    auto names = tm.registeredThemes();
    check(names.size() == 7, "7 presets after register");
    check(std::find(names.begin(), names.end(), "Solarized") != names.end(),
          "Solarized is registered");

    tm.setTheme("Solarized");
    check(tm.themeName() == "Solarized", "can switch to registered theme");
  }

  // Test 9: applyTheme through CommandProcessor
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    // Build scene: 1 pane, 1 layer, 1 drawItem
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":100,"layerId":10})");

    dc::ThemeTarget target;
    target.paneIds = {1};
    target.candleDrawItemIds = {100};

    dc::ThemeManager tm;
    tm.setTheme("Light");

    int count = tm.applyTheme(cp, target);
    check(count == 2, "applyTheme returned 2 commands applied (1 pane + 1 candle)");

    // Verify scene state reflects light theme
    const dc::Pane* pane = scene.getPane(1);
    check(pane != nullptr && pane->hasClearColor, "pane has clear color after apply");
    check(near(pane->clearColor[0], 0.95f), "pane bg R = light theme");

    const dc::DrawItem* di = scene.getDrawItem(100);
    check(di != nullptr, "drawItem exists");
    dc::Theme lt = dc::lightTheme();
    check(near(di->colorUp[1], lt.candleUp[1]), "drawItem colorUp G = light theme");
  }

  // Test 10: No callback if not set
  {
    dc::ThemeManager tm;
    // No setOnThemeChanged — should not crash
    tm.setTheme("Light");
    check(tm.themeName() == "Light", "setTheme works without callback");
  }

  std::printf("=== D58.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
