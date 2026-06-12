// ENC-593/594/595 — Table-layer tests: long→wide pivot ingest, stable per-row
// identity, and the generic reactive dirty/recompute mechanism. All build on the
// ENC-592 TableStore + the UNCHANGED 13-byte ingest feed.
#include "dc/data/PivotIngest.hpp"
#include "dc/data/ReactiveGraph.hpp"
#include "dc/data/RowIdentity.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

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

// ===========================================================================
// ENC-593 — long→wide pivot ingest
// ===========================================================================
static void testPivot() {
  std::printf("\n--- ENC-593: long->wide pivot ingest ---\n");

  dc::IngestProcessor ingest;
  dc::TableStore tables;
  auto src = dc::makeBufferByteSource(ingest);

  const dc::Id kTable = 1;
  const dc::Id kBufT = 200, kBufOpen = 201, kBufHigh = 202, kBufLow = 203,
               kBufClose = 204, kBufVol = 205;

  tables.defineTable(kTable, "ohlc");
  tables.addColumn(kTable, "t", dc::DType::Timestamp, kBufT);
  tables.addColumn(kTable, "open", dc::DType::F32, kBufOpen);
  tables.addColumn(kTable, "high", dc::DType::F32, kBufHigh);
  tables.addColumn(kTable, "low", dc::DType::F32, kBufLow);
  tables.addColumn(kTable, "close", dc::DType::F32, kBufClose);
  tables.addColumn(kTable, "volume", dc::DType::I32, kBufVol);

  dc::PivotIngest pivot(tables, ingest);
  check(pivot.setTable(kTable), "pivot setTable");
  check(pivot.setRowKeyColumn("t"), "pivot rowKey column = t");
  check(pivot.mapField("o", "open"), "map field o->open");
  check(pivot.mapField("h", "high"), "map field h->high");
  check(pivot.mapField("l", "low"), "map field l->low");
  check(pivot.mapField("c", "close"), "map field c->close");
  check(pivot.mapField("v", "volume"), "map field v->volume");
  check(!pivot.mapField("x", "nope"), "map field to unknown column rejected");

  // --- one complete row, fields pushed OUT OF ORDER -----------------------
  const std::int64_t t0 = 1700000000000LL;
  check(pivot.pushEvent(t0, "c", dc::pvF32(10.5f)), "push close");
  check(pivot.pushEvent(t0, "o", dc::pvF32(10.0f)), "push open (out of order)");
  check(pivot.pushEvent(t0, "v", dc::pvI32(1000)), "push volume");
  check(pivot.pushEvent(t0, "h", dc::pvF32(11.0f)), "push high");
  check(pivot.pushEvent(t0, "l", dc::pvF32(9.5f)), "push low");
  check(!pivot.pushEvent(t0, "o", dc::pvI32(1)), "dtype-mismatch event rejected");
  check(pivot.pendingRowCount() == 1, "one pending row accumulating");
  check(tables.rowCount(kTable, src) == 0, "nothing appended before flush");

  check(pivot.flushAll() == 1, "flushAll appends 1 row");
  check(tables.rowCount(kTable, src) == 1, "table has 1 row after flush");
  check(tables.rowCountConsistent(kTable, src),
        "all columns equal-length after pivot");

  // The wide row joined the 5 fields under one t.
  {
    auto tv = tables.viewTimestamp(kTable, "t", src);
    auto ov = tables.viewF32(kTable, "open", src);
    auto hv = tables.viewF32(kTable, "high", src);
    auto lv = tables.viewF32(kTable, "low", src);
    auto cv = tables.viewF32(kTable, "close", src);
    auto vv = tables.viewI32(kTable, "volume", src);
    check(tv.valid() && tv[0] == t0, "rowKey t landed in t column");
    check(ov[0] == 10.0f && hv[0] == 11.0f && lv[0] == 9.5f && cv[0] == 10.5f,
          "OHLC fields pivoted into one wide row");
    check(vv[0] == 1000, "volume (i32) pivoted into the same row");
  }

  // --- a row MISSING a field -> typed sentinel ----------------------------
  const std::int64_t t1 = 1700000060000LL;
  pivot.pushEvent(t1, "o", dc::pvF32(20.0f));
  pivot.pushEvent(t1, "c", dc::pvF32(21.0f));
  // high/low/volume never arrive for t1.
  check(pivot.flushAll() == 1, "flush partial row");
  check(tables.rowCount(kTable, src) == 2, "table has 2 rows");
  {
    auto hv = tables.viewF32(kTable, "high", src);
    auto vv = tables.viewI32(kTable, "volume", src);
    auto ov = tables.viewF32(kTable, "open", src);
    check(ov[1] == 20.0f, "present field on partial row preserved");
    check(std::isnan(hv[1]), "missing f32 field -> NaN sentinel");
    check(vv[1] == 0, "missing i32 field -> 0 sentinel");
  }
  check(tables.rowCountConsistent(kTable, src),
        "columns stay equal-length even with missing fields");

  // --- auto-flush on a newer rowKey (streaming append-only) ---------------
  {
    const std::int64_t t2 = 1700000120000LL, t3 = 1700000180000LL;
    pivot.pushEvent(t2, "o", dc::pvF32(30.0f));
    pivot.pushEvent(t2, "c", dc::pvF32(31.0f));
    check(pivot.pendingRowCount() == 1, "t2 row open");
    // A strictly-newer rowKey arrives -> t2 auto-flushes.
    pivot.pushEvent(t3, "o", dc::pvF32(40.0f));
    check(tables.rowCount(kTable, src) == 3, "t2 auto-flushed on newer rowKey");
    check(pivot.pendingRowCount() == 1, "only t3 still open");
    pivot.flushAll();
    auto ov = tables.viewF32(kTable, "open", src);
    check(ov.size() == 4 && ov[2] == 30.0f && ov[3] == 40.0f,
          "auto-flush preserved row order");
  }

  // --- groupKey keeps concurrent series in separate rows ------------------
  {
    dc::IngestProcessor ing2;
    dc::TableStore tab2;
    auto src2 = dc::makeBufferByteSource(ing2);
    const dc::Id kT = 2;
    tab2.defineTable(kT, "multi");
    tab2.addColumn(kT, "t", dc::DType::Timestamp, 300);
    tab2.addColumn(kT, "sym", dc::DType::Cat, 301);
    tab2.addColumn(kT, "px", dc::DType::F32, 302);

    dc::PivotIngest p2(tab2, ing2);
    p2.setTable(kT);
    p2.setRowKeyColumn("t");
    check(p2.setGroupKeyColumn("sym"), "set group column");
    p2.mapField("price", "px");

    dc::CatDictionary* d = tab2.catDict(kT, "sym");
    std::uint32_t aapl = d->intern("AAPL");
    std::uint32_t msft = d->intern("MSFT");

    const std::int64_t t = 1700000000000LL;
    // Two series share the SAME t but must NOT collide into one row.
    p2.pushEvent(t, "price", dc::pvF32(150.0f), aapl);
    p2.pushEvent(t, "price", dc::pvF32(250.0f), msft);
    check(p2.pendingRowCount() == 2, "same t, two groups -> two rows");
    check(p2.flushAll() == 2, "both group rows flushed");
    check(tab2.rowCount(kT, src2) == 2, "two rows in grouped table");
    auto sv = tab2.viewCat(kT, "sym", src2);
    auto pv = tab2.viewF32(kT, "px", src2);
    check(sv[0] == aapl && pv[0] == 150.0f, "AAPL row intact");
    check(sv[1] == msft && pv[1] == 250.0f, "MSFT row intact");
  }

  // --- pushCatEvent interns label -> code ---------------------------------
  {
    dc::IngestProcessor ing3;
    dc::TableStore tab3;
    auto src3 = dc::makeBufferByteSource(ing3);
    const dc::Id kT = 3;
    tab3.defineTable(kT, "labelled");
    tab3.addColumn(kT, "t", dc::DType::Timestamp, 400);
    tab3.addColumn(kT, "tag", dc::DType::Cat, 401);
    dc::PivotIngest p3(tab3, ing3);
    p3.setTable(kT);
    p3.setRowKeyColumn("t");
    p3.mapField("label", "tag");
    check(p3.pushCatEvent(1000, "label", "buy"), "pushCatEvent interns label");
    p3.flushAll();
    auto cv = tab3.viewCat(kT, "tag", src3);
    const dc::CatDictionary* dd = tab3.catDict(kT, "tag");
    check(cv.valid() && dd->labelOf(cv[0]) == "buy",
          "cat label round-trips through pivot");
  }
}

