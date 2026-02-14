#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// D15.4: Scroll position indicator — thin horizontal bar showing
// viewport position within the full data range.
//
// ID layout (6 slots):
//   0-2: track (buffer, geom, drawItem) — instancedRect@1
//   3-5: thumb (buffer, geom, drawItem) — instancedRect@1
struct ScrollIndicatorConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  float barHeight{0.02f};      // clip-space height
  float barY{-0.98f};          // clip Y position (bottom)
  float barXMin{-0.95f};       // left edge
  float barXMax{0.95f};        // right edge
  float trackColor[4] = {0.2f, 0.2f, 0.2f, 0.5f};
  float thumbColor[4] = {0.6f, 0.6f, 0.6f, 0.8f};
};

class ScrollIndicatorRecipe : public Recipe {
public:
  ScrollIndicatorRecipe(Id idBase, const ScrollIndicatorConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override {
    return {trackDrawItemId(), thumbDrawItemId()};
  }

  Id trackBufferId() const   { return rid(0); }
  Id trackGeometryId() const { return rid(1); }
  Id trackDrawItemId() const { return rid(2); }
  Id thumbBufferId() const   { return rid(3); }
  Id thumbGeometryId() const { return rid(4); }
  Id thumbDrawItemId() const { return rid(5); }

  static constexpr std::uint32_t ID_SLOTS = 6;

  struct IndicatorData {
    float trackRect[4];    // rect4 for track background
    float thumbRect[4];    // rect4 for thumb position
  };

  // Compute indicator positions.
  // fullXMin/fullXMax: the total data range (all data)
  // viewXMin/viewXMax: the currently visible range (viewport)
  IndicatorData computeIndicator(double fullXMin, double fullXMax,
                                  double viewXMin, double viewXMax) const;

private:
  ScrollIndicatorConfig config_;
};

} // namespace dc
