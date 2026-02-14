// D14.4 — LegendRecipe unit test
// Verifies legend layout from series info with colored swatches + text.

#include "dc/recipe/LegendRecipe.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/VolumeRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
#ifndef FONT_PATH
  std::printf("D14.4 legend: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  // ---- Test 1: build() produces valid commands ----
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    cp.setGlyphAtlas(&atlas);

    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");

    dc::LegendRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "legend";

    dc::LegendRecipe recipe(600, cfg);
    auto build = recipe.build();

    requireTrue(build.createCommands.size() >= 8, "at least 8 create commands");

    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, "create command ok");
    }

    requireTrue(scene.hasBuffer(600), "swatch buffer");
    requireTrue(scene.hasGeometry(601), "swatch geom");
    requireTrue(scene.hasDrawItem(602), "swatch DI");
    requireTrue(scene.hasBuffer(603), "text buffer");
    requireTrue(scene.hasGeometry(604), "text geom");
    requireTrue(scene.hasDrawItem(605), "text DI");

    const auto* swatchDI = scene.getDrawItem(602);
    requireTrue(swatchDI && swatchDI->pipeline == "instancedRect@1", "swatch pipeline");
    const auto* textDI = scene.getDrawItem(605);
    requireTrue(textDI && textDI->pipeline == "textSDF@1", "text pipeline");

    std::printf("  Test 1 (build): PASS\n");
  }

  // ---- Test 2: computeLegend ----
  {
    dc::LegendRecipeConfig cfg;
    dc::LegendRecipe recipe(700, cfg);

    // Create series info from candle + volume recipes
    dc::CandleRecipeConfig ccfg;
    ccfg.name = "BTCUSD";
    dc::CandleRecipe candle(100, ccfg);

    dc::VolumeRecipeConfig vcfg;
    vcfg.name = "Volume";
    dc::VolumeRecipe volume(200, vcfg);

    std::vector<dc::SeriesInfo> allSeries;
    for (auto& s : candle.seriesInfoList()) allSeries.push_back(s);
    for (auto& s : volume.seriesInfoList()) allSeries.push_back(s);

    auto data = recipe.computeLegend(allSeries, atlas);

    requireTrue(data.swatchCount == 2, "2 swatches");
    requireTrue(data.swatchRects.size() == 8, "8 floats for 2 rects");
    requireTrue(data.glyphCount > 0, "has text glyphs");
    requireTrue(data.entries.size() == 2, "2 entries");
    requireTrue(data.entries[0].label == "BTCUSD", "first entry label");
    requireTrue(data.entries[1].label == "Volume", "second entry label");

    // Verify vertical ordering (first entry Y > second entry Y)
    requireTrue(data.swatchRects[3] > data.swatchRects[7], "first swatch above second");

    std::printf("  Test 2 (computeLegend): PASS\n");
    std::printf("    swatches: %u, glyphs: %u\n", data.swatchCount, data.glyphCount);
  }

  // ---- Test 3: empty series ----
  {
    dc::LegendRecipeConfig cfg;
    dc::LegendRecipe recipe(800, cfg);

    std::vector<dc::SeriesInfo> empty;
    auto data = recipe.computeLegend(empty, atlas);
    requireTrue(data.swatchCount == 0, "no swatches for empty");
    requireTrue(data.glyphCount == 0, "no glyphs for empty");
    requireTrue(data.entries.empty(), "no entries for empty");

    std::printf("  Test 3 (empty): PASS\n");
  }

  std::printf("D14.4 legend: ALL PASS\n");
  return 0;
#endif
}
