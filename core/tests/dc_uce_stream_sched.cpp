// ENC-616e — Streaming scheduler tests (RESEARCH §3 + §5.1 streaming-class column).
//
// Drives a TransformDag through the StreamingScheduler with an INJECTED fake clock
// (frameTime is a plain int we advance), proving the per-class cadence:
//   1) class-1 (Incremental) recomputes on EVERY dirty frame.
//   2) class-2 (Windowed) recomputes only when a window/HOP boundary advances —
//      not on intra-window ticks.
//   3) class-3 (Global) is THROTTLED: under rapid ticks it runs at most once per
//      interval (a global does NOT run every frame), but DOES run once the interval
//      elapses, and its HELD (deferred) dirtiness is honored then.
//   4) dirty-gating is still respected: a clean (non-dirty) node never runs, no
//      matter the cadence.
// All over the merged DAG + transforms — the scheduler adds no transform and changes
// no transform internals; it only governs WHEN a dirty node recomputes.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/transform/StreamingScheduler.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Formula.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

using namespace dc;

static void appendRecord(std::vector<std::uint8_t>& out, Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF);
    out.push_back((v >> 16) & 0xFF); out.push_back((v >> 24) & 0xFF);
  };
  out.push_back(1);
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

// True iff `id` appears in the list of nodes that ran this tick.
static bool ranContains(const std::vector<NodeId>& ran, NodeId id) {
  for (NodeId r : ran) if (r == id) return true;
  return false;
}

