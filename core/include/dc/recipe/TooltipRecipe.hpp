#pragma once
#include "dc/recipe/Recipe.hpp"
#include "dc/viewport/DataPicker.hpp"
#include "dc/layout/PaneLayout.hpp"
#include <functional>
#include <string>
#include <vector>

namespace dc {

class GlyphAtlas;

using TooltipFormatter = std::function<std::string(const HitResult&)>;

struct TooltipRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  float fontSize{0.03f};
  float glyphPx{48.0f};
  float padding{0.015f};
};

// ID layout (6 slots):
//   0-2: background (buffer, geom, drawItem) — instancedRect@1
//   3-5: text (buffer, geom, drawItem) — textSDF@1
class TooltipRecipe : public Recipe {
public:
  TooltipRecipe(Id idBase, const TooltipRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {bgDrawItemId(), textDrawItemId()}; }

  Id bgBufferId() const     { return rid(0); }
  Id bgGeometryId() const   { return rid(1); }
  Id bgDrawItemId() const   { return rid(2); }
  Id textBufferId() const   { return rid(3); }
  Id textGeometryId() const { return rid(4); }
  Id textDrawItemId() const { return rid(5); }

  static constexpr std::uint32_t ID_SLOTS = 6;

  struct TooltipData {
    std::vector<float> bgRect;      // 1 rect4
    std::vector<float> textGlyphs;  // glyph8
    std::uint32_t bgCount{0}, glyphCount{0};
    bool visible{false};
  };

  TooltipData computeTooltip(const HitResult& hit,
      double cursorClipX, double cursorClipY,
      const PaneRegion& clipRegion,
      const GlyphAtlas& atlas,
      const TooltipFormatter& formatter) const;

private:
  TooltipRecipeConfig config_;
};

} // namespace dc
