#include "dc/recipe/MinimapRecipe.hpp"
#include <cstdio>
#include <string>

namespace dc {

MinimapRecipe::MinimapRecipe(Id idBase, const MinimapRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult MinimapRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // ---- Track (background rect) — instancedRect@1 ----
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(trackBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(trackGeomId()) +
    R"(,"vertexBufferId":)" + idStr(trackBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(trackDIId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_track"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(trackDIId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(trackGeomId()) + "}");

  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemColor","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
      idStr(trackDIId()).c_str(),
      static_cast<double>(config_.trackColor[0]), static_cast<double>(config_.trackColor[1]),
      static_cast<double>(config_.trackColor[2]), static_cast<double>(config_.trackColor[3]));
    result.createCommands.push_back(buf);
  }

  // ---- Window (viewport overlay rect) — instancedRect@1 ----
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(windowBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(windowGeomId()) +
    R"(,"vertexBufferId":)" + idStr(windowBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(windowDIId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_window"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(windowDIId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(windowGeomId()) + "}");

  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemColor","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
      idStr(windowDIId()).c_str(),
      static_cast<double>(config_.windowColor[0]), static_cast<double>(config_.windowColor[1]),
      static_cast<double>(config_.windowColor[2]), static_cast<double>(config_.windowColor[3]));
    result.createCommands.push_back(buf);
  }

  // ---- Border (4 line segments) — lineAA@1 ----
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(borderBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(borderGeomId()) +
    R"(,"vertexBufferId":)" + idStr(borderBufferId()) +
    R"(,"format":"rect4","vertexCount":4})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(borderDIId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_border"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(borderDIId()) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(borderGeomId()) + "}");

  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
      idStr(borderDIId()).c_str(),
      static_cast<double>(config_.borderColor[0]), static_cast<double>(config_.borderColor[1]),
      static_cast<double>(config_.borderColor[2]), static_cast<double>(config_.borderColor[3]),
      static_cast<double>(config_.borderWidth));
    result.createCommands.push_back(buf);
  }

  // ---- Dispose (reverse order) ----
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(borderDIId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(borderGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(borderBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(windowDIId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(windowGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(windowBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(trackDIId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(trackGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(trackBufferId()) + "}");

  return result;
}

std::vector<Id> MinimapRecipe::drawItemIds() const {
  return {trackDIId(), windowDIId(), borderDIId()};
}

MinimapRecipe::MinimapData MinimapRecipe::computeMinimap(
    const MinimapViewWindow& window,
    float clipX0, float clipY0,
    float clipX1, float clipY1) const {

  MinimapData data{};

  // Track rect covers the full clip region
  data.trackRect[0] = clipX0;
  data.trackRect[1] = clipY0;
  data.trackRect[2] = clipX1;
  data.trackRect[3] = clipY1;

  // Window rect maps the normalized view window to the clip region
  float regionW = clipX1 - clipX0;
  float regionH = clipY1 - clipY0;

  float wx0 = clipX0 + window.x0 * regionW;
  float wy0 = clipY0 + window.y0 * regionH;
  float wx1 = clipX0 + window.x1 * regionW;
  float wy1 = clipY0 + window.y1 * regionH;

  data.windowRect[0] = wx0;
  data.windowRect[1] = wy0;
  data.windowRect[2] = wx1;
  data.windowRect[3] = wy1;

  // Border: 4 line segments forming the window rect border
  // Bottom: (wx0,wy0) -> (wx1,wy0)
  data.borderLines.push_back(wx0);
  data.borderLines.push_back(wy0);
  data.borderLines.push_back(wx1);
  data.borderLines.push_back(wy0);

  // Top: (wx0,wy1) -> (wx1,wy1)
  data.borderLines.push_back(wx0);
  data.borderLines.push_back(wy1);
  data.borderLines.push_back(wx1);
  data.borderLines.push_back(wy1);

  // Left: (wx0,wy0) -> (wx0,wy1)
  data.borderLines.push_back(wx0);
  data.borderLines.push_back(wy0);
  data.borderLines.push_back(wx0);
  data.borderLines.push_back(wy1);

  // Right: (wx1,wy0) -> (wx1,wy1)
  data.borderLines.push_back(wx1);
  data.borderLines.push_back(wy0);
  data.borderLines.push_back(wx1);
  data.borderLines.push_back(wy1);

  data.borderCount = 4;

  return data;
}

} // namespace dc
