#include "dc/recipe/AxisRecipe.hpp"
#include "dc/math/NiceTicks.hpp"
#include "dc/math/NiceTimeTicks.hpp"
#include "dc/math/TimeFormat.hpp"
#include "dc/math/Normalize.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/text/TextLayout.hpp"
#include <cstdio>
#include <string>

namespace dc {

AxisRecipe::AxisRecipe(Id idBase, const AxisRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

// Helper: emit setDrawItemStyle command for color + lineWidth
static std::string makeStyleCmd(Id drawItemId, const float color[4], float lineWidth) {
  auto diStr = std::to_string(drawItemId);
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%s,)"
    R"("r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
    diStr.c_str(),
    static_cast<double>(color[0]), static_cast<double>(color[1]),
    static_cast<double>(color[2]), static_cast<double>(color[3]),
    static_cast<double>(lineWidth));
  return buf;
}

// Helper: emit setDrawItemStyle for color only
static std::string makeColorStyleCmd(Id drawItemId, const float color[4]) {
  auto diStr = std::to_string(drawItemId);
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%s,)"
    R"("r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    diStr.c_str(),
    static_cast<double>(color[0]), static_cast<double>(color[1]),
    static_cast<double>(color[2]), static_cast<double>(color[3]));
  return buf;
}

// Helper to create a buf/geom/di group for lineAA@1 rect4
static void buildLineAAGroup(RecipeBuildResult& result,
                              Id bufId, Id geomId, Id diId,
                              Id layerId, const std::string& name) {
  auto idStr = [](Id id) { return std::to_string(id); };

  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufId) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geomId) +
    R"(,"vertexBufferId":)" + idStr(bufId) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(diId) +
    R"(,"layerId":)" + idStr(layerId) +
    R"(,"name":")" + name + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(diId) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(geomId) + "}");

  result.subscriptions.push_back({bufId, geomId, VertexFormat::Rect4});
}

// Helper to dispose a buf/geom/di group (reverse order)
static void disposeGroup(RecipeBuildResult& result, Id diId, Id geomId, Id bufId) {
  auto idStr = [](Id id) { return std::to_string(id); };
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(diId) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(geomId) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(bufId) + "}");
}

