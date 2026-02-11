#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// Bollinger Bands: middle SMA line + upper/lower bands + fill area.
//
// ID layout (offsets from idBase, 13 slots):
//   0-2:  Middle (SMA) line: buffer, geometry, drawItem
//   3-5:  Upper band line: buffer, geometry, drawItem
//   6-8:  Lower band line: buffer, geometry, drawItem
//   9-11: Fill area: buffer, geometry, drawItem
//   12:   Shared transform
struct BollingerRecipeConfig {
  Id paneId{0};
  Id lineLayerId{0};
  Id fillLayerId{0};
  std::string name;
  bool createTransform{true};
  int period{20};
  float numStdDev{2.0f};
};

class BollingerRecipe : public Recipe {
public:
  BollingerRecipe(Id idBase, const BollingerRecipeConfig& config);

  RecipeBuildResult build() const override;

  Id middleBufferId() const   { return rid(0); }
  Id middleGeomId() const     { return rid(1); }
  Id middleDrawItemId() const { return rid(2); }
  Id upperBufferId() const    { return rid(3); }
  Id upperGeomId() const      { return rid(4); }
  Id upperDrawItemId() const  { return rid(5); }
  Id lowerBufferId() const    { return rid(6); }
  Id lowerGeomId() const      { return rid(7); }
  Id lowerDrawItemId() const  { return rid(8); }
  Id fillBufferId() const     { return rid(9); }
  Id fillGeomId() const       { return rid(10); }
  Id fillDrawItemId() const   { return rid(11); }
  Id transformId() const      { return rid(12); }

  static constexpr std::uint32_t ID_SLOTS = 13;

  struct BollingerData {
    std::vector<float> middleVerts, upperVerts, lowerVerts, fillVerts;
    std::uint32_t middleVC, upperVC, lowerVC, fillVC;
  };

  BollingerData compute(const float* closePrices, const float* xPositions,
                          int count, float yMin, float yMax,
                          float clipYMin, float clipYMax) const;

private:
  BollingerRecipeConfig config_;

  // Helper to build line2d recipe commands for a single line
  void buildLineCommands(RecipeBuildResult& result, Id bufId, Id geomId,
                          Id diId, Id layerId, const std::string& name) const;
};

} // namespace dc
