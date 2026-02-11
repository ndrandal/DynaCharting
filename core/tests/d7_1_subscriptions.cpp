// D7.1 — Recipe subscription metadata test (pure C++, no GL)
// Verifies: each recipe's build() produces correct subscriptions and drawItemIds().

#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/SmaRecipe.hpp"
#include "dc/recipe/AreaRecipe.hpp"
#include "dc/recipe/BollingerRecipe.hpp"
#include "dc/recipe/MacdRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/recipe/CrosshairRecipe.hpp"
#include "dc/recipe/LevelLineRecipe.hpp"
#include "dc/recipe/TextRecipe.hpp"
#include "dc/scene/Geometry.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  // --- CandleRecipe ---
  {
    dc::CandleRecipeConfig cfg;
    cfg.layerId = 10; cfg.name = "c";
    dc::CandleRecipe r(100, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 1, "candle: 1 subscription");
    requireTrue(br.subscriptions[0].bufferId == 100, "candle: bufferId");
    requireTrue(br.subscriptions[0].geometryId == 101, "candle: geomId");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Candle6, "candle: format");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 1 && ids[0] == 102, "candle: drawItemIds");
    std::printf("  CandleRecipe PASS\n");
  }

  // --- LineRecipe ---
  {
    dc::LineRecipeConfig cfg;
    cfg.layerId = 10; cfg.name = "l";
    dc::LineRecipe r(200, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 1, "line: 1 subscription");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "line: format");
    requireTrue(br.subscriptions[0].bufferId == 200, "line: bufferId");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 1 && ids[0] == 202, "line: drawItemIds");
    std::printf("  LineRecipe PASS\n");
  }

  // --- SmaRecipe ---
  {
    dc::SmaRecipeConfig cfg;
    cfg.layerId = 10; cfg.name = "s";
    dc::SmaRecipe r(300, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 1, "sma: 1 subscription");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "sma: format");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 1 && ids[0] == 302, "sma: drawItemIds");
    std::printf("  SmaRecipe PASS\n");
  }

  // --- AreaRecipe ---
  {
    dc::AreaRecipeConfig cfg;
    cfg.layerId = 10; cfg.name = "a";
    dc::AreaRecipe r(400, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 1, "area: 1 subscription");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "area: format");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 1 && ids[0] == 402, "area: drawItemIds");
    std::printf("  AreaRecipe PASS\n");
  }

  // --- BollingerRecipe ---
  {
    dc::BollingerRecipeConfig cfg;
    cfg.lineLayerId = 10; cfg.fillLayerId = 11; cfg.name = "bb";
    dc::BollingerRecipe r(500, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 4, "bollinger: 4 subscriptions");
    // middle, upper, lower = Pos2_Clip; fill = Pos2_Clip
    for (std::size_t i = 0; i < 4; i++)
      requireTrue(br.subscriptions[i].format == dc::VertexFormat::Pos2_Clip, "bollinger: format");
    requireTrue(br.subscriptions[0].bufferId == 500, "bollinger: middle bufferId");
    requireTrue(br.subscriptions[1].bufferId == 503, "bollinger: upper bufferId");
    requireTrue(br.subscriptions[2].bufferId == 506, "bollinger: lower bufferId");
    requireTrue(br.subscriptions[3].bufferId == 509, "bollinger: fill bufferId");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 4, "bollinger: 4 drawItemIds");
    requireTrue(ids[0] == 502 && ids[1] == 505 && ids[2] == 508 && ids[3] == 511,
                "bollinger: drawItemIds values");
    std::printf("  BollingerRecipe PASS\n");
  }

  // --- MacdRecipe ---
  {
    dc::MacdRecipeConfig cfg;
    cfg.lineLayerId = 10; cfg.histLayerId = 11; cfg.name = "macd";
    dc::MacdRecipe r(600, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 4, "macd: 4 subscriptions");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "macd: line format");
    requireTrue(br.subscriptions[1].format == dc::VertexFormat::Pos2_Clip, "macd: signal format");
    requireTrue(br.subscriptions[2].format == dc::VertexFormat::Rect4, "macd: posHist format");
    requireTrue(br.subscriptions[3].format == dc::VertexFormat::Rect4, "macd: negHist format");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 4, "macd: 4 drawItemIds");
    requireTrue(ids[0] == 602 && ids[1] == 605 && ids[2] == 608 && ids[3] == 611,
                "macd: drawItemIds values");
    std::printf("  MacdRecipe PASS\n");
  }

  // --- AxisRecipe ---
  {
    dc::AxisRecipeConfig cfg;
    cfg.tickLayerId = 10; cfg.labelLayerId = 11; cfg.name = "axis";
    dc::AxisRecipe r(700, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 3, "axis: 3 subscriptions");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "axis: yTick format");
    requireTrue(br.subscriptions[1].format == dc::VertexFormat::Pos2_Clip, "axis: xTick format");
    requireTrue(br.subscriptions[2].format == dc::VertexFormat::Glyph8, "axis: label format");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 3, "axis: 3 drawItemIds");
    requireTrue(ids[0] == 702 && ids[1] == 705 && ids[2] == 708,
                "axis: drawItemIds values");
    std::printf("  AxisRecipe PASS\n");
  }

  // --- CrosshairRecipe ---
  {
    dc::CrosshairRecipeConfig cfg;
    cfg.lineLayerId = 10; cfg.labelLayerId = 11; cfg.name = "xh";
    dc::CrosshairRecipe r(800, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 4, "crosshair: 4 subscriptions");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "crosshair: hLine");
    requireTrue(br.subscriptions[1].format == dc::VertexFormat::Pos2_Clip, "crosshair: vLine");
    requireTrue(br.subscriptions[2].format == dc::VertexFormat::Glyph8, "crosshair: priceLabel");
    requireTrue(br.subscriptions[3].format == dc::VertexFormat::Glyph8, "crosshair: timeLabel");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 4, "crosshair: 4 drawItemIds");
    requireTrue(ids[0] == 802 && ids[1] == 805 && ids[2] == 808 && ids[3] == 811,
                "crosshair: drawItemIds values");
    std::printf("  CrosshairRecipe PASS\n");
  }

  // --- LevelLineRecipe ---
  {
    dc::LevelLineRecipeConfig cfg;
    cfg.lineLayerId = 10; cfg.labelLayerId = 11; cfg.name = "lvl";
    dc::LevelLineRecipe r(900, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 2, "levelLine: 2 subscriptions");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Pos2_Clip, "levelLine: line");
    requireTrue(br.subscriptions[1].format == dc::VertexFormat::Glyph8, "levelLine: label");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 2, "levelLine: 2 drawItemIds");
    requireTrue(ids[0] == 902 && ids[1] == 905, "levelLine: drawItemIds values");
    std::printf("  LevelLineRecipe PASS\n");
  }

  // --- TextRecipe ---
  {
    dc::TextRecipeConfig cfg;
    cfg.layerId = 10; cfg.name = "txt";
    dc::TextRecipe r(1000, cfg);
    auto br = r.build();
    requireTrue(br.subscriptions.size() == 1, "text: 1 subscription");
    requireTrue(br.subscriptions[0].format == dc::VertexFormat::Glyph8, "text: format");
    requireTrue(br.subscriptions[0].bufferId == 1000, "text: bufferId");
    auto ids = r.drawItemIds();
    requireTrue(ids.size() == 1 && ids[0] == 1002, "text: drawItemIds");
    std::printf("  TextRecipe PASS\n");
  }

  std::printf("\nD7.1 subscriptions PASS\n");
  return 0;
}
