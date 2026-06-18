// ENC-625 (B3) — SelectionPredicate + conditionalColorColumn. See header.
#include "dc/interaction/SelectionPredicate.hpp"

namespace dc {

bool SelectionPredicate::testRow(std::size_t i, const TableStore& tables,
                                 Id tableId, const BufferByteSource& src) const {
  // Row-id part (point/multi/hover): read the RowIdentity i32 column.
  if (!rowIdColumn_.empty()) {
    ColumnView<std::int32_t> col = tables.viewI32(tableId, rowIdColumn_, src);
    if (col.valid() && i < col.count) {
      // RowIdentity ids are non-negative i32; widen to the Id key space.
      const Id rowId = static_cast<Id>(static_cast<std::uint32_t>(col[i]));
      if (!signals_.matchesRow(signalId_, rowId)) return false;
    }
  }
  // Value part (interval/brush/multi-intervals): read the f32 value field.
  if (!valueField_.empty()) {
    ColumnView<float> col = tables.viewF32(tableId, valueField_, src);
    if (col.valid() && i < col.count) {
      if (!signals_.matchesValue(signalId_, static_cast<double>(col[i])))
        return false;
    }
  }
  // Both parts unconstrained (or absent) => the row passes.
  return true;
}

std::size_t SelectionPredicate::materialize(const TableStore& tables, Id tableId,
                                            const BufferByteSource& src,
                                            ColumnStore& out, Id outNode,
                                            const std::string& colName) const {
  const std::size_t n = tables.rowCount(tableId, src);
  out.allocColumn(outNode, colName, DType::I32, n);
  for (std::size_t i = 0; i < n; ++i) {
    out.setI32(outNode, colName, i, testRow(i, tables, tableId, src) ? 1 : 0);
  }
  return n;
}

std::size_t conditionalColorColumn(const ColumnStore& boolColumns, Id boolNode,
                                   const std::string& boolCol,
                                   std::uint32_t selectedRgba8,
                                   std::uint32_t unselectedRgba8,
                                   ColumnStore& out, Id outNode,
                                   const std::string& outCol) {
  ColumnView<std::int32_t> bv = boolColumns.viewI32(boolNode, boolCol);
  if (!bv.valid()) return 0;
  const std::size_t n = bv.count;
  out.allocColumn(outNode, outCol, DType::I32, n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint32_t rgba = bv[i] != 0 ? selectedRgba8 : unselectedRgba8;
    out.setI32(outNode, outCol, i, static_cast<std::int32_t>(rgba));
  }
  return n;
}

}  // namespace dc
