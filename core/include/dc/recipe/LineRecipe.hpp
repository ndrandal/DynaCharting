#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>

namespace dc {

// A line recipe creates: buffer, geometry, drawItem, and binds to lineAA@1
// (ENC-993). The vertex buffer holds rect4 segment records: (x0,y0,x1,y1) per
// segment — byte-identical to the LineList endpoint pairs it used to take.
// Requires a pane and layer to already exist.
//
// ID layout (offsets from idBase):
//   0: Buffer
//   1: Geometry
//   2: DrawItem
//   3: Transform
struct LineRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  bool createTransform{true};
};

class LineRecipe : public Recipe {
public:
  LineRecipe(Id idBase, const LineRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }

  // Accessors for the deterministic IDs
  Id bufferId() const    { return rid(0); }
  Id geometryId() const  { return rid(1); }
  Id drawItemId() const  { return rid(2); }
  Id transformId() const { return rid(3); }

  static constexpr std::uint32_t ID_SLOTS = 4;

private:
  LineRecipeConfig config_;
};

} // namespace dc
