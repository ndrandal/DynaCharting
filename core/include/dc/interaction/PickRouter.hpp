// ENC-629 (C3) — route a per-instance pick hit into the SignalStore.
//
// The bridge from the GPU pick result (DawnPickBackend::renderPick ->
// DawnPickResult{ rowId }) to the interaction signals: a click sets a point
// selection, a shift/ctrl click toggles a row into a multi-selection, and a hover
// updates the hover signal. Each mutation goes through SignalStore::set / clear,
// which marks the signal dirty so the bound TransformDag nodes (selectionFilter /
// conditional color, ENC-624..626) recompute on the next evaluate() — closing the
// pick -> selection -> re-render loop (the InteractionRuntime / ViewSession step).
//
// Pure `dc` (no GPU): it takes the decoded rowId (the int32 DawnPickResult::rowId,
// where < 0 == background / no hit), NOT the GPU result struct, so it is testable
// headlessly. The GPU caller just forwards result.rowId.
//
// ROW-ID 0 CAVEAT: durable row ids are dense from 0 (ENC-594, "row N gets id N"),
// but PointSelection uses kInvalidId (0) as its "empty" sentinel — so a POINT
// selection of row 0 reads as empty (matches everything). Hover (a separate
// `active` flag) and multi-select (an explicit row list) do NOT have this
// ambiguity. This is documented rather than worked around: an offset would break
// SignalStore::matchesRow, which compares against the real i32 row-id column.
#pragma once

#include "dc/interaction/SignalStore.hpp"
#include "dc/ids/Id.hpp"

#include <cstdint>

namespace dc {

class PickRouter {
 public:
  PickRouter() = default;
  explicit PickRouter(SignalStore* signals) : signals_(signals) {}

  void setSignals(SignalStore* signals) { signals_ = signals; }
  SignalStore* signals() const { return signals_; }

  // Which signal receives each gesture. The caller defines the signal (with the
  // matching alternative: PointSelection/MultiSelection for selection, HoverState
  // for hover) before routing. kInvalidId disables that gesture.
  void setSelectionSignal(Id sig) { selectionSignal_ = sig; }
  void setHoverSignal(Id sig) { hoverSignal_ = sig; }
  Id selectionSignal() const { return selectionSignal_; }
  Id hoverSignal() const { return hoverSignal_; }

  // A click on `rowId` (< 0 == background): replace the selection signal with that
  // row (PointSelection), or clear it on a background click. Returns true if the
  // selection signal was mutated (false if no selection signal is configured or it
  // is undefined in the store).
  bool onClick(std::int32_t rowId);

  // A shift/ctrl click on `rowId`: toggle the row in the selection signal's
  // MultiSelection row set (add if absent, remove if present), preserving any
  // existing intervals. A background click (< 0) is a no-op that keeps the current
  // set. Returns true if the signal was mutated.
  bool onToggleClick(std::int32_t rowId);

  // Hover over `rowId` (< 0 == none): activate the hover signal on that row, or
  // deactivate it. Returns true if the hover signal was mutated.
  bool onHover(std::int32_t rowId);

 private:
  SignalStore* signals_{nullptr};
  Id selectionSignal_{kInvalidId};
  Id hoverSignal_{kInvalidId};
};

}  // namespace dc
