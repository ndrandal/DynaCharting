// ENC-616b — bin + aggregate + sort transform tests (RESEARCH §5.1/§5.2).
//
// Covers, each as a real DAG NODE (inferSchema typing + DAG eval):
//   * bin     — boundaries + per-row bin index/lo/hi, step-mode and maxbins-mode,
//               pinned extent, and the bin->aggregate histogram composition.
//   * aggregate — groupBy (single + multi key) + every reducer
//               (count/sum/mean/min/max/median, incl. even/odd median).
//   * sort/rank — reorder asc/desc (stable) + a rank column.
//   * fail-fast inferSchema rejections for each transform.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Aggregate.hpp"
#include "dc/transform/transforms/Bin.hpp"
#include "dc/transform/transforms/Sort.hpp"

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
static bool nearly(double x, double y) { return std::fabs(x - y) < 1e-4; }

int main() {
  std::printf("=== ENC-616b bin + aggregate + sort ===\n");

  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);

  // Source table: f32 price, i32 qty, cat symbol.
  const Id kBufPrice = 400, kBufQty = 401, kBufSym = 402, kBufTs = 403, kTable = 1;
  tables.defineTable(kTable, "trades");
  tables.addColumn(kTable, "price", DType::F32, kBufPrice);
  tables.addColumn(kTable, "qty", DType::I32, kBufQty);
  tables.addColumn(kTable, "sym", DType::Cat, kBufSym);
  tables.addColumn(kTable, "ts", DType::Timestamp, kBufTs);

  auto appendRows = [&](const std::vector<float>& prices,
                        const std::vector<std::int32_t>& qtys,
                        const std::vector<std::uint32_t>& syms,
                        const std::vector<std::int64_t>& tss) {
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBufPrice, prices.data(),
                 static_cast<std::uint32_t>(prices.size() * 4));
    appendRecord(batch, kBufQty, qtys.data(),
                 static_cast<std::uint32_t>(qtys.size() * 4));
    appendRecord(batch, kBufSym, syms.data(),
                 static_cast<std::uint32_t>(syms.size() * 4));
    appendRecord(batch, kBufTs, tss.data(),
                 static_cast<std::uint32_t>(tss.size() * 8));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  };
  // prices 0..9 step 1 ; qty mirrors ; sym alternating 0/1 ; ts monotonic.
  appendRows({0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
             {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
             {0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
             {1700000000000LL, 1700000000001LL, 1700000000002LL, 1700000000003LL,
              1700000000004LL, 1700000000005LL, 1700000000006LL, 1700000000007LL,
              1700000000008LL, 1700000000009LL});

  // ---------------------------------------------------------------------------
  // 1) BIN — step mode. step=2 over price 0..9 -> bins [0,2),[2,4),[4,6),[6,8),[8,10).
  //    Edge anchoring: floor(0/2)*2 = 0. price 9 -> bin 4, lo 8, hi 10.
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    check(dag.addTransform(
              100, kTable,
              std::make_unique<BinTransform>(
                  BinTransform::byStep("price", 2.0, "pb"))),
          "addTransform bin(step) ok");
    check(dag.build(), "build bin ok");

    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->has("pb") && sch->has("pb0") && sch->has("pb1"),
          "bin schema: index + lo + hi columns");
    check(sch->find("pb")->dtype == DType::I32, "bin index is i32");
    check(sch->find("pb0")->dtype == DType::F32, "bin lo edge is f32");

    dag.markTableDirty(kTable);
    dag.evaluate();
    auto bi = dag.columns().viewI32(100, "pb");
    auto lo = dag.columns().viewF32(100, "pb0");
    auto hi = dag.columns().viewF32(100, "pb1");
    check(bi.valid() && bi.size() == 10, "bin labelled all 10 rows");
    check(bi[0] == 0 && bi[1] == 0, "price 0,1 -> bin 0");
    check(bi[2] == 1 && bi[3] == 1, "price 2,3 -> bin 1");
    check(bi[9] == 4, "price 9 -> bin 4");
    check(nearly(lo[9], 8.0) && nearly(hi[9], 10.0), "bin 4 edges [8,10)");
    check(nearly(lo[0], 0.0) && nearly(hi[0], 2.0), "bin 0 edges [0,2)");
  }

  // ---------------------------------------------------------------------------
  // 2) BIN — maxbins mode. maxbins=5 over span 9 -> niceStep(9/5=1.8) -> 2.
  //    Same 5 bins as step=2 above.
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<BinTransform>(
                         BinTransform::byMaxBins("price", 5, "mb")));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto bi = dag.columns().viewI32(100, "mb");
    auto hi = dag.columns().viewF32(100, "mb1");
    check(bi.valid() && bi[0] == 0 && bi[2] == 1 && bi[9] == 4,
          "maxbins=5 picks nice step 2 (same as step-mode)");
    check(nearly(hi[0], 2.0), "maxbins bin0 hi = 2");
  }

  // ---------------------------------------------------------------------------
  // 3) BIN -> AGGREGATE histogram composition: groupBy the bin index, count rows.
  //    Each of the 5 bins holds exactly 2 prices.
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<BinTransform>(
                         BinTransform::byStep("price", 2.0, "pb")));
    std::vector<AggMeasure> meas = {{AggOp::Count, "", "n"}};
    dag.addTransform(200, 100,
                     std::make_unique<AggregateTransform>(
                         std::vector<std::string>{"pb"}, meas));
    check(dag.build(), "build bin->aggregate ok");
    dag.markTableDirty(kTable);
    auto ran = dag.evaluate();
    check(ran.size() == 2 && ran[0] == 100 && ran[1] == 200,
          "topo order bin(100) before aggregate(200)");
    auto keys = dag.columns().viewI32(200, "pb");
    auto n = dag.columns().viewI32(200, "n");
    check(keys.valid() && keys.size() == 5, "histogram has 5 bins");
    bool allTwo = n.valid() && n.size() == 5;
    for (std::size_t i = 0; allTwo && i < 5; ++i) allTwo = (n[i] == 2);
    check(allTwo, "each bin counts exactly 2 rows");
  }

  // ---------------------------------------------------------------------------
  // 4) AGGREGATE — groupBy cat sym, every reducer. sym 0 = prices {0,2,4,6,8},
  //    sym 1 = {1,3,5,7,9}.  sym0: count5 sum20 mean4 min0 max8 median4.
  //    sym1: count5 sum25 mean5 min1 max9 median5.
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    std::vector<AggMeasure> meas = {
        {AggOp::Count, "", "n"},      {AggOp::Sum, "price", "s"},
        {AggOp::Mean, "price", "avg"},{AggOp::Min, "price", "lo"},
        {AggOp::Max, "price", "hi"},  {AggOp::Median, "price", "med"}};
    check(dag.addTransform(100, kTable,
                           std::make_unique<AggregateTransform>(
                               std::vector<std::string>{"sym"}, meas)),
          "addTransform aggregate(all reducers) ok");
    check(dag.build(), "build aggregate ok");

    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->has("sym") && sch->has("n") && sch->has("med"),
          "aggregate schema: key + measures");
    check(sch->find("sym")->dtype == DType::Cat, "key keeps Cat dtype");
    check(sch->find("n")->dtype == DType::I32, "count is i32");
    check(sch->find("avg")->dtype == DType::F32, "mean is f32");

    dag.markTableDirty(kTable);
    dag.evaluate();
    auto sym = dag.columns().viewCat(100, "sym");
    auto n = dag.columns().viewI32(100, "n");
    auto s = dag.columns().viewF32(100, "s");
    auto avg = dag.columns().viewF32(100, "avg");
    auto lo = dag.columns().viewF32(100, "lo");
    auto hi = dag.columns().viewF32(100, "hi");
    auto med = dag.columns().viewF32(100, "med");
    check(sym.valid() && sym.size() == 2, "two groups (sym 0 and 1)");
    // groups appear in discovery order: sym 0 first (row 0), then sym 1 (row 1).
    check(sym[0] == 0 && sym[1] == 1, "groups in discovery order");
    check(n[0] == 5 && n[1] == 5, "count = 5 per group");
    check(nearly(s[0], 20.0) && nearly(s[1], 25.0), "sum 20 / 25");
    check(nearly(avg[0], 4.0) && nearly(avg[1], 5.0), "mean 4 / 5");
    check(nearly(lo[0], 0.0) && nearly(lo[1], 1.0), "min 0 / 1");
    check(nearly(hi[0], 8.0) && nearly(hi[1], 9.0), "max 8 / 9");
    check(nearly(med[0], 4.0) && nearly(med[1], 5.0), "median(odd) 4 / 5");
  }

  // ---------------------------------------------------------------------------
  // 5) AGGREGATE — even-count median averages the two central values.
  //    A second table with prices {1,2,3,4} all one group -> median = (2+3)/2 = 2.5.
  // ---------------------------------------------------------------------------
  {
    const Id kBufP2 = 500, kBufG2 = 501, kTable2 = 2;
    tables.defineTable(kTable2, "t2");
    tables.addColumn(kTable2, "v", DType::F32, kBufP2);
    tables.addColumn(kTable2, "g", DType::I32, kBufG2);
    { std::vector<std::uint8_t> b;
      float vs[4] = {1, 2, 3, 4};
      std::int32_t gs[4] = {7, 7, 7, 7};
      appendRecord(b, kBufP2, vs, sizeof(vs));
      appendRecord(b, kBufG2, gs, sizeof(gs));
      ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size())); }

    TransformDag dag(tables, src);
    dag.addSource(kTable2);
    std::vector<AggMeasure> meas = {{AggOp::Median, "v", "med"},
                                    {AggOp::Mean, "v", "avg"}};
    dag.addTransform(100, kTable2,
                     std::make_unique<AggregateTransform>(
                         std::vector<std::string>{"g"}, meas));
    dag.build();
    dag.markTableDirty(kTable2);
    dag.evaluate();
    auto med = dag.columns().viewF32(100, "med");
    auto avg = dag.columns().viewF32(100, "avg");
    check(med.valid() && med.size() == 1, "single group");
    check(nearly(med[0], 2.5), "even-count median = (2+3)/2 = 2.5");
    check(nearly(avg[0], 2.5), "mean of 1..4 = 2.5");
  }

  // ---------------------------------------------------------------------------
  // 6) AGGREGATE — multi-key groupBy (qty parity is not a column, so group by
  //    sym AND a binned price). Use bin->aggregate with two keys.
  //    bin price step 5 -> bins [0,5)=0, [5,10)=1. Cross sym(0/1) x binPrice(0/1)
  //    -> 4 groups, each with the right count.
  //    sym0 prices {0,2,4,6,8}: bin0={0,2,4}(3) bin1={6,8}(2).
  //    sym1 prices {1,3,5,7,9}: bin0={1,3}(2)   bin1={5,7,9}(3).
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<BinTransform>(
                         BinTransform::byStep("price", 5.0, "pb")));
    std::vector<AggMeasure> meas = {{AggOp::Count, "", "n"}};
    dag.addTransform(200, 100,
                     std::make_unique<AggregateTransform>(
                         std::vector<std::string>{"sym", "pb"}, meas));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto sym = dag.columns().viewCat(200, "sym");
    auto pb = dag.columns().viewI32(200, "pb");
    auto n = dag.columns().viewI32(200, "n");
    check(sym.valid() && sym.size() == 4, "4 (sym x binPrice) groups");
    // verify counts by matching key tuples regardless of order.
    auto countFor = [&](std::uint32_t s, std::int32_t b) -> int {
      for (std::size_t i = 0; i < sym.size(); ++i)
        if (sym[i] == s && pb[i] == b) return n[i];
      return -1;
    };
    check(countFor(0, 0) == 3, "sym0 binPrice0 = 3");
    check(countFor(0, 1) == 2, "sym0 binPrice1 = 2");
    check(countFor(1, 0) == 2, "sym1 binPrice0 = 2");
    check(countFor(1, 1) == 3, "sym1 binPrice1 = 3");
  }

  // ---------------------------------------------------------------------------
  // 7) SORT — reorder ascending + descending (stable, dtype-preserving).
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<SortTransform>(
                         SortTransform::reorder("price", /*asc=*/false)));
    check(dag.build(), "build sort(desc) ok");
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->columns.size() == 4, "sort reorder preserves schema");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto pv = dag.columns().viewF32(100, "price");
    auto qv = dag.columns().viewI32(100, "qty");
    check(pv.valid() && pv.size() == 10, "sort kept all rows");
    check(nearly(pv[0], 9.0) && nearly(pv[9], 0.0), "descending by price");
    // qty mirrors price, so it must be permuted in lockstep.
    check(qv[0] == 9 && qv[9] == 0, "qty permuted in lockstep with price");
  }
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<SortTransform>(
                         SortTransform::reorder("price", /*asc=*/true)));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto pv = dag.columns().viewF32(100, "price");
    check(pv.valid() && nearly(pv[0], 0.0) && nearly(pv[9], 9.0),
          "ascending by price");
  }

  // ---------------------------------------------------------------------------
  // 8) RANK — add an i32 rank column, original row order preserved. Ascending:
  //    smallest price gets rank 0. price col is 0..9 already ascending, so
  //    rank == row index. Descending: rank = 9 - row.
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<SortTransform>(
                         SortTransform::rank("price", "r", /*asc=*/true)));
    check(dag.build(), "build rank(asc) ok");
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->has("r") && sch->find("r")->dtype == DType::I32,
          "rank adds an i32 column");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto pv = dag.columns().viewF32(100, "price");
    auto rv = dag.columns().viewI32(100, "r");
    check(pv.valid() && nearly(pv[0], 0.0) && nearly(pv[5], 5.0),
          "rank keeps original row order");
    check(rv.valid() && rv[0] == 0 && rv[5] == 5 && rv[9] == 9,
          "ascending rank == position for sorted input");
  }
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<SortTransform>(
                         SortTransform::rank("price", "r", /*asc=*/false)));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto rv = dag.columns().viewI32(100, "r");
    check(rv.valid() && rv[0] == 9 && rv[9] == 0,
          "descending rank: largest price -> rank 0");
  }

  // ---------------------------------------------------------------------------
  // 9) FAIL-FAST typing rejections (each transform's inferSchema, data-free).
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    check(!dag.addTransform(100, kTable,
                            std::make_unique<BinTransform>(
                                BinTransform::byStep("nope", 1.0, "b"))),
          "REJECT bin on unknown field");
    check(!dag.addTransform(101, kTable,
                            std::make_unique<BinTransform>(
                                BinTransform::byStep("sym", 1.0, "b"))),
          "REJECT bin on a non-numeric (cat) field");
    check(!dag.addTransform(102, kTable,
                            std::make_unique<BinTransform>(
                                BinTransform::byStep("price", 0.0, "b"))),
          "REJECT bin step <= 0");
    check(!dag.addTransform(103, kTable,
                            std::make_unique<BinTransform>(
                                BinTransform::byStep("price", 1.0, "price"))),
          "REJECT bin output colliding with input column");
    // aggregate
    std::vector<AggMeasure> badKey = {{AggOp::Count, "", "n"}};
    check(!dag.addTransform(104, kTable,
                            std::make_unique<AggregateTransform>(
                                std::vector<std::string>{"nope"}, badKey)),
          "REJECT aggregate groupBy unknown column");
    std::vector<AggMeasure> badField = {{AggOp::Sum, "nope", "s"}};
    check(!dag.addTransform(105, kTable,
                            std::make_unique<AggregateTransform>(
                                std::vector<std::string>{"sym"}, badField)),
          "REJECT aggregate sum on unknown field");
    // Cat codes widen to a number (consistent with the resolver) so a Cat measure
    // is ACCEPTED; a Timestamp is the genuinely non-numeric reject (no epoch-ms sum).
    std::vector<AggMeasure> nonNum = {{AggOp::Sum, "ts", "s"}};
    check(!dag.addTransform(106, kTable,
                            std::make_unique<AggregateTransform>(
                                std::vector<std::string>{"price"}, nonNum)),
          "REJECT aggregate sum on a timestamp (non-numeric) field");
    std::vector<AggMeasure> noGroup;
    check(!dag.addTransform(107, kTable,
                            std::make_unique<AggregateTransform>(
                                std::vector<std::string>{}, noGroup)),
          "REJECT aggregate with no groupBy");
    // sort/rank
    check(!dag.addTransform(108, kTable,
                            std::make_unique<SortTransform>(
                                SortTransform::reorder("nope"))),
          "REJECT sort on unknown key");
    check(!dag.addTransform(109, kTable,
                            std::make_unique<SortTransform>(
                                SortTransform::rank("price", "price"))),
          "REJECT rank output colliding with input column");
  }

  std::printf("=== ENC-616b Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
