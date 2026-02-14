#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// D17.2: RSI (Relative Strength Index) recipe.
// Renders RSI as a lineAA@1 line with optional overbought/oversold reference lines.
//
// ID layout (6 slots):
//   0: RSI line buffer (rect4)
//   1: RSI line geometry
//   2: RSI line drawItem (lineAA@1)
//   3: Ref lines buffer (rect4)
//   4: Ref lines geometry
//   5: Ref lines drawItem (lineAA@1)
struct RSIRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name{"RSI"};
  float color[4] = {0.5f, 0.0f, 1.0f, 1.0f};         // purple
  float lineWidth{1.5f};
  int period{14};
  // Reference lines at overbought/oversold levels
  bool showRefLines{true};
  float overboughtLevel{70.0f};
  float oversoldLevel{30.0f};
  float refLineColor[4] = {0.5f, 0.5f, 0.5f, 0.5f};   // gray
};

class RSIRecipe : public Recipe {
public:
  RSIRecipe(Id idBase, const RSIRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override;
  std::vector<SeriesInfo> seriesInfoList() const override;

  Id lineBufferId() const   { return rid(0); }
  Id lineGeomId() const     { return rid(1); }
  Id lineDrawItemId() const { return rid(2); }
  Id refBufferId() const    { return rid(3); }
  Id refGeomId() const      { return rid(4); }
  Id refDrawItemId() const  { return rid(5); }

  static constexpr std::uint32_t ID_SLOTS = 6;

  struct RSIData {
    std::vector<float> lineSegments;   // rect4: x0,y0,x1,y1 per segment
    std::uint32_t segmentCount{0};
  };

  // Compute RSI line segments from close prices.
  // xCoords: X position per data point (same length as closes).
  // RSI values are in range [0, 100] on Y axis.
  RSIData computeRSI(const float* closes, int count,
                      const float* xCoords) const;

  struct RefLineData {
    std::vector<float> lineSegments;   // rect4: two horizontal lines
    std::uint32_t segmentCount{0};
  };

  // Compute horizontal reference lines spanning [xMin, xMax].
  RefLineData computeRefLines(float xMin, float xMax) const;

private:
  RSIRecipeConfig config_;
};

} // namespace dc
