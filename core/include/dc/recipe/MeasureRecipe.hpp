#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/measure/MeasureState.hpp"
#include <string>
#include <vector>

namespace dc {

struct MeasureRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name{"measure"};
  float lineColor[4] = {0.8f, 0.8f, 0.8f, 0.8f};
  float lineWidth{1.0f};
};

// D23.3: Measure tool overlay — renders diagonal + horizontal + vertical
// lines forming an L-shape to show dx and dy between two data-space points.
//
// ID layout (3 slots):
//   0: Buffer (rect4 line segments)
//   1: Geometry
//   2: DrawItem
class MeasureRecipe : public Recipe {
public:
  MeasureRecipe(Id idBase, const MeasureRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }

  Id bufferId() const   { return rid(0); }
  Id geometryId() const { return rid(1); }
  Id drawItemId() const { return rid(2); }

  static constexpr std::uint32_t ID_SLOTS = 3;

  struct MeasureData {
    std::vector<float> lineSegments; // rect4: x0,y0,x1,y1 per segment
    std::uint32_t segmentCount{0};
  };

  // Generate line segments for the measure overlay.
  // Draws: diagonal from (x0,y0)->(x1,y1), horizontal (x0,y0)->(x1,y0),
  // vertical (x1,y0)->(x1,y1) to show dx and dy.
  MeasureData computeMeasure(const MeasureResult& measure) const;

private:
  MeasureRecipeConfig config_;
};

} // namespace dc
