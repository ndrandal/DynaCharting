#include "dc/recipe/MacdRecipe.hpp"
#include "dc/math/Ema.hpp"
#include "dc/math/Normalize.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace dc {

MacdRecipe::MacdRecipe(Id idBase, const MacdRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult MacdRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  // MACD line (line2d@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(macdLineBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(macdLineGeomId()) +
    R"(,"vertexBufferId":)" + idStr(macdLineBufferId()) +
    R"(,"format":"pos2_clip","vertexCount":2})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(macdLineDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.lineLayerId) +
    R"(,"name":")" + config_.name + "_macd" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(macdLineDrawItemId()) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(macdLineGeomId()) + "}");

  // Signal line (line2d@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(signalLineBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(signalLineGeomId()) +
    R"(,"vertexBufferId":)" + idStr(signalLineBufferId()) +
    R"(,"format":"pos2_clip","vertexCount":2})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(signalLineDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.lineLayerId) +
    R"(,"name":")" + config_.name + "_signal" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(signalLineDrawItemId()) +
    R"(,"pipeline":"line2d@1","geometryId":)" + idStr(signalLineGeomId()) + "}");

  // Positive histogram (instancedRect@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(posHistBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(posHistGeomId()) +
    R"(,"vertexBufferId":)" + idStr(posHistBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(posHistDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.histLayerId) +
    R"(,"name":")" + config_.name + "_posHist" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(posHistDrawItemId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(posHistGeomId()) + "}");

  // Negative histogram (instancedRect@1)
  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(negHistBufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(negHistGeomId()) +
    R"(,"vertexBufferId":)" + idStr(negHistBufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(negHistDrawItemId()) +
    R"(,"layerId":)" + idStr(config_.histLayerId) +
    R"(,"name":")" + config_.name + "_negHist" + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(negHistDrawItemId()) +
    R"(,"pipeline":"instancedRect@1","geometryId":)" + idStr(negHistGeomId()) + "}");

  // Shared transform
  if (config_.createTransform) {
    result.createCommands.push_back(
      R"({"cmd":"createTransform","id":)" + idStr(transformId()) + "}");
    for (Id diId : {macdLineDrawItemId(), signalLineDrawItemId(),
                    posHistDrawItemId(), negHistDrawItemId()}) {
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
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(negHistDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(negHistGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(negHistBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(posHistDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(posHistGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(posHistBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(signalLineDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(signalLineGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(signalLineBufferId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(macdLineDrawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(macdLineGeomId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(macdLineBufferId()) + "}");

  return result;
}

MacdRecipe::MacdData MacdRecipe::compute(
    const float* closePrices, const float* xPositions,
    int count, float halfWidth,
    float clipYMin, float clipYMax) const {

  MacdData data;
  data.macdVC = data.signalVC = data.posHistCount = data.negHistCount = 0;

  int slowP = config_.slowPeriod;
  if (count < slowP) return data;

  // Compute fast and slow EMA
  std::vector<float> fastEma(static_cast<std::size_t>(count));
  std::vector<float> slowEma(static_cast<std::size_t>(count));
  computeEma(closePrices, fastEma.data(), count, config_.fastPeriod);
  computeEma(closePrices, slowEma.data(), count, config_.slowPeriod);

  // MACD line = fast EMA - slow EMA (valid from slowPeriod-1 onward)
  int macdStart = slowP - 1;
  int macdCount = count - macdStart;
  std::vector<float> macdValues(static_cast<std::size_t>(macdCount));
  for (int i = 0; i < macdCount; i++) {
    macdValues[static_cast<std::size_t>(i)] =
      fastEma[static_cast<std::size_t>(macdStart + i)] -
      slowEma[static_cast<std::size_t>(macdStart + i)];
  }

  // Signal line = EMA of MACD values
  int sigP = config_.signalPeriod;
  if (macdCount < sigP) return data;
  std::vector<float> signalValues(static_cast<std::size_t>(macdCount));
  computeEma(macdValues.data(), signalValues.data(), macdCount, sigP);

  // Histogram = MACD - Signal (valid from signalPeriod-1 of macdValues onward)
  int histStart = sigP - 1;
  int histCount = macdCount - histStart;
  if (histCount < 2) return data;

  // Find MACD range for normalization
  float macdMin = macdValues[static_cast<std::size_t>(histStart)];
  float macdMax = macdMin;
  for (int i = histStart; i < macdCount; i++) {
    float v = macdValues[static_cast<std::size_t>(i)];
    float s = signalValues[static_cast<std::size_t>(i)];
    float h = v - s;
    macdMin = std::min({macdMin, v, s, h});
    macdMax = std::max({macdMax, v, s, h});
  }
  // Ensure symmetric range around zero for histogram
  float absMax = std::max(std::fabs(macdMin), std::fabs(macdMax));
  if (absMax < 1e-9f) absMax = 1.0f;
  macdMin = -absMax;
  macdMax = absMax;

  float clipMid = (clipYMin + clipYMax) * 0.5f;

  auto toClip = [&](float val) {
    return normalizeToClip(val, macdMin, macdMax, clipYMin, clipYMax);
  };

  // Generate MACD + signal line segments
  for (int i = histStart; i < macdCount - 1; i++) {
    int srcIdx = macdStart + i;
    float x0 = xPositions[srcIdx];
    float x1 = xPositions[srcIdx + 1];

    float my0 = toClip(macdValues[static_cast<std::size_t>(i)]);
    float my1 = toClip(macdValues[static_cast<std::size_t>(i + 1)]);
    data.macdLineVerts.push_back(x0); data.macdLineVerts.push_back(my0);
    data.macdLineVerts.push_back(x1); data.macdLineVerts.push_back(my1);

    float sy0 = toClip(signalValues[static_cast<std::size_t>(i)]);
    float sy1 = toClip(signalValues[static_cast<std::size_t>(i + 1)]);
    data.signalLineVerts.push_back(x0); data.signalLineVerts.push_back(sy0);
    data.signalLineVerts.push_back(x1); data.signalLineVerts.push_back(sy1);
  }

  std::uint32_t lineSegs = static_cast<std::uint32_t>(histCount - 1);
  data.macdVC = lineSegs * 2;
  data.signalVC = lineSegs * 2;

  // Generate histogram bars (rect4: x0, y0, x1, y1)
  for (int i = histStart; i < macdCount; i++) {
    int srcIdx = macdStart + i;
    float x = xPositions[srcIdx];
    float histVal = macdValues[static_cast<std::size_t>(i)] -
                    signalValues[static_cast<std::size_t>(i)];
    float barY = toClip(histVal);

    float rx0 = x - halfWidth;
    float rx1 = x + halfWidth;
    if (histVal >= 0.0f) {
      data.posHistRects.push_back(rx0);
      data.posHistRects.push_back(clipMid);
      data.posHistRects.push_back(rx1);
      data.posHistRects.push_back(barY);
      data.posHistCount++;
    } else {
      data.negHistRects.push_back(rx0);
      data.negHistRects.push_back(barY);
      data.negHistRects.push_back(rx1);
      data.negHistRects.push_back(clipMid);
      data.negHistCount++;
    }
  }

  return data;
}

} // namespace dc
