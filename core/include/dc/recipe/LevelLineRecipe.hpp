#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/layout/PaneLayout.hpp"
#include <string>
#include <utility>
#include <vector>

namespace dc {

class GlyphAtlas;

// Horizontal price level lines with labels.
//
// ID layout (offsets from idBase, 6 slots):
//   0-2: Lines (buffer, geometry, drawItem) — line2d@1
//   3-5: Labels (buffer, geometry, drawItem) — textSDF@1

struct LevelLineRecipeConfig {
  Id paneId{0};
  Id lineLayerId{0};
  Id labelLayerId{0};
  std::string name;
};

class LevelLineRecipe : public Recipe {
public:
  LevelLineRecipe(Id idBase, const LevelLineRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override {
    return {lineDrawItemId(), labelDrawItemId()};
  }

  Id lineBufferId() const    { return rid(0); }
  Id lineGeomId() const      { return rid(1); }
  Id lineDrawItemId() const  { return rid(2); }
  Id labelBufferId() const   { return rid(3); }
  Id labelGeomId() const     { return rid(4); }
  Id labelDrawItemId() const { return rid(5); }

  static constexpr std::uint32_t ID_SLOTS = 6;

  struct LevelData {
    std::vector<float> lineVerts;    // pos2_clip
    std::vector<float> labelGlyphs;  // glyph8
    std::uint32_t lineVertexCount{0};
    std::uint32_t labelGlyphCount{0};
  };

  LevelData computeLevels(
      const std::vector<std::pair<double, std::string>>& levels, // {dataY, label}
      const PaneRegion& clipRegion,
      double dataYMin, double dataYMax,
      const GlyphAtlas& atlas, float glyphPx, float fontSize) const;

private:
  LevelLineRecipeConfig config_;
};

} // namespace dc
