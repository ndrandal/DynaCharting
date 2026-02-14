#include "dc/recipe/StochasticRecipe.hpp"
#include "dc/math/Indicators.hpp"
#include <cmath>
#include <cstdio>
#include <string>

namespace dc {

StochasticRecipe::StochasticRecipe(Id idBase, const StochasticRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult StochasticRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // %K line: buffer + geometry + drawItem + bind to lineAA@1
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(kBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(kGeomId()) +
    R"(,"vertexBufferId":)" + idStr(kBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(kDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + "_%K" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(kDrawItemId()) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(kGeomId()) + "}");

  // Set %K line style
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
      idStr(kDrawItemId()).c_str(),
      static_cast<double>(config_.kColor[0]),
      static_cast<double>(config_.kColor[1]),
      static_cast<double>(config_.kColor[2]),
      static_cast<double>(config_.kColor[3]),
      static_cast<double>(config_.lineWidth));
    result.createCommands.push_back(buf);
  }

  // %D line: buffer + geometry + drawItem + bind to lineAA@1
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(dBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(dGeomId()) +
    R"(,"vertexBufferId":)" + idStr(dBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(dDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + "_%D" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(dDrawItemId()) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(dGeomId()) + "}");

  // Set %D line style
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
      idStr(dDrawItemId()).c_str(),
      static_cast<double>(config_.dColor[0]),
      static_cast<double>(config_.dColor[1]),
      static_cast<double>(config_.dColor[2]),
      static_cast<double>(config_.dColor[3]),
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
  result.subscriptions.push_back({kBufferId(), kGeomId(), VertexFormat::Rect4});
  result.subscriptions.push_back({dBufferId(), dGeomId(), VertexFormat::Rect4});
  if (config_.showRefLines) {
    result.subscriptions.push_back({refBufferId(), refGeomId(), VertexFormat::Rect4});
  }

  // Dispose (reverse order)
  if (config_.showRefLines) {
    result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(refDrawItemId()) + "}");
    result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(refGeomId()) + "}");
    result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(refBufferId()) + "}");
  }
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(dDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(dGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(dBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(kDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(kGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(kBufferId()) + "}");

  return result;
}

std::vector<Id> StochasticRecipe::drawItemIds() const {
  if (config_.showRefLines)
    return {kDrawItemId(), dDrawItemId(), refDrawItemId()};
  return {kDrawItemId(), dDrawItemId()};
}

std::vector<SeriesInfo> StochasticRecipe::seriesInfoList() const {
  SeriesInfo kInfo;
  kInfo.name = config_.name.empty() ? "%K" : config_.name + " %K";
  kInfo.colorHint[0] = config_.kColor[0];
  kInfo.colorHint[1] = config_.kColor[1];
  kInfo.colorHint[2] = config_.kColor[2];
  kInfo.colorHint[3] = config_.kColor[3];
  kInfo.defaultVisible = true;
  kInfo.drawItemIds = {kDrawItemId()};

  SeriesInfo dInfo;
  dInfo.name = config_.name.empty() ? "%D" : config_.name + " %D";
  dInfo.colorHint[0] = config_.dColor[0];
  dInfo.colorHint[1] = config_.dColor[1];
  dInfo.colorHint[2] = config_.dColor[2];
  dInfo.colorHint[3] = config_.dColor[3];
  dInfo.defaultVisible = true;
  dInfo.drawItemIds = {dDrawItemId()};

  return {kInfo, dInfo};
}

StochasticRecipe::StochData StochasticRecipe::computeStochastic(
    const float* highs, const float* lows, const float* closes,
    int count, const float* xCoords) const {
  StochData data;

  auto result = dc::computeStochastic(highs, lows, closes, count,
                                       config_.kPeriod, config_.dPeriod);

  // Generate %K line segments (rect4: x0,y0,x1,y1) — skip NaN
  for (int i = 0; i < count - 1; i++) {
    float y0 = result.percentK[static_cast<std::size_t>(i)];
    float y1 = result.percentK[static_cast<std::size_t>(i + 1)];
    if (std::isnan(y0) || std::isnan(y1)) continue;

    data.kSegments.push_back(xCoords[i]);
    data.kSegments.push_back(y0);
    data.kSegments.push_back(xCoords[i + 1]);
    data.kSegments.push_back(y1);
    data.kCount++;
  }

  // Generate %D line segments — skip NaN
  for (int i = 0; i < count - 1; i++) {
    float y0 = result.percentD[static_cast<std::size_t>(i)];
    float y1 = result.percentD[static_cast<std::size_t>(i + 1)];
    if (std::isnan(y0) || std::isnan(y1)) continue;

    data.dSegments.push_back(xCoords[i]);
    data.dSegments.push_back(y0);
    data.dSegments.push_back(xCoords[i + 1]);
    data.dSegments.push_back(y1);
    data.dCount++;
  }

  return data;
}

StochasticRecipe::RefLineData StochasticRecipe::computeRefLines(
    float xMin, float xMax) const {
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
