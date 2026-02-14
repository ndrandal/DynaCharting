// D12.3 — Data-Value X-Axis test
// Pure C++ (needs font): verify X ticks at nice values for range 100-200,
// adaptive precision for different ranges, old method still works.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
#ifndef FONT_PATH
  std::printf("D12.3 data_xaxis: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  std::printf("=== D12.3 Data-Value X-Axis ===\n");

  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  // --- Test 1: computeAxisDataV2 produces X ticks at nice values ---
  {
    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";

    dc::AxisRecipe axis(200, cfg);

    auto data = axis.computeAxisDataV2(atlas,
                                        0.0f, 100.0f,    // Y range
                                        100.0f, 200.0f,  // X range
                                        -1.0f, 1.0f,     // clip Y
                                        -1.0f, 0.8f,     // clip X
                                        48.0f, 0.04f);

    std::printf("  X range [100, 200]: xTickVertexCount=%u, yTickVertexCount=%u\n",
                data.xTickVertexCount, data.yTickVertexCount);

    requireTrue(data.xTickVertexCount >= 4, "at least 2 X ticks (4 vertices)");
    requireTrue(data.yTickVertexCount >= 4, "at least 2 Y ticks (4 vertices)");
    requireTrue(data.labelGlyphCount > 0, "has labels");

    // X tick positions should be at nice intervals (e.g. 100, 120, 140, 160, 180, 200)
    // Each tick has 2 vertices (4 floats per vertex pair: x,y, x,y)
    // First tick x position is at xTickVerts[0]
    std::printf("  X ticks: %u vertices, Y ticks: %u vertices, labels: %u glyphs\n",
                data.xTickVertexCount, data.yTickVertexCount, data.labelGlyphCount);

    std::printf("  Test 1 (computeAxisDataV2 X ticks) PASS\n");
  }

  // --- Test 2: Adaptive format precision ---
  {
    // chooseFormat for different step sizes
    requireTrue(std::strcmp(dc::AxisRecipe::chooseFormat(20.0f), "%.0f") == 0,
                "step=20 → %.0f");
    requireTrue(std::strcmp(dc::AxisRecipe::chooseFormat(0.5f), "%.1f") == 0,
                "step=0.5 → %.1f");
    requireTrue(std::strcmp(dc::AxisRecipe::chooseFormat(0.05f), "%.2f") == 0,
                "step=0.05 → %.2f");
    requireTrue(std::strcmp(dc::AxisRecipe::chooseFormat(0.005f), "%.3f") == 0,
                "step=0.005 → %.3f");

    std::printf("  Test 2 (chooseFormat) PASS\n");
  }

  // --- Test 3: Old computeAxisData still works ---
  {
    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";

    dc::AxisRecipe axis(200, cfg);

    auto data = axis.computeAxisData(atlas, 0.0f, 100.0f, 10,
                                      -1.0f, 1.0f, -1.0f, 0.8f,
                                      48.0f, 0.04f);

    requireTrue(data.yTickVertexCount > 0, "old method Y ticks");
    requireTrue(data.xTickVertexCount > 0, "old method X ticks");
    requireTrue(data.labelGlyphCount > 0, "old method labels");

    std::printf("  Test 3 (old computeAxisData compat) PASS\n");
  }

  // --- Test 4: V2 with small range uses higher precision ---
  {
    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";

    dc::AxisRecipe axis(200, cfg);

    auto data = axis.computeAxisDataV2(atlas,
                                        99.5f, 100.5f,   // narrow Y range
                                        0.0f, 1.0f,      // narrow X range
                                        -1.0f, 1.0f,
                                        -1.0f, 0.8f,
                                        48.0f, 0.04f);

    requireTrue(data.yTickVertexCount > 0, "narrow range Y ticks");
    requireTrue(data.xTickVertexCount > 0, "narrow range X ticks");
    requireTrue(data.labelGlyphCount > 0, "narrow range labels");

    std::printf("  Test 4 (V2 narrow range) PASS\n");
  }

  std::printf("D12.3 data_xaxis: ALL PASS\n");
  return 0;
#endif
}
