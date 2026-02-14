#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/drawing/DrawingStore.hpp"
#include <string>
#include <vector>

namespace dc {

// D16.4: Drawing recipe — renders user drawings as lineAA@1 lines.
//
// ID layout (3 slots):
//   0: Buffer (rect4 line segments)
//   1: Geometry
//   2: DrawItem
struct DrawingRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  float defaultColor[4] = {1.0f, 1.0f, 0.0f, 1.0f};
  float defaultLineWidth{2.0f};
};

class DrawingRecipe : public Recipe {
public:
  DrawingRecipe(Id idBase, const DrawingRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }

  Id bufferId() const   { return rid(0); }
  Id geometryId() const { return rid(1); }
  Id drawItemId() const { return rid(2); }

  static constexpr std::uint32_t ID_SLOTS = 3;

  struct DrawingData {
    std::vector<float> lineSegments;  // rect4: x0,y0,x1,y1 per segment
    std::uint32_t segmentCount{0};
  };

  // Generate line segments from all drawings in the store.
  // Horizontal levels extend from dataXMin to dataXMax.
  // Vertical lines extend from dataYMin to dataYMax.
  // D21.3: added dataYMin/dataYMax for vertical line support.
  DrawingData computeDrawings(const DrawingStore& store,
                               double dataXMin, double dataXMax,
                               double dataYMin, double dataYMax) const;

private:
  DrawingRecipeConfig config_;
};

} // namespace dc
