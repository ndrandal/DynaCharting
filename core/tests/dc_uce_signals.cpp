// ENC-623 (B1) — SignalStore tests: typed signal storage, the ReactiveGraph
// dirty-notification path (a mutation schedules dependents that registered on
// signalInput(signalId)), and the selection predicate seeds (matchesRow /
// matchesValue / isEmpty).
#include "dc/data/ReactiveGraph.hpp"
#include "dc/interaction/SignalStore.hpp"

#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

// ---------------------------------------------------------------------------
// Storage + typed access
// ---------------------------------------------------------------------------
static void testStorage() {
  std::printf("\n--- B1: storage + typed access ---\n");
  dc::SignalStore store;

  const dc::Id kSel = 10;
  check(!store.has(kSel), "undefined signal: has() == false");
  check(store.get(kSel) == nullptr, "undefined signal: get() == nullptr");

  store.define(kSel, dc::PointSelection{42});
  check(store.has(kSel), "after define: has() == true");
  check(store.size() == 1, "size() == 1");

  const auto* p = store.getAs<dc::PointSelection>(kSel);
  check(p != nullptr && p->rowId == 42, "getAs<PointSelection> reads value");
  check(store.getAs<dc::BrushRect>(kSel) == nullptr, "getAs wrong type == nullptr");

  check(store.set(kSel, dc::PointSelection{99}), "set() on defined == true");
  check(store.getAs<dc::PointSelection>(kSel)->rowId == 99, "set() updates value");
  check(!store.set(404, dc::PointSelection{1}), "set() on undefined == false");

  store.remove(kSel);
  check(!store.has(kSel) && store.size() == 0, "remove() erases");
}

// ---------------------------------------------------------------------------
// ReactiveGraph dirty-notification path
// ---------------------------------------------------------------------------
static void testReactiveNotify() {
  std::printf("\n--- B1: ReactiveGraph dirty notification ---\n");
  dc::ReactiveGraph graph;
  dc::SignalStore store(&graph);

  const dc::Id kBrush = 7;
  const dc::DependentId kFilterNode = 1000;

  // A transform/encode node registers on the SIGNAL input (not data) — the §5/§6
  // feedback edge. A buffer id 7 must NOT collide with signal id 7.
  graph.addDependency(kFilterNode, dc::signalInput(kBrush));
  graph.addDependency(2000, dc::dataInput(7));  // distinct node, Data kind, same key

  store.define(kBrush, dc::BrushRect{0, 0, 10, 10});
  {
    auto drained = graph.drain();
    check(drained.size() == 1 && drained[0] == kFilterNode,
          "define() schedules only the signal dependent (not the data one)");
  }

  // A mutation marks it dirty again.
  check(store.set(kBrush, dc::BrushRect{1, 1, 5, 5}), "set brush");
  check(graph.isPending(kFilterNode), "set() re-schedules the dependent");
  check(graph.drain().size() == 1, "drain returns the re-scheduled dependent");

  // clear() also notifies.
  store.clear(kBrush);
  check(graph.isPending(kFilterNode), "clear() schedules the dependent");
  graph.drain();

  // Detached store does not touch the graph.
  store.setGraph(nullptr);
  store.set(kBrush, dc::BrushRect{2, 2, 3, 3});
  check(!graph.isPending(kFilterNode), "detached store does not notify");
}

// ---------------------------------------------------------------------------
// Predicate semantics
// ---------------------------------------------------------------------------
static void testPredicates() {
  std::printf("\n--- B1: predicate semantics ---\n");
  dc::SignalStore store;
  const dc::Id kField = 500;

  // Point selection: row-based.
  const dc::Id kPoint = 1;
  store.define(kPoint, dc::PointSelection{42});
  check(!store.isEmpty(kPoint), "point selection not empty");
  check(store.matchesRow(kPoint, 42), "point matches selected row");
  check(!store.matchesRow(kPoint, 7), "point rejects other row");
  check(store.matchesValue(kPoint, 12345.0), "point imposes no value constraint");
  store.clear(kPoint);
  check(store.isEmpty(kPoint), "cleared point is empty");
  check(store.matchesRow(kPoint, 7), "empty point matches everything");

  // Interval selection: value-based.
  const dc::Id kIv = 2;
  store.define(kIv, dc::IntervalSelection{kField, 10.0, 20.0});
  check(!store.isEmpty(kIv), "interval not empty");
  check(store.matchesValue(kIv, 15.0), "interval contains 15");
  check(store.matchesValue(kIv, 20.0), "interval inclusive of hi");
  check(!store.matchesValue(kIv, 21.0), "interval rejects 21");
  check(store.matchesRow(kIv, 999), "interval imposes no row constraint");

  // Multi selection: OR of intervals + explicit rows.
  const dc::Id kMulti = 3;
  dc::MultiSelection m;
  m.intervals.push_back(dc::IntervalSelection{kField, 0.0, 5.0});
  m.intervals.push_back(dc::IntervalSelection{kField, 90.0, 100.0});
  m.rows = {7, 8};
  store.define(kMulti, std::move(m));
  check(store.matchesValue(kMulti, 3.0), "multi matches first interval");
  check(store.matchesValue(kMulti, 95.0), "multi matches second interval");
  check(!store.matchesValue(kMulti, 50.0), "multi rejects gap value");
  check(store.matchesRow(kMulti, 7), "multi matches explicit row");
  check(!store.matchesRow(kMulti, 9), "multi rejects unlisted row");

  // Hover: row-based, inactive => no constraint.
  const dc::Id kHover = 4;
  store.define(kHover, dc::HoverState{55, true});
  check(store.matchesRow(kHover, 55), "hover matches hovered row");
  check(!store.matchesRow(kHover, 1), "hover rejects other row");
  store.define(kHover, dc::HoverState{55, false});
  check(store.isEmpty(kHover), "inactive hover is empty");
  check(store.matchesRow(kHover, 1), "inactive hover matches everything");

  // Camera/clock never constrain.
  const dc::Id kCam = 5, kClock = 6;
  store.define(kCam, dc::CameraState{1, 2, 3});
  store.define(kClock, dc::TransitionClock{0.5f});
  check(store.isEmpty(kCam) && store.isEmpty(kClock), "camera/clock impose no constraint");
  check(store.matchesRow(kCam, 1) && store.matchesValue(kClock, 1.0), "camera/clock match all");

  // Undefined signal: no constraint.
  check(store.isEmpty(9999) && store.matchesRow(9999, 1) && store.matchesValue(9999, 1.0),
        "undefined signal matches everything");
}

int main() {
  std::printf("=== ENC-623 (B1) SignalStore ===\n");
  testStorage();
  testReactiveNotify();
  testPredicates();
  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
