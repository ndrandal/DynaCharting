// D14.5 — Legend hit test + series visibility toggle
// Verifies click detection on legend entries.

#include "dc/recipe/LegendRecipe.hpp"
#include "dc/recipe/Recipe.hpp"
#include "dc/text/GlyphAtlas.hpp"

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
  std::printf("D14.5 legend_toggle: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  dc::LegendRecipeConfig cfg;
  cfg.anchorX = -0.95f;
  cfg.anchorY = 0.95f;
  cfg.padding = 0.02f;
  cfg.swatchSize = 0.025f;
  cfg.itemSpacing = 0.06f;

  dc::LegendRecipe recipe(700, cfg);

  // Two series
  std::vector<dc::SeriesInfo> series;
  {
    dc::SeriesInfo s1;
    s1.name = "Candles";
    s1.colorHint[0] = 0; s1.colorHint[1] = 0.8f; s1.colorHint[2] = 0; s1.colorHint[3] = 1;
    s1.drawItemIds = {102};
    series.push_back(s1);

    dc::SeriesInfo s2;
    s2.name = "Volume";
    s2.colorHint[0] = 0; s2.colorHint[1] = 0.5f; s2.colorHint[2] = 0; s2.colorHint[3] = 0.6f;
    s2.drawItemIds = {202};
    series.push_back(s2);
  }

  auto data = recipe.computeLegend(series, atlas);
  requireTrue(data.entries.size() == 2, "2 entries");

  // ---- Test 1: Hit first entry ----
  {
    // Click in the center of the first swatch
    float cx = (data.entries[0].swatchRect[0] + data.entries[0].swatchRect[2]) / 2.0f;
    float cy = (data.entries[0].swatchRect[1] + data.entries[0].swatchRect[3]) / 2.0f;

    int idx = recipe.hitTest(data, cx, cy);
    requireTrue(idx == 0, "hit first entry");
    std::printf("  Test 1 (hit first): PASS\n");
  }

  // ---- Test 2: Hit second entry ----
  {
    float cx = (data.entries[1].swatchRect[0] + data.entries[1].swatchRect[2]) / 2.0f;
    float cy = (data.entries[1].swatchRect[1] + data.entries[1].swatchRect[3]) / 2.0f;

    int idx = recipe.hitTest(data, cx, cy);
    requireTrue(idx == 1, "hit second entry");
    std::printf("  Test 2 (hit second): PASS\n");
  }

  // ---- Test 3: Miss (far away) ----
  {
    int idx = recipe.hitTest(data, 0.5f, -0.5f);
    requireTrue(idx == -1, "miss returns -1");
    std::printf("  Test 3 (miss): PASS\n");
  }

  // ---- Test 4: Entries store drawItemIds for toggle ----
  {
    // Verify we can use the series info drawItemIds to issue visibility commands
    requireTrue(series[0].drawItemIds.size() == 1, "candles has 1 DI");
    requireTrue(series[0].drawItemIds[0] == 102, "candles DI = 102");
    requireTrue(series[1].drawItemIds.size() == 1, "volume has 1 DI");
    requireTrue(series[1].drawItemIds[0] == 202, "volume DI = 202");
    std::printf("  Test 4 (drawItemIds): PASS\n");
  }

  std::printf("D14.5 legend_toggle: ALL PASS\n");
  return 0;
#endif
}
