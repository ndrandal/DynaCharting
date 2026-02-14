#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

class GlyphAtlas;

// Axis recipe: Y-axis ticks + X-axis ticks + tick labels + optional grid/spine/AA.
//
// ID layout (offsets from idBase, 26 slots):
//   0-2:   Y-tick lines  (buf/geom/di) — line2d@1
//   3-5:   X-tick lines  (buf/geom/di) — line2d@1
//   6-8:   Labels        (buf/geom/di) — textSDF@1
//   9:     Label identity transform
//  10-12:  H-grid lines  (buf/geom/di) — lineAA@1  [enableGrid]
//  13-15:  V-grid lines  (buf/geom/di) — lineAA@1  [enableGrid]
//  16-18:  Y-tick AA     (buf/geom/di) — lineAA@1  [enableAALines]
//  19-21:  X-tick AA     (buf/geom/di) — lineAA@1  [enableAALines]
//  22-24:  Spine lines   (buf/geom/di) — lineAA@1  [enableSpine]
//  25:     Grid/spine identity transform [enableGrid || enableSpine]
struct AxisRecipeConfig {
  Id paneId{0};
  Id tickLayerId{0};
  Id labelLayerId{0};
  Id dataTransformId{0};
  std::string name;
  float yAxisClipX{0.85f};
  float xAxisClipY{-0.9f};

  // D12 extensions
  bool enableGrid{false};
  Id gridLayerId{0};
  bool enableAALines{false};
  float yTickLength{0.03f};
  float xTickLength{0.03f};
  bool xAxisIsTime{false};   // X-axis uses NiceTimeTicks + TimeFormat
  bool useUTC{true};          // UTC vs local time for labels
  bool enableSpine{false};
  float spineColor[4] {0.5f, 0.5f, 0.5f, 1.0f};
  float spineLineWidth{2.0f};
  float gridColor[4]  {0.2f, 0.2f, 0.2f, 0.5f};
  float tickColor[4]  {0.6f, 0.6f, 0.6f, 1.0f};
  float labelColor[4] {0.8f, 0.8f, 0.8f, 1.0f};
  float gridLineWidth{1.0f};
  float tickLineWidth{1.0f};
};

class AxisRecipe : public Recipe {
public:
  AxisRecipe(Id idBase, const AxisRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override {
    return {yTickDrawItemId(), xTickDrawItemId(), labelDrawItemId()};
  }

  // Existing accessors (slots 0-9)
  Id yTickBufferId() const   { return rid(0); }
  Id yTickGeomId() const     { return rid(1); }
  Id yTickDrawItemId() const { return rid(2); }
  Id xTickBufferId() const   { return rid(3); }
  Id xTickGeomId() const     { return rid(4); }
  Id xTickDrawItemId() const { return rid(5); }
  Id labelBufferId() const   { return rid(6); }
  Id labelGeomId() const     { return rid(7); }
  Id labelDrawItemId() const { return rid(8); }
  Id labelTransformId() const { return rid(9); }

  // D12 accessors (slots 10-25)
  Id hGridBufferId() const     { return rid(10); }
  Id hGridGeomId() const       { return rid(11); }
  Id hGridDrawItemId() const   { return rid(12); }
  Id vGridBufferId() const     { return rid(13); }
  Id vGridGeomId() const       { return rid(14); }
  Id vGridDrawItemId() const   { return rid(15); }
  Id yTickAABufferId() const   { return rid(16); }
  Id yTickAAGeomId() const     { return rid(17); }
  Id yTickAADrawItemId() const { return rid(18); }
  Id xTickAABufferId() const   { return rid(19); }
  Id xTickAAGeomId() const     { return rid(20); }
  Id xTickAADrawItemId() const { return rid(21); }
  Id spineBufferId() const     { return rid(22); }
  Id spineGeomId() const       { return rid(23); }
  Id spineDrawItemId() const   { return rid(24); }
  Id gridSpineTransformId() const { return rid(25); }

  static constexpr std::uint32_t ID_SLOTS = 26;

  struct AxisData {
    std::vector<float> yTickVerts, xTickVerts;
    std::vector<float> labelInstances;
    std::uint32_t yTickVertexCount{0}, xTickVertexCount{0}, labelGlyphCount{0};

    // D12 extensions
    std::vector<float> hGridVerts, vGridVerts;
    std::uint32_t hGridLineCount{0}, vGridLineCount{0};
    std::vector<float> yTickAAVerts, xTickAAVerts;
    std::uint32_t yTickAAVertexCount{0}, xTickAAVertexCount{0};
    std::vector<float> spineVerts;
    std::uint32_t spineLineCount{0};
  };

  // Original method (backward compatible, index-based X-axis)
  AxisData computeAxisData(const GlyphAtlas& atlas,
                            float yMin, float yMax, int xCount,
                            float clipYMin, float clipYMax,
                            float clipXMin, float clipXMax,
                            float glyphPx, float fontSize) const;

  // D12.3: Data-value X-axis with nice ticks + adaptive formatting
  AxisData computeAxisDataV2(const GlyphAtlas& atlas,
                              float yMin, float yMax,
                              float xMin, float xMax,
                              float clipYMin, float clipYMax,
                              float clipXMin, float clipXMax,
                              float glyphPx, float fontSize) const;

  static const char* chooseFormat(float step);

  const AxisRecipeConfig& config() const { return config_; }

private:
  AxisRecipeConfig config_;
};

} // namespace dc
