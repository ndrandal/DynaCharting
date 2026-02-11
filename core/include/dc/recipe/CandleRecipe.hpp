#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>

namespace dc {

// A candlestick recipe creates: buffer, geometry, drawItem, and binds to
// instancedCandle@1.
//
// ID layout (offsets from idBase):
//   0: Buffer (candle6 data)
//   1: Geometry
//   2: DrawItem
//   3: Transform
struct CandleRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  bool createTransform{true};
};

class CandleRecipe : public Recipe {
public:
  CandleRecipe(Id idBase, const CandleRecipeConfig& config);

  RecipeBuildResult build() const override;

  Id bufferId() const    { return rid(0); }
  Id geometryId() const  { return rid(1); }
  Id drawItemId() const  { return rid(2); }
  Id transformId() const { return rid(3); }

  static constexpr std::uint32_t ID_SLOTS = 4;

private:
  CandleRecipeConfig config_;
};

} // namespace dc
