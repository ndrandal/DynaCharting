#include "dc/recipe/LegendRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/text/TextLayout.hpp"

#include <cstdio>
#include <string>

namespace dc {

LegendRecipe::LegendRecipe(Id idBase, const LegendRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult LegendRecipe::build() const {
  RecipeBuildResult result;

  auto idStr = [](Id id) { return std::to_string(id); };

  // Swatch rects (instancedRect@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(swatchBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(swatchGeometryId()) +
    R"(,"vertexBufferId":)" + idStr(swatchBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(swatchDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_swatch"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(swatchDrawItemId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(swatchGeometryId()) + "}");

  // Text labels (textSDF@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(textBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(textGeometryId()) +
    R"(,"vertexBufferId":)" + idStr(textBufferId()) +
    R"(,"format":"glyph8","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(textDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_text"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(textDrawItemId()) +
    R"(,"pipeline":"textSDF@1","geometryId":)" + idStr(textGeometryId()) + "}");

  // Set text color to white
  result.createCommands.push_back(
    R"({"cmd":"setDrawItemColor","drawItemId":)" + idStr(textDrawItemId()) +
    R"(,"r":0.9,"g":0.9,"b":0.9,"a":1})");

  // Dispose (reverse)
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(textDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(textGeometryId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(textBufferId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(swatchDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(swatchGeometryId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(swatchBufferId()) + "}");

  return result;
}

LegendRecipe::LegendData LegendRecipe::computeLegend(
    const std::vector<SeriesInfo>& series,
    const GlyphAtlas& atlas) const {
  LegendData data;
  if (series.empty()) return data;

  float curY = config_.anchorY - config_.padding;
  float leftX = config_.anchorX + config_.padding;

  for (std::size_t i = 0; i < series.size(); i++) {
    const auto& si = series[i];

    LegendEntry entry;
    entry.label = si.name;
    entry.visible = si.defaultVisible;

    // Swatch rect: small colored square
    float sx0 = leftX;
    float sy1 = curY;
    float sy0 = curY - config_.swatchSize;
    float sx1 = leftX + config_.swatchSize;

    entry.swatchRect[0] = sx0;
    entry.swatchRect[1] = sy0;
    entry.swatchRect[2] = sx1;
    entry.swatchRect[3] = sy1;

    entry.swatchColor[0] = si.colorHint[0];
    entry.swatchColor[1] = si.colorHint[1];
    entry.swatchColor[2] = si.colorHint[2];
    entry.swatchColor[3] = si.colorHint[3];

    // Add swatch rect to data
    data.swatchRects.push_back(sx0);
    data.swatchRects.push_back(sy0);
    data.swatchRects.push_back(sx1);
    data.swatchRects.push_back(sy1);
    data.swatchCount++;

    // Text label to the right of the swatch
    float textX = sx1 + config_.padding * 0.5f;
    float textBaseY = sy0 + config_.swatchSize * 0.3f;

    auto layout = layoutText(atlas, si.name.c_str(),
                              textX, textBaseY,
                              config_.fontSize, config_.glyphPx);

    for (const auto& v : layout.glyphInstances) {
      data.textGlyphs.push_back(v);
    }
    data.glyphCount += static_cast<std::uint32_t>(layout.glyphCount);

    data.entries.push_back(std::move(entry));

    curY -= config_.itemSpacing;
  }

  return data;
}

int LegendRecipe::hitTest(const LegendData& legendData,
                           float clipX, float clipY) const {
  for (std::size_t i = 0; i < legendData.entries.size(); i++) {
    const auto& entry = legendData.entries[i];
    // Hit area extends from swatch left to some width past text
    float hitX0 = entry.swatchRect[0] - config_.padding * 0.5f;
    float hitY0 = entry.swatchRect[1] - config_.padding * 0.5f;
    float hitX1 = entry.swatchRect[2] + 0.3f;  // generous hit width
    float hitY1 = entry.swatchRect[3] + config_.padding * 0.5f;

    if (clipX >= hitX0 && clipX <= hitX1 &&
        clipY >= hitY0 && clipY <= hitY1) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

} // namespace dc
