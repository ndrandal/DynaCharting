#include "dc/recipe/TooltipRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/text/TextLayout.hpp"

#include <algorithm>
#include <string>

namespace dc {

TooltipRecipe::TooltipRecipe(Id idBase, const TooltipRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult TooltipRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // Background: buffer + geometry(rect4) + drawItem(instancedRect@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bgBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(bgGeometryId()) +
    R"(,"vertexBufferId":)" + idStr(bgBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(bgDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + "_bg" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(bgDrawItemId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(bgGeometryId()) + "}");
  // Dark background color
  result.createCommands.push_back(
    R"({"cmd":"setDrawItemColor","drawItemId":)" + idStr(bgDrawItemId()) +
    R"(,"r":0.15,"g":0.15,"b":0.15,"a":0.9})");

  // Text: buffer + geometry(glyph8) + drawItem(textSDF@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(textBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(textGeometryId()) +
    R"(,"vertexBufferId":)" + idStr(textBufferId()) +
    R"(,"format":"glyph8","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(textDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + "_text" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(textDrawItemId()) +
    R"(,"pipeline":"textSDF@1","geometryId":)" + idStr(textGeometryId()) + "}");
  // White text color
  result.createCommands.push_back(
    R"({"cmd":"setDrawItemColor","drawItemId":)" + idStr(textDrawItemId()) +
    R"(,"r":1.0,"g":1.0,"b":1.0,"a":1.0})");

  // Dispose (reverse order)
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(textDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(textGeometryId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(textBufferId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(bgDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(bgGeometryId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(bgBufferId()) + "}");

  return result;
}

TooltipRecipe::TooltipData TooltipRecipe::computeTooltip(
    const HitResult& hit,
    double cursorClipX, double cursorClipY,
    const PaneRegion& clipRegion,
    const GlyphAtlas& atlas,
    const TooltipFormatter& formatter) const {

  TooltipData data;
  if (!hit.hit) return data;

  std::string text = formatter(hit);
  if (text.empty()) return data;

  // Layout text starting at cursor position with offset
  float offsetX = config_.padding;
  float offsetY = config_.padding;
  float startX = static_cast<float>(cursorClipX) + offsetX;
  float baseY  = static_cast<float>(cursorClipY) + offsetY;

  auto layout = layoutText(atlas, text.c_str(), startX, baseY,
                           config_.fontSize, config_.glyphPx);

  if (layout.glyphCount == 0) return data;

  // Compute text bounding box
  float textMinX = 1e30f, textMinY = 1e30f;
  float textMaxX = -1e30f, textMaxY = -1e30f;
  for (int i = 0; i < layout.glyphCount; i++) {
    float x0 = layout.glyphInstances[i * 8 + 0];
    float y0 = layout.glyphInstances[i * 8 + 1];
    float x1 = layout.glyphInstances[i * 8 + 2];
    float y1 = layout.glyphInstances[i * 8 + 3];
    textMinX = std::min(textMinX, x0);
    textMinY = std::min(textMinY, y0);
    textMaxX = std::max(textMaxX, x1);
    textMaxY = std::max(textMaxY, y1);
  }

  // Clamp to pane bounds
  float pad = config_.padding;
  float bgX0 = textMinX - pad;
  float bgY0 = textMinY - pad;
  float bgX1 = textMaxX + pad;
  float bgY1 = textMaxY + pad;

  float bgW = bgX1 - bgX0;
  float bgH = bgY1 - bgY0;

  // If tooltip goes beyond right edge, shift left
  if (bgX1 > clipRegion.clipXMax) {
    float shift = bgX1 - clipRegion.clipXMax;
    bgX0 -= shift;
    bgX1 -= shift;
    // Also shift glyphs
    for (int i = 0; i < layout.glyphCount; i++) {
      layout.glyphInstances[i * 8 + 0] -= shift;
      layout.glyphInstances[i * 8 + 2] -= shift;
    }
  }
  // If tooltip goes beyond top edge, shift down
  if (bgY1 > clipRegion.clipYMax) {
    float shift = bgY1 - clipRegion.clipYMax;
    bgY0 -= shift;
    bgY1 -= shift;
    for (int i = 0; i < layout.glyphCount; i++) {
      layout.glyphInstances[i * 8 + 1] -= shift;
      layout.glyphInstances[i * 8 + 3] -= shift;
    }
  }

  (void)bgW;
  (void)bgH;

  data.bgRect = {bgX0, bgY0, bgX1, bgY1};
  data.bgCount = 1;
  data.textGlyphs = std::move(layout.glyphInstances);
  data.glyphCount = static_cast<std::uint32_t>(layout.glyphCount);
  data.visible = true;

  return data;
}

} // namespace dc
