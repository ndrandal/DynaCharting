// ENC-616b — `aggregate` transform (RESEARCH §5.1, core tier).
//
// groupBy one or more KEY columns + REDUCE each measure to one value per group:
// the §5.1 "groupby+reduce" producing ONE output row per distinct key tuple
// (RESEARCH §5.2 "aggregate -> keys + measures"). The classic histogram /
// volume-profile / streamgraph core.
//
// REDUCERS (RESEARCH line 170 "sum/mean/min/max/count/p50"):
//   * Count   — rows in the group (streaming-class-1).         [no input field]
//   * Sum     — sum of the measure column (class-1).
//   * Mean    — arithmetic mean (class-1: running sum/count).
//   * Min     — minimum (class-1: running min).
//   * Max     — maximum (class-1: running max).
//   * Median  — the p50 quantile (class-2: needs the group's values; computed by
//               nth_element on the collected group — exact, not a sketch).
//
// OUTPUT SCHEMA (data-free): the KEY columns (dtype preserved from the input) +
// ONE measure column per reducer, named off its `as`. Key dtypes are carried
// through (a Cat key stays Cat, an I32 bin index stays I32) so a downstream encode
// can bind them; every measure column is F32 (the GPU-native numeric type), except
// Count which is I32. Row order is GROUP-DISCOVERY order (first appearance of each
// key tuple) — deterministic for a given input, and stable under append for the
// already-seen groups (new groups append at the end).
//
// Fail-fast typing (inferSchema): every groupBy column must exist; every non-count
// reducer's field must exist and be numeric; `as` names must be unique and must not
// collide with a key column.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {

// The reducer kind applied to a measure field.
enum class AggOp { Count, Sum, Mean, Min, Max, Median };

// One measure: a reducer over a field, named `as` in the output. For Count the
// field is ignored (it counts rows).
struct AggMeasure {
  AggOp op{AggOp::Count};
  std::string field;  // ignored for Count
  std::string as;
};

class AggregateTransform : public TransformNode {
 public:
  AggregateTransform(std::vector<std::string> groupBy,
                     std::vector<AggMeasure> measures)
      : groupBy_(std::move(groupBy)), measures_(std::move(measures)) {}

  const char* op() const override { return "aggregate"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::vector<std::string> groupBy_;
  std::vector<AggMeasure> measures_;
};

}  // namespace dc