int main() {
  std::printf("=== ENC-616e Streaming Scheduler ===\n");

  // -------------------------------------------------------------------------
  // Source table: a single f32 price column over the ingest feed.
  // -------------------------------------------------------------------------
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufPrice = 700, kTable = 1;
  tables.defineTable(kTable, "trades");
  tables.addColumn(kTable, "price", DType::F32, kBufPrice);

  auto appendPrice = [&](float v) {
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBufPrice, &v, sizeof(v));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  };
  appendPrice(10.0f);

  // Three sibling transform nodes on the same source, one per class:
  //   N1 (class-1 Incremental) — formula, runs every dirty frame.
  //   N2 (class-2 Windowed, hop=4) — formula, runs only on a hop boundary.
  //   N3 (class-3 Global, interval=10) — formula, throttled.
  const NodeId N1 = 101, N2 = 102, N3 = 103;
  TransformDag dag(tables, src);
  dag.addSource(kTable);
  check(dag.addTransform(N1, kTable,
                         std::make_unique<FormulaTransform>("price + 1", "a")),
        "add class-1 node");
  check(dag.addTransform(N2, kTable,
                         std::make_unique<FormulaTransform>("price + 2", "b")),
        "add class-2 node");
  check(dag.addTransform(N3, kTable,
                         std::make_unique<FormulaTransform>("price + 3", "c")),
        "add class-3 node");
  check(dag.build(), "build DAG");

  StreamingScheduler sched(dag);
  sched.setIncremental(N1);
  sched.setWindowed(N2, /*hop=*/4);
  sched.setGlobal(N3, /*interval=*/10);

  // A "data tick": mark the source dirty (the ChartSession touched-buffer path).
  auto dataTick = [&] { dag.markTouchedBuffers({kBufPrice}); };

  // -------------------------------------------------------------------------
  // 1) FIRST tick at t=0 with data dirty: every node is "never ran" -> all due,
  //    all recompute (seed). Establishes each node's cadence anchor at t=0.
  // -------------------------------------------------------------------------
  {
    dataTick();
    auto ran = sched.tick(/*frameTime=*/0);
    check(ranContains(ran, N1) && ranContains(ran, N2) && ranContains(ran, N3),
          "t=0: all three classes seed (first run always due)");
    check(dag.recomputeCount(N1) == 1 && dag.recomputeCount(N2) == 1 &&
              dag.recomputeCount(N3) == 1,
          "t=0: each node ran exactly once");
    check(sched.lastRunTime(N3) == 0, "class-3 anchored at t=0");
  }

  // -------------------------------------------------------------------------
  // 2) class-1 recomputes on EVERY dirty frame. Tick t=1,2,3 with data dirty.
  //    N1 runs each; N2 stays in hop bucket 0 (1/4,2/4,3/4 -> 0) so it does NOT;
  //    N3 is inside its 10-unit throttle window so it does NOT.
  // -------------------------------------------------------------------------
  for (std::int64_t t = 1; t <= 3; ++t) {
    appendPrice(10.0f + static_cast<float>(t));  // grow the source each frame
    dataTick();
    auto ran = sched.tick(t);
    check(ranContains(ran, N1), "class-1 runs every dirty frame");
    check(!ranContains(ran, N2), "class-2 quiet within the same hop bucket");
    check(!ranContains(ran, N3), "class-3 quiet within its throttle window");
  }
  check(dag.recomputeCount(N1) == 4, "class-1 ran 4x (t=0..3, every frame)");
  check(dag.recomputeCount(N2) == 1, "class-2 still 1 (no hop crossing yet)");
  check(dag.recomputeCount(N3) == 1, "class-3 still 1 (throttled)");
  // The class-2 / class-3 nodes are HELD: dirty but deferred, owed a recompute.
  check(dag.isHeld(N2) && dag.isHeld(N3), "class-2 & class-3 are HELD (deferred)");

  // -------------------------------------------------------------------------
  // 3) class-2 recomputes ONLY on a window/HOP boundary. t=4 crosses from hop
  //    bucket 0 into bucket 1 (4/4 == 1) -> N2 is due and runs. N1 runs (every
  //    frame). N3 still throttled (t-last = 4 < 10).
  // -------------------------------------------------------------------------
  {
    appendPrice(20.0f);
    dataTick();
    auto ran = sched.tick(4);
    check(ranContains(ran, N2), "class-2 runs on hop-boundary crossing (t=4)");
    check(ranContains(ran, N1), "class-1 still every frame at the boundary");
    check(!ranContains(ran, N3), "class-3 still throttled at t=4");
    check(dag.recomputeCount(N2) == 2, "class-2 ran a 2nd time at the boundary");
    check(!dag.isHeld(N2), "class-2 no longer held after it ran");
    check(dag.isHeld(N3), "class-3 still held (owes a run)");
  }

  // class-2 stays quiet again within hop bucket 1 (t=5,6,7).
  for (std::int64_t t = 5; t <= 7; ++t) {
    appendPrice(20.0f + static_cast<float>(t));
    dataTick();
    auto ran = sched.tick(t);
    check(!ranContains(ran, N2), "class-2 quiet again inside hop bucket 1");
  }
  check(dag.recomputeCount(N2) == 2, "class-2 unchanged across hop bucket 1 interior");

  // -------------------------------------------------------------------------
  // 4) class-3 GLOBAL throttle: it has NOT run since t=0 despite ~7 dirty ticks
  //    (proves a global does NOT run every frame). At t=10 the 10-unit interval
  //    has elapsed (10 - 0 >= 10) -> it is finally due and runs ONCE, draining its
  //    held dirtiness.
  // -------------------------------------------------------------------------
  check(dag.recomputeCount(N3) == 1,
        "class-3 ran only ONCE across many rapid ticks (throttled global)");
  {
    appendPrice(99.0f);
    dataTick();
    auto ran = sched.tick(10);
    check(ranContains(ran, N3), "class-3 runs once its throttle interval elapsed");
    check(dag.recomputeCount(N3) == 2, "class-3 bumped to 2 after the interval");
    check(sched.lastRunTime(N3) == 10, "class-3 re-anchored at t=10");
    check(!dag.isHeld(N3), "class-3 no longer held after the throttled run");
    // Its output reflects the latest source (price+3); newest appended price 99.
    auto cv = dag.columns().viewF32(N3, "c");
    check(cv.valid() && cv.size() >= 1, "class-3 produced output on the throttled run");
  }

  // Immediately after, within the next interval, the global is quiet again even
  // though data keeps changing (debounce holds until t >= 20).
  {
    appendPrice(1.0f);
    dataTick();
    auto ran = sched.tick(11);
    check(!ranContains(ran, N3), "class-3 quiet again right after its throttled run");
    check(ranContains(ran, N1), "class-1 still fires every frame meanwhile");
  }

  // -------------------------------------------------------------------------
  // 5) DIRTY-GATING still respected: with NO data change AND nothing held, a tick
  //    recomputes NOTHING — not even class-1 (cheap, but CLEAN). The cadence gate
  //    is only consulted for dirty nodes.
  //    First FLUSH any held (deferred) work with a due tick far in the future so
  //    the DAG is genuinely clean, then assert the clean tick runs nothing.
  // -------------------------------------------------------------------------
  {
    // t=900 is past every cadence boundary/interval -> any held node drains here.
    appendPrice(7.0f);
    dataTick();
    sched.tick(900);
    check(!dag.isHeld(N2) && !dag.isHeld(N3),
          "held work drained by the far-future due tick (clean DAG)");

    std::uint64_t before1 = dag.recomputeCount(N1);
    std::uint64_t before2 = dag.recomputeCount(N2);
    std::uint64_t before3 = dag.recomputeCount(N3);
    auto ran = sched.tick(1000, /*dataChanged=*/false);  // no dataTick() -> clean
    check(ran.empty(), "clean tick recomputes nothing (dirty-gating intact)");
    check(dag.recomputeCount(N1) == before1, "class-1 skipped when clean");
    check(dag.recomputeCount(N2) == before2, "class-2 skipped when clean");
    check(dag.recomputeCount(N3) == before3, "class-3 skipped when clean");
  }

  // -------------------------------------------------------------------------
  // 6) After a clean tick, a fresh data tick re-runs the due nodes. At t=1001 the
  //    class-3 throttle has long elapsed -> it runs; class-1 runs; class-2 runs
  //    (new hop bucket, far past bucket 1).
  // -------------------------------------------------------------------------
  {
    appendPrice(2.0f);
    dataTick();
    auto ran = sched.tick(1001);
    check(ranContains(ran, N1), "class-1 runs on the new data tick");
    check(ranContains(ran, N2), "class-2 due (far past last hop bucket)");
    check(ranContains(ran, N3), "class-3 due (throttle long elapsed)");
  }

  // -------------------------------------------------------------------------
  // 7) Default class = Incremental: a node with NO assignment behaves class-1.
  // -------------------------------------------------------------------------
  {
    const NodeId N4 = 104;
    TransformDag dag2(tables, src);
    dag2.addSource(kTable);
    dag2.addTransform(N4, kTable,
                      std::make_unique<FormulaTransform>("price + 5", "e"));
    dag2.build();
    StreamingScheduler s2(dag2);  // N4 left UNASSIGNED
    check(s2.policyOf(N4).cls == StreamClass::Incremental,
          "unassigned node defaults to class-1");
    dag2.markTouchedBuffers({kBufPrice});
    auto r0 = s2.tick(0);
    appendPrice(3.0f);
    dag2.markTouchedBuffers({kBufPrice});
    auto r1 = s2.tick(1);
    check(ranContains(r0, N4) && ranContains(r1, N4),
          "default (class-1) node runs every dirty frame");
  }

  std::printf("=== ENC-616e Scheduler Results: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
