// ENC-623 (B1) — SignalStore: typed mutable interaction signals.
//
// WHAT THIS IS
// ------------
// The interaction layer's mutable runtime state — the §6.1/§7.3 "signals":
// selections (point / interval / multi), hover, brush rect, camera, and a
// transition clock. Each signal is named by an Id in the SAME key space that
// ReactiveGraph (ENC-595) uses for signalInput()/markSignalDirty(), so a mutation
// here flows back into the transform DAG through the EXISTING reactive path with
// no new plumbing: bind a ReactiveGraph, and every set/clear marks the signal
// dirty, scheduling the dependents that registered on signalInput(signalId).
//
// Selection-type signals expose a PREDICATE — does a row id or a scalar value
// satisfy the selection? — the seed of the §6.1 conditional-encoding (B3) and
// selection-filter transform (B4). matchesRow() tests only the row-id-based part
// of a selection and matchesValue() only the value/interval-based part; an EMPTY
// constraint always matches (a cleared selection filters nothing). B3 combines
// these against real table columns; B1 keeps them pure and unit-testable.
//
// Pure-`dc` control-plane primitive: C++17, no Scene, no GPU. It owns the signal
// values; it does NOT own ReactiveGraph edges (the caller registers/removes those).
#pragma once

#include "dc/data/ReactiveGraph.hpp"
#include "dc/ids/Id.hpp"

#include <unordered_map>
#include <variant>
#include <vector>

namespace dc {

// ---- signal value types ---------------------------------------------------

// A single durable row id (RowIdentity, ENC-594) is selected. rowId == kInvalidId
// means "empty" — no row-based constraint.
struct PointSelection {
  Id rowId{kInvalidId};
};

// A closed interval [lo, hi] over one named field's scalar value. field ==
// kInvalidId means "empty" — no value-based constraint.
struct IntervalSelection {
  Id field{kInvalidId};
  double lo{0.0};
  double hi{0.0};
  bool contains(double v) const { return v >= lo && v <= hi; }
  bool empty() const { return field == kInvalidId; }
};

// A disjunction (OR): any of `intervals` over a field, and/or any explicitly
// selected row in `rows` (shift/ctrl-click unions). Empty when both are empty.
struct MultiSelection {
  std::vector<IntervalSelection> intervals;
  std::vector<Id> rows;
};

// A 2-D brush rectangle in DATA space. Degenerate (zero-area) == empty. Field
// mapping (which column is x / y) resolves in B3; B1 carries the geometry.
struct BrushRect {
  double x0{0}, y0{0}, x1{0}, y1{0};
  double minX() const { return x0 < x1 ? x0 : x1; }
  double maxX() const { return x0 > x1 ? x0 : x1; }
  double minY() const { return y0 < y1 ? y0 : y1; }
  double maxY() const { return y0 > y1 ? y0 : y1; }
  bool contains(double x, double y) const {
    return x >= minX() && x <= maxX() && y >= minY() && y <= maxY();
  }
  bool empty() const { return x0 == x1 && y0 == y1; }
};

// The hovered row (RowIdentity), or inactive.
struct HoverState {
  Id rowId{kInvalidId};
  bool active{false};
};

// Pan/zoom viewport state (never a selection constraint).
struct CameraState {
  double panX{0}, panY{0}, zoom{1.0};
};

// A transition clock in [0,1] for the data-bound animation layer (E).
struct TransitionClock {
  float t{0.0f};
};

using SignalValue =
    std::variant<PointSelection, IntervalSelection, MultiSelection, BrushRect,
                 HoverState, CameraState, TransitionClock>;

// ---------------------------------------------------------------------------
// SignalStore
// ---------------------------------------------------------------------------
class SignalStore {
 public:
  SignalStore() = default;
  explicit SignalStore(ReactiveGraph* graph) {
    if (graph) graphs_.push_back(graph);
  }

  // Bind exactly one ReactiveGraph (clears any existing subscribers). nullptr
  // detaches all. Back-compat sugar over the multi-graph subscriber set below.
  void setGraph(ReactiveGraph* graph) {
    graphs_.clear();
    if (graph) graphs_.push_back(graph);
  }
  // First subscribed graph (or nullptr) — back-compat accessor.
  ReactiveGraph* graph() const { return graphs_.empty() ? nullptr : graphs_.front(); }

  // ENC-638 (F1): cross-view fan-out. Subscribe an ADDITIONAL ReactiveGraph so a
  // signal mutation dirties dependents in EVERY subscribed view's DAG — the basis
  // for one shared selection driving multiple coordinated views (ViewSession).
  // Idempotent; nullptr ignored.
  void addGraph(ReactiveGraph* graph) {
    if (!graph) return;
    for (auto* g : graphs_)
      if (g == graph) return;
    graphs_.push_back(graph);
  }
  void removeGraph(ReactiveGraph* graph) {
    for (std::size_t i = 0; i < graphs_.size(); ++i) {
      if (graphs_[i] == graph) {
        graphs_.erase(graphs_.begin() + static_cast<std::ptrdiff_t>(i));
        return;
      }
    }
  }
  std::size_t graphCount() const { return graphs_.size(); }

  // Define (or redefine) a signal with an initial value. Marks it dirty.
  void define(Id signalId, SignalValue value);

  bool has(Id signalId) const { return signals_.count(signalId) > 0; }
  std::size_t size() const { return signals_.size(); }

  // Current value, or nullptr if undefined.
  const SignalValue* get(Id signalId) const;

  // Typed view: nullptr if undefined or the wrong alternative.
  template <class T>
  const T* getAs(Id signalId) const {
    const SignalValue* v = get(signalId);
    return v ? std::get_if<T>(v) : nullptr;
  }

  // Set a new value and mark the signal dirty. Returns false if undefined.
  bool set(Id signalId, SignalValue value);

  // Reset a selection-type signal to its empty state (point -> kInvalidId,
  // interval/multi -> empty, brush -> zero rect, hover -> inactive) and mark
  // dirty. Camera/clock are left unchanged. No-op (false) if undefined.
  bool clear(Id signalId);

  // Erase the signal entirely. Does NOT touch ReactiveGraph edges (caller-owned).
  void remove(Id signalId);

  // ---- predicates ----------------------------------------------------------

  // Is there NO active constraint on this signal (so it filters nothing)? True
  // for an undefined signal, an empty selection, and always for camera/clock.
  bool isEmpty(Id signalId) const;

  // Does row `rowId` satisfy the row-id part of the selection? Empty/none -> true.
  // (Interval/value-only constraints are not judged here — see matchesValue.)
  bool matchesRow(Id signalId, Id rowId) const;

  // Does scalar `value` satisfy the value/interval part of the selection?
  // Empty/none -> true. (Row-id-only constraints are not judged here.)
  bool matchesValue(Id signalId, double value) const;

 private:
  std::vector<ReactiveGraph*> graphs_;
  std::unordered_map<Id, SignalValue> signals_;

  // Fan the dirty mark to every subscribed view DAG (one for a single view, N for
  // a cross-view ViewSession).
  void notify(Id signalId) {
    for (auto* g : graphs_)
      if (g) g->markSignalDirty(signalId);
  }
};

}  // namespace dc
