#include "dc/recipe/BollingerRecipe.hpp"
#include "dc/math/Normalize.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace dc {

BollingerRecipe::BollingerRecipe(Id idBase, const BollingerRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

void BollingerRecipe::buildLineCommands(RecipeBuildResult& result,
                                         Id bufId, Id geomId, Id diId,
                                         Id layerId, const std::string& name) const {
  auto idStr = [](Id id) { return std::to_string(id); };

  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufId) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geomId) +
    R"(,"vertexBufferId":)" + idStr(bufId) +
    R"(,"format":"pos2_clip","vertexCount":2})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(diId) +
    R"(,"layerId":)" + idStr(layerId) +
    R"(,"name":")" + name + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(diId) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(geomId) + "}");
}

RecipeBuildResult BollingerRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // Middle SMA line
  buildLineCommands(result, middleBufferId(), middleGeomId(), middleDrawItemId(),
                    config_.lineLayerId, config_.name + "_mid");
  // Upper band
  buildLineCommands(result, upperBufferId(), upperGeomId(), upperDrawItemId(),
                    config_.lineLayerId, config_.name + "_upper");
  // Lower band
  buildLineCommands(result, lowerBufferId(), lowerGeomId(), lowerDrawItemId(),
                    config_.lineLayerId, config_.name + "_lower");

  // Fill area (triSolid@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(fillBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(fillGeomId()) +
    R"(,"vertexBufferId":)" + idStr(fillBufferId()) +
    R"(,"format":"pos2_clip","vertexCount":3})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(fillDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.fillLayerId) +
    R"(,"name":")" + config_.name + "_fill" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(fillDrawItemId()) +
    R"(,"pipeline":"triSolid@1","geometryId":)" + idStr(fillGeomId()) + "}");

  // Shared transform
  if (config_.createTransform) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(transformId()) + "}");
    // Attach to all 4 draw items
    for (Id diId : {middleDrawItemId(), upperDrawItemId(), lowerDrawItemId(), fillDrawItemId()}) {
      result.createCommands.push_back(
        R"({"cmd":"attachTransform","drawItemId":)" + idStr(diId) +
        R"(,"transformId":)" + idStr(transformId()) + "}");
    }
  }

  // Dispose (reverse order)
  if (config_.createTransform) {
    result.disposeCommands.push_back(
      R"({"cmd":"delete","id":)" + idStr(transformId()) + "}");
  }
  // Fill
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(fillDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(fillGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(fillBufferId()) + "}");
  // Lower
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(lowerDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(lowerGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(lowerBufferId()) + "}");
  // Upper
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(upperDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(upperGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(upperBufferId()) + "}");
  // Middle
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(middleDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(middleGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(middleBufferId()) + "}");

  return result;
}

BollingerRecipe::BollingerData BollingerRecipe::compute(
    const float* closePrices, const float* xPositions,
    int count, float yMin, float yMax,
    float clipYMin, float clipYMax) const {

  BollingerData data;
  data.middleVC = data.upperVC = data.lowerVC = data.fillVC = 0;
  int period = config_.period;
  if (count < period || period < 1) return data;

  int validCount = count - period + 1;

  // Compute SMA + stddev
  std::vector<float> sma(static_cast<std::size_t>(validCount));
  std::vector<float> upper(static_cast<std::size_t>(validCount));
  std::vector<float> lower(static_cast<std::size_t>(validCount));

  for (int i = 0; i < validCount; i++) {
    float sum = 0.0f;
    for (int j = 0; j < period; j++) sum += closePrices[i + j];
    float mean = sum / static_cast<float>(period);
    sma[static_cast<std::size_t>(i)] = mean;

    float varSum = 0.0f;
    for (int j = 0; j < period; j++) {
      float d = closePrices[i + j] - mean;
      varSum += d * d;
    }
    float stddev = std::sqrt(varSum / static_cast<float>(period));
    upper[static_cast<std::size_t>(i)] = mean + config_.numStdDev * stddev;
    lower[static_cast<std::size_t>(i)] = mean - config_.numStdDev * stddev;
  }

  if (validCount < 2) return data;

  auto toClip = [&](float val) {
    return normalizeToClip(val, yMin, yMax, clipYMin, clipYMax);
  };

  // Generate line segments and fill triangles
  for (int i = 0; i < validCount - 1; i++) {
    int srcIdx = i + period - 1;
    float x0 = xPositions[srcIdx];
    float x1 = xPositions[srcIdx + 1];

    float mY0 = toClip(sma[static_cast<std::size_t>(i)]);
    float mY1 = toClip(sma[static_cast<std::size_t>(i + 1)]);
    data.middleVerts.push_back(x0); data.middleVerts.push_back(mY0);
    data.middleVerts.push_back(x1); data.middleVerts.push_back(mY1);

    float uY0 = toClip(upper[static_cast<std::size_t>(i)]);
    float uY1 = toClip(upper[static_cast<std::size_t>(i + 1)]);
    data.upperVerts.push_back(x0); data.upperVerts.push_back(uY0);
    data.upperVerts.push_back(x1); data.upperVerts.push_back(uY1);

    float lY0 = toClip(lower[static_cast<std::size_t>(i)]);
    float lY1 = toClip(lower[static_cast<std::size_t>(i + 1)]);
    data.lowerVerts.push_back(x0); data.lowerVerts.push_back(lY0);
    data.lowerVerts.push_back(x1); data.lowerVerts.push_back(lY1);

    // Fill between upper and lower: 2 triangles per segment
    data.fillVerts.push_back(x0); data.fillVerts.push_back(uY0);
    data.fillVerts.push_back(x0); data.fillVerts.push_back(lY0);
    data.fillVerts.push_back(x1); data.fillVerts.push_back(uY1);

    data.fillVerts.push_back(x1); data.fillVerts.push_back(uY1);
    data.fillVerts.push_back(x0); data.fillVerts.push_back(lY0);
    data.fillVerts.push_back(x1); data.fillVerts.push_back(lY1);
  }

  std::uint32_t segments = static_cast<std::uint32_t>(validCount - 1);
  data.middleVC = segments * 2;
  data.upperVC = segments * 2;
  data.lowerVC = segments * 2;
  data.fillVC = segments * 6;

  return data;
}

} // namespace dc
