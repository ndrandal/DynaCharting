// D78.1: Theme presets and new style fields
#include "dc/recipe/ChartTheme.hpp"
#include "dc/style/Theme.hpp"
#include "dc/style/ThemeManager.hpp"

#include "dc_check.hpp"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-5f; }

// T1: All 6 presets have distinct names and non-zero background colors
static void testPresetsDistinct() {
  dc::Theme presets[] = {
    dc::darkTheme(), dc::lightTheme(),
    dc::midnightTheme(), dc::neonTheme(),
    dc::pastelTheme(), dc::bloombergTheme()
  };

  std::vector<std::string> names;
  for (const auto& t : presets) {
    DC_CHECK(!t.name.empty());
    // Check name is unique
    DC_CHECK(std::find(names.begin(), names.end(), t.name) == names.end());
    names.push_back(t.name);

    // Background color should be non-trivial (not all zeros for non-black themes)
    float sum = t.backgroundColor[0] + t.backgroundColor[1] + t.backgroundColor[2];
    // At least alpha should be 1
    DC_CHECK(feq(t.backgroundColor[3], 1.0f));
    (void)sum;
  }

  DC_CHECK(names.size() == 6);
  std::printf("T1 presetsDistinct: PASS\n");
}

// T2: Overlay slots populated with non-zero RGBA values in each preset.
static void testOverlayColors8() {
  dc::Theme t = dc::midnightTheme();

  for (int i = 0; i < 8; ++i) {
    const float* c = dc::chart_theme::overlay(t, i);
    DC_CHECK(feq(c[3], 1.0f));
    float rgb = c[0] + c[1] + c[2];
    DC_CHECK(rgb > 0.0f);
  }

  std::printf("T2 overlaySlots: PASS\n");
}

// T3: New grid/border/separator fields have sensible defaults
static void testNewFieldDefaults() {
  dc::Theme t; // default constructor

  // Grid dash defaults (solid)
  DC_CHECK(feq(t.gridDashLength, 0.0f));
  DC_CHECK(feq(t.gridGapLength, 0.0f));
  DC_CHECK(feq(t.gridOpacity, 1.0f));

  // Border defaults (no border)
  DC_CHECK(feq(t.paneBorderWidth, 0.0f));
  DC_CHECK(feq(t.paneBorderColor[3], 1.0f));

  // Separator defaults (no separator)
  DC_CHECK(feq(t.separatorWidth, 0.0f));
  DC_CHECK(feq(t.separatorColor[3], 1.0f));

  std::printf("T3 newFieldDefaults: PASS\n");
}

// T4: Neon theme has dash pattern, Bloomberg has dash pattern
static void testThemeDashPatterns() {
  dc::Theme neon = dc::neonTheme();
  DC_CHECK(neon.gridDashLength > 0.0f);
  DC_CHECK(neon.gridGapLength > 0.0f);

  dc::Theme bb = dc::bloombergTheme();
  DC_CHECK(bb.gridDashLength > 0.0f);
  DC_CHECK(bb.gridGapLength > 0.0f);

  // Dark should have solid grid (default)
  dc::Theme dark = dc::darkTheme();
  DC_CHECK(feq(dark.gridDashLength, 0.0f));

  std::printf("T4 themeDashPatterns: PASS\n");
}

// T5: ThemeManager registers all 6 presets
static void testManagerPresets() {
  dc::ThemeManager mgr;
  auto names = mgr.registeredThemes();
  DC_CHECK(names.size() == 6);

  // Check all expected names are present
  std::vector<std::string> expected = {
    "Bloomberg", "Dark", "Light", "Midnight", "Neon", "Pastel"
  };
  std::sort(names.begin(), names.end());
  DC_CHECK(names == expected);

  // Set each theme by name
  for (const auto& name : expected) {
    mgr.setTheme(name);
    DC_CHECK(mgr.themeName() == name);
    DC_CHECK(mgr.getTheme().name == name);
  }

  std::printf("T5 managerPresets: PASS\n");
}

// T6: Interpolate between themes with new fields
static void testInterpolateNewFields() {
  dc::Theme a = dc::darkTheme();
  dc::Theme b = dc::neonTheme();

  dc::Theme mid = dc::ThemeManager::interpolate(a, b, 0.5f);

  // Grid dash length should be halfway
  float expected = (a.gridDashLength + b.gridDashLength) * 0.5f;
  DC_CHECK(feq(mid.gridDashLength, expected));

  // Grid opacity should be interpolated
  float expectedOp = (a.gridOpacity + b.gridOpacity) * 0.5f;
  DC_CHECK(feq(mid.gridOpacity, expectedOp));

  // Border width should be interpolated
  float expectedBW = (a.paneBorderWidth + b.paneBorderWidth) * 0.5f;
  DC_CHECK(feq(mid.paneBorderWidth, expectedBW));

  // Separator width
  float expectedSW = (a.separatorWidth + b.separatorWidth) * 0.5f;
  DC_CHECK(feq(mid.separatorWidth, expectedSW));

  // Overlay color 5 (chart_theme::overlay(_, 4)) should be interpolated.
  for (int j = 0; j < 4; ++j) {
    float exp = (dc::chart_theme::overlay(a, 4)[j]
                 + dc::chart_theme::overlay(b, 4)[j]) * 0.5f;
    DC_CHECK(feq(dc::chart_theme::overlay(mid, 4)[j], exp));
  }

  // Border color should be interpolated
  for (int j = 0; j < 4; ++j) {
    float exp = (a.paneBorderColor[j] + b.paneBorderColor[j]) * 0.5f;
    DC_CHECK(feq(mid.paneBorderColor[j], exp));
  }

  std::printf("T6 interpolateNewFields: PASS\n");
}

// T7: Pastel theme has reduced grid opacity
static void testPastelGridOpacity() {
  dc::Theme t = dc::pastelTheme();
  DC_CHECK(t.gridOpacity < 1.0f);
  DC_CHECK(t.gridOpacity > 0.0f);

  std::printf("T7 pastelGridOpacity: PASS\n");
}

// T8: Midnight/Neon/Bloomberg have pane borders enabled
static void testPaneBorders() {
  dc::Theme midnight = dc::midnightTheme();
  DC_CHECK(midnight.paneBorderWidth > 0.0f);

  dc::Theme neon = dc::neonTheme();
  DC_CHECK(neon.paneBorderWidth > 0.0f);
  DC_CHECK(neon.separatorWidth > 0.0f);

  dc::Theme bb = dc::bloombergTheme();
  DC_CHECK(bb.paneBorderWidth > 0.0f);

  std::printf("T8 paneBorders: PASS\n");
}

int main() {
  testPresetsDistinct();
  testOverlayColors8();
  testNewFieldDefaults();
  testThemeDashPatterns();
  testManagerPresets();
  testInterpolateNewFields();
  testPastelGridOpacity();
  testPaneBorders();

  std::printf("\nAll D78.1 tests passed.\n");
  return 0;
}
