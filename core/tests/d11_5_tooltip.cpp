// D11.5 — TooltipRecipe test
// Tests: build → 6 resources, HitResult(hit=true) + formatter → visible with glyphs,
// bg rect width ≈ text advance + 2×padding, HitResult(hit=false) → not visible.

#include "dc/recipe/TooltipRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/viewport/DataPicker.hpp"
#include "dc/layout/PaneLayout.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
#ifndef FONT_PATH
  std::printf("D11.5 tooltip: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  dc::TooltipRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.layerId = 10;
  cfg.name = "tooltip";
  cfg.fontSize = 0.03f;
  cfg.glyphPx = 48.0f;
  cfg.padding = 0.015f;

  dc::TooltipRecipe recipe(2000, cfg);

  // --- Test 1: build() → 6 resources ---
  {
    auto buildResult = recipe.build();
    // 5 create cmds for bg (buf, geo, drawItem, bind, color) + 5 for text = 10
    requireTrue(buildResult.createCommands.size() >= 10, "build creates 10+ commands");
    requireTrue(buildResult.disposeCommands.size() == 6, "dispose has 6 commands");
    requireTrue(recipe.drawItemIds().size() == 2, "2 drawItems (bg + text)");

    std::printf("  Test 1 (build → 6 resources) PASS\n");
  }

  // --- Test 2: HitResult(hit=true) + formatter → visible, glyphCount > 0 ---
  {
    dc::HitResult hit;
    hit.hit = true;
    hit.drawItemId = 40;
    hit.recordIndex = 0;
    hit.dataX = 5.0;
    hit.dataY = 100.0;
    hit.distancePx = 3.0;

    dc::TooltipFormatter formatter = [](const dc::HitResult& h) -> std::string {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "Price: %.0f", h.dataY);
      return buf;
    };

    dc::PaneRegion clip{-1.0f, 1.0f, -1.0f, 1.0f};
    auto data = recipe.computeTooltip(hit, 0.0, 0.0, clip, atlas, formatter);

    requireTrue(data.visible, "tooltip visible");
    requireTrue(data.glyphCount > 0, "has glyphs");
    requireTrue(data.bgCount == 1, "1 bg rect");
    requireTrue(data.bgRect.size() == 4, "bg rect has 4 floats");

    // bg rect width ≈ text advance + 2×padding
    float bgWidth = data.bgRect[2] - data.bgRect[0];
    requireTrue(bgWidth > 2.0f * cfg.padding, "bg width > 2×padding");

    std::printf("  glyphCount=%u bgWidth=%.4f\n", data.glyphCount, bgWidth);
    std::printf("  Test 2 (hit=true → visible tooltip) PASS\n");
  }

  // --- Test 3: HitResult(hit=false) → not visible ---
  {
    dc::HitResult noHit;
    noHit.hit = false;

    dc::TooltipFormatter formatter = [](const dc::HitResult&) -> std::string {
      return "should not appear";
    };

    dc::PaneRegion clip{-1.0f, 1.0f, -1.0f, 1.0f};
    auto data = recipe.computeTooltip(noHit, 0.0, 0.0, clip, atlas, formatter);

    requireTrue(!data.visible, "tooltip not visible when no hit");
    requireTrue(data.glyphCount == 0, "no glyphs");
    requireTrue(data.bgCount == 0, "no bg rect");

    std::printf("  Test 3 (hit=false → not visible) PASS\n");
  }

  std::printf("D11.5 tooltip: ALL PASS\n");
  return 0;
#endif
}
