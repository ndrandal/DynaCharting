// ENC-626 (B4) — `selectionFilter` transform (RESEARCH §5/§6, the feedback edge).
//
// A transform node whose predicate is an interaction SIGNAL (a SignalStore
// selection) rather than a data expression. Two modes:
//   * Filter      — DROP rows the selection rejects (output schema == input, row
//                   count shrinks; mirrors FilterTransform's compaction).
//   * Deemphasize — KEEP every row, APPEND a boolean `selected` i32 column (1/0)
//                   for downstream conditional encoding (dim the unselected).
//
// The predicate combines a row-id part (matchesRow over `rowIdColumn`, for
// point/multi/hover selections) with a value part (matchesValue over `valueField`,
// for interval/brush selections); either name empty => that part is unconstrained.
// An empty selection matches every row (filters nothing).
//
// Wire the node to its signal via TransformDag::addSignalDependency(node, signalId)
// (ENC-624) so a selection change recomputes it through the existing drain()/topo
// path. The SignalStore is BORROWED (must outlive the node) and read live at
// evaluate() — the node is otherwise pure.
#pragma once

#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/Transform.hpp"

#include <string>

namespace dc {

class SelectionFilterTransform : public TransformNode {
 public:
  enum class Mode { Filter, Deemphasize };

  SelectionFilterTransform(const SignalStore* signals, Id signalId,
                           std::string rowIdColumn = {},
                           std::string valueField = {}, Mode mode = Mode::Filter,
                           std::string selectedColumn = "selected")
      : signals_(signals),
        signalId_(signalId),
        rowIdColumn_(std::move(rowIdColumn)),
        valueField_(std::move(valueField)),
        mode_(mode),
        selectedColumn_(std::move(selectedColumn)) {}

  const char* op() const override { return "selectionFilter"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  // The per-row selection test, read through the resolver (works at any DAG
  // position). A null SignalStore or empty selection => true (matches all).
  bool rowMatches(const ColumnResolver& res, std::size_t i) const;

  const SignalStore* signals_{nullptr};
  Id signalId_{kInvalidId};
  std::string rowIdColumn_;
  std::string valueField_;
  Mode mode_{Mode::Filter};
  std::string selectedColumn_;
};

}  // namespace dc
