#include "dc/recipe/CrosshairRecipe.hpp"
#include "dc/text/TextLayout.hpp"
#include <cstdio>
#include <string>

namespace dc {

CrosshairRecipe::CrosshairRecipe(Id idBase, const CrosshairRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult CrosshairRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // Helper: create buffer + geometry + drawItem triplet
  auto addTriplet = [&](Id bufId, Id geomId, Id diId,
                        const char* suffix, const char* pipeline,
                        const char* format, Id layerId, int initVC) {
    result.createCommands.push_back(
      R"({"cmd":"createBuffer","id":)" + idStr(bufId) + R"(,"byteLength":0})");
    result.createCommands.push_back(
      R"({"cmd":"createGeometry","id":)" + idStr(geomId) +
      R"(,"vertexBufferId":)" + idStr(bufId) +
      R"(,"format":")" + format + R"(","vertexCount":)" + std::to_string(initVC) + "}");
    result.createCommands.push_back(
      R"({"cmd":"createDrawItem","id":)" + idStr(diId) +
      R"(,"layerId":)" + idStr(layerId) +
      R"(,"name":")" + config_.name + suffix + R"("})");
    result.createCommands.push_back(
      R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(diId) +
      R"(,"pipeline":")" + pipeline + R"(","geometryId":)" + idStr(geomId) + "}");
  };

  addTriplet(hLineBufferId(), hLineGeomId(), hLineDrawItemId(),
             "_hLine", "line2d@1", "pos2_clip", config_.lineLayerId, 2);
  addTriplet(vLineBufferId(), vLineGeomId(), vLineDrawItemId(),
             "_vLine", "line2d@1", "pos2_clip", config_.lineLayerId, 2);
  addTriplet(priceLabelBufferId(), priceLabelGeomId(), priceLabelDrawItemId(),
             "_priceLabel", "textSDF@1", "glyph8", config_.labelLayerId, 1);
  addTriplet(timeLabelBufferId(), timeLabelGeomId(), timeLabelDrawItemId(),
             "_timeLabel", "textSDF@1", "glyph8", config_.labelLayerId, 1);

  // Subscriptions
  result.subscriptions.push_back({hLineBufferId(), hLineGeomId(), VertexFormat::Pos2_Clip});
  result.subscriptions.push_back({vLineBufferId(), vLineGeomId(), VertexFormat::Pos2_Clip});
  result.subscriptions.push_back({priceLabelBufferId(), priceLabelGeomId(), VertexFormat::Glyph8});
  result.subscriptions.push_back({timeLabelBufferId(), timeLabelGeomId(), VertexFormat::Glyph8});

  // Dispose (reverse order)
  for (int i = 11; i >= 0; i--) {
    result.disposeCommands.push_back(
      R"({"cmd":"delete","id":)" + idStr(rid(static_cast<std::uint32_t>(i))) + "}");
  }

  return result;
}

CrosshairRecipe::CrosshairData CrosshairRecipe::computeCrosshairData(
    double clipX, double clipY, double dataX, double dataY,
    const PaneRegion& clipRegion,
    const GlyphAtlas& atlas, float glyphPx, float fontSize) const {

  CrosshairData data;

  // Check if cursor is inside clip region
  float cx = static_cast<float>(clipX);
  float cy = static_cast<float>(clipY);
  if (cx < clipRegion.clipXMin || cx > clipRegion.clipXMax ||
      cy < clipRegion.clipYMin || cy > clipRegion.clipYMax) {
    data.visible = false;
    return data;
  }

  data.visible = true;

  // Horizontal line spanning full pane width at cursor Y
  data.hLineVerts = {clipRegion.clipXMin, cy, clipRegion.clipXMax, cy};

  // Vertical line spanning full pane height at cursor X
  data.vLineVerts = {cx, clipRegion.clipYMin, cx, clipRegion.clipYMax};

  // Price label (right-aligned at right edge of pane)
  char priceBuf[32];
  std::snprintf(priceBuf, sizeof(priceBuf), "%.2f", dataY);
  auto priceLayout = layoutTextRightAligned(atlas, priceBuf,
      clipRegion.clipXMax - 0.01f, cy - fontSize * 0.3f,
      fontSize, glyphPx);
  data.priceLabelGlyphs = std::move(priceLayout.glyphInstances);
  data.priceLabelGC = static_cast<std::uint32_t>(priceLayout.glyphCount);

  // Time label (centered below cursor X, below pane)
  char timeBuf[32];
  std::snprintf(timeBuf, sizeof(timeBuf), "%.1f", dataX);
  auto timeLayout = layoutText(atlas, timeBuf,
      cx - fontSize * 1.5f, clipRegion.clipYMin - fontSize * 1.2f,
      fontSize, glyphPx);
  data.timeLabelGlyphs = std::move(timeLayout.glyphInstances);
  data.timeLabelGC = static_cast<std::uint32_t>(timeLayout.glyphCount);

  return data;
}

} // namespace dc
