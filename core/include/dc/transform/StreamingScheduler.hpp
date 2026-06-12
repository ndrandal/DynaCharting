// ENC-616e — Streaming scheduler: per-frame DAG recompute driven by each node's
// STREAMING CLASS, decoupling expensive globals from the data tick (RESEARCH §3
// "Streaming scheduler" + §5.1 streaming-class column). The final piece of the
// CPU/WASM transform DAG (Epic ENC-616).
//
// THE PROBLEM (RESEARCH §3)
// -------------------------
// A naive DAG re-runs every dirty node on every data tick. But the transforms have
// very different streaming costs (RESEARCH §5.1):
//   * class-1 — O(Δ) incremental, cheap: running min/max, bin increment, append
//     marks, EMA, count/sum/mean/min/max aggregate, stack-zero. Fine every frame.
//   * class-2 — window/hop boundary: rolling fixed-window aggregate, STFT hop,
//     M4/min-max viewport sample, median aggregate. Only needs to recompute when a
//     window or hop boundary ADVANCES, not on every intra-window tick.
//   * class-3 — GLOBAL recompute: sort/rank, treemap, force layout, global KDE,
//     stack-normalize (100% stack). Expensive (a full reorder / global pass) — must
//     run on a THROTTLED / debounced cadence DECOUPLED from the data tick, so a
//     treemap or sort does not re-run on every append.
//   * class-4 — needs a manifest-DECLARED baseline policy (percent-of-whole, global
//     z-score, streamgraph wiggle). Treated as class-3 cadence here, but flagged so
//     a caller knows a baseline policy is required (the policy itself lives on the
//     transform — Stack's BaselinePolicy — this scheduler only governs WHEN).
//
// THE SCHEDULER
// -------------
// Wraps a built TransformDag. Each transform node is assigned a StreamClass + a
// small CadencePolicy descriptor (throttle interval for class-3; window/hop for
// class-2). tick(frameTime, dataChanged) installs a per-pass GATE on the DAG that
// decides which DIRTY nodes are DUE:
//   * class-1 -> due every frame (if dirty).
//   * class-2 -> due only when its window/hop boundary advanced since it last ran.
//   * class-3/4 -> due only when its throttle interval has elapsed since it last
//     ran (debounced: rapid ticks coalesce into at most one run per interval).
// The DAG HOLDS (defers, keeps dirty) any node the gate says is not due — so a
// throttled global does not run every tick, but DOES run once its interval passes,
// and its still-pending dirtiness is honored then. Clean (non-dirty) nodes are
// never asked about — dirty-gating from the ReactiveGraph/ChartSession is intact.
//
// FRAME TIME is injected (any monotonic unit — ms, frame index, samples), so tests
// drive it from a fake clock. The scheduler holds NO wall clock of its own.
//
// Pure `dc` (C++17, no GPU). Borrows a built TransformDag; assigns classes/cadence
// over its existing nodes. Adds NO transforms and changes NO transform internals.
#pragma once

#include "dc/transform/TransformDag.hpp"

#include <cstdint>
#include <unordered_map>

namespace dc {

// The four streaming classes (RESEARCH §5.1). The integer values match the class
// numbers in the research for readability in diagnostics/tests.
enum class StreamClass : std::uint8_t {
  Incremental = 1,  // class-1: O(Δ) per frame — run every dirty frame.
  Windowed = 2,     // class-2: recompute on a window/hop boundary crossing.
  Global = 3,       // class-3: throttled/debounced global recompute.
  Baseline = 4,     // class-4: needs a declared baseline policy; cadenced as global.
};

// Per-node cadence/policy descriptor. Only the fields relevant to the node's class
// are read:
//   * Windowed   -> hop (boundary stride): a node is due when frameTime has crossed
//                   into a new hop bucket (floor(t/hop)) since it last ran.
//   * Global/    -> throttleInterval: a node is due at most once per interval; a run
//     Baseline      at time t blocks the next run until t + throttleInterval.
// Times are in the SAME injected unit as tick()'s frameTime (ms / frame / sample).
struct CadencePolicy {
  StreamClass cls{StreamClass::Incremental};
  std::int64_t hop{1};               // class-2 hop/window stride (>=1).
  std::int64_t throttleInterval{0};  // class-3/4 min gap between runs (>=0).
};

// ---------------------------------------------------------------------------
// StreamingScheduler — cadence orchestration over a built TransformDag.
// ---------------------------------------------------------------------------
class StreamingScheduler {
 public:
  // Borrow the DAG this scheduler drives. The DAG must outlive the scheduler and
  // should be build()-finalized before the first tick().
  explicit StreamingScheduler(TransformDag& dag) : dag_(dag) {}

