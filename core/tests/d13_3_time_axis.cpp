// D13.3 — AxisRecipe time-axis integration test (pure C++, needs font)

#include "dc/recipe/AxisRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <cstdlib>

static int tests = 0;
static int passed = 0;

static void check(bool cond, const char* msg) {
  tests++;
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    std::exit(1);
  }
  passed++;
  std::printf("  OK: %s\n", msg);
}

int main() {
#ifndef FONT_PATH
  std::printf("D13.3 time_axis: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  dc::GlyphAtlas atlas;
  check(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  // 24-hour range in epoch seconds
  float tMin = 1700000000.0f;
  float tMax = tMin + 86400.0f;

  // Time-axis config
  dc::AxisRecipeConfig timeCfg;
  timeCfg.paneId = 1;
  timeCfg.tickLayerId = 1;
  timeCfg.labelLayerId = 2;
  timeCfg.name = "timeAxis";
  timeCfg.xAxisIsTime = true;
  timeCfg.useUTC = true;

  dc::AxisRecipe timeAxis(100, timeCfg);

  auto timeData = timeAxis.computeAxisDataV2(atlas,
    90.0f, 110.0f,
    tMin, tMax,
    -1.0f, 1.0f,
    -1.0f, 0.75f,
    48.0f, 0.04f);

  // X ticks should be time-aligned
  check(timeData.xTickVertexCount >= 2, "time-axis: has X tick vertices");
  check(timeData.labelGlyphCount > 0, "time-axis: has label glyphs");

  std::printf("  xTickVertexCount = %u\n", timeData.xTickVertexCount);
  std::printf("  labelGlyphCount = %u\n", timeData.labelGlyphCount);

  // Numeric axis for comparison
  dc::AxisRecipeConfig numCfg;
  numCfg.paneId = 1;
  numCfg.tickLayerId = 1;
  numCfg.labelLayerId = 2;
  numCfg.name = "numAxis";
  numCfg.xAxisIsTime = false;

  dc::AxisRecipe numAxis(200, numCfg);

  auto numData = numAxis.computeAxisDataV2(atlas,
    90.0f, 110.0f,
    tMin, tMax,
    -1.0f, 1.0f,
    -1.0f, 0.75f,
    48.0f, 0.04f);

  // The time and numeric axes should produce different tick positions
  // because NiceTimeTicks snaps to time intervals, not powers of 10
  bool ticksDiffer = (timeData.xTickVertexCount != numData.xTickVertexCount);
  if (!ticksDiffer && timeData.xTickVerts.size() >= 2 && numData.xTickVerts.size() >= 2) {
    // Even if count matches, positions should differ
    ticksDiffer = (timeData.xTickVerts[0] != numData.xTickVerts[0]);
  }
  check(ticksDiffer, "time-axis vs numeric: different tick positions");

  // Y-axis should be identical (both use numeric NiceTicks)
  check(timeData.yTickVertexCount == numData.yTickVertexCount,
        "Y-axis unchanged between time/numeric");

  std::printf("D13.3 time_axis: %d/%d PASS\n", passed, tests);
  return 0;
#endif
}
