#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scale/Scale.hpp"

#include <string>
#include <vector>

namespace dc {

class Scene;
class IngestProcessor;
class Viewport;
class TableStore;
struct BufferByteSource;

struct AutoScaleConfig {
  float marginFraction{0.05f};   // 5% padding on each side
  bool includeZero{false};
};

class AutoScale {
public:
  void setConfig(const AutoScaleConfig& cfg) { config_ = cfg; }

  // Compute optimal Y range for drawItems visible in viewport's X range.
  // Returns false if no visible data found.
  //
  // NOTE (ENC-596): this is the LEGACY geometry-walk path — a full O(N) rescan
  // over heterogeneous vertex buffers, X-window-filtered. It is retained for the
  // visible-window autoscale it already serves. The DATA-AWARE replacement for
  // the rescan is computeYRangeStreaming() below: a RunningDomain reducer that
  // folds only the new tail rows of a bound f32 column in O(Δ) per tick.
  bool computeYRange(const std::vector<Id>& drawItemIds,
                     const Scene& scene, const IngestProcessor& ingest,
                     const Viewport& viewport,
                     double& yMin, double& yMax) const;

  // ENC-596 — streaming O(Δ) auto-domain. Folds ONLY the rows appended to `column`
  // since `running` was last advanced (append-only), so a per-tick update costs
  // O(Δ), not O(N). Applies the same margin / includeZero policy as the legacy
  // path. Pass the SAME `running` reducer across ticks to keep it incremental;
  // reset it (or bind a new column) when the source changes. Returns false (and
  // leaves the outputs untouched) if no rows have been folded yet.
  bool computeYRangeStreaming(const TableStore& tables, Id tableId,
                              const std::string& column,
                              const BufferByteSource& src,
                              RunningDomain& running,
                              double& yMin, double& yMax) const;

private:
  // Apply marginFraction + includeZero to a raw [lo,hi]. Shared by both paths.
  void applyPolicy(double& lo, double& hi) const;

  AutoScaleConfig config_;
};

} // namespace dc
