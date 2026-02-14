// D14.3 — SeriesInfo on Recipe base
// Verifies that recipes expose series metadata for legend display.

#include "dc/recipe/Recipe.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/VolumeRecipe.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  // ---- Test 1: Recipe base returns empty ----
  {
    class DummyRecipe : public dc::Recipe {
    public:
      DummyRecipe() : Recipe(1) {}
      dc::RecipeBuildResult build() const override { return {}; }
    };

    DummyRecipe dummy;
    auto info = dummy.seriesInfoList();
    requireTrue(info.empty(), "base recipe returns empty seriesInfoList");
    std::printf("  Test 1 (base empty): PASS\n");
  }

  // ---- Test 2: CandleRecipe returns series info ----
  {
    dc::CandleRecipeConfig cfg;
    cfg.name = "BTCUSD";
    cfg.colorUp[0] = 0.0f; cfg.colorUp[1] = 1.0f;
    cfg.colorUp[2] = 0.0f; cfg.colorUp[3] = 1.0f;

    dc::CandleRecipe recipe(100, cfg);
    auto info = recipe.seriesInfoList();

    requireTrue(info.size() == 1, "candle has 1 series");
    requireTrue(info[0].name == "BTCUSD", "series name matches");
    requireTrue(info[0].colorHint[1] == 1.0f, "colorHint green channel");
    requireTrue(info[0].defaultVisible, "default visible");
    requireTrue(info[0].drawItemIds.size() == 1, "1 drawItem");
    requireTrue(info[0].drawItemIds[0] == 102, "drawItemId = 102");

    std::printf("  Test 2 (CandleRecipe): PASS\n");
  }

  // ---- Test 3: VolumeRecipe returns series info ----
  {
    dc::VolumeRecipeConfig cfg;
    cfg.name = "Volume";
    cfg.colorUp[0] = 0.0f; cfg.colorUp[1] = 0.5f;
    cfg.colorUp[2] = 0.0f; cfg.colorUp[3] = 0.6f;

    dc::VolumeRecipe recipe(200, cfg);
    auto info = recipe.seriesInfoList();

    requireTrue(info.size() == 1, "volume has 1 series");
    requireTrue(info[0].name == "Volume", "series name");
    requireTrue(info[0].colorHint[1] == 0.5f, "colorHint green");
    requireTrue(info[0].drawItemIds.size() == 1, "1 drawItem");
    requireTrue(info[0].drawItemIds[0] == 202, "drawItemId = 202");

    std::printf("  Test 3 (VolumeRecipe): PASS\n");
  }

  // ---- Test 4: Default name fallback ----
  {
    dc::CandleRecipeConfig cfg;
    // name is empty
    dc::CandleRecipe recipe(300, cfg);
    auto info = recipe.seriesInfoList();
    requireTrue(info[0].name == "Candles", "default candle name fallback");

    dc::VolumeRecipeConfig vcfg;
    dc::VolumeRecipe vrec(400, vcfg);
    auto vinfo = vrec.seriesInfoList();
    requireTrue(vinfo[0].name == "Volume", "default volume name fallback");

    std::printf("  Test 4 (default names): PASS\n");
  }

  std::printf("D14.3 series_info: ALL PASS\n");
  return 0;
}
