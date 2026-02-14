#include "dc/recipe/ScrollIndicatorRecipe.hpp"
#include <algorithm>
#include <cstdio>
#include <string>

namespace dc {

ScrollIndicatorRecipe::ScrollIndicatorRecipe(Id idBase, const ScrollIndicatorConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult ScrollIndicatorRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // Track (background bar)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(trackBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(trackGeometryId()) +
    R"(,"vertexBufferId":)" + idStr(trackBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(trackDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_track"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(trackDrawItemId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(trackGeometryId()) + "}");

  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemColor","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
      idStr(trackDrawItemId()).c_str(),
      static_cast<double>(config_.trackColor[0]), static_cast<double>(config_.trackColor[1]),
      static_cast<double>(config_.trackColor[2]), static_cast<double>(config_.trackColor[3]));
    result.createCommands.push_back(buf);
  }

  // Thumb (position indicator)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(thumbBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(thumbGeometryId()) +
    R"(,"vertexBufferId":)" + idStr(thumbBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(thumbDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"(_thumb"})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(thumbDrawItemId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(thumbGeometryId()) + "}");

  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemColor","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
      idStr(thumbDrawItemId()).c_str(),
      static_cast<double>(config_.thumbColor[0]), static_cast<double>(config_.thumbColor[1]),
      static_cast<double>(config_.thumbColor[2]), static_cast<double>(config_.thumbColor[3]));
    result.createCommands.push_back(buf);
  }

  // Dispose
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(thumbDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(thumbGeometryId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(thumbBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(trackDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(trackGeometryId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(trackBufferId()) + "}");

  return result;
}

ScrollIndicatorRecipe::IndicatorData ScrollIndicatorRecipe::computeIndicator(
    double fullXMin, double fullXMax,
    double viewXMin, double viewXMax) const {
  IndicatorData data{};

  float barW = config_.barXMax - config_.barXMin;
  float y0 = config_.barY;
  float y1 = config_.barY + config_.barHeight;

  // Track: full width bar
  data.trackRect[0] = config_.barXMin;
  data.trackRect[1] = y0;
  data.trackRect[2] = config_.barXMax;
  data.trackRect[3] = y1;

  // Thumb: proportional to visible range within full range
  double fullRange = fullXMax - fullXMin;
  if (fullRange <= 0.0) fullRange = 1.0;

  double thumbStart = std::max(0.0, (viewXMin - fullXMin) / fullRange);
  double thumbEnd   = std::min(1.0, (viewXMax - fullXMin) / fullRange);

  // Clamp minimum thumb width
  if (thumbEnd - thumbStart < 0.02) {
    double mid = (thumbStart + thumbEnd) * 0.5;
    thumbStart = mid - 0.01;
    thumbEnd = mid + 0.01;
  }

  data.thumbRect[0] = config_.barXMin + static_cast<float>(thumbStart) * barW;
  data.thumbRect[1] = y0;
  data.thumbRect[2] = config_.barXMin + static_cast<float>(thumbEnd) * barW;
  data.thumbRect[3] = y1;

  return data;
}

} // namespace dc
