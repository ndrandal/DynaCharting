// ENC-638 (F1) — ViewSession: cross-view linking (RESEARCH §7.3/§7.4).
//
// The session/app layer that lets ONE selection span MULTIPLE coordinated views
// (linked brushing / cross-filtering). It owns a shared SignalStore and a set of
// views; a gesture from any view mutates the shared signal, whose dirty mark fans
// (SignalStore::addGraph) to EVERY view's TransformDag, and then every view
// re-renders through its InteractionRuntime. This is composition of views — NOT a
// transform and NOT in the manifest (a manifest is one view's pure feed->frames).
//
// Each view's selectionFilter / conditional node must register the shared signal
// id (TransformDag::addSignalDependency). Views link by a SHARED field id (an
// interval brush over the same column) or by row id (point/multi over the global
// RowIdentity) — both already supported by SignalStore's predicates.
//
// Pure `dc` (no GPU): refreshAll() produces each view's compiled marks; the host
// wires them to its scenes/renderers.
#pragma once

#include "dc/interaction/InteractionRuntime.hpp"
#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/TransformDag.hpp"

#include <cstdint>
#include <vector>

namespace dc {

using ViewId = std::uint32_t;

class ViewSession {
 public:
  // The selection state shared across all views.
  SignalStore& signals() { return signals_; }
  const SignalStore& signals() const { return signals_; }

  // Register a view (its DAG + runtime, both borrowed). The DAG's ReactiveGraph is
  // subscribed to the shared SignalStore so a shared-signal mutation dirties this
  // view's dependents too. Returns the view's id (its registration index).
  ViewId addView(TransformDag& dag, InteractionRuntime& runtime) {
    signals_.addGraph(&dag.reactive());
    views_.push_back(View{&dag, &runtime});
    return static_cast<ViewId>(views_.size() - 1);
  }

  // Drop a view (unsubscribe its graph). Existing ViewIds past `id` are unchanged
  // (slot is tombstoned, not compacted) so ids stay stable.
  void removeView(ViewId id) {
    if (id >= views_.size() || !views_[id].dag) return;
    signals_.removeGraph(&views_[id].dag->reactive());
    views_[id] = View{};
  }

  // Mutate a shared signal (a gesture from any view) and refresh every view. The
  // signal's dirty mark already fanned to all view DAGs; refreshAll re-evaluates +
  // re-encodes each. Returns false if the signal is undefined.
  bool setSignal(Id signalId, SignalValue value) {
    const bool ok = signals_.set(signalId, std::move(value));
    refreshAll();
    return ok;
  }

  // Define a shared signal across the session (then refreshAll so views pick it up).
  void defineSignal(Id signalId, SignalValue value) {
    signals_.define(signalId, std::move(value));
    refreshAll();
  }

  // Re-render every view (each runtime re-evaluates its DAG + re-encodes). Correct
  // and simple — refreshes all views, not only the ones whose dependents changed.
  void refreshAll() {
    for (auto& v : views_)
      if (v.runtime) v.runtime->refresh();
  }

  std::size_t viewCount() const {
    std::size_t n = 0;
    for (const auto& v : views_)
      if (v.dag) ++n;
    return n;
  }

 private:
  struct View {
    TransformDag* dag{nullptr};
    InteractionRuntime* runtime{nullptr};
  };
  SignalStore signals_;
  std::vector<View> views_;
};

}  // namespace dc
