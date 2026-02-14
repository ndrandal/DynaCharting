#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/minimap/MinimapState.hpp"
#include <string>
#include <vector>

namespace dc {

struct MinimapRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name{"minimap"};
  float trackColor[4] = {0.15f, 0.15f, 0.18f, 0.8f};   // dark bg
  float windowColor[4] = {0.3f, 0.5f, 1.0f, 0.25f};     // semi-transparent blue fill
  float borderColor[4] = {0.3f, 0.5f, 1.0f, 0.8f};      // blue border
  float borderWidth{1.0f};
};

class MinimapRecipe : public Recipe {
public:
  MinimapRecipe(Id idBase, const MinimapRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override;

  // IDs:
  // 0: track buffer (rect4, 1 rect)
  // 1: track geometry
  // 2: track drawItem (instancedRect@1)
  // 3: window buffer (rect4, 1 rect)
  // 4: window geometry
  // 5: window drawItem (instancedRect@1)
  // 6: border buffer (rect4, 4 line segments)
  // 7: border geometry
  // 8: border drawItem (lineAA@1)

  Id trackBufferId() const  { return rid(0); }
  Id trackGeomId() const    { return rid(1); }
  Id trackDIId() const      { return rid(2); }
  Id windowBufferId() const { return rid(3); }
  Id windowGeomId() const   { return rid(4); }
  Id windowDIId() const     { return rid(5); }
  Id borderBufferId() const { return rid(6); }
  Id borderGeomId() const   { return rid(7); }
  Id borderDIId() const     { return rid(8); }

  static constexpr std::uint32_t ID_SLOTS = 9;

  struct MinimapData {
    float trackRect[4];             // x0,y0,x1,y1 for background
    float windowRect[4];            // x0,y0,x1,y1 for viewport window
    std::vector<float> borderLines; // rect4 line segments (4 segments = 16 floats)
    std::uint32_t borderCount{0};
  };

  // Compute minimap visuals from the view window.
  // clipX0..clipY1 define the minimap's clip-space region (where it's drawn on screen).
  MinimapData computeMinimap(const MinimapViewWindow& window,
                              float clipX0, float clipY0,
                              float clipX1, float clipY1) const;

private:
  MinimapRecipeConfig config_;
};

} // namespace dc
