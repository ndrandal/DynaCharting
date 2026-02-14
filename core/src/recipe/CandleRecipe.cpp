#include "dc/recipe/CandleRecipe.hpp"
#include <cstdio>
#include <string>

namespace dc {

CandleRecipe::CandleRecipe(Id idBase, const CandleRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult CandleRecipe::build() const {
  RecipeBuildResult result;

  auto idStr = [](Id id) { return std::to_string(id); };

  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufferId()) + R"(,"byteLength":0})");

  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geometryId()) +
    R"(,"vertexBufferId":)" + idStr(bufferId()) +
    R"(,"format":"candle6","vertexCount":1})");

  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(drawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"("})");

  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(drawItemId()) +
    R"(,"pipeline":"instancedCandle@1","geometryId":)" + idStr(geometryId()) + "}");

  // Emit setDrawItemStyle with candle colors from config
  {
    char styleBuf[512];
    std::snprintf(styleBuf, sizeof(styleBuf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,)"
      R"("colorUpR":%.9g,"colorUpG":%.9g,"colorUpB":%.9g,"colorUpA":%.9g,)"
      R"("colorDownR":%.9g,"colorDownG":%.9g,"colorDownB":%.9g,"colorDownA":%.9g})",
      idStr(drawItemId()).c_str(),
      static_cast<double>(config_.colorUp[0]), static_cast<double>(config_.colorUp[1]),
      static_cast<double>(config_.colorUp[2]), static_cast<double>(config_.colorUp[3]),
      static_cast<double>(config_.colorDown[0]), static_cast<double>(config_.colorDown[1]),
      static_cast<double>(config_.colorDown[2]), static_cast<double>(config_.colorDown[3]));
    result.createCommands.push_back(styleBuf);
  }

  if (config_.createTransform) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(transformId()) + "}");
    result.createCommands.push_back(
      R"({"cmd":"attachTransform","drawItemId":)" + idStr(drawItemId()) +
      R"(,"transformId":)" + idStr(transformId()) + "}");
  }

  // Subscriptions
  result.subscriptions.push_back({bufferId(), geometryId(), VertexFormat::Candle6, drawItemId()});

  // Dispose (reverse)
  if (config_.createTransform) {
    result.disposeCommands.push_back(
      R"({"cmd":"delete","id":)" + idStr(transformId()) + "}");
  }
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(drawItemId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(geometryId()) + "}");
  result.disposeCommands.push_back(
    R"({"cmd":"delete","id":)" + idStr(bufferId()) + "}");

  return result;
}

std::vector<SeriesInfo> CandleRecipe::seriesInfoList() const {
  SeriesInfo si;
  si.name = config_.name.empty() ? "Candles" : config_.name;
  si.colorHint[0] = config_.colorUp[0];
  si.colorHint[1] = config_.colorUp[1];
  si.colorHint[2] = config_.colorUp[2];
  si.colorHint[3] = config_.colorUp[3];
  si.defaultVisible = true;
  si.drawItemIds = {drawItemId()};
  return {si};
}

} // namespace dc
