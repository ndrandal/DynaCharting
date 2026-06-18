// ENC-624 (B2) — Signal -> TransformDag binding: a SignalStore mutation drives a
// transform-node recompute through the DAG's EXISTING ReactiveGraph drain()/topo
// path (the §5/§6 feedback edge), with dirty-gating preserved (an unrelated signal
// or no change recomputes nothing).
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
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
  std::printf("=== ENC-624 (B2) signal -> TransformDag binding ===\n");

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
  check(dag.addTransform(kFilter, kTable,
                         std::make_unique<FilterTransform>("price > 8")),
        "addTransform filter ok");

  // ---- guards ---------------------------------------------------------------
  check(!dag.addSignalDependency(999, 5000), "addSignalDependency unknown node -> false");
  check(!dag.addSignalDependency(kTable, 5000),
        "addSignalDependency on a SOURCE -> false (only transforms read signals)");

  // ---- bind a signal to the filter node ------------------------------------
  const Id kBrush = 5000;
  SignalStore store(&dag.reactive());  // mutations notify the DAG's graph
  store.define(kBrush, IntervalSelection{kBufPrice, 0.0, 100.0});
  check(dag.addSignalDependency(kFilter, kBrush), "bind signal to filter node");

  check(dag.build(), "build ok");

  // Initial data pass: the node computes once.
  dag.markTableDirty(kTable);
  auto ran = dag.evaluate();
  check(ran.size() == 1 && ran[0] == kFilter, "data pass: filter ran");
  check(dag.recomputeCount(kFilter) == 1, "recompute count == 1");

  // No change -> dirty-gating: nothing recomputes.
  ran = dag.evaluate();
  check(ran.empty() && dag.recomputeCount(kFilter) == 1,
        "no change -> filter does NOT recompute");

  // A SIGNAL mutation alone schedules the bound node -> it recomputes.
  check(store.set(kBrush, IntervalSelection{kBufPrice, 10.0, 30.0}), "mutate brush signal");
  ran = dag.evaluate();
  check(ran.size() == 1 && ran[0] == kFilter, "signal change: filter recomputed");
  check(dag.recomputeCount(kFilter) == 2, "recompute count == 2 after signal");

  // clear() also drives a recompute (it notifies the graph).
  store.clear(kBrush);
  ran = dag.evaluate();
  check(ran.size() == 1, "signal clear: filter recomputed");
  check(dag.recomputeCount(kFilter) == 3, "recompute count == 3 after clear");

  // An UNRELATED signal (no node bound to it) recomputes nothing.
  const Id kOther = 6000;
  store.define(kOther, PointSelection{42});  // notifies graph, but nobody depends
  ran = dag.evaluate();
  check(ran.empty() && dag.recomputeCount(kFilter) == 3,
        "unrelated signal -> no recompute (dirty-gating)");

  // markSignalDirty() directly (the SignalStore-less path) also works.
  dag.markSignalDirty(kBrush);
  ran = dag.evaluate();
  check(ran.size() == 1 && dag.recomputeCount(kFilter) == 4,
        "dag.markSignalDirty drives recompute");

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
