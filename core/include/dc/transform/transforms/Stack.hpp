// ENC-616c — `stack` transform (RESEARCH §5.1, core tier).
//
// Cumulative stacking of series for stacked bars / stacked areas / streamgraph:
// given a numeric measure column, produce a per-row baseline `y0` and top `y1`
// (so a bar/area mark draws the band [y0,y1]). Rows are accumulated WITHIN an
// x-position group (the `groupBy` column), in input order, so the manifest's
// sort tier (616b) decides the series stacking order; row i's y0 is the running
// sum of the prior rows in its group and y1 = y0 + value.
//
// OFFSET MODES (the §5.1 row "wiggle/normalize -> baseline policy"):
//   * Zero      — class-1 append-only. Baseline is a fixed 0; y0/y1 are the plain
//                 cumulative band. No baseline policy needed.
//   * Normalize — class-3. Each group is rescaled so its total is 1 (100% stack):
//                 a row's band is value/groupTotal. The baseline DRIFTS as the
//                 group total changes on append, so it REQUIRES a baseline policy.
//   * Wiggle    — class-4 (streamgraph). The whole group is shifted by a wiggle
//                 offset that minimizes silhouette movement; the baseline DRIFTS
//                 globally, so it too REQUIRES a baseline policy.
//
// BASELINE POLICY (RESEARCH §5.1, class-3/4 contract). normalize/wiggle have a
// drifting baseline; replaying them naively makes the chart jitter every tick.
// They are therefore REJECTED at inferSchema unless the node carries an explicit
// BaselinePolicy declaring HOW the drift is pinned:
//   * FixedEpoch      — baseline frozen at a reference epoch (totals from then).
//   * Decaying        — baseline relaxes toward the new total with a decay factor.
//   * ReferenceWindow — baseline computed over a trailing window of N groups.
// Zero offset needs NO policy (and rejects one if given — it has no drift).
//
// Fail-fast typing (inferSchema, data-free): the `value` (and optional `groupBy`)
// columns must exist; `value` must be numeric (f32/i32). The output schema is the
// input schema PLUS two f32 columns `y0`,`y1` (names overridable). normalize/wiggle
// without a baseline policy is rejected here, before any row runs.
#pragma once

#include "dc/transform/Transform.hpp"

#include <optional>
#include <string>

namespace dc {

// How a stacked band is offset from the zero baseline.
enum class StackOffset {
  Zero,       // plain cumulative band from 0 (class-1, append-only)
  Normalize,  // 100% stack: each group rescaled to total 1 (class-3, drifting)
  Wiggle,     // streamgraph: silhouette-minimizing shift (class-4, drifting)
};

// The manifest-style baseline policy that PINS a drifting (normalize/wiggle)
// baseline so replay is stable. Required for normalize/wiggle; forbidden for zero.
struct BaselinePolicy {
  enum class Kind {
    FixedEpoch,       // freeze the baseline at a reference epoch
    Decaying,         // relax the baseline toward the new total (decay in (0,1])
    ReferenceWindow,  // baseline from a trailing window of N groups
  };
  Kind kind{Kind::FixedEpoch};
  // FixedEpoch: the reference epoch the baseline is pinned to (ms).
  std::int64_t fixedEpochMs{0};
  // Decaying: per-tick relaxation factor in (0,1]; 1 == follow instantly.
  double decay{1.0};
  // ReferenceWindow: number of trailing groups the baseline is computed over (>0).
  std::size_t windowGroups{0};
};

class StackTransform : public TransformNode {
 public:
  // Stack `value`, optionally per `groupBy` group (empty = one global group), with
  // the given offset. A baseline policy is required iff offset != Zero.
  StackTransform(std::string value, std::string groupBy, StackOffset offset,
                 std::optional<BaselinePolicy> policy = std::nullopt,
                 std::string y0 = "y0", std::string y1 = "y1")
      : value_(std::move(value)),
        groupBy_(std::move(groupBy)),
        offset_(offset),
        policy_(std::move(policy)),
        y0_(std::move(y0)),
        y1_(std::move(y1)) {}

  const char* op() const override { return "stack"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::string value_;
  std::string groupBy_;  // empty => single global group
  StackOffset offset_;
  std::optional<BaselinePolicy> policy_;
  std::string y0_;
  std::string y1_;
};

}  // namespace dc
