#include "dc/recipe/LineRecipe.hpp"
#include <string>

namespace dc {

LineRecipe::LineRecipe(Id idBase, const LineRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult LineRecipe::build() const {
  RecipeBuildResult result;

  auto idStr = [](Id id) { return std::to_string(id); };

  // Create commands
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufferId()) + R"(,"byteLength":0})");

  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geometryId()) +
    R"(,"vertexBufferId":)" + idStr(bufferId()) +
    R"(,"format":"pos2_clip","vertexCount":2})");

  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(drawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"("})");

  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(drawItemId()) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(geometryId()) + "}");

  if (config_.createTransform) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(transformId()) + "}");
    result.createCommands.push_back(
      R"({"cmd":"attachTransform","drawItemId":)" + idStr(drawItemId()) +
      R"(,"transformId":)" + idStr(transformId()) + "}");
  }

  // Subscriptions
  result.subscriptions.push_back({bufferId(), geometryId(), VertexFormat::Pos2_Clip});

  // Dispose commands (reverse order)
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
