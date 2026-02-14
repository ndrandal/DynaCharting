#include "dc/recipe/MeasureRecipe.hpp"
#include <cstdio>
#include <string>

namespace dc {

MeasureRecipe::MeasureRecipe(Id idBase, const MeasureRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult MeasureRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geometryId()) +
    R"(,"vertexBufferId":)" + idStr(bufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(drawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(drawItemId()) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(geometryId()) + "}");

  // Set default style
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
      idStr(drawItemId()).c_str(),
      static_cast<double>(config_.lineColor[0]),
      static_cast<double>(config_.lineColor[1]),
      static_cast<double>(config_.lineColor[2]),
      static_cast<double>(config_.lineColor[3]),
      static_cast<double>(config_.lineWidth));
    result.createCommands.push_back(buf);
  }

  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(drawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(geometryId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(bufferId()) + "}");

  return result;
}

MeasureRecipe::MeasureData MeasureRecipe::computeMeasure(const MeasureResult& measure) const {
  MeasureData data;
  if (!measure.valid) return data;

  auto x0 = static_cast<float>(measure.x0);
  auto y0 = static_cast<float>(measure.y0);
  auto x1 = static_cast<float>(measure.x1);
  auto y1 = static_cast<float>(measure.y1);

  // Segment 1: Diagonal from (x0,y0) to (x1,y1)
  data.lineSegments.push_back(x0);
  data.lineSegments.push_back(y0);
  data.lineSegments.push_back(x1);
  data.lineSegments.push_back(y1);
  data.segmentCount++;

  // Segment 2: Horizontal from (x0,y0) to (x1,y0)
  data.lineSegments.push_back(x0);
  data.lineSegments.push_back(y0);
  data.lineSegments.push_back(x1);
  data.lineSegments.push_back(y0);
  data.segmentCount++;

  // Segment 3: Vertical from (x1,y0) to (x1,y1)
  data.lineSegments.push_back(x1);
  data.lineSegments.push_back(y0);
  data.lineSegments.push_back(x1);
  data.lineSegments.push_back(y1);
  data.segmentCount++;

  return data;
}

} // namespace dc
