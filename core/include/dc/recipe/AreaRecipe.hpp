#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// An area recipe creates a filled region under a polyline using triSolid@1.
// For N points → (N-1) quads → (N-1)*6 vertices (Pos2_Clip).
//
// ID layout (offsets from idBase):
//   0: Buffer
//   1: Geometry
//   2: DrawItem
//   3: Transform
struct AreaRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  bool createTransform{true};
};

class AreaRecipe : public Recipe {
public:
  AreaRecipe(Id idBase, const AreaRecipeConfig& config);

  RecipeBuildResult build() const override;

  Id bufferId() const    { return rid(0); }
  Id geometryId() const  { return rid(1); }
  Id drawItemId() const  { return rid(2); }
  Id transformId() const { return rid(3); }

  static constexpr std::uint32_t ID_SLOTS = 4;

  struct AreaData {
    std::vector<float> triVerts;
    std::uint32_t vertexCount;
  };

  // Triangulate area under polyline down to baselineY.
  AreaData compute(const float* x, const float* y, int count, float baselineY) const;

private:
  AreaRecipeConfig config_;
};

} // namespace dc
