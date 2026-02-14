#include "dc/recipe/HighlightRecipe.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdio>
#include <string>

namespace dc {

HighlightRecipe::HighlightRecipe(Id idBase, const HighlightRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult HighlightRecipe::build() const {
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
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(geometryId()) + "}");

  // Yellow highlight color
  result.createCommands.push_back(
    R"({"cmd":"setDrawItemColor","drawItemId":)" + idStr(drawItemId()) +
    R"(,"r":1.0,"g":1.0,"b":0.0,"a":0.7})");

  // Dispose
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(drawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(geometryId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(bufferId()) + "}");

  return result;
}

HighlightRecipe::HighlightData HighlightRecipe::computeHighlights(
    const SelectionState& selection,
    const Scene& scene,
    const IngestProcessor& ingest) const {

  HighlightData data;
  if (!selection.hasSelection()) return data;

  float sz = config_.markerSize;

  for (auto& key : selection.selectedKeys()) {
    const DrawItem* di = scene.getDrawItem(key.drawItemId);
    if (!di || di->geometryId == 0) continue;

    const Geometry* geo = scene.getGeometry(di->geometryId);
    if (!geo) continue;

    const std::uint8_t* bufData = ingest.getBufferData(geo->vertexBufferId);
    std::uint32_t bufSize = ingest.getBufferSize(geo->vertexBufferId);
    if (!bufData || bufSize == 0) continue;

    std::uint32_t stride = strideOf(geo->format);
    if (stride == 0) continue;
    std::uint32_t recordCount = bufSize / stride;
    if (key.recordIndex >= recordCount) continue;

    const float* rec = reinterpret_cast<const float*>(bufData + key.recordIndex * stride);

    float x = 0, y = 0;
    switch (geo->format) {
      case VertexFormat::Pos2_Clip:
        x = rec[0];
        y = rec[1];
        break;
      case VertexFormat::Candle6:
        x = rec[0]; // cx
        y = (rec[1] + rec[4]) * 0.5f; // (open + close) / 2
        break;
      case VertexFormat::Rect4:
        x = (rec[0] + rec[2]) * 0.5f;
        y = (rec[1] + rec[3]) * 0.5f;
        break;
      default:
        continue;
    }

    // rect4: [x-sz, y-sz, x+sz, y+sz]
    data.rects.push_back(x - sz);
    data.rects.push_back(y - sz);
    data.rects.push_back(x + sz);
    data.rects.push_back(y + sz);
    data.instanceCount++;
  }

  return data;
}

} // namespace dc
