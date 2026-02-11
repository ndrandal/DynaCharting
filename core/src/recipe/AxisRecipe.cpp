#include "dc/recipe/AxisRecipe.hpp"
#include "dc/math/NiceTicks.hpp"
#include "dc/math/Normalize.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include <cstdio>
#include <string>

namespace dc {

AxisRecipe::AxisRecipe(Id idBase, const AxisRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult AxisRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // Y-tick lines (line2d@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(yTickBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(yTickGeomId()) +
    R"(,"vertexBufferId":)" + idStr(yTickBufferId()) +
    R"(,"format":"pos2_clip","vertexCount":2})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(yTickDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.tickLayerId) +
    R"(,"name":")" + config_.name + "_yTicks" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(yTickDrawItemId()) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(yTickGeomId()) + "}");

  // X-tick lines (line2d@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(xTickBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(xTickGeomId()) +
    R"(,"vertexBufferId":)" + idStr(xTickBufferId()) +
    R"(,"format":"pos2_clip","vertexCount":2})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(xTickDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.tickLayerId) +
    R"(,"name":")" + config_.name + "_xTicks" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(xTickDrawItemId()) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(xTickGeomId()) + "}");

  // Labels (textSDF@1, identity transform)
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

  // Identity transform for labels (fixed at viewport edge)
  result.createCommands.push_back(
    R"({"cmd":"createTransform","id":)" + idStr(labelTransformId()) + "}");
  result.createCommands.push_back(
    R"({"cmd":"attachTransform","drawItemId":)" + idStr(labelDrawItemId()) +
    R"(,"transformId":)" + idStr(labelTransformId()) + "}");

  // Attach data transform to tick lines if provided
  if (config_.dataTransformId != 0) {
    result.createCommands.push_back(
      R"({"cmd":"attachTransform","drawItemId":)" + idStr(yTickDrawItemId()) +
      R"(,"transformId":)" + idStr(config_.dataTransformId) + "}");
    result.createCommands.push_back(
      R"({"cmd":"attachTransform","drawItemId":)" + idStr(xTickDrawItemId()) +
      R"(,"transformId":)" + idStr(config_.dataTransformId) + "}");
  }

  // Dispose (reverse order)
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(labelTransformId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(labelDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(labelGeomId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(labelBufferId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(xTickDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(xTickGeomId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(xTickBufferId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(yTickDrawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(yTickGeomId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(yTickBufferId()) + "}");

  return result;
}

// Helper: build glyph instances for a text label at a given position
static void buildLabelGlyphs(const GlyphAtlas& atlas, const char* text,
                               float startX, float baselineY, float fontSize,
                               float glyphPx, std::vector<float>& out, int& count) {
  float cursorX = startX;
  float scale = fontSize / glyphPx;

  for (const char* p = text; *p; p++) {
    const GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (!g) continue;
    if (g->w <= 0 || g->h <= 0) {
      cursorX += g->advance * scale;
      continue;
    }
    float x0 = cursorX + g->bearingX * scale;
    float y1 = baselineY + g->bearingY * scale;
    float y0 = y1 - g->h * scale;
    float x1 = x0 + g->w * scale;
    out.push_back(x0); out.push_back(y0); out.push_back(x1); out.push_back(y1);
    out.push_back(g->u0); out.push_back(g->v0); out.push_back(g->u1); out.push_back(g->v1);
    count++;
    cursorX += g->advance * scale;
  }
}

AxisRecipe::AxisData AxisRecipe::computeAxisData(
    const GlyphAtlas& atlas,
    float yMin, float yMax, int xCount,
    float clipYMin, float clipYMax,
    float clipXMin, float clipXMax,
    float glyphPx, float fontSize) const {

  AxisData data;
  data.yTickVertexCount = 0;
  data.xTickVertexCount = 0;
  data.labelGlyphCount = 0;

  int labelCount = 0;

  // Y-axis ticks
  TickSet yTicks = computeNiceTicks(yMin, yMax, 5);
  for (float val : yTicks.values) {
    float clipY = normalizeToClip(val, yMin, yMax, clipYMin, clipYMax);
    if (clipY < clipYMin || clipY > clipYMax) continue;

    // Horizontal tick line
    data.yTickVerts.push_back(clipXMin); data.yTickVerts.push_back(clipY);
    data.yTickVerts.push_back(config_.yAxisClipX); data.yTickVerts.push_back(clipY);
    data.yTickVertexCount += 2;

    // Y-axis label
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(val));
    buildLabelGlyphs(atlas, buf, config_.yAxisClipX + 0.01f, clipY - fontSize * 0.3f,
                     fontSize, glyphPx, data.labelInstances, labelCount);
  }

  // X-axis ticks (every Nth index)
  if (xCount > 0) {
    int step = xCount / 5;
    if (step < 1) step = 1;
    float barWidth = (clipXMax - clipXMin) / static_cast<float>(xCount);

    for (int i = 0; i < xCount; i += step) {
      float clipX = clipXMin + (static_cast<float>(i) + 0.5f) * barWidth;

      // Vertical tick line
      data.xTickVerts.push_back(clipX); data.xTickVerts.push_back(clipYMin);
      data.xTickVerts.push_back(clipX); data.xTickVerts.push_back(config_.xAxisClipY);
      data.xTickVertexCount += 2;

      // X-axis label
      char buf[16];
      std::snprintf(buf, sizeof(buf), "%d", i);
      buildLabelGlyphs(atlas, buf, clipX - fontSize * 0.3f,
                       config_.xAxisClipY - fontSize * 1.2f,
                       fontSize, glyphPx, data.labelInstances, labelCount);
    }
  }

  data.labelGlyphCount = static_cast<std::uint32_t>(labelCount);
  return data;
}

} // namespace dc
