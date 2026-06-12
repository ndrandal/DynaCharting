// ENC-616b — `bin` transform (RESEARCH §5.1, core tier; streaming-class-1).
//
// Partitions a NUMERIC input column into uniform-width bins. Two configuration
// modes (RESEARCH line 169 + the §6.4 manifest `bin:{field,maxbins|step,as}`):
//   * step    — an explicit bin WIDTH; the first edge is anchored at floor(min/step)*step.
//   * maxbins — a target bin COUNT; a "nice" step is chosen so the data range is
//               covered by AT MOST maxbins bins (the Vega/d3 tickStep rule).
//
// OUTPUT SCHEMA (RESEARCH §5.2 "bin -> {bin0,bin1,count}") — the input columns are
// passed through unchanged, plus THREE derived columns named off `as`:
//   * <as>        (i32)  the bin INDEX for each row (0-based from the first edge)
//   * <as>0/lo    (f32)  the bin's lower edge   (`<as>` + "0")
//   * <as>1/hi    (f32)  the bin's upper edge   (`<as>` + "1")
// This is a per-row label (NOT a histogram) — one output row per input row — so a
// downstream `aggregate` groupBy=<as> produces the histogram counts (RESEARCH's
// `bin -> aggregate` composition). The per-bin row count is the §5.1 "increment
// target bin on append" the streaming scheduler exploits.
//
// DOMAIN: by default the bin domain is derived from the data (a single O(N) min/max
// pass — class-1 running min/max in the streaming scheduler). An explicit
// {extentLo, extentHi} pins it (a manifest-declared domain), which makes the bin
// edges STABLE across appends (no re-anchoring) — the incremental-friendly path.
//
// Fail-fast typing (inferSchema, data-free): the field must exist and be numeric
// (f32/i32; NOT a timestamp — epoch-ms binning stays on the i64-safe path, out of
// scope here), step>0 / maxbins>0, and `as`/`as0`/`as1` must not collide with an
// input column.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>

namespace dc {

class BinTransform : public TransformNode {
 public:
  // step-mode factory: fixed bin width `step` (> 0). Anchored on the data (or the
  // pinned extent if extentLo<extentHi).
  static BinTransform byStep(std::string field, double step, std::string as) {
    BinTransform b(std::move(field), std::move(as));
    b.step_ = step;
    b.mode_ = Mode::Step;
    return b;
  }
  // maxbins-mode factory: choose a nice step so the range is covered by <= maxbins.
  static BinTransform byMaxBins(std::string field, int maxbins, std::string as) {
    BinTransform b(std::move(field), std::move(as));
    b.maxbins_ = maxbins;
    b.mode_ = Mode::MaxBins;
    return b;
  }

  // Pin the bin domain (stable edges across appends). lo<hi to take effect.
  BinTransform& withExtent(double lo, double hi) {
    extentLo_ = lo;
    extentHi_ = hi;
    return *this;
  }

  const char* op() const override { return "bin"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

  // The derived column names this transform emits (index, lo edge, hi edge).
  std::string idxField() const { return as_; }
  std::string loField() const { return as_ + "0"; }
  std::string hiField() const { return as_ + "1"; }

 private:
  enum class Mode { Step, MaxBins };
  BinTransform(std::string field, std::string as)
      : field_(std::move(field)), as_(std::move(as)) {}

  // Resolve {firstEdge, step, count} from the data extent (or the pinned extent).
  // Shared by evaluate(); pure arithmetic over a [min,max] range.
  struct BinSpec {
    double firstEdge{0.0};
    double step{1.0};
    int count{1};
  };
  BinSpec resolveSpec(double dataMin, double dataMax) const;

  std::string field_;
  std::string as_;
  Mode mode_{Mode::MaxBins};
  double step_{1.0};
  int maxbins_{10};
  double extentLo_{0.0};
  double extentHi_{0.0};  // extentLo_<extentHi_ => pinned
};

}  // namespace dc
