// ENC-625 (B3) — Predicate model: a SignalStore selection compiled to a per-row
// test over a live table, and materialized as a boolean column (RESEARCH §6.1).
//
// WHAT THIS IS
// ------------
// SignalStore (B1) holds a selection and answers matchesRow(id)/matchesValue(v)
// in the abstract. SelectionPredicate binds that to a concrete table: it reads
// the table's RowIdentity column (row-based selections — point/multi/hover) AND
// an optional value field (interval/brush selections), combining both parts so
// ANY selection type produces one per-row boolean. An empty selection / empty
// part imposes no constraint (everything passes) — a cleared selection filters
// nothing.
//
// materialize() writes that boolean as an i32 (0/1) column into a ColumnStore —
// the §6.1 predicate the selection-filter transform (B4) consumes and the
// conditionalColorColumn() helper below turns into per-row color.
//
// conditionalColorColumn() is the §6.1 conditional ENCODING for color
// (color:{condition:{test:isSelected,value:sel},value:unsel}) as data: it maps the
// boolean column to per-row packed RGBA8, producing exactly the i32 color column
// the ENC-608 per-instance color path (Encoding::setColorField) already consumes —
// so conditional color needs ZERO change to the encode pass.
//
// Pure `dc` (C++17, no GPU).
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ids/Id.hpp"
#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/ColumnStore.hpp"

#include <cstdint>
#include <string>

namespace dc {

class SelectionPredicate {
 public:
  // signalId   : the selection signal in `signals`.
  // rowIdColumn: the table's RowIdentity i32 column (row-based parts test it);
  //              empty => the row-based part imposes no constraint.
  // valueField : an f32 column the interval/value parts test; empty => the
  //              value-based part imposes no constraint.
  SelectionPredicate(const SignalStore& signals, Id signalId,
                     std::string rowIdColumn = {}, std::string valueField = {})
      : signals_(signals),
        signalId_(signalId),
        rowIdColumn_(std::move(rowIdColumn)),
        valueField_(std::move(valueField)) {}

  // Does row `i` of table `tableId` satisfy the selection? Combines the row-id
  // part (matchesRow on the rowId column) AND the value part (matchesValue on the
  // value field). Either part absent / empty => that part is unconstrained (true),
  // so any selection type works and an empty selection matches every row.
  bool testRow(std::size_t i, const TableStore& tables, Id tableId,
               const BufferByteSource& src) const;

  // Materialize the per-row boolean (i32, 1=selected / 0=not) for ALL rows of the
  // table into `out` under (outNode, colName). Returns the row count written.
  std::size_t materialize(const TableStore& tables, Id tableId,
                          const BufferByteSource& src, ColumnStore& out,
                          Id outNode, const std::string& colName) const;

 private:
  const SignalStore& signals_;
  Id signalId_{kInvalidId};
  std::string rowIdColumn_;
  std::string valueField_;
};

// Conditional color encoding (RESEARCH §6.1) as a column: map a boolean column
// (1 => selected) to per-row packed RGBA8 (selected vs unselected), writing the
// i32 color column that Encoding::setColorField (ENC-608 per-instance color)
// consumes. RGBA8 byte order is R,G,B,A in the low..high bytes (the ENC-608
// convention). Returns rows written; 0 if the boolean column is missing.
std::size_t conditionalColorColumn(const ColumnStore& boolColumns, Id boolNode,
                                   const std::string& boolCol,
                                   std::uint32_t selectedRgba8,
                                   std::uint32_t unselectedRgba8,
                                   ColumnStore& out, Id outNode,
                                   const std::string& outCol);

}  // namespace dc
