#include "dc/recipe/CandleRecipe.hpp"
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

  if (config_.createTransform) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(transformId()) + "}");
    result.createCommands.push_back(
      R"({"cmd":"attachTransform","drawItemId":)" + idStr(drawItemId()) +
      R"(,"transformId":)" + idStr(transformId()) + "}");
  }

  // Subscriptions
  result.subscriptions.push_back({bufferId(), geometryId(), VertexFormat::Candle6});

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

} // namespace dc
