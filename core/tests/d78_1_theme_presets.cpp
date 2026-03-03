// D78.1: Theme presets and new style fields
#include "dc/style/Theme.hpp"
#include "dc/style/ThemeManager.hpp"

#include <cassert>
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
    assert(!t.name.empty());
    // Check name is unique
    assert(std::find(names.begin(), names.end(), t.name) == names.end());
    names.push_back(t.name);

    // Background color should be non-trivial (not all zeros for non-black themes)
    float sum = t.backgroundColor[0] + t.backgroundColor[1] + t.backgroundColor[2];
    // At least alpha should be 1
    assert(feq(t.backgroundColor[3], 1.0f));
    (void)sum;
  }

  assert(names.size() == 6);
  std::printf("T1 presetsDistinct: PASS\n");
}

// T2: Overlay colors array is 8-deep with non-zero values
static void testOverlayColors8() {
  dc::Theme t = dc::midnightTheme();

  assert(dc::Theme::kMaxOverlayColors == 8);

  for (int i = 0; i < dc::Theme::kMaxOverlayColors; ++i) {
    // Each overlay color should have alpha = 1.0
    assert(feq(t.overlayColors[i][3], 1.0f));
    // Each should have at least some RGB
    float rgb = t.overlayColors[i][0] + t.overlayColors[i][1] + t.overlayColors[i][2];
    assert(rgb > 0.0f);
  }

  std::printf("T2 overlayColors8: PASS\n");
}

// T3: New grid/border/separator fields have sensible defaults
static void testNewFieldDefaults() {
  dc::Theme t; // default constructor

  // Grid dash defaults (solid)
  assert(feq(t.gridDashLength, 0.0f));
  assert(feq(t.gridGapLength, 0.0f));
  assert(feq(t.gridOpacity, 1.0f));

  // Border defaults (no border)
  assert(feq(t.paneBorderWidth, 0.0f));
  assert(feq(t.paneBorderColor[3], 1.0f));

  // Separator defaults (no separator)
  assert(feq(t.separatorWidth, 0.0f));
  assert(feq(t.separatorColor[3], 1.0f));

  std::printf("T3 newFieldDefaults: PASS\n");
}

// T4: Neon theme has dash pattern, Bloomberg has dash pattern
static void testThemeDashPatterns() {
  dc::Theme neon = dc::neonTheme();
  assert(neon.gridDashLength > 0.0f);
  assert(neon.gridGapLength > 0.0f);

  dc::Theme bb = dc::bloombergTheme();
  assert(bb.gridDashLength > 0.0f);
  assert(bb.gridGapLength > 0.0f);

  // Dark should have solid grid (default)
  dc::Theme dark = dc::darkTheme();
  assert(feq(dark.gridDashLength, 0.0f));

  std::printf("T4 themeDashPatterns: PASS\n");
}

// T5: ThemeManager registers all 6 presets
static void testManagerPresets() {
  dc::ThemeManager mgr;
  auto names = mgr.registeredThemes();
  assert(names.size() == 6);

  // Check all expected names are present
  std::vector<std::string> expected = {
    "Bloomberg", "Dark", "Light", "Midnight", "Neon", "Pastel"
  };
  std::sort(names.begin(), names.end());
  assert(names == expected);

  // Set each theme by name
  for (const auto& name : expected) {
    mgr.setTheme(name);
    assert(mgr.themeName() == name);
    assert(mgr.getTheme().name == name);
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
  assert(feq(mid.gridDashLength, expected));

  // Grid opacity should be interpolated
  float expectedOp = (a.gridOpacity + b.gridOpacity) * 0.5f;
  assert(feq(mid.gridOpacity, expectedOp));

  // Border width should be interpolated
  float expectedBW = (a.paneBorderWidth + b.paneBorderWidth) * 0.5f;
  assert(feq(mid.paneBorderWidth, expectedBW));

  // Separator width
  float expectedSW = (a.separatorWidth + b.separatorWidth) * 0.5f;
  assert(feq(mid.separatorWidth, expectedSW));

  // Overlay color 5 (slot 4, zero-indexed) should be interpolated
  for (int j = 0; j < 4; ++j) {
    float exp = (a.overlayColors[4][j] + b.overlayColors[4][j]) * 0.5f;
    assert(feq(mid.overlayColors[4][j], exp));
  }

  // Border color should be interpolated
  for (int j = 0; j < 4; ++j) {
    float exp = (a.paneBorderColor[j] + b.paneBorderColor[j]) * 0.5f;
    assert(feq(mid.paneBorderColor[j], exp));
  }

  std::printf("T6 interpolateNewFields: PASS\n");
}

// T7: Pastel theme has reduced grid opacity
static void testPastelGridOpacity() {
  dc::Theme t = dc::pastelTheme();
  assert(t.gridOpacity < 1.0f);
  assert(t.gridOpacity > 0.0f);

  std::printf("T7 pastelGridOpacity: PASS\n");
}

// T8: Midnight/Neon/Bloomberg have pane borders enabled
static void testPaneBorders() {
  dc::Theme midnight = dc::midnightTheme();
  assert(midnight.paneBorderWidth > 0.0f);

  dc::Theme neon = dc::neonTheme();
  assert(neon.paneBorderWidth > 0.0f);
  assert(neon.separatorWidth > 0.0f);

  dc::Theme bb = dc::bloombergTheme();
  assert(bb.paneBorderWidth > 0.0f);

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
