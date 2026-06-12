// ENC-616a — Transform DAG foundation tests (RESEARCH §5.1/§5.2).
//
// Covers: typed ColumnStore read/write; DAG topological eval order; dirty-gating
// (only dirty nodes + downstream recompute, proven via a recompute counter); the
// fail-fast schema typing; `filter` selecting the right rows; `formula` deriving
// the right column; and a 2-node DAG (filter -> formula) recomputing incrementally
// on an input append.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Filter.hpp"
#include "dc/transform/transforms/Formula.hpp"

#include <cmath>
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

// Append one op=1 APPEND record (the unchanged 13-byte wire format).
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
static bool nearly(double x, double y) { return std::fabs(x - y) < 1e-5; }

int main() {
  std::printf("=== ENC-616a Transform DAG ===\n");

  // -------------------------------------------------------------------------
  // 1) ColumnStore typed read/write (+ dtype guard, timestamp f32 trap)
  // -------------------------------------------------------------------------
  {
    ColumnStore cs;
    const Id node = 10;
    check(cs.allocColumn(node, "px", DType::F32, 3), "alloc f32 col");
    check(cs.allocColumn(node, "ts", DType::Timestamp, 3), "alloc ts col");
    cs.setF32(node, "px", 0, 1.5f);
    cs.setF32(node, "px", 2, 9.0f);
    cs.setTimestamp(node, "ts", 0, 1700000000000LL);

    auto v = cs.viewF32(node, "px");
    check(v.valid() && v.size() == 3, "f32 view 3 rows");
    check(v[0] == 1.5f && v[1] == 0.0f && v[2] == 9.0f, "f32 written values");
    auto tv = cs.viewTimestamp(node, "ts");
    check(tv.valid() && tv[0] == 1700000000000LL, "timestamp i64 exact");
    // dtype guard: wrong-typed view is empty; timestamp has NO f32 view.
    check(!cs.viewI32(node, "px").valid(), "i32 view of f32 -> empty");
    check(!cs.viewF32(node, "ts").valid(), "f32 view of timestamp -> empty (trap)");
    check(cs.rowCount(node, "px") == 3, "rowCount");
    cs.dropNode(node);
    check(!cs.hasColumn(node, "px"), "dropNode clears columns");
  }

  // -------------------------------------------------------------------------
  // Build a source table with f32 price + i32 qty over the ingest feed.
  // -------------------------------------------------------------------------
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufPrice = 200, kBufQty = 201, kTable = 1;
  tables.defineTable(kTable, "trades");
  tables.addColumn(kTable, "price", DType::F32, kBufPrice);
  tables.addColumn(kTable, "qty", DType::I32, kBufQty);

  auto appendRows = [&](const std::vector<float>& prices,
                        const std::vector<std::int32_t>& qtys) {
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBufPrice, prices.data(),
                 static_cast<std::uint32_t>(prices.size() * 4));
    appendRecord(batch, kBufQty, qtys.data(),
                 static_cast<std::uint32_t>(qtys.size() * 4));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  };
  appendRows({10.0f, 25.0f, 5.0f, 40.0f}, {1, 2, 3, 4});

  // -------------------------------------------------------------------------
  // 2) fail-fast typing: a formula on a missing column is rejected at build.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    bool ok = dag.addTransform(
        100, kTable, std::make_unique<FormulaTransform>("missingCol * 2", "y"));
    check(!ok, "REJECT formula referencing unknown column (fail fast)");
    // a filter whose predicate is numeric (not bool) is rejected
    ok = dag.addTransform(101, kTable,
                          std::make_unique<FilterTransform>("price + qty"));
    check(!ok, "REJECT filter predicate that is not boolean");
  }

  // -------------------------------------------------------------------------
  // 3) formula derives the right column; output passes input columns through.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    // notional = price * qty
    check(dag.addTransform(
              100, kTable,
              std::make_unique<FormulaTransform>("price * qty", "notional")),
          "addTransform formula ok");
    check(dag.build(), "build ok");

    // schema check (data-free): output = price,qty,notional.
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->columns.size() == 3 && sch->has("notional"),
          "formula output schema = inputs + new col");

    dag.markTableDirty(kTable);
    auto ran = dag.evaluate();
    check(ran.size() == 1 && ran[0] == 100, "formula node ran once");

    auto nv = dag.columns().viewF32(100, "notional");
    check(nv.valid() && nv.size() == 4, "notional has 4 rows");
    check(nearly(nv[0], 10.0) && nearly(nv[1], 50.0) && nearly(nv[2], 15.0) &&
              nearly(nv[3], 160.0),
          "notional = price*qty per row");
    // passthrough columns preserved
    auto pv = dag.columns().viewF32(100, "price");
    check(pv.valid() && nearly(pv[3], 40.0), "price passed through");
    auto qv = dag.columns().viewI32(100, "qty");
    check(qv.valid() && qv[1] == 2, "qty passed through (i32)");
  }

  // -------------------------------------------------------------------------
  // 4) filter selects the right rows (predicate over columns).
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    // keep rows where price >= 10 && qty < 4  -> rows {10,1},{25,2}  (5 fails ge,
    // 40 has qty 4 which fails <4). Survivors: price 10 (qty1), 25 (qty2).
    check(dag.addTransform(
              100, kTable,
              std::make_unique<FilterTransform>("price >= 10 && qty < 4")),
          "addTransform filter ok");
    check(dag.build(), "build ok");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto pv = dag.columns().viewF32(100, "price");
    check(pv.valid() && pv.size() == 2, "filter kept 2 rows");
    check(nearly(pv[0], 10.0) && nearly(pv[1], 25.0), "filter kept right prices");
    auto qv = dag.columns().viewI32(100, "qty");
    check(qv.valid() && qv[0] == 1 && qv[1] == 2, "filter compacted qty in lockstep");
  }

  // -------------------------------------------------------------------------
  // 5) topological eval ORDER + dirty-gating with a recompute COUNTER, and
  //    the 2-node DAG (filter -> formula) recomputing incrementally on append.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    // node 100: filter price > 8  ; node 200: formula on 100 -> gain = price - 8
    check(dag.addTransform(100, kTable,
                           std::make_unique<FilterTransform>("price > 8")),
          "filter node added");
    check(dag.addTransform(200, 100,
                           std::make_unique<FormulaTransform>("price - 8", "gain")),
          "formula node chained on filter");
    check(dag.build(), "build 2-node chain ok");

    // First eval: both nodes recompute, IN TOPO ORDER (100 before 200).
    dag.markTableDirty(kTable);
    auto ran = dag.evaluate();
    check(ran.size() == 2 && ran[0] == 100 && ran[1] == 200,
          "topo order: filter (100) before formula (200)");
    check(dag.recomputeCount(100) == 1 && dag.recomputeCount(200) == 1,
          "each node recomputed once");

    // Survivors of price>8 from {10,25,5,40}: 10,25,40. gain = price-8.
    auto gv = dag.columns().viewF32(200, "gain");
    check(gv.valid() && gv.size() == 3, "downstream formula sees 3 filtered rows");
    check(nearly(gv[0], 2.0) && nearly(gv[1], 17.0) && nearly(gv[2], 32.0),
          "gain = price-8 over filtered rows");

    // DIRTY-GATING: a second evaluate WITHOUT marking anything dirty recomputes
    // NOTHING (the per-node dirty flag gates recompute).
    auto ran2 = dag.evaluate();
    check(ran2.empty(), "no dirty -> nothing recomputes");
    check(dag.recomputeCount(100) == 1 && dag.recomputeCount(200) == 1,
          "counters unchanged when clean");

    // INCREMENTAL: append a new row to the SOURCE -> mark the table dirty (the
    // ChartSession touched-buffer path) -> BOTH nodes recompute (source dirties
    // 100, the downstream closure dirties 200), and the new row flows through.
    appendRows({100.0f}, {9});  // price 100 > 8 survives; gain = 92
    dag.markTouchedBuffers({kBufPrice, kBufQty});
    auto ran3 = dag.evaluate();
    check(ran3.size() == 2 && ran3[0] == 100 && ran3[1] == 200,
          "append -> both nodes recompute in topo order");
    check(dag.recomputeCount(100) == 2 && dag.recomputeCount(200) == 2,
          "counters bumped to 2 after append");
    auto gv2 = dag.columns().viewF32(200, "gain");
    check(gv2.valid() && gv2.size() == 4 && nearly(gv2[3], 92.0),
          "incremental: new row's gain appears downstream");
  }

  // -------------------------------------------------------------------------
  // 6) dirty-gating across INDEPENDENT branches: dirtying one source does not
  //    recompute a node on a different source. (Two tables, two chains.)
  // -------------------------------------------------------------------------
  {
    const Id kBufB = 300, kTableB = 2;
    tables.defineTable(kTableB, "other");
    tables.addColumn(kTableB, "v", DType::F32, kBufB);
    { std::vector<std::uint8_t> b; float vs[2] = {1.0f, 2.0f};
      appendRecord(b, kBufB, vs, sizeof(vs));
      ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size())); }

    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addSource(kTableB);
    dag.addTransform(100, kTable, std::make_unique<FormulaTransform>("price + 1", "p1"));
    dag.addTransform(200, kTableB, std::make_unique<FormulaTransform>("v * 10", "v10"));
    dag.build();

    // Initial: both dirty (versions advanced) -> both run once.
    dag.markTableDirty(kTable);
    dag.markTableDirty(kTableB);
    dag.evaluate();
    check(dag.recomputeCount(100) == 1 && dag.recomputeCount(200) == 1,
          "both branches ran once initially");

    // Now dirty ONLY table A's buffers: node 100 recomputes, node 200 does NOT.
    dag.markTouchedBuffers({kBufPrice});
    auto ran = dag.evaluate();
    check(ran.size() == 1 && ran[0] == 100, "only table-A branch recomputes");
    check(dag.recomputeCount(100) == 2 && dag.recomputeCount(200) == 1,
          "table-B branch untouched (dirty gating is per-source)");
  }

  // -------------------------------------------------------------------------
  // 7) cycle rejection (build fails). (Construct A->B then B as input of A is
  //    impossible by id ordering; instead make a self-referential edge attempt.)
  //    We approximate: a transform cannot take itself as input (unknown at add),
  //    and a 2-cycle is unrepresentable because addTransform needs the input to
  //    already exist. The acyclicity invariant therefore holds by construction;
  //    we assert build() succeeds for a valid chain (covered above) and that an
  //    unknown-input add is rejected.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    bool ok = dag.addTransform(500, 999 /*unknown*/,
                               std::make_unique<FormulaTransform>("price+1", "z"));
    check(!ok, "REJECT transform with unknown input node");
  }

  std::printf("=== ENC-616a DAG Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