RecipeBuildResult AxisRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // ---- Existing slots 0-9 (unchanged) ----

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

  // Subscriptions for base slots
  result.subscriptions.push_back({yTickBufferId(), yTickGeomId(), VertexFormat::Pos2_Clip});
  result.subscriptions.push_back({xTickBufferId(), xTickGeomId(), VertexFormat::Pos2_Clip});
  result.subscriptions.push_back({labelBufferId(), labelGeomId(), VertexFormat::Glyph8});

  // ---- D12: Optional grid lines (slots 10-15) ----
  if (config_.enableGrid) {
    Id gridLayer = config_.gridLayerId ? config_.gridLayerId : config_.tickLayerId;
    buildLineAAGroup(result, hGridBufferId(), hGridGeomId(), hGridDrawItemId(),
                     gridLayer, config_.name + "_hGrid");
    buildLineAAGroup(result, vGridBufferId(), vGridGeomId(), vGridDrawItemId(),
                     gridLayer, config_.name + "_vGrid");
    result.createCommands.push_back(makeStyleCmd(hGridDrawItemId(), config_.gridColor, config_.gridLineWidth));
    result.createCommands.push_back(makeStyleCmd(vGridDrawItemId(), config_.gridColor, config_.gridLineWidth));
  }

  // ---- D12: Optional AA tick lines (slots 16-21) ----
  if (config_.enableAALines) {
    buildLineAAGroup(result, yTickAABufferId(), yTickAAGeomId(), yTickAADrawItemId(),
                     config_.tickLayerId, config_.name + "_yTickAA");
    buildLineAAGroup(result, xTickAABufferId(), xTickAAGeomId(), xTickAADrawItemId(),
                     config_.tickLayerId, config_.name + "_xTickAA");
    result.createCommands.push_back(makeStyleCmd(yTickAADrawItemId(), config_.tickColor, config_.tickLineWidth));
    result.createCommands.push_back(makeStyleCmd(xTickAADrawItemId(), config_.tickColor, config_.tickLineWidth));
  }

  // ---- D12: Optional spine lines (slots 22-24) ----
  if (config_.enableSpine) {
    buildLineAAGroup(result, spineBufferId(), spineGeomId(), spineDrawItemId(),
                     config_.tickLayerId, config_.name + "_spine");
    result.createCommands.push_back(makeStyleCmd(spineDrawItemId(), config_.spineColor, config_.spineLineWidth));
  }

  // ---- D12: Identity transform for grid/spine (slot 25) ----
  if (config_.enableGrid || config_.enableSpine) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(gridSpineTransformId()) + "}");

    if (config_.enableGrid) {
      result.createCommands.push_back(
        R"({"cmd":"attachTransform","drawItemId":)" + idStr(hGridDrawItemId()) +
        R"(,"transformId":)" + idStr(gridSpineTransformId()) + "}");
      result.createCommands.push_back(
        R"({"cmd":"attachTransform","drawItemId":)" + idStr(vGridDrawItemId()) +
        R"(,"transformId":)" + idStr(gridSpineTransformId()) + "}");
    }
    if (config_.enableSpine) {
      result.createCommands.push_back(
        R"({"cmd":"attachTransform","drawItemId":)" + idStr(spineDrawItemId()) +
        R"(,"transformId":)" + idStr(gridSpineTransformId()) + "}");
    }
  }

  // ---- D12.4: Style for base tick/label draw items ----
  result.createCommands.push_back(makeStyleCmd(yTickDrawItemId(), config_.tickColor, config_.tickLineWidth));
  result.createCommands.push_back(makeStyleCmd(xTickDrawItemId(), config_.tickColor, config_.tickLineWidth));
  result.createCommands.push_back(makeColorStyleCmd(labelDrawItemId(), config_.labelColor));

  // ---- Dispose (reverse order) ----
  if (config_.enableGrid || config_.enableSpine) {
    result.disposeCommands.push_back(
      R"({"cmd":"delete","id":)" + idStr(gridSpineTransformId()) + "}");
  }
  if (config_.enableSpine) {
    disposeGroup(result, spineDrawItemId(), spineGeomId(), spineBufferId());
  }
  if (config_.enableAALines) {
    disposeGroup(result, xTickAADrawItemId(), xTickAAGeomId(), xTickAABufferId());
    disposeGroup(result, yTickAADrawItemId(), yTickAAGeomId(), yTickAABufferId());
  }
  if (config_.enableGrid) {
    disposeGroup(result, vGridDrawItemId(), vGridGeomId(), vGridBufferId());
    disposeGroup(result, hGridDrawItemId(), hGridGeomId(), hGridBufferId());
  }

  // Original resources
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

const char* AxisRecipe::chooseFormat(float step) {
  if (step >= 1.0f)    return "%.0f";
  if (step >= 0.1f)    return "%.1f";
  if (step >= 0.01f)   return "%.2f";
  return "%.3f";
}

