#include "dc/recipe/VolumeRecipe.hpp"
#include <cstdio>
#include <string>

namespace dc {

VolumeRecipe::VolumeRecipe(Id idBase, const VolumeRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult VolumeRecipe::build() const {
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

  // Emit setDrawItemStyle with volume bar colors
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

  // No data subscription — volume data is driven by compute callback
  // (no raw data feed; derived from candle data)

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

VolumeRecipe::VolumeData VolumeRecipe::computeVolumeBars(
    const float* candleData,
    const float* volumes,
    int count,
    float barHalfWidth) const {
  VolumeData data;
  if (!candleData || !volumes || count <= 0) return data;

  data.candle6.resize(static_cast<std::size_t>(count) * 6);
  data.barCount = static_cast<std::uint32_t>(count);

  for (int i = 0; i < count; i++) {
    float x     = candleData[i * 6 + 0];
    float open  = candleData[i * 6 + 1];
    float close = candleData[i * 6 + 4];
    float vol   = volumes[i];
    bool isUp   = (close >= open);

    std::size_t base = static_cast<std::size_t>(i) * 6;
    data.candle6[base + 0] = x;
    data.candle6[base + 1] = isUp ? 0.0f : vol;   // open
    data.candle6[base + 2] = vol;                   // high
    data.candle6[base + 3] = 0.0f;                  // low
    data.candle6[base + 4] = isUp ? vol : 0.0f;    // close
    data.candle6[base + 5] = barHalfWidth;           // halfWidth
  }

  return data;
}

std::vector<SeriesInfo> VolumeRecipe::seriesInfoList() const {
  SeriesInfo si;
  si.name = config_.name.empty() ? "Volume" : config_.name;
  si.colorHint[0] = config_.colorUp[0];
  si.colorHint[1] = config_.colorUp[1];
  si.colorHint[2] = config_.colorUp[2];
  si.colorHint[3] = config_.colorUp[3];
  si.defaultVisible = true;
  si.drawItemIds = {drawItemId()};
  return {si};
}

} // namespace dc
