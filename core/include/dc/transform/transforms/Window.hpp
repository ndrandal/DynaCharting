// ENC-616c — `window` / rolling transform (RESEARCH §5.1, core tier).
//
// A rolling aggregate over a trailing frame [-N, 0] of a numeric column, emitting
// one derived f32 column. Two regimes per §5.1:
//   * Fixed-window aggregates (mean/sum/min/max) over the last `window` rows —
//     class-2 (a fixed-window recompute). Row i aggregates rows (i-window+1 .. i),
//     clamped at the head (a partial window for the first rows).
//   * EMA — class-1, O(1) per row, computed through the EXISTING helper
//     `core/math/Ema.cpp` (`computeEma`): a `period`-seeded exponential mean. No
//     re-summation of a frame; the §5.1 "EMA O(1)" fast path reused verbatim.
//
// Fail-fast typing (inferSchema, data-free): the source column must exist and be
// numeric (f32/i32); `window`/`period` must be >= 1; the output name must not
// collide. Output schema = input schema PLUS one new f32 column.
#pragma once

#include "dc/transform/Transform.hpp"

#include <cstdint>
#include <string>

namespace dc {

// The rolling aggregate applied over the frame.
enum class WindowAgg {
  Mean,  // moving average over the last `window` rows
  Sum,   // moving sum
  Min,   // moving minimum
  Max,   // moving maximum
  Ema,   // exponential moving average (O(1), via core/math/Ema.cpp)
};

class WindowTransform : public TransformNode {
 public:
  // Aggregate `source` over a trailing frame of `window` rows into column `as`.
  // For Ema, `window` is the EMA period (the seed length).
  WindowTransform(std::string source, WindowAgg agg, std::uint32_t window,
                  std::string as)
      : source_(std::move(source)),
        agg_(agg),
        window_(window),
        as_(std::move(as)) {}

  const char* op() const override { return "window"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::string source_;
  WindowAgg agg_;
  std::uint32_t window_;  // frame length / EMA period (>= 1)
  std::string as_;
};

}  // namespace dc
