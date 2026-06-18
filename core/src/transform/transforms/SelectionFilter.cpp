// ENC-626 (B4) — `selectionFilter` transform implementation. See SelectionFilter.hpp.
#include "dc/transform/transforms/SelectionFilter.hpp"

#include "TransformUtil.hpp"

#include <cstdint>
#include <vector>

namespace dc {

SchemaResult SelectionFilterTransform::inferSchema(
    const ColumnSchema& input) const {
  SchemaResult r;
  // Fail fast: referenced columns must exist (data-free check, §6.1 check-2).
  if (!rowIdColumn_.empty() && !input.has(rowIdColumn_)) {
    r.error = "selectionFilter: rowId column '" + rowIdColumn_ + "' not in input";
    return r;
  }
  if (!valueField_.empty() && !input.has(valueField_)) {
    r.error = "selectionFilter: value field '" + valueField_ + "' not in input";
    return r;
  }
  if (mode_ == Mode::Filter) {
    // Drop-rows: schema preserved, only the row count shrinks.
    r.schema = input;
    r.ok = true;
    return r;
  }
  // Deemphasize: pass all rows + append a boolean `selected` i32 column.
  if (input.has(selectedColumn_)) {
    r.error = "selectionFilter: output column '" + selectedColumn_ +
              "' collides with an input column";
    return r;
  }
  r.schema = input;
  r.schema.columns.push_back(SchemaColumn{selectedColumn_, DType::I32});
  r.ok = true;
  return r;
}

bool SelectionFilterTransform::rowMatches(const ColumnResolver& res,
                                          std::size_t i) const {
  if (!signals_) return true;
  if (!rowIdColumn_.empty()) {
    // RowIdentity ids are non-negative i32; readNum widens losslessly.
    const Id rowId = static_cast<Id>(
        static_cast<std::uint32_t>(static_cast<std::int32_t>(
            res.readNum(rowIdColumn_, i))));
    if (!signals_->matchesRow(signalId_, rowId)) return false;
  }
  if (!valueField_.empty()) {
    if (!signals_->matchesValue(signalId_, res.readNum(valueField_, i)))
      return false;
  }
  return true;
}

void SelectionFilterTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  if (mode_ == Mode::Filter) {
    // Pass 1: survivors (sized-exactly output, no dynamic growth).
    std::vector<std::size_t> survivors;
    survivors.reserve(rows);
    for (std::size_t i = 0; i < rows; ++i)
      if (rowMatches(res, i)) survivors.push_back(i);

    for (const auto& col : in.columns)
      ctx.out->allocColumn(node, col.name, col.dtype, survivors.size());
    for (std::size_t dst = 0; dst < survivors.size(); ++dst) {
      const std::size_t srcRow = survivors[dst];
      for (const auto& col : in.columns)
        tfutil::copyCell(col, res, srcRow, *ctx.out, node, dst);
    }
    return;
  }

  // Deemphasize: copy all rows through, then write the boolean `selected` column.
  for (const auto& col : in.columns)
    ctx.out->allocColumn(node, col.name, col.dtype, rows);
  ctx.out->allocColumn(node, selectedColumn_, DType::I32, rows);
  for (std::size_t i = 0; i < rows; ++i) {
    for (const auto& col : in.columns)
      tfutil::copyCell(col, res, i, *ctx.out, node, i);
    ctx.out->setI32(node, selectedColumn_, i, rowMatches(res, i) ? 1 : 0);
  }
}

}  // namespace dc
