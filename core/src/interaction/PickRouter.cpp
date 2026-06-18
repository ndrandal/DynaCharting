// ENC-629 (C3) — PickRouter implementation. See header.
#include "dc/interaction/PickRouter.hpp"

#include <algorithm>
#include <utility>

namespace dc {

bool PickRouter::onClick(std::int32_t rowId) {
  if (!signals_ || selectionSignal_ == kInvalidId) return false;
  if (rowId < 0) return signals_->clear(selectionSignal_);  // background -> clear
  return signals_->set(selectionSignal_,
                       PointSelection{static_cast<Id>(rowId)});
}

bool PickRouter::onToggleClick(std::int32_t rowId) {
  if (!signals_ || selectionSignal_ == kInvalidId) return false;
  if (rowId < 0) return false;  // background: keep the current set (no-op)
  const Id id = static_cast<Id>(rowId);

  // Copy the current MultiSelection (preserving intervals + rows) if present;
  // start fresh otherwise (or if the signal currently holds another alternative).
  MultiSelection sel;
  if (const MultiSelection* cur =
          signals_->getAs<MultiSelection>(selectionSignal_)) {
    sel = *cur;
  }
  auto it = std::find(sel.rows.begin(), sel.rows.end(), id);
  if (it == sel.rows.end()) {
    sel.rows.push_back(id);  // add
  } else {
    sel.rows.erase(it);  // toggle off
  }
  return signals_->set(selectionSignal_, std::move(sel));
}

bool PickRouter::onHover(std::int32_t rowId) {
  if (!signals_ || hoverSignal_ == kInvalidId) return false;
  if (rowId < 0) return signals_->set(hoverSignal_, HoverState{});  // deactivate
  return signals_->set(hoverSignal_,
                       HoverState{static_cast<Id>(rowId), /*active*/ true});
}

}  // namespace dc
