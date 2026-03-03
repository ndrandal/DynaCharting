#pragma once
#include <cstdint>

namespace dc {

struct VirtualRange {
  std::uint32_t startIndex{0};
  std::uint32_t endIndex{0};
  std::uint32_t totalCount{0};
  bool changed{false};
};

struct VirtualConfig {
  std::uint32_t recordStride{0};
  std::uint32_t overscan{50};
  double dataXMin{0};
  double dataXStep{1.0};
};

class VirtualizationManager {
public:
  void setConfig(const VirtualConfig& cfg);
  void setTotalCount(std::uint32_t count);

  VirtualRange update(double viewXMin, double viewXMax);
  const VirtualRange& currentRange() const { return range_; }

private:
  VirtualConfig config_;
  VirtualRange range_;
};

} // namespace dc