  // Assign (or reassign) a node's streaming class + cadence. A node with no
  // assignment defaults to class-1 (Incremental: run every dirty frame) — the safe,
  // always-fresh default. `nodeId` should be a transform node in the DAG.
  void setNodeClass(NodeId nodeId, const CadencePolicy& policy) {
    policies_[nodeId] = policy;
  }
  // Convenience: class-1 (every frame).
  void setIncremental(NodeId nodeId) {
    setNodeClass(nodeId, {StreamClass::Incremental, 1, 0});
  }
  // Convenience: class-2 with hop stride `hop` (>=1).
  void setWindowed(NodeId nodeId, std::int64_t hop) {
    setNodeClass(nodeId, {StreamClass::Windowed, hop < 1 ? 1 : hop, 0});
  }
  // Convenience: class-3 throttled global, min `interval` between runs (>=0).
  void setGlobal(NodeId nodeId, std::int64_t interval) {
    setNodeClass(nodeId, {StreamClass::Global, 1, interval < 0 ? 0 : interval});
  }
  // Convenience: class-4 baseline-policy node, cadenced as a throttled global.
  void setBaseline(NodeId nodeId, std::int64_t interval) {
    setNodeClass(nodeId, {StreamClass::Baseline, 1, interval < 0 ? 0 : interval});
  }

  // The policy assigned to a node (the default class-1 policy if unassigned).
  CadencePolicy policyOf(NodeId nodeId) const {
    auto it = policies_.find(nodeId);
    return it == policies_.end() ? CadencePolicy{} : it->second;
  }

  // Drive one frame. `frameTime` is the injected monotonic time (any unit, must be
  // non-decreasing across calls). `dataChanged` lets the caller fold in the data
  // tick: when true, the source set the caller already marked dirty
  // (markTouchedBuffers / markTableDirty before this tick) flows through; the flag
  // is advisory and does not itself dirty anything. Installs the per-class gate on
  // the DAG, runs evaluate(), then clears the gate. Returns the node ids that
  // recomputed this tick (the ones that were dirty AND due).
  std::vector<NodeId> tick(std::int64_t frameTime, bool dataChanged = true);

  // ----- introspection (tests / diagnostics) --------------------------------

  // The frame time at which `node` last actually RAN (its cadence anchor). Returns
  // the sentinel `kNeverRan` if the node has not run under this scheduler yet.
  static constexpr std::int64_t kNeverRan = INT64_MIN;
  std::int64_t lastRunTime(NodeId node) const {
    auto it = lastRun_.find(node);
    return it == lastRun_.end() ? kNeverRan : it->second;
  }

  // Total ticks driven (frames). Diagnostics only.
  std::uint64_t tickCount() const { return ticks_; }

 private:
  // The cadence decision for ONE node at `frameTime`: is it DUE to run now? Reads
  // the node's policy + its last-run time. (Dirtiness is decided by the DAG; the
  // gate is only consulted for dirty nodes.)
  bool isDue(NodeId node, std::int64_t frameTime) const;

  TransformDag& dag_;
  std::unordered_map<NodeId, CadencePolicy> policies_;
  std::unordered_map<NodeId, std::int64_t> lastRun_;  // node -> frameTime it ran
  std::uint64_t ticks_{0};
};

}  // namespace dc
