// ENC-654 (B5a) — RenderBridge: a selection-filtered TransformDag node materializes
// into a TableStore that the EXISTING encode pass renders, proving selection output
// reaches geometry (the gap that blocked the whole interaction layer being visible).
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/RenderBridge.hpp"
#include "dc/interaction/SignalStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/SelectionFilter.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

static int passed = 0, failed = 0;
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
  out.push_back(1); u32(static_cast<std::uint32_t>(bufferId)); u32(0); u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

int main() {
  std::printf("=== ENC-654 (B5a) RenderBridge ===\n");

  // Source: rowid (i32) + price (f32), 4 rows.
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufRowid = 600, kBufPrice = 601, kTable = 1;
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

  // DAG: selectionFilter (Filter) on signal interval [0,20] -> survivors {10,5}.
  SignalStore store;
  const Id kSig = 9000;
  TransformDag dag(tables, src);
  store.setGraph(&dag.reactive());
  store.define(kSig, IntervalSelection{kBufPrice, 0.0, 20.0});
  const NodeId kFilter = 100;
  dag.addSource(kTable);
  dag.addTransform(kFilter, kTable,
                   std::make_unique<SelectionFilterTransform>(
                       &store, kSig, "rowid", "price",
                       SelectionFilterTransform::Mode::Filter));
  dag.addSignalDependency(kFilter, kSig);
  dag.build();
  dag.markTableDirty(kTable);
  dag.evaluate();

  // Materialize the filtered node into a fresh table the encode pass can read.
  IngestProcessor outIngest;
  TableStore outTables;
  auto outSrc = makeBufferByteSource(outIngest);
  const Id kOutTable = 1;
  std::size_t rows = materializeNodeToTable(*dag.schemaOf(kFilter), dag.columns(),
                                            kFilter, outIngest, outTables, kOutTable,
                                            500);
  check(rows == 2, "materialized 2 survivor rows");
  check(outTables.rowCount(kOutTable, outSrc) == 2, "out table row count == 2");
  {
    auto pv = outTables.viewF32(kOutTable, "price", outSrc);
    auto rv = outTables.viewI32(kOutTable, "rowid", outSrc);
    check(pv.valid() && pv.count == 2 && pv[0] == 10.0f && pv[1] == 5.0f,
          "out price column = filtered {10,5}");
    check(rv.valid() && rv[0] == 100 && rv[1] == 102, "out rowid column = {100,102}");
  }

  // The EXISTING encode pass renders the materialized (filtered) table unchanged.
  {
    EncodePass pass;
    Encoding enc;
    enc.field(Channel::X, "price").field(Channel::Y, "price");
    auto res = pass.compile(Mark::Point, enc, outTables, kOutTable, outSrc, 10, 20, 30);
    check(res.ok, "encode point mark from materialized table OK");
    // points@1 packs Pos2_Clip (8B) per vertex; 2 survivor rows -> 16 bytes.
    check(res.bytes.size() == 16, "encoded geometry has 2 vertices (filtered rows)");
  }

  // Re-materialize after a selection change refreshes in place (same buffer ids).
  store.set(kSig, PointSelection{103});  // rowid 103 -> price 40, 1 row
  dag.evaluate();
  rows = materializeNodeToTable(*dag.schemaOf(kFilter), dag.columns(), kFilter,
                                outIngest, outTables, kOutTable, 500);
  check(rows == 1, "re-materialize after selection change -> 1 row");
  {
    auto pv = outTables.viewF32(kOutTable, "price", outSrc);
    check(pv.valid() && pv.count == 1 && pv[0] == 40.0f, "refreshed table = {40}");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
