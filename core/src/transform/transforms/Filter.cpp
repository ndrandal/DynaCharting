// ENC-616a — `filter` transform implementation. See Filter.hpp.
#include "dc/transform/transforms/Filter.hpp"

#include "TransformUtil.hpp"

#include <vector>

namespace dc {

SchemaResult FilterTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  CompileResult c = compileExpr(predicate_, tfutil::bindingsFor(input));
  if (!c.ok) {
    r.error = c.error;
    return r;
  }
  if (c.expr.resultKind != ExprKind::Bool) {
    r.error = "filter predicate must be boolean";
    return r;
  }
  // Filter preserves the schema; only the row count shrinks.
  r.schema = input;
  r.ok = true;
  return r;
}

void FilterTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  CompileResult c = compileExpr(predicate_, tfutil::bindingsFor(in));
  if (!c.ok || !c.expr.valid()) return;

  // Pass 1: count survivors so output columns can be sized exactly (no dynamic
  // growth — mirrors the future GPU prefix-sum compaction, RESEARCH §5.1).
  std::vector<double> row;
  std::vector<std::size_t> survivors;
  survivors.reserve(rows);
  for (std::size_t i = 0; i < rows; ++i) {
    tfutil::buildRow(in, res, i, row);
    if (evalBool(*c.expr.root, row)) survivors.push_back(i);
  }

  // Allocate output columns to the survivor count, then compact.
  for (const auto& col : in.columns) {
    ctx.out->allocColumn(node, col.name, col.dtype, survivors.size());
  }
  for (std::size_t dst = 0; dst < survivors.size(); ++dst) {
    const std::size_t srcRow = survivors[dst];
    for (const auto& col : in.columns) {
      tfutil::copyCell(col, res, srcRow, *ctx.out, node, dst);
    }
  }
}

}  // namespace dc