// ===========================================================================
// ENC-594 — stable per-row identity
// ===========================================================================
static void testRowIdentity() {
  std::printf("\n--- ENC-594: stable per-row identity ---\n");

  dc::IngestProcessor ingest;
  dc::TableStore tables;
  auto src = dc::makeBufferByteSource(ingest);

  const dc::Id kTable = 1;
  const dc::Id kBufVal = 500, kBufId = 501;
  tables.defineTable(kTable, "rows");
  tables.addColumn(kTable, "val", dc::DType::F32, kBufVal);
  tables.addColumn(kTable, "rowId", dc::DType::I32, kBufId);

  dc::RowIdentity rid;
  check(!rid.bound(), "unbound initially");
  // Binding to a non-i32 column is rejected.
  check(!rid.bind(tables, kTable, "val"), "bind rejects non-i32 column");
  check(rid.bind(tables, kTable, "rowId"), "bind to i32 column ok");
  check(rid.bound(), "bound after bind");
  // The designation landed in the TableStore hook (ENC-592 slot).
  check(tables.rowIdColumn(kTable).value_or("") == "rowId",
        "rowIdColumn designation recorded in TableStore");

  // Helper to append `n` data rows + their ids in lockstep.
  auto appendRows = [&](std::size_t n, float base) {
    std::vector<float> vals(n);
    for (std::size_t i = 0; i < n; ++i) vals[i] = base + static_cast<float>(i);
    std::vector<std::uint8_t> batch;
    auto u32 = [&batch](std::uint32_t v) {
      batch.push_back(v & 0xFF);
      batch.push_back((v >> 8) & 0xFF);
      batch.push_back((v >> 16) & 0xFF);
      batch.push_back((v >> 24) & 0xFF);
    };
    batch.push_back(1);
    u32(static_cast<std::uint32_t>(kBufVal));
    u32(0);
    u32(static_cast<std::uint32_t>(n * sizeof(float)));
    const auto* p = reinterpret_cast<const std::uint8_t*>(vals.data());
    batch.insert(batch.end(), p, p + n * sizeof(float));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    rid.appendIds(ingest, n);
  };

  appendRows(3, 10.0f);
  check(rid.nextId() == 3, "3 ids assigned");
  check(rid.liveRowCount(src) == 3, "3 live id rows");
  check(rid.idAt(src, 0) == 0 && rid.idAt(src, 1) == 1 && rid.idAt(src, 2) == 2,
        "ids dense from 0");
  check(rid.indexOfId(src, 1).value_or(99) == 1, "indexOfId maps id->index");
  check(!rid.indexOfId(src, 7).has_value(), "indexOfId unknown id -> nullopt");

  // --- ids stay STABLE across appends -------------------------------------
  std::int32_t id0 = rid.idAt(src, 0);
  std::int32_t id2 = rid.idAt(src, 2);
  appendRows(2, 100.0f);
  check(rid.idAt(src, 0) == id0, "existing row 0 keeps its id across append");
  check(rid.idAt(src, 2) == id2, "existing row 2 keeps its id across append");
  check(rid.idAt(src, 3) == 3 && rid.idAt(src, 4) == 4,
        "new rows get fresh monotonic ids");
  check(rid.nextId() == 5, "counter advanced to 5");

  // --- ids survive RETENTION/eviction -------------------------------------
  // Evict the oldest 2 rows from BOTH columns (front bytes, lockstep) — exactly
  // what smart retention does. Surviving rows keep their exact ids.
  ingest.evictFront(kBufVal, 2 * sizeof(float));
  ingest.evictFront(kBufId, 2 * sizeof(std::int32_t));
  check(rid.liveRowCount(src) == 3, "3 rows survive eviction");
  // The row that WAS index 2 (id 2) is now index 0 — but its id is still 2.
  check(rid.idAt(src, 0) == 2, "surviving front row keeps durable id (2)");
  check(rid.idAt(src, 2) == 4, "surviving back row keeps durable id (4)");
  check(rid.indexOfId(src, 2).value_or(99) == 0,
        "indexOfId tracks the shifted position of a durable id");
  check(!rid.indexOfId(src, 0).has_value(),
        "evicted id 0 no longer resolves (not reused)");

  // A fresh append after eviction still gets id 5 (never reuses 0/1).
  appendRows(1, 999.0f);
  check(rid.idAt(src, rid.liveRowCount(src) - 1) == 5,
        "post-eviction append continues monotonic ids (no reuse)");
}

