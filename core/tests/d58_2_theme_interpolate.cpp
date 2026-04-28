// D58.2 — ThemeManager::interpolate: verify t=0, t=0.5, t=1 produce correct results.
#include "dc/recipe/ChartTheme.hpp"
#include "dc/style/ThemeManager.hpp"

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

static bool near(float a, float b, float eps = 0.001f) {
  return std::fabs(a - b) < eps;
}

static float midpoint(float a, float b) {
  return a + (b - a) * 0.5f;
}

int main() {
  namespace ct = dc::chart_theme;
  std::printf("=== D58.2 Theme Interpolation Tests ===\n");

  dc::Theme dark = dc::darkTheme();
  dc::Theme light = dc::lightTheme();

  // Test 1: t=0 should match t1 (dark)
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 0.0f);
    check(near(result.backgroundColor[0], dark.backgroundColor[0]), "t=0 bg R matches dark");
    check(near(result.backgroundColor[1], dark.backgroundColor[1]), "t=0 bg G matches dark");
    check(near(result.backgroundColor[2], dark.backgroundColor[2]), "t=0 bg B matches dark");
    check(near(result.backgroundColor[3], dark.backgroundColor[3]), "t=0 bg A matches dark");

    check(near(ct::candleUp(result)[0], ct::candleUp(dark)[0]), "t=0 candle up R matches dark");
    check(near(ct::candleUp(result)[1], ct::candleUp(dark)[1]), "t=0 candle up G matches dark");
    check(near(ct::candleDown(result)[0], ct::candleDown(dark)[0]), "t=0 candle down R matches dark");

    check(near(result.gridColor[0], dark.gridColor[0]), "t=0 gridColor R matches dark");
    check(near(result.labelColor[0], dark.labelColor[0]), "t=0 labelColor R matches dark");
    check(near(ct::crosshair(result)[0], ct::crosshair(dark)[0]), "t=0 crosshair R matches dark");

    check(near(result.gridLineWidth, dark.gridLineWidth), "t=0 gridLineWidth matches dark");
    check(near(result.tickLineWidth, dark.tickLineWidth), "t=0 tickLineWidth matches dark");

    check(near(ct::overlay(result, 0)[0], ct::overlay(dark, 0)[0]), "t=0 overlay[0] R matches dark");
    check(near(ct::overlay(result, 1)[0], ct::overlay(dark, 1)[0]), "t=0 overlay[1] R matches dark");

    check(near(ct::volumeUp(result)[0], ct::volumeUp(dark)[0]), "t=0 volume up R matches dark");
    check(near(result.textColor[0], dark.textColor[0]), "t=0 textColor R matches dark");
    check(near(result.highlightColor[0], dark.highlightColor[0]), "t=0 highlightColor R matches dark");
    check(near(result.drawingColor[0], dark.drawingColor[0]), "t=0 drawingColor R matches dark");
  }

  // Test 2: t=1 should match t2 (light)
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 1.0f);
    check(near(result.backgroundColor[0], light.backgroundColor[0]), "t=1 bg R matches light");
    check(near(ct::candleUp(result)[0], ct::candleUp(light)[0]), "t=1 candle up R matches light");
    check(near(ct::candleDown(result)[0], ct::candleDown(light)[0]), "t=1 candle down R matches light");

    check(near(result.gridColor[0], light.gridColor[0]), "t=1 gridColor R matches light");
    check(near(result.labelColor[0], light.labelColor[0]), "t=1 labelColor R matches light");
    check(near(ct::crosshair(result)[0], ct::crosshair(light)[0]), "t=1 crosshair R matches light");

    check(near(ct::overlay(result, 0)[0], ct::overlay(light, 0)[0]), "t=1 overlay[0] R matches light");
    check(near(ct::overlay(result, 2)[0], ct::overlay(light, 2)[0]), "t=1 overlay[2] R matches light");

    check(near(ct::volumeUp(result)[0], ct::volumeUp(light)[0]), "t=1 volume up R matches light");
    check(near(ct::volumeDown(result)[0], ct::volumeDown(light)[0]), "t=1 volume down R matches light");
    check(near(result.textColor[0], light.textColor[0]), "t=1 textColor R matches light");
    check(near(result.highlightColor[0], light.highlightColor[0]), "t=1 highlightColor R matches light");
    check(near(result.drawingColor[0], light.drawingColor[0]), "t=1 drawingColor R matches light");
  }

  // Test 3: t=0.5 should be midpoint
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 0.5f);

    check(near(result.backgroundColor[0], midpoint(dark.backgroundColor[0], light.backgroundColor[0])),
          "t=0.5 bg R is midpoint");
    check(near(ct::candleUp(result)[1], midpoint(ct::candleUp(dark)[1], ct::candleUp(light)[1])),
          "t=0.5 candle up G is midpoint");
    check(near(result.gridColor[0], midpoint(dark.gridColor[0], light.gridColor[0])),
          "t=0.5 gridColor R is midpoint");
    check(near(result.labelColor[0], midpoint(dark.labelColor[0], light.labelColor[0])),
          "t=0.5 labelColor R is midpoint");
    check(near(ct::crosshair(result)[0], midpoint(ct::crosshair(dark)[0], ct::crosshair(light)[0])),
          "t=0.5 crosshair R is midpoint");
    check(near(ct::overlay(result, 0)[2], midpoint(ct::overlay(dark, 0)[2], ct::overlay(light, 0)[2])),
          "t=0.5 overlay[0] B is midpoint");
    check(near(ct::volumeUp(result)[0], midpoint(ct::volumeUp(dark)[0], ct::volumeUp(light)[0])),
          "t=0.5 volume up R is midpoint");
    check(near(result.textColor[0], midpoint(dark.textColor[0], light.textColor[0])),
          "t=0.5 textColor R is midpoint");
    check(near(result.highlightColor[0], midpoint(dark.highlightColor[0], light.highlightColor[0])),
          "t=0.5 highlightColor R is midpoint");
    check(near(result.drawingColor[0], midpoint(dark.drawingColor[0], light.drawingColor[0])),
          "t=0.5 drawingColor R is midpoint");
    check(near(result.gridLineWidth, midpoint(dark.gridLineWidth, light.gridLineWidth)),
          "t=0.5 gridLineWidth is midpoint");
  }

  // Test 4: t=0.25 — quarter interpolation on bg.
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 0.25f);
    float expected = dark.backgroundColor[0]
                     + (light.backgroundColor[0] - dark.backgroundColor[0]) * 0.25f;
    check(near(result.backgroundColor[0], expected), "t=0.25 bg R correct");
  }

  // Test 5: same-theme interpolation is identity.
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, dark, 0.5f);
    check(near(result.backgroundColor[0], dark.backgroundColor[0]), "same theme interp bg R unchanged");
    check(near(ct::candleUp(result)[1], ct::candleUp(dark)[1]), "same theme interp candle up G unchanged");
    check(near(result.gridLineWidth, dark.gridLineWidth), "same theme interp gridLineWidth unchanged");
  }

  std::printf("=== D58.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
