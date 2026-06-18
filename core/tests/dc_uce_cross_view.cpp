// ENC-638 (F1) — ViewSession cross-view linking: ONE shared selection signal,
// mutated once, filters TWO independent views (linked brushing), and unsubscribes
// cleanly on removeView.
#include "dc/data/TableStore.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/InteractionRuntime.hpp"
#include "dc/interaction/ViewSession.hpp"
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

// Build a view: table(rowid,price) + DAG(selectionFilter on `sig`) + runtime(point mark).
struct View {
  IngestProcessor ingest;
  TableStore tables;
  std::unique_ptr<TransformDag> dag;
  std::unique_ptr<InteractionRuntime> rt;
};

static void buildView(View& v, SignalStore& signals, Id sig, Id bufRowid, Id bufPrice,
                      const std::vector<std::int32_t>& ids,
                      const std::vector<float>& prices) {
  const Id kTable = 1;
  v.tables.defineTable(kTable, "v");
  v.tables.addColumn(kTable, "rowid", DType::I32, bufRowid);
  v.tables.addColumn(kTable, "price", DType::F32, bufPrice);
  std::vector<std::uint8_t> batch;
  appendRecord(batch, bufRowid, ids.data(), static_cast<std::uint32_t>(ids.size() * 4));
  appendRecord(batch, bufPrice, prices.data(), static_cast<std::uint32_t>(prices.size() * 4));
  v.ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

  auto src = makeBufferByteSource(v.ingest);
  v.dag = std::make_unique<TransformDag>(v.tables, src);
  v.dag->addSource(kTable);
  v.dag->addTransform(100, kTable,
                      std::make_unique<SelectionFilterTransform>(
                          &signals, sig, "rowid", "price",
                          SelectionFilterTransform::Mode::Filter));
  v.dag->addSignalDependency(100, sig);
  v.dag->build();
  v.dag->markTableDirty(kTable);

  v.rt = std::make_unique<InteractionRuntime>(*v.dag);
  Encoding enc;
  enc.field(Channel::X, "price").field(Channel::Y, "price");
  v.rt->addMark("pts", 100, Mark::Point, enc, 10, 20, 30);
}

static std::size_t markBytes(const InteractionRuntime& rt) {
  const RuntimeMark* m = rt.compiledMark("pts");
  return (m && m->result.ok) ? m->result.bytes.size() : 0;
}

int main() {
  std::printf("=== ENC-638 (F1) ViewSession cross-view linking ===\n");

  const Id kSig = 9200;
  ViewSession session;
  session.signals().define(kSig, PointSelection{});  // empty -> all rows

  // Two independent views (different data, different buffer ids).
  View a, b;
  buildView(a, session.signals(), kSig, 800, 801, {1, 2, 3, 4}, {10, 25, 5, 40});
  buildView(b, session.signals(), kSig, 810, 811, {5, 6, 7, 8}, {15, 30, 8, 50});
  session.addView(*a.dag, *a.rt);
  session.addView(*b.dag, *b.rt);
  check(session.viewCount() == 2, "two views registered");
  check(session.signals().graphCount() == 2, "shared store fans to 2 graphs");

  // Empty selection -> both views render all 4 rows (32B each).
  session.refreshAll();
  check(markBytes(*a.rt) == 32 && markBytes(*b.rt) == 32, "empty: both views show 4 points");

  // ONE shared brush [0,20], set once, links BOTH views:
  //   A {10,25,5,40} -> {10,5} = 2 ; B {15,30,8,50} -> {15,8} = 2.
  session.setSignal(kSig, IntervalSelection{801, 0.0, 20.0});
  check(markBytes(*a.rt) == 16, "brush links view A -> 2 points");
  check(markBytes(*b.rt) == 16, "brush links view B -> 2 points (one setSignal)");

  // Tighter brush [0,12]: A -> {10,5}=2 ; B -> {8}=1.
  session.setSignal(kSig, IntervalSelection{801, 0.0, 12.0});
  check(markBytes(*a.rt) == 16, "tighter brush view A -> 2");
  check(markBytes(*b.rt) == 8, "tighter brush view B -> 1");

  // Clear -> both back to 4.
  session.setSignal(kSig, PointSelection{});
  check(markBytes(*a.rt) == 32 && markBytes(*b.rt) == 32, "cleared: both back to 4");

  // removeView unsubscribes that view's graph from the shared store.
  session.removeView(1);
  check(session.viewCount() == 1 && session.signals().graphCount() == 1,
        "removeView drops the view + its graph subscription");

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
