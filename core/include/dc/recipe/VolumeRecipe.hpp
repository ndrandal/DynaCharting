#pragma once
#include "dc/recipe/Recipe.hpp"
#include <string>
#include <vector>

namespace dc {

// Volume bar recipe — uses instancedCandle@1 to get automatic up/down coloring.
// Each volume bar is encoded as a candle6 record:
//   x = timestamp, open/close encode direction, low=0, high=volume.
//
// ID layout (4 slots):
//   0: Buffer (candle6 data)
//   1: Geometry
//   2: DrawItem
//   3: Transform
struct VolumeRecipeConfig {
  Id paneId{0};
  Id layerId{0};
  std::string name;
  bool createTransform{true};
  float colorUp[4]   = {0.0f, 0.5f, 0.0f, 0.6f};
  float colorDown[4] = {0.5f, 0.0f, 0.0f, 0.6f};
};

class VolumeRecipe : public Recipe {
public:
  VolumeRecipe(Id idBase, const VolumeRecipeConfig& config);

  RecipeBuildResult build() const override;
  std::vector<Id> drawItemIds() const override { return {drawItemId()}; }
  std::vector<SeriesInfo> seriesInfoList() const override;

  Id bufferId() const    { return rid(0); }
  Id geometryId() const  { return rid(1); }
  Id drawItemId() const  { return rid(2); }
  Id transformId() const { return rid(3); }

  static constexpr std::uint32_t ID_SLOTS = 4;

  struct VolumeData {
    std::vector<float> candle6;   // candle6 format records
    std::uint32_t barCount{0};
  };

  // Generate volume bars from candle data + separate volume values.
  // candleData: candle6 records (6 floats each: x, open, high, low, close, hw)
  // volumes: one float per candle (volume value)
  // count: number of candles
  // barHalfWidth: half-width of each volume bar in data space
  VolumeData computeVolumeBars(const float* candleData,
                                const float* volumes,
                                int count,
                                float barHalfWidth) const;

private:
  VolumeRecipeConfig config_;
};

} // namespace dc
