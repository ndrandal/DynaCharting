#include "dc/recipe/RSIRecipe.hpp"
#include "dc/math/Indicators.hpp"
#include <cmath>
#include <cstdio>
#include <string>

namespace dc {

RSIRecipe::RSIRecipe(Id idBase, const RSIRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult RSIRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // RSI line: buffer + geometry + drawItem + bind to lineAA@1
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(lineBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(lineGeomId()) +
    R"(,"vertexBufferId":)" + idStr(lineBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(lineDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(lineDrawItemId()) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(lineGeomId()) + "}");

  // Set RSI line style
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
      idStr(lineDrawItemId()).c_str(),
      static_cast<double>(config_.color[0]),
      static_cast<double>(config_.color[1]),
      static_cast<double>(config_.color[2]),
      static_cast<double>(config_.color[3]),
      static_cast<double>(config_.lineWidth));
    result.createCommands.push_back(buf);
  }

  // Ref lines: buffer + geometry + drawItem + bind
  if (config_.showRefLines) {
    result.createCommands.push_back(
      R"({"cmd":"createBuffer","id":)" + idStr(refBufferId()) + R"(,"byteLength":0})");
    result.createCommands.push_back(
      R"({"cmd":"createGeometry","id":)" + idStr(refGeomId()) +
      R"(,"vertexBufferId":)" + idStr(refBufferId()) +
      R"(,"format":"rect4","vertexCount":1})");
    result.createCommands.push_back(
      R"({"cmd":"createDrawItem","id":)" + idStr(refDrawItemId()) +
      R"(,"layerId":)" + idStr(config_.layerId) +
      R"(,"name":")" + config_.name + "_ref" + R"("})");
    result.createCommands.push_back(
      R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(refDrawItemId()) +
      R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(refGeomId()) + "}");

    // Set ref line style
    {
      char buf[256];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
        idStr(refDrawItemId()).c_str(),
        static_cast<double>(config_.refLineColor[0]),
        static_cast<double>(config_.refLineColor[1]),
        static_cast<double>(config_.refLineColor[2]),
        static_cast<double>(config_.refLineColor[3]),
        1.0);  // thin ref lines
      result.createCommands.push_back(buf);
    }
  }

  // Subscriptions
  result.subscriptions.push_back({lineBufferId(), lineGeomId(), VertexFormat::Rect4});
  if (config_.showRefLines) {
    result.subscriptions.push_back({refBufferId(), refGeomId(), VertexFormat::Rect4});
  }

  // Dispose (reverse order)
  if (config_.showRefLines) {
    result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(refDrawItemId()) + "}");
    result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(refGeomId()) + "}");
    result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(refBufferId()) + "}");
  }
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(lineDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(lineGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(lineBufferId()) + "}");

  return result;
}

std::vector<Id> RSIRecipe::drawItemIds() const {
  if (config_.showRefLines)
    return {lineDrawItemId(), refDrawItemId()};
  return {lineDrawItemId()};
}

std::vector<SeriesInfo> RSIRecipe::seriesInfoList() const {
  SeriesInfo si;
  si.name = config_.name.empty() ? "RSI" : config_.name;
  si.colorHint[0] = config_.color[0];
  si.colorHint[1] = config_.color[1];
  si.colorHint[2] = config_.color[2];
  si.colorHint[3] = config_.color[3];
  si.defaultVisible = true;
  si.drawItemIds = {lineDrawItemId()};
  return {si};
}

RSIRecipe::RSIData RSIRecipe::computeRSI(const float* closes, int count,
                                           const float* xCoords) const {
  RSIData data;
  auto rsi = dc::computeRSI(closes, count, config_.period);

  // Generate rect4 line segments (x0,y0,x1,y1) — skip NaN values
  for (int i = 0; i < count - 1; i++) {
    float y0 = rsi[static_cast<std::size_t>(i)];
    float y1 = rsi[static_cast<std::size_t>(i + 1)];
    if (std::isnan(y0) || std::isnan(y1)) continue;

    data.lineSegments.push_back(xCoords[i]);
    data.lineSegments.push_back(y0);
    data.lineSegments.push_back(xCoords[i + 1]);
    data.lineSegments.push_back(y1);
    data.segmentCount++;
  }

  return data;
}

RSIRecipe::RefLineData RSIRecipe::computeRefLines(float xMin, float xMax) const {
  RefLineData data;

  // Overbought line
  data.lineSegments.push_back(xMin);
  data.lineSegments.push_back(config_.overboughtLevel);
  data.lineSegments.push_back(xMax);
  data.lineSegments.push_back(config_.overboughtLevel);
  data.segmentCount++;

  // Oversold line
  data.lineSegments.push_back(xMin);
  data.lineSegments.push_back(config_.oversoldLevel);
  data.lineSegments.push_back(xMax);
  data.lineSegments.push_back(config_.oversoldLevel);
  data.segmentCount++;

  return data;
}

} // namespace dc
