#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/layout/PaneLayout.hpp"
#include <string>
#include <vector>

namespace dc {

class GlyphAtlas;

struct LegendRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  float fontSize{0.035f};
  float glyphPx{48.0f};
  float swatchSize{0.025f};     // swatch width/height in clip space
  float itemSpacing{0.06f};     // vertical spacing between legend items
  float padding{0.02f};         // padding from top-left corner
  float anchorX{-0.95f};        // clip X anchor (left edge)
  float anchorY{0.95f};         // clip Y anchor (top edge)
};

// Legend overlay — colored swatches + text labels for each series.
// ID layout (6 slots):
//   0-2: swatch rects (buffer, geom, drawItem) — instancedRect@1
//   3-5: label text   (buffer, geom, drawItem) — textSDF@1
class LegendRecipe : public Recipe {
public:
  LegendRecipe(Id idBase, const LegendRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override {
    return {swatchDrawItemId(), textDrawItemId()};
  }

  Id swatchBufferId() const   { return rid(0); }
  Id swatchGeometryId() const { return rid(1); }
  Id swatchDrawItemId() const { return rid(2); }
  Id textBufferId() const     { return rid(3); }
  Id textGeometryId() const   { return rid(4); }
  Id textDrawItemId() const   { return rid(5); }

  static constexpr std::uint32_t ID_SLOTS = 6;

  struct LegendEntry {
    float swatchRect[4];    // rect4: x0, y0, x1, y1
    float swatchColor[4];   // color
    std::string label;
    bool visible{true};
  };

  struct LegendData {
    std::vector<float> swatchRects;   // rect4 per entry
    std::vector<float> textGlyphs;    // glyph8 per glyph
    std::uint32_t swatchCount{0};
    std::uint32_t glyphCount{0};
    std::vector<LegendEntry> entries; // for hit testing
  };

  // Compute legend layout from a list of series info.
  LegendData computeLegend(const std::vector<SeriesInfo>& series,
                            const GlyphAtlas& atlas) const;

  // D14.5: Hit test — returns index of clicked legend entry, or -1.
  int hitTest(const LegendData& legendData,
              float clipX, float clipY) const;

  const LegendRecipeConfig& config() const { return config_; }

private:
  LegendRecipeConfig config_;
};

} // namespace dc
