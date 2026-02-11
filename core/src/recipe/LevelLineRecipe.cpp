#include "dc/recipe/LevelLineRecipe.hpp"
#include "dc/text/TextLayout.hpp"
#include "dc/math/Normalize.hpp"

namespace dc {

LevelLineRecipe::LevelLineRecipe(Id idBase, const LevelLineRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult LevelLineRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // Lines (line2d@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(lineBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(lineGeomId()) +
    R"(,"vertexBufferId":)" + idStr(lineBufferId()) +
    R"(,"format":"pos2_clip","vertexCount":2})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(lineDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.lineLayerId) +
    R"(,"name":")" + config_.name + "_lines" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(lineDrawItemId()) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(lineGeomId()) + "}");

  // Labels (textSDF@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(labelBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(labelGeomId()) +
    R"(,"vertexBufferId":)" + idStr(labelBufferId()) +
    R"(,"format":"glyph8","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(labelDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.labelLayerId) +
    R"(,"name":")" + config_.name + "_labels" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(labelDrawItemId()) +
    R"(,"pipeline":"textSDF@1","geometryId":)" + idStr(labelGeomId()) + "}");

  // Subscriptions
  result.subscriptions.push_back({lineBufferId(), lineGeomId(), VertexFormat::Pos2_Clip});
  result.subscriptions.push_back({labelBufferId(), labelGeomId(), VertexFormat::Glyph8});

  // Dispose (reverse order)
  for (int i = 5; i >= 0; i--) {
    result.disposeCommands.push_back(
      R"({"cmd":"delete","id":)" + idStr(rid(static_cast<std::uint32_t>(i))) + "}");
  }

  return result;
}

LevelLineRecipe::LevelData LevelLineRecipe::computeLevels(
    const std::vector<std::pair<double, std::string>>& levels,
    const PaneRegion& clipRegion,
    double dataYMin, double dataYMax,
    const GlyphAtlas& atlas, float glyphPx, float fontSize) const {

  LevelData data;

  for (const auto& [dataY, label] : levels) {
    // Map data Y to clip Y
    float clipY = normalizeToClip(static_cast<float>(dataY),
        static_cast<float>(dataYMin), static_cast<float>(dataYMax),
        clipRegion.clipYMin, clipRegion.clipYMax);

    // Skip if outside visible range
    if (clipY < clipRegion.clipYMin || clipY > clipRegion.clipYMax)
      continue;

    // Horizontal line spanning pane width
    data.lineVerts.push_back(clipRegion.clipXMin);
    data.lineVerts.push_back(clipY);
    data.lineVerts.push_back(clipRegion.clipXMax);
    data.lineVerts.push_back(clipY);
    data.lineVertexCount += 2;

    // Right-aligned label
    auto layout = layoutTextRightAligned(atlas, label.c_str(),
        clipRegion.clipXMax - 0.01f, clipY - fontSize * 0.3f,
        fontSize, glyphPx);
    data.labelGlyphs.insert(data.labelGlyphs.end(),
        layout.glyphInstances.begin(), layout.glyphInstances.end());
    data.labelGlyphCount += static_cast<std::uint32_t>(layout.glyphCount);
  }

  return data;
}

} // namespace dc
