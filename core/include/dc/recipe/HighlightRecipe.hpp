#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/selection/SelectionState.hpp"
#include <string>
#include <vector>

namespace dc {

class Scene;
class IngestProcessor;

struct HighlightRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  float markerSize{0.02f};   // clip-space half-size
};

// ID layout (3 slots): buffer(0), geometry(1), drawItem(2) — instancedRect@1
class HighlightRecipe : public Recipe {
public:
  HighlightRecipe(Id idBase, const HighlightRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }

  Id bufferId() const   { return rid(0); }
  Id geometryId() const { return rid(1); }
  Id drawItemId() const { return rid(2); }

  static constexpr std::uint32_t ID_SLOTS = 3;

  struct HighlightData {
    std::vector<float> rects;        // rect4 instances
    std::uint32_t instanceCount{0};
  };

  HighlightData computeHighlights(const SelectionState& selection,
                                   const Scene& scene,
                                   const IngestProcessor& ingest) const;

private:
  HighlightRecipeConfig config_;
};

} // namespace dc
