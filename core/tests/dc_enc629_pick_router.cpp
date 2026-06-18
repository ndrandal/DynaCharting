// ENC-629 (C3) — PickRouter: route a per-instance pick hit into the SignalStore.
//
// Two halves:
//   (1) Routing semantics on a standalone SignalStore — onClick sets/clears a
//       PointSelection, onToggleClick unions/toggles a MultiSelection row set,
//       onHover activates/deactivates a HoverState, background (rowId < 0) clears
//       point / deactivates hover / is a no-op for toggle, and unconfigured
//       gestures return false.
//   (2) Dirty propagation — with the store bound to a TransformDag's ReactiveGraph
//       and a filter node bound to the selection signal, a routed click drives a
//       recompute through the EXISTING ENC-624 feedback edge (markSignalDirty).
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/PickRouter.hpp"
#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Filter.hpp"

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

int main() {
  std::printf("=== ENC-629 (C3) PickRouter: pick hit -> SignalStore ===\n");

  const Id kSelection = 5000;
  const Id kHover = 5001;
  const Id kMulti = 5002;

  // ---- (1) routing semantics on a standalone store -------------------------
  {
    SignalStore store;
    store.define(kSelection, PointSelection{});  // empty
    store.define(kHover, HoverState{});          // inactive
    store.define(kMulti, MultiSelection{});      // empty

    PickRouter router(&store);

    // No selection signal configured yet -> onClick is a no-op.
    check(!router.onClick(7), "onClick with no selection signal -> false");

    router.setSelectionSignal(kSelection);
    router.setHoverSignal(kHover);

    // A click selects the clicked row.
    check(router.onClick(7), "onClick(7) mutates the selection signal");
    const PointSelection* ps = store.getAs<PointSelection>(kSelection);
    check(ps && ps->rowId == 7, "selection signal == PointSelection{7}");
    check(!store.isEmpty(kSelection), "selection is non-empty after a click");

    // A background click clears the selection.
    check(router.onClick(-1), "onClick(-1) clears the selection");
    check(store.isEmpty(kSelection), "selection empty after background click");

    // Hover activates / deactivates.
    check(router.onHover(3), "onHover(3) mutates the hover signal");
    const HoverState* hv = store.getAs<HoverState>(kHover);
    check(hv && hv->active && hv->rowId == 3, "hover active on row 3");
    check(router.onHover(-1), "onHover(-1) deactivates");
    hv = store.getAs<HoverState>(kHover);
    check(hv && !hv->active, "hover inactive after background hover");
    // Hover of row 0 is unambiguous (active flag is separate from the sentinel).
    check(router.onHover(0), "onHover(0)");
    hv = store.getAs<HoverState>(kHover);
    check(hv && hv->active && hv->rowId == 0, "hover active on row 0 (no sentinel clash)");

    // Toggle/multi selection: union then toggle off; intervals preserved.
    router.setSelectionSignal(kMulti);
    store.set(kMulti, MultiSelection{{IntervalSelection{99, 1.0, 2.0}}, {}});
    check(router.onToggleClick(4), "onToggleClick(4) adds row 4");
    check(router.onToggleClick(8), "onToggleClick(8) adds row 8");
    const MultiSelection* ms = store.getAs<MultiSelection>(kMulti);
    check(ms && ms->rows.size() == 2 && ms->rows[0] == 4 && ms->rows[1] == 8,
          "multi rows == {4, 8}");
    check(ms && ms->intervals.size() == 1,
          "toggle preserves the existing interval constraint");
    check(router.onToggleClick(4), "onToggleClick(4) again toggles row 4 OFF");
    ms = store.getAs<MultiSelection>(kMulti);
    check(ms && ms->rows.size() == 1 && ms->rows[0] == 8, "multi rows == {8}");
    check(!router.onToggleClick(-1), "background toggle is a no-op");
    ms = store.getAs<MultiSelection>(kMulti);
    check(ms && ms->rows.size() == 1, "background toggle kept the set");
  }

  // ---- (2) the routed mutation drives a TransformDag recompute -------------
  {
    IngestProcessor ingest;
    TableStore tables;
    auto src = makeBufferByteSource(ingest);
    const Id kBufPrice = 200, kTable = 1;
    tables.defineTable(kTable, "trades");
    tables.addColumn(kTable, "price", DType::F32, kBufPrice);
    {
      std::vector<float> prices = {10.0f, 25.0f, 5.0f, 40.0f};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kBufPrice, prices.data(),
                   static_cast<std::uint32_t>(prices.size() * 4));
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }

    TransformDag dag(tables, src);
    dag.addSource(kTable);
    const NodeId kFilter = 100;
    dag.addTransform(kFilter, kTable,
                     std::make_unique<FilterTransform>("price > 8"));

    SignalStore store(&dag.reactive());  // mutations notify the DAG's graph
    store.define(kSelection, PointSelection{});
    check(dag.addSignalDependency(kFilter, kSelection),
          "bind selection signal to the filter node");
    check(dag.build(), "dag build ok");

    dag.markTableDirty(kTable);
    auto ran = dag.evaluate();
    check(ran.size() == 1 && dag.recomputeCount(kFilter) == 1,
          "initial data pass: filter ran once");

    PickRouter router(&store);
    router.setSelectionSignal(kSelection);

    // A routed click marks the signal dirty -> the bound filter recomputes.
    check(router.onClick(2), "routed click mutates the bound selection signal");
    ran = dag.evaluate();
    check(ran.size() == 1 && ran[0] == kFilter, "routed click recomputed the filter");
    check(dag.recomputeCount(kFilter) == 2, "recompute count == 2 after the click");

    // A background click (clear) also drives a recompute.
    check(router.onClick(-1), "routed background click clears");
    ran = dag.evaluate();
    check(ran.size() == 1 && dag.recomputeCount(kFilter) == 3,
          "routed clear recomputed the filter");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
