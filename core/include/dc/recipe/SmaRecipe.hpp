#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// SMA overlay recipe. Uses line2d@1 to render a simple moving average.
// For N candles, period P → (N-P) valid points → (N-P-1)*2 vertices.
//
// ID layout (offsets from idBase):
//   0: Buffer
//   1: Geometry
//   2: DrawItem
//   3: Transform
struct SmaRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  bool createTransform{true};
  int period{20};
};

class SmaRecipe : public Recipe {
public:
  SmaRecipe(Id idBase, const SmaRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }

  Id bufferId() const    { return rid(0); }
  Id geometryId() const  { return rid(1); }
  Id drawItemId() const  { return rid(2); }
  Id transformId() const { return rid(3); }

  static constexpr std::uint32_t ID_SLOTS = 4;

  struct SmaData {
    std::vector<float> lineVerts;
    std::uint32_t vertexCount;
  };

  // Compute SMA and generate line segments in clip space.
  SmaData compute(const float* closePrices, const float* xPositions,
                   int count, float yMin, float yMax,
                   float clipYMin, float clipYMax) const;

private:
  SmaRecipeConfig config_;
};

} // namespace dc
