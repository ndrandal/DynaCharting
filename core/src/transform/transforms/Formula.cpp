// ENC-616a — `formula` transform implementation. See Formula.hpp.
#include "dc/transform/transforms/Formula.hpp"

#include "TransformUtil.hpp"

namespace dc {

SchemaResult FormulaTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  if (input.has(as_)) {
    r.error = "formula output '" + as_ + "' collides with an input column";
    return r;
  }
  // Compile the expression against the input columns — fail fast on unknown
  // column / type error. A formula must produce a NUMERIC value (it lands in an
  // f32 column); a boolean-valued expression is rejected here.
  CompileResult c = compileExpr(expr_, tfutil::bindingsFor(input));
  if (!c.ok) {
    r.error = c.error;
    return r;
  }
  if (c.expr.resultKind != ExprKind::Num) {
    r.error = "formula expression must be numeric (got bool)";
    return r;
  }
  // Output = all input columns (passthrough) + the new f32 column.
  r.schema = input;
  r.schema.columns.push_back({as_, DType::F32});
  r.ok = true;
  return r;
}

void FormulaTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  // (Re)allocate every output column for this recompute: passthrough columns +
  // the derived one. A transform fully rewrites its outputs each pass.
  for (const auto& col : in.columns) {
    ctx.out->allocColumn(node, col.name, col.dtype, rows);
  }
  ctx.out->allocColumn(node, as_, DType::F32, rows);

  // Recompile against the input schema (cheap; the node owns the source string).
  // inferSchema already proved this compiles — so we can trust it here.
  CompileResult c = compileExpr(expr_, tfutil::bindingsFor(in));
  if (!c.ok || !c.expr.valid()) return;

  std::vector<double> row;
  for (std::size_t i = 0; i < rows; ++i) {
    // copy passthrough columns
    for (const auto& col : in.columns) {
      tfutil::copyCell(col, res, i, *ctx.out, node, i);
    }
    // compute the derived column
    tfutil::buildRow(in, res, i, row);
    double v = evalNum(*c.expr.root, row);
    ctx.out->setF32(node, as_, i, static_cast<float>(v));
  }
}

}  // namespace dc
