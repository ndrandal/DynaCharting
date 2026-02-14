#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// D17.4: Stochastic oscillator recipe (%K and %D lines).
// Renders two lineAA@1 lines with optional overbought/oversold reference lines.
//
// ID layout (9 slots):
//   0: %K line buffer (rect4)
//   1: %K line geometry
//   2: %K line drawItem (lineAA@1)
//   3: %D line buffer (rect4)
//   4: %D line geometry
//   5: %D line drawItem (lineAA@1)
//   6: Ref lines buffer (rect4)
//   7: Ref lines geometry
//   8: Ref lines drawItem (lineAA@1)
struct StochasticRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name{"Stochastic"};
  float kColor[4] = {0.0f, 0.5f, 1.0f, 1.0f};        // blue
  float dColor[4] = {1.0f, 0.5f, 0.0f, 1.0f};        // orange
  float lineWidth{1.5f};
  int kPeriod{14};
  int dPeriod{3};
  bool showRefLines{true};
  float overboughtLevel{80.0f};
  float oversoldLevel{20.0f};
  float refLineColor[4] = {0.5f, 0.5f, 0.5f, 0.5f};
};

class StochasticRecipe : public Recipe {
public:
  StochasticRecipe(Id idBase, const StochasticRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override;
  std::vector<SeriesInfo> seriesInfoList() const override;

  Id kBufferId() const      { return rid(0); }
  Id kGeomId() const        { return rid(1); }
  Id kDrawItemId() const    { return rid(2); }
  Id dBufferId() const      { return rid(3); }
  Id dGeomId() const        { return rid(4); }
  Id dDrawItemId() const    { return rid(5); }
  Id refBufferId() const    { return rid(6); }
  Id refGeomId() const      { return rid(7); }
  Id refDrawItemId() const  { return rid(8); }

  static constexpr std::uint32_t ID_SLOTS = 9;

  struct StochData {
    std::vector<float> kSegments;   // rect4: x0,y0,x1,y1 per segment
    std::vector<float> dSegments;   // rect4
    std::uint32_t kCount{0};
    std::uint32_t dCount{0};
  };

  // Compute %K and %D line segments from OHLC data.
  // xCoords: X position per data point (same length as highs/lows/closes).
  StochData computeStochastic(const float* highs, const float* lows,
                               const float* closes, int count,
                               const float* xCoords) const;

  struct RefLineData {
    std::vector<float> lineSegments;
    std::uint32_t segmentCount{0};
  };

  // Compute horizontal reference lines spanning [xMin, xMax].
  RefLineData computeRefLines(float xMin, float xMax) const;

private:
  StochasticRecipeConfig config_;
};

} // namespace dc
