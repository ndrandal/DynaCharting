#pragma once
#include "dc/ids/Id.hpp"
#include <vector>

namespace dc {

class Scene;
class IngestProcessor;
class Viewport;

struct AutoScaleConfig {
  float marginFraction{0.05f};   // 5% padding on each side
  bool includeZero{false};
};

class AutoScale {
public:
  void setConfig(const AutoScaleConfig& cfg) { config_ = cfg; }

  // Compute optimal Y range for drawItems visible in viewport's X range.
  // Returns false if no visible data found.
  bool computeYRange(const std::vector<Id>& drawItemIds,
                     const Scene& scene, const IngestProcessor& ingest,
                     const Viewport& viewport,
                     double& yMin, double& yMax) const;

private:
  AutoScaleConfig config_;
};

} // namespace dc