// ===========================================================================
// ENC-595 — generic reactive dirty/recompute mechanism
// ===========================================================================
static void testReactive() {
  std::printf("\n--- ENC-595: generic reactive dirty mechanism ---\n");

  dc::ReactiveGraph graph;

  // Dependents (opaque handles — e.g. recipe handles / transform nodes).
  const dc::DependentId kEncodePass = 1;
  const dc::DependentId kHoverPanel = 2;

  // The encode pass depends on a DATA buffer (a table column).
  const dc::Id kDataBuf = 700;
  graph.addDependency(kEncodePass, dc::dataInput(kDataBuf));

  // The hover panel depends on a future-style interaction SIGNAL node — proving
  // the mechanism is generic over input KIND, not hardcoded to data.
  const dc::Id kSelectionSignal = 1;
  graph.addDependency(kHoverPanel, dc::signalInput(kSelectionSignal));

  check(graph.dependsOn(kEncodePass, dc::dataInput(kDataBuf)),
        "encode pass registered on data input");
  check(graph.dependsOn(kHoverPanel, dc::signalInput(kSelectionSignal)),
        "hover panel registered on signal input");
  // Same numeric key, different kind == different node (no collision).
  check(!graph.dependsOn(kHoverPanel, dc::dataInput(kSelectionSignal)),
        "data input 1 != signal input 1 (kind disambiguates)");

  // --- a DATA input firing recomputes only its dependents -----------------
  std::size_t n = graph.markDataBuffersDirty({kDataBuf});
  check(n == 1, "marking data buffer scheduled 1 dependent");
  check(graph.isPending(kEncodePass), "encode pass pending after data dirty");
  check(!graph.isPending(kHoverPanel), "hover panel NOT pending (data only)");
  {
    auto fired = graph.drain();
    check(fired.size() == 1 && fired[0] == kEncodePass,
          "drain surfaces only the data dependent");
    check(graph.pendingCount() == 0, "pending cleared after drain");
  }

  // --- a NON-DATA (signal) input firing recomputes its dependents ----------
  // THE crux of the ticket: a stub interaction-signal node uses the SAME path.
  std::size_t s = graph.markSignalDirty(kSelectionSignal);
  check(s == 1, "marking signal scheduled 1 dependent");
  check(graph.isPending(kHoverPanel), "hover panel pending after signal dirty");
  check(!graph.isPending(kEncodePass), "encode pass NOT pending (signal only)");
  {
    auto fired = graph.drain();
    check(fired.size() == 1 && fired[0] == kHoverPanel,
          "non-data signal triggers recompute of its dependent");
  }

  // --- one input fanning out to MANY dependents ---------------------------
  graph.addDependency(kHoverPanel, dc::dataInput(kDataBuf));  // panel also on data
  graph.markDataBuffersDirty({kDataBuf});
  {
    auto fired = graph.drain();
    check(fired.size() == 2 && fired[0] == kEncodePass && fired[1] == kHoverPanel,
          "data input fans out to both dependents, sorted");
  }

  // --- TableStore version() drive surface ---------------------------------
  // A structural table change (e.g. a pivot adding a column) advances version()
  // even with no buffer byte appended; syncTableVersions propagates it.
  {
    dc::TableStore tables;
    const dc::Id kTbl = 42;
    tables.defineTable(kTbl, "t");
    dc::ReactiveGraph g2;
    const dc::DependentId kDomainScan = 9;
    g2.addDependency(kDomainScan, dc::dataInput(kTbl));  // keyed by table id

    // First sync establishes the baseline -> dirty (first sight).
    g2.syncTableVersions({{kTbl, tables.version(kTbl)}});
    check(g2.isPending(kDomainScan), "first version sight marks dirty");
    g2.drain();

    // No change -> no fire.
    std::size_t c0 = g2.syncTableVersions({{kTbl, tables.version(kTbl)}});
    check(c0 == 0 && !g2.isPending(kDomainScan),
          "unchanged version does not refire");

    // Structural change bumps version -> fires.
    tables.addColumn(kTbl, "c", dc::DType::F32, 800);
    std::size_t c1 = g2.syncTableVersions({{kTbl, tables.version(kTbl)}});
    check(c1 == 1 && g2.isPending(kDomainScan),
          "version bump (addColumn) triggers recompute via version() surface");
  }

  // --- removeDependent drops edges ----------------------------------------
  {
    dc::ReactiveGraph g3;
    g3.addDependency(5, dc::dataInput(900));
    g3.removeDependent(5);
    check(g3.markDataBuffersDirty({900}) == 0,
          "removed dependent no longer scheduled");
    check(!g3.dependsOn(5, dc::dataInput(900)), "edge gone after removeDependent");
  }
}

