// ENC-616e — Streaming scheduler implementation. See StreamingScheduler.hpp for
// the model (per-class cadence over the transform DAG, throttled globals).
#include "dc/transform/StreamingScheduler.hpp"

namespace dc {

bool StreamingScheduler::isDue(NodeId node, std::int64_t frameTime) const {
  const CadencePolicy pol = policyOf(node);

  auto lastIt = lastRun_.find(node);
  const bool neverRan = (lastIt == lastRun_.end());
  const std::int64_t last = neverRan ? kNeverRan : lastIt->second;

  switch (pol.cls) {
    case StreamClass::Incremental:
      // class-1: O(Δ) cheap — due every frame it is dirty.
      return true;

    case StreamClass::Windowed: {
      // class-2: due only when frameTime crossed into a NEW hop bucket since the
      // last run. floor-divide into hop-sized buckets; a run within the same bucket
      // is suppressed. The first time (never ran) is always due so the node seeds.
      if (neverRan) return true;
      const std::int64_t hop = pol.hop < 1 ? 1 : pol.hop;
      // Floor division toward -inf so negative times bucket correctly.
      auto bucket = [hop](std::int64_t t) {
        std::int64_t q = t / hop;
        if ((t % hop != 0) && ((t < 0) != (hop < 0))) --q;
        return q;
      };
      return bucket(frameTime) != bucket(last);
    }

    case StreamClass::Global:
    case StreamClass::Baseline: {
      // class-3/4: throttled/debounced. Due at most once per throttleInterval; a
      // run at `last` blocks the next until last + interval. First run always due.
      if (neverRan) return true;
      const std::int64_t interval =
          pol.throttleInterval < 0 ? 0 : pol.throttleInterval;
      return frameTime - last >= interval;
    }
  }
  return true;  // unreachable; default to "always fresh".
}

std::vector<NodeId> StreamingScheduler::tick(std::int64_t frameTime,
                                             bool /*dataChanged*/) {
  ++ticks_;

  // Install the per-class gate: for each dirty node the DAG asks whether it is due
  // at this frameTime. Not-due nodes are HELD by the DAG (kept dirty for a later
  // tick). The gate closes over `frameTime` for this pass only.
  dag_.setNodeGate(
      [this, frameTime](NodeId id) { return isDue(id, frameTime); });

  std::vector<NodeId> ran = dag_.evaluate();

  // Anchor each node that actually ran at this frameTime — its cadence resets from
  // here (the next window boundary / throttle window is measured from now).
  for (NodeId id : ran) lastRun_[id] = frameTime;

  // The gate captured `frameTime`; drop it so the DAG is not left holding a stale
  // closure between ticks (the next tick installs a fresh one).
  dag_.clearNodeGate();
  return ran;
}

}  // namespace dc
