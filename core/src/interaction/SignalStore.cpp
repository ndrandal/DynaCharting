// ENC-623 (B1) — SignalStore implementation. See SignalStore.hpp.
#include "dc/interaction/SignalStore.hpp"

namespace dc {

void SignalStore::define(Id signalId, SignalValue value) {
  signals_[signalId] = std::move(value);
  notify(signalId);
}

const SignalValue* SignalStore::get(Id signalId) const {
  auto it = signals_.find(signalId);
  return it == signals_.end() ? nullptr : &it->second;
}

bool SignalStore::set(Id signalId, SignalValue value) {
  auto it = signals_.find(signalId);
  if (it == signals_.end()) return false;
  it->second = std::move(value);
  notify(signalId);
  return true;
}

bool SignalStore::clear(Id signalId) {
  auto it = signals_.find(signalId);
  if (it == signals_.end()) return false;
  // Reset selection-type signals to empty; leave camera/clock untouched.
  std::visit(
      [](auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PointSelection>) {
          v = PointSelection{};
        } else if constexpr (std::is_same_v<T, IntervalSelection>) {
          v = IntervalSelection{};
        } else if constexpr (std::is_same_v<T, MultiSelection>) {
          v = MultiSelection{};
        } else if constexpr (std::is_same_v<T, BrushRect>) {
          v = BrushRect{};
        } else if constexpr (std::is_same_v<T, HoverState>) {
          v = HoverState{};
        }
        // CameraState / TransitionClock: unchanged.
      },
      it->second);
  notify(signalId);
  return true;
}

void SignalStore::remove(Id signalId) { signals_.erase(signalId); }

bool SignalStore::isEmpty(Id signalId) const {
  const SignalValue* v = get(signalId);
  if (!v) return true;
  return std::visit(
      [](const auto& s) -> bool {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, PointSelection>) {
          return s.rowId == kInvalidId;
        } else if constexpr (std::is_same_v<T, IntervalSelection>) {
          return s.empty();
        } else if constexpr (std::is_same_v<T, MultiSelection>) {
          return s.intervals.empty() && s.rows.empty();
        } else if constexpr (std::is_same_v<T, BrushRect>) {
          return s.empty();
        } else if constexpr (std::is_same_v<T, HoverState>) {
          return !s.active;
        } else {
          // CameraState / TransitionClock never impose a selection constraint.
          return true;
        }
      },
      *v);
}

bool SignalStore::matchesRow(Id signalId, Id rowId) const {
  const SignalValue* v = get(signalId);
  if (!v) return true;
  return std::visit(
      [rowId](const auto& s) -> bool {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, PointSelection>) {
          return s.rowId == kInvalidId || s.rowId == rowId;
        } else if constexpr (std::is_same_v<T, MultiSelection>) {
          if (s.rows.empty()) return true;  // no row-based constraint
          for (Id id : s.rows)
            if (id == rowId) return true;
          return false;
        } else if constexpr (std::is_same_v<T, HoverState>) {
          return !s.active || s.rowId == rowId;
        } else {
          // Interval / Brush judge by value; camera/clock never constrain.
          return true;
        }
      },
      *v);
}

bool SignalStore::matchesValue(Id signalId, double value) const {
  const SignalValue* v = get(signalId);
  if (!v) return true;
  return std::visit(
      [value](const auto& s) -> bool {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, IntervalSelection>) {
          return s.empty() || s.contains(value);
        } else if constexpr (std::is_same_v<T, MultiSelection>) {
          if (s.intervals.empty()) return true;  // no value-based constraint
          for (const auto& iv : s.intervals)
            if (iv.contains(value)) return true;
          return false;
        } else {
          // Point / Brush / Hover judge by row; camera/clock never constrain.
          return true;
        }
      },
      *v);
}

}  // namespace dc
