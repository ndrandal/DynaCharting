#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>

namespace dc {

class Scene;
class IngestProcessor;
class Viewport;

struct HitResult {
  bool hit{false};
  Id drawItemId{0};
  std::uint32_t recordIndex{0};
  double dataX{0}, dataY{0};
  double distancePx{0};
};

struct PickConfig {
  double maxDistancePx{20.0};
};

class DataPicker {
public:
  void setConfig(const PickConfig& cfg) { config_ = cfg; }

  HitResult pick(double cursorPx, double cursorPy, Id paneId,
                 const Scene& scene, const IngestProcessor& ingest,
                 const Viewport& viewport) const;

private:
  PickConfig config_;
};

} // namespace dc
