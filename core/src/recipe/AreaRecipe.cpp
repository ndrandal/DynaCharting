#include "dc/recipe/AreaRecipe.hpp"
#include <string>

namespace dc {

AreaRecipe::AreaRecipe(Id idBase, const AreaRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult AreaRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufferId()) + R"(,"byteLength":0})");

  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geometryId()) +
    R"(,"vertexBufferId":)" + idStr(bufferId()) +
    R"(,"format":"pos2_clip","vertexCount":3})");

  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(drawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"("})");

  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(drawItemId()) +
    R"(,"pipeline":"triSolid@1","geometryId":)" + idStr(geometryId()) + "}");

  if (config_.createTransform) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(transformId()) + "}");
    result.createCommands.push_back(
      R"({"cmd":"attachTransform","drawItemId":)" + idStr(drawItemId()) +
      R"(,"transformId":)" + idStr(transformId()) + "}");
  }

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

AreaRecipe::AreaData AreaRecipe::compute(const float* x, const float* y,
                                          int count, float baselineY) const {
  AreaData data;
  data.vertexCount = 0;
  if (count < 2) return data;

  // For each segment (i, i+1), generate 2 triangles (a quad):
  // top-left = (x[i], y[i]), top-right = (x[i+1], y[i+1])
  // bottom-left = (x[i], baselineY), bottom-right = (x[i+1], baselineY)
  for (int i = 0; i < count - 1; i++) {
    float x0 = x[i],   y0 = y[i];
    float x1 = x[i+1], y1 = y[i+1];
    float bY = baselineY;

    // Triangle 1: top-left, bottom-left, top-right
    data.triVerts.push_back(x0); data.triVerts.push_back(y0);
    data.triVerts.push_back(x0); data.triVerts.push_back(bY);
    data.triVerts.push_back(x1); data.triVerts.push_back(y1);

    // Triangle 2: top-right, bottom-left, bottom-right
    data.triVerts.push_back(x1); data.triVerts.push_back(y1);
    data.triVerts.push_back(x0); data.triVerts.push_back(bY);
    data.triVerts.push_back(x1); data.triVerts.push_back(bY);
  }

  data.vertexCount = static_cast<std::uint32_t>((count - 1) * 6);
  return data;
}

} // namespace dc
