// D58.2 — ThemeManager::interpolate: verify t=0, t=0.5, t=1 produce correct results
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

    check(near(result.candleUp[0], dark.candleUp[0]), "t=0 candleUp R matches dark");
    check(near(result.candleUp[1], dark.candleUp[1]), "t=0 candleUp G matches dark");
    check(near(result.candleDown[0], dark.candleDown[0]), "t=0 candleDown R matches dark");

    check(near(result.gridColor[0], dark.gridColor[0]), "t=0 gridColor R matches dark");
    check(near(result.labelColor[0], dark.labelColor[0]), "t=0 labelColor R matches dark");
    check(near(result.crosshairColor[0], dark.crosshairColor[0]), "t=0 crosshairColor R matches dark");

    check(near(result.gridLineWidth, dark.gridLineWidth), "t=0 gridLineWidth matches dark");
    check(near(result.tickLineWidth, dark.tickLineWidth), "t=0 tickLineWidth matches dark");

    check(near(result.overlayColors[0][0], dark.overlayColors[0][0]), "t=0 overlay[0] R matches dark");
    check(near(result.overlayColors[1][0], dark.overlayColors[1][0]), "t=0 overlay[1] R matches dark");

    check(near(result.volumeUp[0], dark.volumeUp[0]), "t=0 volumeUp R matches dark");
    check(near(result.textColor[0], dark.textColor[0]), "t=0 textColor R matches dark");
    check(near(result.highlightColor[0], dark.highlightColor[0]), "t=0 highlightColor R matches dark");
    check(near(result.drawingColor[0], dark.drawingColor[0]), "t=0 drawingColor R matches dark");
  }

  // Test 2: t=1 should match t2 (light)
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 1.0f);
    check(near(result.backgroundColor[0], light.backgroundColor[0]), "t=1 bg R matches light");
    check(near(result.backgroundColor[1], light.backgroundColor[1]), "t=1 bg G matches light");
    check(near(result.backgroundColor[2], light.backgroundColor[2]), "t=1 bg B matches light");
    check(near(result.backgroundColor[3], light.backgroundColor[3]), "t=1 bg A matches light");

    check(near(result.candleUp[0], light.candleUp[0]), "t=1 candleUp R matches light");
    check(near(result.candleDown[0], light.candleDown[0]), "t=1 candleDown R matches light");

    check(near(result.gridColor[0], light.gridColor[0]), "t=1 gridColor R matches light");
    check(near(result.labelColor[0], light.labelColor[0]), "t=1 labelColor R matches light");
    check(near(result.crosshairColor[0], light.crosshairColor[0]), "t=1 crosshairColor R matches light");

    check(near(result.overlayColors[0][0], light.overlayColors[0][0]), "t=1 overlay[0] R matches light");
    check(near(result.overlayColors[2][0], light.overlayColors[2][0]), "t=1 overlay[2] R matches light");

    check(near(result.volumeUp[0], light.volumeUp[0]), "t=1 volumeUp R matches light");
    check(near(result.volumeDown[0], light.volumeDown[0]), "t=1 volumeDown R matches light");
    check(near(result.textColor[0], light.textColor[0]), "t=1 textColor R matches light");
    check(near(result.highlightColor[0], light.highlightColor[0]), "t=1 highlightColor R matches light");
    check(near(result.drawingColor[0], light.drawingColor[0]), "t=1 drawingColor R matches light");
  }

  // Test 3: t=0.5 should be midpoint
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 0.5f);

    float expectedBgR = midpoint(dark.backgroundColor[0], light.backgroundColor[0]);
    float expectedBgG = midpoint(dark.backgroundColor[1], light.backgroundColor[1]);
    float expectedBgB = midpoint(dark.backgroundColor[2], light.backgroundColor[2]);
    check(near(result.backgroundColor[0], expectedBgR), "t=0.5 bg R is midpoint");
    check(near(result.backgroundColor[1], expectedBgG), "t=0.5 bg G is midpoint");
    check(near(result.backgroundColor[2], expectedBgB), "t=0.5 bg B is midpoint");

    float expectedCandleUpG = midpoint(dark.candleUp[1], light.candleUp[1]);
    check(near(result.candleUp[1], expectedCandleUpG), "t=0.5 candleUp G is midpoint");

    float expectedGridR = midpoint(dark.gridColor[0], light.gridColor[0]);
    check(near(result.gridColor[0], expectedGridR), "t=0.5 gridColor R is midpoint");

    float expectedLabelR = midpoint(dark.labelColor[0], light.labelColor[0]);
    check(near(result.labelColor[0], expectedLabelR), "t=0.5 labelColor R is midpoint");

    float expectedCrosshairR = midpoint(dark.crosshairColor[0], light.crosshairColor[0]);
    check(near(result.crosshairColor[0], expectedCrosshairR), "t=0.5 crosshairColor R is midpoint");

    float expectedOverlay0B = midpoint(dark.overlayColors[0][2], light.overlayColors[0][2]);
    check(near(result.overlayColors[0][2], expectedOverlay0B), "t=0.5 overlay[0] B is midpoint");

    float expectedVolumeUpR = midpoint(dark.volumeUp[0], light.volumeUp[0]);
    check(near(result.volumeUp[0], expectedVolumeUpR), "t=0.5 volumeUp R is midpoint");

    float expectedTextR = midpoint(dark.textColor[0], light.textColor[0]);
    check(near(result.textColor[0], expectedTextR), "t=0.5 textColor R is midpoint");

    float expectedHighlightR = midpoint(dark.highlightColor[0], light.highlightColor[0]);
    check(near(result.highlightColor[0], expectedHighlightR), "t=0.5 highlightColor R is midpoint");

    float expectedDrawingR = midpoint(dark.drawingColor[0], light.drawingColor[0]);
    check(near(result.drawingColor[0], expectedDrawingR), "t=0.5 drawingColor R is midpoint");

    float expectedGridLW = midpoint(dark.gridLineWidth, light.gridLineWidth);
    check(near(result.gridLineWidth, expectedGridLW), "t=0.5 gridLineWidth is midpoint");
  }

  // Test 4: t=0.25 — verify quarter interpolation
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, light, 0.25f);
    float expectedBgR = dark.backgroundColor[0] + (light.backgroundColor[0] - dark.backgroundColor[0]) * 0.25f;
    check(near(result.backgroundColor[0], expectedBgR), "t=0.25 bg R correct");
  }

  // Test 5: Interpolation between identical themes yields same theme
  {
    dc::Theme result = dc::ThemeManager::interpolate(dark, dark, 0.5f);
    check(near(result.backgroundColor[0], dark.backgroundColor[0]), "same theme interp bg R unchanged");
    check(near(result.candleUp[1], dark.candleUp[1]), "same theme interp candleUp G unchanged");
    check(near(result.gridLineWidth, dark.gridLineWidth), "same theme interp gridLineWidth unchanged");
  }

  std::printf("=== D58.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
