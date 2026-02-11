#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// MACD indicator recipe: MACD line, signal line, and histogram bars.
//
// ID layout (offsets from idBase, 14 slots):
//   0-2:   MACD line: buffer, geometry, drawItem
//   3-5:   Signal line: buffer, geometry, drawItem
//   6-8:   Positive histogram: buffer, geometry, drawItem
//   9-11:  Negative histogram: buffer, geometry, drawItem
//   12:    Shared transform
//   13:    Reserved
struct MacdRecipeConfig {
  Id paneId{0};
  Id lineLayerId{0};
  Id histLayerId{0};
  std::string name;
  bool createTransform{true};
  int fastPeriod{12};
  int slowPeriod{26};
  int signalPeriod{9};
};

class MacdRecipe : public Recipe {
public:
  MacdRecipe(Id idBase, const MacdRecipeConfig& config);

  RecipeBuildResult build() const override;

  Id macdLineBufferId() const    { return rid(0); }
  Id macdLineGeomId() const      { return rid(1); }
  Id macdLineDrawItemId() const  { return rid(2); }
  Id signalLineBufferId() const  { return rid(3); }
  Id signalLineGeomId() const    { return rid(4); }
  Id signalLineDrawItemId() const { return rid(5); }
  Id posHistBufferId() const     { return rid(6); }
  Id posHistGeomId() const       { return rid(7); }
  Id posHistDrawItemId() const   { return rid(8); }
  Id negHistBufferId() const     { return rid(9); }
  Id negHistGeomId() const       { return rid(10); }
  Id negHistDrawItemId() const   { return rid(11); }
  Id transformId() const         { return rid(12); }

  static constexpr std::uint32_t ID_SLOTS = 14;

  struct MacdData {
    std::vector<float> macdLineVerts, signalLineVerts;
    std::vector<float> posHistRects, negHistRects;
    std::uint32_t macdVC, signalVC, posHistCount, negHistCount;
  };

  MacdData compute(const float* closePrices, const float* xPositions,
                    int count, float halfWidth,
                    float clipYMin, float clipYMax) const;

private:
  MacdRecipeConfig config_;
};

} // namespace dc
