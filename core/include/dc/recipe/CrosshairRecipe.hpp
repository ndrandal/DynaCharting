#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/layout/PaneLayout.hpp"
#include <string>
#include <vector>

namespace dc {

class GlyphAtlas;

// Crosshair overlay: H-line, V-line, price label, time label.
//
// ID layout (offsets from idBase, 12 slots):
//   0-2:  H-line (buffer, geometry, drawItem) — line2d@1
//   3-5:  V-line (buffer, geometry, drawItem) — line2d@1
//   6-8:  Price label (buffer, geometry, drawItem) — textSDF@1
//   9-11: Time label (buffer, geometry, drawItem) — textSDF@1

struct CrosshairRecipeConfig {
  Id paneId{0};
  Id lineLayerId{0};   // topmost layer
  Id labelLayerId{0};
  std::string name;
};

class CrosshairRecipe : public Recipe {
public:
  CrosshairRecipe(Id idBase, const CrosshairRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override {
    return {hLineDrawItemId(), vLineDrawItemId(),
            priceLabelDrawItemId(), timeLabelDrawItemId()};
  }

  Id hLineBufferId() const      { return rid(0); }
  Id hLineGeomId() const        { return rid(1); }
  Id hLineDrawItemId() const    { return rid(2); }
  Id vLineBufferId() const      { return rid(3); }
  Id vLineGeomId() const        { return rid(4); }
  Id vLineDrawItemId() const    { return rid(5); }
  Id priceLabelBufferId() const { return rid(6); }
  Id priceLabelGeomId() const   { return rid(7); }
  Id priceLabelDrawItemId() const { return rid(8); }
  Id timeLabelBufferId() const  { return rid(9); }
  Id timeLabelGeomId() const    { return rid(10); }
  Id timeLabelDrawItemId() const { return rid(11); }

  static constexpr std::uint32_t ID_SLOTS = 12;

  struct CrosshairData {
    std::vector<float> hLineVerts, vLineVerts;             // pos2_clip
    std::vector<float> priceLabelGlyphs, timeLabelGlyphs;  // glyph8
    std::uint32_t priceLabelGC{0}, timeLabelGC{0};
    bool visible{false};
  };

  CrosshairData computeCrosshairData(
      double clipX, double clipY, double dataX, double dataY,
      const PaneRegion& clipRegion,
      const GlyphAtlas& atlas, float glyphPx, float fontSize) const;

private:
  CrosshairRecipeConfig config_;
};

} // namespace dc
