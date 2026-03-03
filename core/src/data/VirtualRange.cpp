#include "dc/data/VirtualRange.hpp"
#include <algorithm>

namespace dc {

void VirtualizationManager::setConfig(const VirtualConfig& cfg) {
  config_ = cfg;
}

void VirtualizationManager::setTotalCount(std::uint32_t count) {
  range_.totalCount = count;
}

VirtualRange VirtualizationManager::update(double viewXMin, double viewXMax) {
  if (config_.dataXStep <= 0 || range_.totalCount == 0) {
    VirtualRange r;
    r.totalCount = range_.totalCount;
    r.changed = (r.startIndex != range_.startIndex || r.endIndex != range_.endIndex);
    range_ = r;
    return range_;
  }

  double relMin = (viewXMin - config_.dataXMin) / config_.dataXStep;
  double relMax = (viewXMax - config_.dataXMin) / config_.dataXStep;

  int iMin = static_cast<int>(relMin) - static_cast<int>(config_.overscan);
  int iMax = static_cast<int>(relMax) + 1 + static_cast<int>(config_.overscan);

  std::uint32_t start = static_cast<std::uint32_t>(std::max(0, iMin));
  std::uint32_t end = static_cast<std::uint32_t>(
    std::min(static_cast<int>(range_.totalCount), iMax));

  if (end < start) end = start;

  bool changed = (start != range_.startIndex || end != range_.endIndex);

  range_.startIndex = start;
  range_.endIndex = end;
  range_.changed = changed;

  return range_;
}

} // namespace dc