AxisRecipe::AxisData AxisRecipe::computeAxisData(
    const GlyphAtlas& atlas,
    float yMin, float yMax, int xCount,
    float clipYMin, float clipYMax,
    float clipXMin, float clipXMax,
    float glyphPx, float fontSize) const {

  AxisData data;
  int labelCount = 0;

  // Track clip positions for grid/AA generation
  std::vector<float> yTickClips;
  std::vector<float> xTickClips;

  // Y-axis ticks
  TickSet yTicks = computeNiceTicks(yMin, yMax, 5);
  for (float val : yTicks.values) {
    float clipY = normalizeToClip(val, yMin, yMax, clipYMin, clipYMax);
    if (clipY < clipYMin || clipY > clipYMax) continue;

    yTickClips.push_back(clipY);

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

      xTickClips.push_back(clipX);

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

  // D12 extensions
  if (config_.enableGrid) {
    for (float clipY : yTickClips) {
      data.hGridVerts.push_back(clipXMin); data.hGridVerts.push_back(clipY);
      data.hGridVerts.push_back(clipXMax); data.hGridVerts.push_back(clipY);
      data.hGridLineCount++;
    }
    for (float clipX : xTickClips) {
      data.vGridVerts.push_back(clipX); data.vGridVerts.push_back(clipYMin);
      data.vGridVerts.push_back(clipX); data.vGridVerts.push_back(clipYMax);
      data.vGridLineCount++;
    }
  }

  if (config_.enableAALines) {
    for (float clipY : yTickClips) {
      data.yTickAAVerts.push_back(config_.yAxisClipX - config_.yTickLength);
      data.yTickAAVerts.push_back(clipY);
      data.yTickAAVerts.push_back(config_.yAxisClipX);
      data.yTickAAVerts.push_back(clipY);
      data.yTickAAVertexCount++;
    }
    for (float clipX : xTickClips) {
      data.xTickAAVerts.push_back(clipX);
      data.xTickAAVerts.push_back(config_.xAxisClipY);
      data.xTickAAVerts.push_back(clipX);
      data.xTickAAVerts.push_back(config_.xAxisClipY + config_.xTickLength);
      data.xTickAAVertexCount++;
    }
  }

  if (config_.enableSpine) {
    // Vertical spine at yAxisClipX
    data.spineVerts.push_back(config_.yAxisClipX); data.spineVerts.push_back(clipYMin);
    data.spineVerts.push_back(config_.yAxisClipX); data.spineVerts.push_back(clipYMax);
    data.spineLineCount++;
    // Horizontal spine at xAxisClipY
    data.spineVerts.push_back(clipXMin); data.spineVerts.push_back(config_.xAxisClipY);
    data.spineVerts.push_back(clipXMax); data.spineVerts.push_back(config_.xAxisClipY);
    data.spineLineCount++;
  }

  return data;
}

AxisRecipe::AxisData AxisRecipe::computeAxisDataV2(
    const GlyphAtlas& atlas,
    float yMin, float yMax,
    float xMin, float xMax,
    float clipYMin, float clipYMax,
    float clipXMin, float clipXMax,
    float glyphPx, float fontSize) const {

  AxisData data;
  int labelCount = 0;

  std::vector<float> yTickClips;
  std::vector<float> xTickClips;

  // Y-axis ticks (nice values)
  TickSet yTicks = computeNiceTicks(yMin, yMax, 5);
  const char* yFmt = chooseFormat(yTicks.step);
  for (float val : yTicks.values) {
    float clipY = normalizeToClip(val, yMin, yMax, clipYMin, clipYMax);
    if (clipY < clipYMin || clipY > clipYMax) continue;

    yTickClips.push_back(clipY);

    // Horizontal tick line (line2d)
    data.yTickVerts.push_back(clipXMin); data.yTickVerts.push_back(clipY);
    data.yTickVerts.push_back(config_.yAxisClipX); data.yTickVerts.push_back(clipY);
    data.yTickVertexCount += 2;

    // Y-axis label — right-aligned left of spine
    char buf[32];
    std::snprintf(buf, sizeof(buf), yFmt, static_cast<double>(val));
    auto lres = layoutTextRightAligned(atlas, buf, config_.yAxisClipX - 0.01f,
                                        clipY - fontSize * 0.3f, fontSize, glyphPx);
    data.labelInstances.insert(data.labelInstances.end(),
                                lres.glyphInstances.begin(), lres.glyphInstances.end());
    labelCount += lres.glyphCount;
  }

  // X-axis ticks
  std::vector<float> xTickValues;
  float xStep;

  if (config_.xAxisIsTime) {
    // Time-aligned ticks
    TimeTickSet timeTicks = computeNiceTimeTicks(xMin, xMax, 5);
    xTickValues = timeTicks.values;
    xStep = timeTicks.stepSeconds;
  } else {
    // Numeric nice ticks
    TickSet xTicks = computeNiceTicks(xMin, xMax, 5);
    xTickValues = xTicks.values;
    xStep = xTicks.step;
  }

  for (float val : xTickValues) {
    float clipX = normalizeToClip(val, xMin, xMax, clipXMin, clipXMax);
    if (clipX < clipXMin || clipX > clipXMax) continue;

    xTickClips.push_back(clipX);

    // Vertical tick line (line2d)
    data.xTickVerts.push_back(clipX); data.xTickVerts.push_back(clipYMin);
    data.xTickVerts.push_back(clipX); data.xTickVerts.push_back(config_.xAxisClipY);
    data.xTickVertexCount += 2;

    // X-axis label — centered below tick
    std::string labelStr;
    if (config_.xAxisIsTime) {
      const char* timeFmt = chooseTimeFormat(xStep);
      labelStr = formatTimestamp(val, timeFmt, config_.useUTC);
    } else {
      const char* xFmt = chooseFormat(xStep);
      char buf[32];
      std::snprintf(buf, sizeof(buf), xFmt, static_cast<double>(val));
      labelStr = buf;
    }

    // Measure width, then layout centered
    auto measure = layoutText(atlas, labelStr.c_str(), 0, 0, fontSize, glyphPx);
    float halfWidth = measure.advanceWidth * 0.5f;
    auto centered = layoutText(atlas, labelStr.c_str(), clipX - halfWidth,
                                config_.xAxisClipY - fontSize * 1.2f,
                                fontSize, glyphPx);
    data.labelInstances.insert(data.labelInstances.end(),
                                centered.glyphInstances.begin(), centered.glyphInstances.end());
    labelCount += centered.glyphCount;
  }

  data.labelGlyphCount = static_cast<std::uint32_t>(labelCount);

  // D12 extensions
  if (config_.enableGrid) {
    for (float clipY : yTickClips) {
      data.hGridVerts.push_back(clipXMin); data.hGridVerts.push_back(clipY);
      data.hGridVerts.push_back(clipXMax); data.hGridVerts.push_back(clipY);
      data.hGridLineCount++;
    }
    for (float clipX : xTickClips) {
      data.vGridVerts.push_back(clipX); data.vGridVerts.push_back(clipYMin);
      data.vGridVerts.push_back(clipX); data.vGridVerts.push_back(clipYMax);
      data.vGridLineCount++;
    }
  }

  if (config_.enableAALines) {
    for (float clipY : yTickClips) {
      data.yTickAAVerts.push_back(config_.yAxisClipX - config_.yTickLength);
      data.yTickAAVerts.push_back(clipY);
      data.yTickAAVerts.push_back(config_.yAxisClipX);
      data.yTickAAVerts.push_back(clipY);
      data.yTickAAVertexCount++;
    }
    for (float clipX : xTickClips) {
      data.xTickAAVerts.push_back(clipX);
      data.xTickAAVerts.push_back(config_.xAxisClipY);
      data.xTickAAVerts.push_back(clipX);
      data.xTickAAVerts.push_back(config_.xAxisClipY + config_.xTickLength);
      data.xTickAAVertexCount++;
    }
  }

  if (config_.enableSpine) {
    // Vertical spine at yAxisClipX
    data.spineVerts.push_back(config_.yAxisClipX); data.spineVerts.push_back(clipYMin);
    data.spineVerts.push_back(config_.yAxisClipX); data.spineVerts.push_back(clipYMax);
    data.spineLineCount++;
    // Horizontal spine at xAxisClipY
    data.spineVerts.push_back(clipXMin); data.spineVerts.push_back(config_.xAxisClipY);
    data.spineVerts.push_back(clipXMax); data.spineVerts.push_back(config_.xAxisClipY);
    data.spineLineCount++;
  }

  return data;
}

} // namespace dc
