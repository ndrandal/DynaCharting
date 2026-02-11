#include "dc/recipe/SmaRecipe.hpp"
#include "dc/math/Normalize.hpp"
#include <string>
#include <vector>

namespace dc {

SmaRecipe::SmaRecipe(Id idBase, const SmaRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult SmaRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

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

SmaRecipe::SmaData SmaRecipe::compute(const float* closePrices,
                                        const float* xPositions,
                                        int count, float yMin, float yMax,
                                        float clipYMin, float clipYMax) const {
  SmaData data;
  data.vertexCount = 0;
  int period = config_.period;
  if (count < period || period < 1) return data;

  // Compute SMA values
  std::vector<float> sma(static_cast<std::size_t>(count - period + 1));
  float sum = 0.0f;
  for (int i = 0; i < period; i++) sum += closePrices[i];
  sma[0] = sum / static_cast<float>(period);
  for (int i = period; i < count; i++) {
    sum += closePrices[i] - closePrices[i - period];
    sma[static_cast<std::size_t>(i - period + 1)] = sum / static_cast<float>(period);
  }

  // Generate line segments
  int validPoints = static_cast<int>(sma.size());
  if (validPoints < 2) return data;

  for (int i = 0; i < validPoints - 1; i++) {
    int srcIdx = i + period - 1;
    float x0 = xPositions[srcIdx];
    float y0 = normalizeToClip(sma[static_cast<std::size_t>(i)], yMin, yMax, clipYMin, clipYMax);
    float x1 = xPositions[srcIdx + 1];
    float y1 = normalizeToClip(sma[static_cast<std::size_t>(i + 1)], yMin, yMax, clipYMin, clipYMax);

    data.lineVerts.push_back(x0); data.lineVerts.push_back(y0);
    data.lineVerts.push_back(x1); data.lineVerts.push_back(y1);
  }

  data.vertexCount = static_cast<std::uint32_t>((validPoints - 1) * 2);
  return data;
}

} // namespace dc
