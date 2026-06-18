// ENC-625 (B3) — SelectionPredicate: compile a SignalStore selection to a per-row
// boolean column over a live table (row-based + value-based parts combined), and
// conditionalColorColumn: map that boolean to per-row packed RGBA8 (the §6.1
// conditional color encoding feeding the ENC-608 setColorField path).
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/SelectionPredicate.hpp"
#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/ColumnStore.hpp"

#include <cstdint>
#include <cstdio>
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
  std::printf("=== ENC-625 (B3) SelectionPredicate + conditional color ===\n");

  // Table: rowid (i32 RowIdentity-style) + price (f32). 4 rows.
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufRowid = 300, kBufPrice = 301, kTable = 1;
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
  check(tables.rowCount(kTable, src) == 4, "table has 4 rows");

  const Id kSig = 7000;
  SignalStore store;
  store.define(kSig, PointSelection{});  // empty initially

  auto materializeBools = [&](const char* col) {
    SelectionPredicate pred(store, kSig, "rowid", "price");
    ColumnStore cs;
    pred.materialize(tables, kTable, src, cs, 1, col);
    auto v = cs.viewI32(1, col);
    std::vector<int> out;
    for (std::size_t i = 0; i < v.count; ++i) out.push_back(v[i]);
    return out;
  };
  auto eq = [](const std::vector<int>& a, std::vector<int> b) { return a == b; };

  // Empty selection: everything passes.
  check(eq(materializeBools("e"), {1, 1, 1, 1}), "empty selection -> all rows pass");

  // Point selection by row id 101 (the 2nd row).
  store.set(kSig, PointSelection{101});
  check(eq(materializeBools("p"), {0, 1, 0, 0}), "point selection -> only row 101");

  // Interval on price [0,20]: rows 10 and 5 pass; 25 and 40 fail.
  store.set(kSig, IntervalSelection{kBufPrice, 0.0, 20.0});
  check(eq(materializeBools("i"), {1, 0, 1, 0}), "interval [0,20] on price");

  // Multi: explicit rows {100,103} (row-based) OR intervals (none) -> rows 1 and 4.
  {
    MultiSelection m;
    m.rows = {100, 103};
    store.set(kSig, std::move(m));
  }
  check(eq(materializeBools("m"), {1, 0, 0, 1}), "multi by explicit rows {100,103}");

  // Multi by value intervals: price in [0,12] OR [38,50]. Prices {10,25,5,40}:
  // 10 and 5 fall in [0,12], 40 in [38,50], 25 in neither -> {1,0,1,1}.
  {
    MultiSelection m;
    m.intervals.push_back(IntervalSelection{kBufPrice, 0.0, 12.0});
    m.intervals.push_back(IntervalSelection{kBufPrice, 38.0, 50.0});
    store.set(kSig, std::move(m));
  }
  check(eq(materializeBools("mi"), {1, 0, 1, 1}), "multi by value intervals");

  // ---- conditional color encoding ------------------------------------------
  // Boolean column [1,0,1,0] -> selected/unselected packed RGBA8.
  {
    store.set(kSig, IntervalSelection{kBufPrice, 0.0, 20.0});  // -> {1,0,1,0}
    SelectionPredicate pred(store, kSig, "rowid", "price");
    ColumnStore bools;
    pred.materialize(tables, kTable, src, bools, 1, "sel");

    const std::uint32_t kSelected = 0xFF0000FFu;    // opaque red-ish (R,G,B,A bytes)
    const std::uint32_t kUnselected = 0x80808080u;  // translucent grey
    ColumnStore colors;
    std::size_t n = conditionalColorColumn(bools, 1, "sel", kSelected, kUnselected,
                                           colors, 2, "color");
    check(n == 4, "conditional color wrote 4 rows");
    auto cv = colors.viewI32(2, "color");
    check(cv.valid() && cv.count == 4, "color column valid, 4 rows");
    auto rgba = [&](std::size_t i) { return static_cast<std::uint32_t>(cv[i]); };
    check(rgba(0) == kSelected, "row0 selected -> selected color");
    check(rgba(1) == kUnselected, "row1 not selected -> unselected color");
    check(rgba(2) == kSelected, "row2 selected -> selected color");
    check(rgba(3) == kUnselected, "row3 not selected -> unselected color");

    // missing boolean column -> 0 rows, no crash.
    ColumnStore empty;
    check(conditionalColorColumn(bools, 1, "nope", kSelected, kUnselected, empty, 9,
                                 "c") == 0,
          "missing bool column -> 0 rows");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
