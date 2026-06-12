// ENC-616a — shared helpers for the expression-driven transforms (filter/formula).
// Internal (src-only): builds the Expr binding schema from a ColumnSchema and a
// per-row value vector, and copies an input row through to an output column.
#pragma once

#include "dc/transform/Expr.hpp"
#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {
namespace tfutil {

// Build Expr ColumnBindings from a transform's input schema: every column becomes
// a binding, slot = its index, kind = Num (f32/i32/cat are read as double on the
// CPU path; a timestamp is also exposed as Num for completeness but expressions
// should not do epoch-ms arithmetic — the typing allows it, the encode path never
// feeds it to the GPU). The slot order matches buildRow() below.
inline std::vector<ColumnBinding> bindingsFor(const ColumnSchema& schema) {
  std::vector<ColumnBinding> b;
  b.reserve(schema.columns.size());
  for (std::uint32_t i = 0; i < schema.columns.size(); ++i) {
    b.push_back({schema.columns[i].name, i, ExprKind::Num});
  }
  return b;
}

// Read row `i` of the input into the value vector the evaluator indexes by slot.
inline void buildRow(const ColumnSchema& schema, const ColumnResolver& in,
                     std::size_t i, std::vector<double>& row) {
  row.resize(schema.columns.size());
  for (std::size_t s = 0; s < schema.columns.size(); ++s) {
    row[s] = in.readNum(schema.columns[s].name, i);
  }
}

// Copy input column `col` at input row `srcRow` into output (node,col) at row
// `dstRow`, preserving dtype. Timestamp copies as i64 (no f32 trap). Cat copies as
// the raw u32 code (dictionary lives upstream; codes are stable).
inline void copyCell(const SchemaColumn& col, const ColumnResolver& in,
                     std::size_t srcRow, ColumnStore& out, Id node,
                     std::size_t dstRow) {
  switch (col.dtype) {
    case DType::F32:
      out.setF32(node, col.name, dstRow,
                 static_cast<float>(in.readNum(col.name, srcRow)));
      break;
    case DType::I32:
      out.setI32(node, col.name, dstRow,
                 static_cast<std::int32_t>(in.readNum(col.name, srcRow)));
      break;
    case DType::Cat:
      out.setCat(node, col.name, dstRow,
                 static_cast<std::uint32_t>(in.readNum(col.name, srcRow)));
      break;
    case DType::Timestamp:
      out.setTimestamp(node, col.name, dstRow, in.readTimestamp(col.name, srcRow));
      break;
  }
}

}  // namespace tfutil
}  // namespace dc
