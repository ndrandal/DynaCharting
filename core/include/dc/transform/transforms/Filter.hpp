// ENC-616a — `filter` transform (RESEARCH §5.1, core tier).
//
// A predicate expression -> selects/compacts rows. The output schema EQUALS the
// input schema (same columns + dtypes); only the row COUNT shrinks: rows for which
// the predicate is false are dropped and the survivors compacted to a dense range.
// Every input column is copied through (dtype-preserving, timestamp as i64).
//
// Fail-fast typing (inferSchema, data-free): the predicate is COMPILED against the
// input schema and MUST be boolean-valued; an unknown column, a type error, or a
// numeric (non-bool) predicate is rejected before any row runs.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>

namespace dc {

class FilterTransform : public TransformNode {
 public:
  explicit FilterTransform(std::string predicate)
      : predicate_(std::move(predicate)) {}

  const char* op() const override { return "filter"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::string predicate_;
};

}  // namespace dc