// ===========================================================================
// Integration: pivot + row identity together (the realistic table-layer flow)
// ===========================================================================
static void testPivotWithIdentity() {
  std::printf("\n--- ENC-593+594: pivot drives row identity ---\n");

  dc::IngestProcessor ingest;
  dc::TableStore tables;
  auto src = dc::makeBufferByteSource(ingest);

  const dc::Id kTable = 1;
  tables.defineTable(kTable, "ohlc_id");
  tables.addColumn(kTable, "t", dc::DType::Timestamp, 600);
  tables.addColumn(kTable, "close", dc::DType::F32, 601);
  tables.addColumn(kTable, "rowId", dc::DType::I32, 602);

  dc::RowIdentity rid;
  rid.bind(tables, kTable, "rowId");

  dc::PivotIngest pivot(tables, ingest);
  pivot.setTable(kTable);
  pivot.setRowKeyColumn("t");
  pivot.mapField("c", "close");
  pivot.setRowIdentity(&rid);

  pivot.pushEvent(1000, "c", dc::pvF32(1.0f));
  pivot.pushEvent(2000, "c", dc::pvF32(2.0f));  // auto-flushes row @1000
  pivot.pushEvent(3000, "c", dc::pvF32(3.0f));  // auto-flushes row @2000
  pivot.flushAll();

  check(tables.rowCount(kTable, src) == 3, "3 pivoted rows");
  check(tables.rowCountConsistent(kTable, src),
        "data + id columns stay equal-length through pivot");
  auto idv = tables.viewI32(kTable, "rowId", src);
  check(idv.valid() && idv.size() == 3 && idv[0] == 0 && idv[1] == 1 && idv[2] == 2,
        "pivot assigned dense durable ids per row");
  check(rid.indexOfId(src, 1).value_or(99) == 1,
        "durable id queryable after pivot");
}

int main() {
  std::printf("=== ENC-593/594/595 Table layer ===\n");
  testPivot();
  testRowIdentity();
  testReactive();
  testPivotWithIdentity();
  std::printf("\n=== Table-layer results: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
