#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>

namespace dc {

// A text recipe creates: buffer, geometry, drawItem, and binds to textSDF@1.
//
// ID layout (offsets from idBase):
//   0: Buffer (glyph8 data)
//   1: Geometry
//   2: DrawItem
//   3: Transform
struct TextRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  bool createTransform{true};
};

class TextRecipe : public Recipe {
public:
  TextRecipe(Id idBase, const TextRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }

  Id bufferId() const    { return rid(0); }
  Id geometryId() const  { return rid(1); }
  Id drawItemId() const  { return rid(2); }
  Id transformId() const { return rid(3); }

  static constexpr std::uint32_t ID_SLOTS = 4;

private:
  TextRecipeConfig config_;
};

} // namespace dc
