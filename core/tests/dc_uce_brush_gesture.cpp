// ENC-633 (D3) — BrushGesture: a drag produces an interval/rect selection signal,
// live-updating + dirtying the ReactiveGraph so a bound selectionFilter re-renders
// as the user brushes.
#include "dc/data/ReactiveGraph.hpp"
#include "dc/interaction/BrushGesture.hpp"
#include "dc/interaction/SignalStore.hpp"

#include <cstdio>

static int passed = 0, failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}
using namespace dc;

int main() {
  std::printf("=== ENC-633 (D3) BrushGesture ===\n");

  ReactiveGraph graph;
  SignalStore store(&graph);
  const Id kSig = 9300, kField = 42;
  const DependentId kFilter = 500;
  graph.addDependency(kFilter, signalInput(kSig));

  // X-interval brush.
  BrushGesture brush(&store, kSig);
  brush.setMode(BrushGesture::Mode::XInterval);
  brush.setField(kField);

  brush.begin(5.0, 100.0);
  check(brush.isActive(), "brush active after begin");
  check(brush.update(15.0, 200.0), "update mutates signal (live)");
  {
    const auto* iv = store.getAs<IntervalSelection>(kSig);
    check(iv && iv->field == kField && iv->lo == 5.0 && iv->hi == 15.0,
          "live interval = [5,15] on field");
    check(graph.isPending(kFilter), "brush dirties the bound filter node");
    graph.drain();
  }

  // Extend, then end -> [5,25].
  brush.update(25.0, 50.0);
  check(brush.end(25.0, 50.0), "end mutates signal");
  {
    const auto* iv = store.getAs<IntervalSelection>(kSig);
    check(iv && iv->lo == 5.0 && iv->hi == 25.0, "final interval = [5,25]");
    check(!brush.isActive(), "inactive after end");
  }

  // Backwards drag normalizes (min/max).
  brush.begin(30.0, 0.0);
  brush.end(10.0, 0.0);
  {
    const auto* iv = store.getAs<IntervalSelection>(kSig);
    check(iv && iv->lo == 10.0 && iv->hi == 30.0, "backwards drag -> [10,30]");
  }

  // cancel() clears the selection (empty).
  graph.drain();
  brush.begin(1.0, 1.0);
  brush.cancel();
  check(store.isEmpty(kSig), "cancel clears the brush signal");
  check(graph.isPending(kFilter), "cancel dirties the filter node");
  graph.drain();

  // Non-live mode: update does NOT write, end does.
  BrushGesture lazy(&store, kSig);
  lazy.setMode(BrushGesture::Mode::XInterval);
  lazy.setField(kField);
  lazy.setLiveUpdate(false);
  lazy.begin(2.0, 0.0);
  check(!lazy.update(8.0, 0.0), "non-live: update does not write");
  check(graph.drain().empty(), "non-live: nothing pending after update");
  check(lazy.end(8.0, 0.0), "non-live: end writes");
  {
    const auto* iv = store.getAs<IntervalSelection>(kSig);
    check(iv && iv->lo == 2.0 && iv->hi == 8.0, "non-live final = [2,8]");
  }

  // Rect mode -> BrushRect.
  BrushGesture rect(&store, 9301);
  rect.setMode(BrushGesture::Mode::Rect);
  rect.begin(1.0, 2.0);
  rect.end(3.0, 4.0);
  {
    const auto* r = store.getAs<BrushRect>(9301);
    check(r && r->minX() == 1.0 && r->maxX() == 3.0 && r->minY() == 2.0 && r->maxY() == 4.0,
          "rect mode -> BrushRect {1,2,3,4}");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
