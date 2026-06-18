// ENC-626 (B4) — selectionFilter transform: a SignalStore selection drives a
// DAG transform that DROPS (Filter) or FLAGS (Deemphasize) rows, recomputing
// through the ENC-624 signal feedback edge.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/SelectionFilter.hpp"

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
  std::printf("=== ENC-626 (B4) selectionFilter transform ===\n");

  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufRowid = 400, kBufPrice = 401, kTable = 1;
  tables.defineTable(kTable, "trades");
  tables.addColumn(kTable, "rowid", DType::I32, kBufRowid);
  tables.addColumn(kTable, "price", DType::F32, kBufPrice);
  {
    std::vector<std::int32_t> ids = {100, 101, 102, 103};
    std::vector<float> prices = {10.0f, 25.0f, 5.0f, 40.0f};
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBufRowid, ids.data(), 16);
    appendRecord(batch, kBufPrice, prices.data(), 16);
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  }

  const Id kSig = 8000;
  SignalStore store;

  // --- Filter mode: drop rows the selection rejects -------------------------
  {
    TransformDag dag(tables, src);
    store.setGraph(&dag.reactive());
    store.define(kSig, IntervalSelection{kBufPrice, 0.0, 20.0});  // prices 10,5

    const NodeId kFilter = 100;
    dag.addSource(kTable);
    check(dag.addTransform(kFilter, kTable,
                           std::make_unique<SelectionFilterTransform>(
                               &store, kSig, "rowid", "price",
                               SelectionFilterTransform::Mode::Filter)),
          "addTransform selectionFilter (Filter) ok");
    check(dag.addSignalDependency(kFilter, kSig), "bind filter node to signal");
    check(dag.build(), "build ok");

    dag.markTableDirty(kTable);
    dag.evaluate();
    auto pv = dag.columns().viewF32(kFilter, "price");
    check(pv.valid() && pv.count == 2, "interval [0,20] keeps 2 rows");
    check(pv[0] == 10.0f && pv[1] == 5.0f, "kept prices 10 and 5");

    // Mutate the signal -> the node recomputes via the feedback edge.
    store.set(kSig, PointSelection{102});  // rowid 102 -> price 5
    auto ran = dag.evaluate();
    check(ran.size() == 1 && ran[0] == kFilter, "signal change recomputed the node");
    pv = dag.columns().viewF32(kFilter, "price");
    auto rv = dag.columns().viewI32(kFilter, "rowid");
    check(pv.valid() && pv.count == 1 && pv[0] == 5.0f, "point sel keeps 1 row (price 5)");
    check(rv.valid() && rv[0] == 102, "kept row is rowid 102");

    // Empty selection -> all rows pass.
    store.clear(kSig);
    dag.evaluate();
    check(dag.columns().viewF32(kFilter, "price").count == 4, "cleared selection keeps all 4");
    store.setGraph(nullptr);
  }

  // --- Deemphasize mode: keep all rows + append boolean `selected` ----------
  {
    TransformDag dag(tables, src);
    store.setGraph(&dag.reactive());
    store.define(kSig, IntervalSelection{kBufPrice, 0.0, 20.0});  // 10,5 selected

    const NodeId kDim = 200;
    dag.addSource(kTable);
    check(dag.addTransform(kDim, kTable,
                           std::make_unique<SelectionFilterTransform>(
                               &store, kSig, "rowid", "price",
                               SelectionFilterTransform::Mode::Deemphasize)),
          "addTransform selectionFilter (Deemphasize) ok");
    check(dag.schemaOf(kDim) && dag.schemaOf(kDim)->has("selected"),
          "deemphasize output schema has `selected` column");
    dag.addSignalDependency(kDim, kSig);
    check(dag.build(), "build ok");
    dag.markTableDirty(kTable);
    dag.evaluate();

    auto pv = dag.columns().viewF32(kDim, "price");
    auto sv = dag.columns().viewI32(kDim, "selected");
    check(pv.valid() && pv.count == 4, "deemphasize keeps all 4 rows");
    check(sv.valid() && sv.count == 4, "selected column has 4 rows");
    // prices {10,25,5,40} vs [0,20] -> selected {1,0,1,0}
    check(sv[0] == 1 && sv[1] == 0 && sv[2] == 1 && sv[3] == 0,
          "selected flags match the interval");
    store.setGraph(nullptr);
  }

  // --- fail-fast typing -----------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    bool ok = dag.addTransform(300, kTable,
                               std::make_unique<SelectionFilterTransform>(
                                   &store, kSig, "no_such_col", "price"));
    check(!ok, "REJECT selectionFilter referencing an unknown rowId column");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
