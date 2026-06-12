// ENC-616a — `formula` transform (RESEARCH §5.1, core tier).
//
// A per-row expression -> a new derived f32 column. The output schema is the input
// schema PLUS one new column `as` (f32). All input columns are copied through
// unchanged (preserving dtype, including timestamp as i64 — never f32) so a
// downstream node / the encode pass can still reference them by name. The new
// column is computed per row by the Expression DSL (Expr.hpp): every input column
// is a binding (read as a double on the CPU path), and the result is stored as f32.
//
// Fail-fast typing (inferSchema, data-free): the expression is COMPILED against the
// input schema; an unknown column, a type error, or a non-numeric result is
// rejected before any row runs. `as` must not collide with an existing column.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>

namespace dc {

class FormulaTransform : public TransformNode {
 public:
  FormulaTransform(std::string expr, std::string as)
      : expr_(std::move(expr)), as_(std::move(as)) {}

  const char* op() const override { return "formula"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::string expr_;  // the per-row expression source
  std::string as_;    // the new column's name
};

}  // namespace dc
