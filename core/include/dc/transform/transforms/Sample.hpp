// ENC-616c — `sample` / LOD transform (RESEARCH §5.1, core tier).
//
// Downsample a large series to a target POINT BUDGET while preserving the visible
// shape — the M4 / min-max bucketed LOD. Per §5.1 this is the "sample/lod (M4)"
// row: class-2 (viewport-hop), reusing the LodManager-family min-max reduction.
//
// THE M4 ALGORITHM. Sort-stable in input order, the `x` domain is split into
// `budget/4` equal-width buckets; from each bucket we KEEP exactly the four rows
// that pin the rasterized pixel column: the FIRST x, the LAST x, the row with the
// MIN y, and the row with the MAX y (deduplicated, emitted in input order). This
// reduces N rows to ~budget while GUARANTEEING the per-bucket y-extremes survive —
// the property a naive every-k-th stride loses. (Buckets smaller than 4 rows just
// pass through.) Output is a COMPACTED row subset, schema-preserving like `filter`.
//
// Fail-fast typing (inferSchema, data-free): the `x` and `y` columns must exist and
// be numeric (f32/i32; x may also be a timestamp, read as i64 ordering); `budget`
// must be >= 2. Output schema EQUALS the input schema; only the row count shrinks.
#pragma once

#include "dc/transform/Transform.hpp"

#include <cstdint>
#include <string>

namespace dc {

class SampleTransform : public TransformNode {
 public:
  // Downsample to <= `budget` rows, bucketing on `x` and preserving `y` extremes.
  SampleTransform(std::string x, std::string y, std::uint32_t budget)
      : x_(std::move(x)), y_(std::move(y)), budget_(budget) {}

  const char* op() const override { return "sample"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::string x_;
  std::string y_;
  std::uint32_t budget_;  // target point budget (>= 2)
};

}  // namespace dc
