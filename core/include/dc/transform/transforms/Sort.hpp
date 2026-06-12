// ENC-616b — `sort` / `rank` transform (RESEARCH §5.1, core tier; class-3 global).
//
// Orders the input rows by a KEY column (ascending or descending) and either:
//   * REORDER mode — emit the input columns permuted into sorted order (every
//     column copied through, dtype-preserving, in the new row order). One output
//     row per input row.
//   * RANK mode    — keep the input row order but ADD an i32 `<as>` column giving
//     each row's 0-based rank in the sort key's ordering (0 = the extreme per the
//     direction). Ties share the lower rank (dense-by-position competition rank:
//     equal keys get consecutive distinct ranks in a stable order — see below).
//
// STABILITY: the sort is STABLE (std::stable_sort) — rows with an equal key keep
// their original relative order, so the output is deterministic for a given input
// (the §6.1 replay-determinism requirement). Rank values follow the same stable
// order, so two rows with an equal key get distinct, consecutive ranks (the row
// that appeared first gets the lower rank).
//
// This is a class-3 GLOBAL transform (RESEARCH line 172): a full reorder on every
// recompute (the streaming scheduler runs it on a throttled/debounced cadence, not
// per-frame). It is correct under append — it simply re-sorts the larger input.
//
// Fail-fast typing (inferSchema): the key column must exist (any dtype that reads
// as a number — f32/i32/cat; a timestamp sorts on its i64 value). REORDER preserves
// the input schema; RANK adds one i32 column `<as>` which must not collide.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>

namespace dc {

class SortTransform : public TransformNode {
 public:
  // Reorder rows by `key`. ascending=true sorts low->high.
  static SortTransform reorder(std::string key, bool ascending = true) {
    SortTransform s(std::move(key), ascending);
    s.mode_ = Mode::Reorder;
    return s;
  }
  // Keep row order; add an i32 rank column `as` (0 = the first in sort order).
  static SortTransform rank(std::string key, std::string as,
                            bool ascending = true) {
    SortTransform s(std::move(key), ascending);
    s.mode_ = Mode::Rank;
    s.as_ = std::move(as);
    return s;
  }

  const char* op() const override { return "sort"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  enum class Mode { Reorder, Rank };
  SortTransform(std::string key, bool ascending)
      : key_(std::move(key)), ascending_(ascending) {}

  std::string key_;
  bool ascending_{true};
  Mode mode_{Mode::Reorder};
  std::string as_;  // Rank mode only
};

}  // namespace dc
