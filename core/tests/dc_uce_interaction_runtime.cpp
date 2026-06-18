// ENC-655 (B5b) — InteractionRuntime: a SignalStore mutation flows through the DAG
// (selectionFilter) and the render bridge to UPDATED geometry, closing the
// signal -> transform -> encode -> pixels loop headlessly.
#include "dc/data/TableStore.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/InteractionRuntime.hpp"
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
  std::printf("=== ENC-655 (B5b) InteractionRuntime ===\n");

  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufRowid = 700, kBufPrice = 701, kTable = 1;
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

  // DAG: selectionFilter on a brush signal.
  SignalStore store;
  const Id kSig = 9100;
  TransformDag dag(tables, src);
  store.setGraph(&dag.reactive());
  store.define(kSig, PointSelection{});  // empty -> all rows pass
  const NodeId kFilter = 100;
  dag.addSource(kTable);
  dag.addTransform(kFilter, kTable,
                   std::make_unique<SelectionFilterTransform>(
                       &store, kSig, "rowid", "price",
                       SelectionFilterTransform::Mode::Filter));
  dag.addSignalDependency(kFilter, kSig);
  dag.build();
  dag.markTableDirty(kTable);

  // Runtime: a point mark sourced from the filtered node.
  InteractionRuntime rt(dag);
  Encoding enc;
  enc.field(Channel::X, "price").field(Channel::Y, "price");
  rt.addMark("pts", kFilter, Mark::Point, enc, 10, 20, 30);

  // First refresh: empty selection -> all 4 rows -> 4 verts * 8B = 32B.
  rt.refresh();
  const RuntimeMark* m = rt.compiledMark("pts");
  check(m && m->result.ok, "compiled mark ok");
  check(m && m->result.bytes.size() == 32, "empty selection -> 4 points (32B)");

  // Brush [0,20]: prices 10,5 pass -> 2 verts * 8B = 16B. A signal change alone
  // (no data change) must re-render through the loop.
  store.set(kSig, IntervalSelection{kBufPrice, 0.0, 20.0});
  rt.refresh();
  m = rt.compiledMark("pts");
  check(m && m->result.bytes.size() == 16, "brush [0,20] -> 2 points (16B)");

  // Point select rowid 103 -> 1 row -> 8B.
  store.set(kSig, PointSelection{103});
  rt.refresh();
  m = rt.compiledMark("pts");
  check(m && m->result.bytes.size() == 8, "point select -> 1 point (8B)");

  // Clear -> back to all 4.
  store.clear(kSig);
  rt.refresh();
  m = rt.compiledMark("pts");
  check(m && m->result.bytes.size() == 32, "cleared selection -> 4 points again");

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
