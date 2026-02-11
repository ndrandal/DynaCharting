#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

class GlyphAtlas;

// Axis recipe: Y-axis ticks + X-axis ticks + tick labels.
//
// ID layout (offsets from idBase, 10 slots):
//   0-2: Y-tick lines (buffer, geometry, drawItem)
//   3-5: X-tick lines (buffer, geometry, drawItem)
//   6-8: Labels (buffer, geometry, drawItem)
//   9:   Label identity transform
struct AxisRecipeConfig {
  Id paneId{0};
  Id tickLayerId{0};
  Id labelLayerId{0};
  Id dataTransformId{0};
  std::string name;
  float yAxisClipX{0.85f};
  float xAxisClipY{-0.9f};
};

class AxisRecipe : public Recipe {
public:
  AxisRecipe(Id idBase, const AxisRecipeConfig& config);

  RecipeBuildResult build() const override;

  Id yTickBufferId() const   { return rid(0); }
  Id yTickGeomId() const     { return rid(1); }
  Id yTickDrawItemId() const { return rid(2); }
  Id xTickBufferId() const   { return rid(3); }
  Id xTickGeomId() const     { return rid(4); }
  Id xTickDrawItemId() const { return rid(5); }
  Id labelBufferId() const   { return rid(6); }
  Id labelGeomId() const     { return rid(7); }
  Id labelDrawItemId() const { return rid(8); }
  Id labelTransformId() const { return rid(9); }

  static constexpr std::uint32_t ID_SLOTS = 10;

  struct AxisData {
    std::vector<float> yTickVerts, xTickVerts;
    std::vector<float> labelInstances;
    std::uint32_t yTickVertexCount, xTickVertexCount, labelGlyphCount;
  };

  AxisData computeAxisData(const GlyphAtlas& atlas,
                            float yMin, float yMax, int xCount,
                            float clipYMin, float clipYMax,
                            float clipXMin, float clipXMax,
                            float glyphPx, float fontSize) const;

private:
  AxisRecipeConfig config_;
};

} // namespace dc
