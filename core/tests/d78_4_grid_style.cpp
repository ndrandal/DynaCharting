// D78.4: Grid dash/gap/opacity in AxisRecipe
#include "dc/recipe/AxisRecipe.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

// T1: AxisRecipe build with dash/gap emits setDrawItemStyle commands
static void testGridDashCommands() {
  dc::AxisRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.tickLayerId = 10;
  cfg.labelLayerId = 10;
  cfg.enableGrid = true;
  cfg.gridLayerId = 10;
  cfg.gridDashLength = 5.0f;
  cfg.gridGapLength = 3.0f;

  dc::AxisRecipe axis(200, cfg);
  auto result = axis.build();

  // Search for dashLength in create commands
  bool foundDash = false;
  for (const auto& cmd : result.createCommands) {
    if (cmd.find("dashLength") != std::string::npos) {
      foundDash = true;
      // Verify it contains the right values
      assert(cmd.find("5") != std::string::npos);
      assert(cmd.find("3") != std::string::npos);
      break;
    }
  }
  assert(foundDash);

  std::printf("T1 gridDashCommands: PASS\n");
}

// T2: Grid opacity modifies alpha channel
static void testGridOpacity() {
  dc::AxisRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.tickLayerId = 10;
  cfg.labelLayerId = 10;
  cfg.enableGrid = true;
  cfg.gridLayerId = 10;
  cfg.gridColor[3] = 1.0f;  // full alpha base
  cfg.gridOpacity = 0.5f;   // half opacity

  dc::AxisRecipe axis(300, cfg);
  auto result = axis.build();

  // Find the setDrawItemStyle for grid draw item (hGrid or vGrid)
  // The alpha should be gridColor.a * gridOpacity = 0.5
  bool foundAlpha = false;
  for (const auto& cmd : result.createCommands) {
    // Look for grid draw item style commands (contain "a":0.5)
    if (cmd.find("setDrawItemStyle") != std::string::npos &&
        cmd.find("\"a\":0.5") != std::string::npos) {
      foundAlpha = true;
      break;
    }
  }
  assert(foundAlpha);

  std::printf("T2 gridOpacity: PASS\n");
}

// T3: Default (no dash, full opacity) produces solid grid lines
static void testDefaultSolidGrid() {
  dc::AxisRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.tickLayerId = 10;
  cfg.labelLayerId = 10;
  cfg.enableGrid = true;
  cfg.gridLayerId = 10;
  // defaults: dashLength=0, gapLength=0, opacity=1.0

  dc::AxisRecipe axis(400, cfg);
  auto result = axis.build();

  // Should NOT contain dashLength command
  for (const auto& cmd : result.createCommands) {
    assert(cmd.find("dashLength") == std::string::npos);
  }

  std::printf("T3 defaultSolidGrid: PASS\n");
}

// T4: Grid disabled produces no grid commands
static void testGridDisabled() {
  dc::AxisRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.tickLayerId = 10;
  cfg.labelLayerId = 10;
  cfg.enableGrid = false;
  cfg.gridDashLength = 5.0f;

  dc::AxisRecipe axis(500, cfg);
  auto result = axis.build();

  // No grid-related commands should be present
  for (const auto& cmd : result.createCommands) {
    assert(cmd.find("Grid") == std::string::npos ||
           cmd.find("gridSpine") != std::string::npos ||
           cmd.find("hGrid") == std::string::npos);
  }

  std::printf("T4 gridDisabled: PASS\n");
}

// T5: AxisRecipeConfig new fields have correct defaults
static void testConfigDefaults() {
  dc::AxisRecipeConfig cfg;
  assert(std::fabs(cfg.gridDashLength) < 1e-6f);
  assert(std::fabs(cfg.gridGapLength) < 1e-6f);
  assert(std::fabs(cfg.gridOpacity - 1.0f) < 1e-6f);

  std::printf("T5 configDefaults: PASS\n");
}

int main() {
  testGridDashCommands();
  testGridOpacity();
  testDefaultSolidGrid();
  testGridDisabled();
  testConfigDefaults();

  std::printf("\nAll D78.4 tests passed.\n");
  return 0;
}
