// ENC-616d — relational `join`/`lookup` transform tests (RESEARCH §5.1/§7.3).
//
// Covers:
//   1) single-key lookup appends the right column(s), dtype-preserving;
//   2) THE edge-bearing case: resolve BOTH endpoints (src + dst) of an edge table
//      against a nodes table in one node -> src.x/src.y/dst.x/dst.y on each edge;
//   3) miss policy: Null (sentinel + keep) vs Drop (drop the row);
//   4) fail-fast typing: key DTYPE MISMATCH rejected at inferSchema/addJoin, plus
//      missing-column rejects;
//   5) recompute on a RIGHT-side change (the lookup table moved) via dirty gating.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Join.hpp"

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

// Append one op=1 APPEND record at a given byte offset (the unchanged wire format).
static void appendRecordAt(std::vector<std::uint8_t>& out, Id bufferId,
                           std::uint32_t offset, const void* bytes,
                           std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF);
    out.push_back((v >> 16) & 0xFF); out.push_back((v >> 24) & 0xFF);
  };
  out.push_back(1);
  u32(static_cast<std::uint32_t>(bufferId));
  u32(offset);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}
// op=2 UPDATE_RANGE record (overwrites bytes in place — the right-side mutation).
static void updateRecordAt(std::vector<std::uint8_t>& out, Id bufferId,
                           std::uint32_t offset, const void* bytes,
                           std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF);
    out.push_back((v >> 16) & 0xFF); out.push_back((v >> 24) & 0xFF);
  };
  out.push_back(2);
  u32(static_cast<std::uint32_t>(bufferId));
  u32(offset);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}
static bool nearly(double x, double y) { return std::fabs(x - y) < 1e-5; }

int main() {
  std::printf("=== ENC-616d Relational join/lookup ===\n");

  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);

  // -------------------------------------------------------------------------
  // NODES table: id (cat) + x (f32) + y (f32). The lookup table (RIGHT input).
  //   id codes: 0,1,2,3  positions: (0,0),(10,1),(20,2),(30,3)
  // EDGES table: src (cat) + dst (cat). The left input (the rows resolved).
  //   edges: 0->2, 1->3, 2->0
  // -------------------------------------------------------------------------
  const Id kNodeId = 100, kNodeX = 101, kNodeY = 102, kNodes = 1;
  const Id kEdgeSrc = 110, kEdgeDst = 111, kEdges = 2;
  tables.defineTable(kNodes, "nodes");
  tables.addColumn(kNodes, "id", DType::Cat, kNodeId);
  tables.addColumn(kNodes, "x", DType::F32, kNodeX);
  tables.addColumn(kNodes, "y", DType::F32, kNodeY);
  tables.defineTable(kEdges, "edges");
  tables.addColumn(kEdges, "src", DType::Cat, kEdgeSrc);
  tables.addColumn(kEdges, "dst", DType::Cat, kEdgeDst);

  {
    std::vector<std::uint8_t> batch;
    std::uint32_t nodeIds[4] = {0, 1, 2, 3};
    float nodeX[4] = {0.0f, 10.0f, 20.0f, 30.0f};
    float nodeY[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    std::uint32_t eSrc[3] = {0, 1, 2};
    std::uint32_t eDst[3] = {2, 3, 0};
    appendRecordAt(batch, kNodeId, 0, nodeIds, sizeof(nodeIds));
    appendRecordAt(batch, kNodeX, 0, nodeX, sizeof(nodeX));
    appendRecordAt(batch, kNodeY, 0, nodeY, sizeof(nodeY));
    appendRecordAt(batch, kEdgeSrc, 0, eSrc, sizeof(eSrc));
    appendRecordAt(batch, kEdgeDst, 0, eDst, sizeof(eDst));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  }

  // -------------------------------------------------------------------------
  // 1) Single-key lookup: resolve edges.src against nodes.id, pulling x -> n.x.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kEdges);
    dag.addSource(kNodes);
    JoinLookup lk;
    lk.leftKey = "src";
    lk.prefix = "n";
    lk.fields = {"x"};
    bool ok = dag.addJoin(
        500, kEdges, kNodes,
        std::make_unique<JoinTransform>("id", std::vector<JoinLookup>{lk}));
    check(ok, "addJoin single-key ok");
    check(dag.build(), "build ok");

    // schema: src, dst (left passthrough) + n.x (pulled, f32).
    const ColumnSchema* sch = dag.schemaOf(500);
    check(sch && sch->columns.size() == 3 && sch->has("n.x"),
          "single-key output schema = left + n.x");
    const SchemaColumn* nx = sch ? sch->find("n.x") : nullptr;
    check(nx && nx->dtype == DType::F32, "pulled column keeps f32 dtype");

    dag.markTableDirty(kEdges);
    dag.markTableDirty(kNodes);
    auto ran = dag.evaluate();
    check(ran.size() == 1 && ran[0] == 500, "join node ran once");

    auto nxv = dag.columns().viewF32(500, "n.x");
    check(nxv.valid() && nxv.size() == 3, "n.x has 3 edge rows");
    // edges src 0,1,2 -> node x 0,10,20
    check(nearly(nxv[0], 0.0) && nearly(nxv[1], 10.0) && nearly(nxv[2], 20.0),
          "n.x resolved correctly by src key");
    // left passthrough preserved (cat codes)
    auto srcv = dag.columns().viewCat(500, "src");
    check(srcv.valid() && srcv[0] == 0 && srcv[2] == 2, "left src passed through");
  }

  // -------------------------------------------------------------------------
  // 2) THE edge-bearing case: resolve BOTH endpoints in one node ->
  //    src.x/src.y/dst.x/dst.y on every edge row. (RESEARCH §7.3.)
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kEdges);
    dag.addSource(kNodes);
    JoinLookup s, d;
    s.leftKey = "src"; s.prefix = "src"; s.fields = {"x", "y"};
    d.leftKey = "dst"; d.prefix = "dst"; d.fields = {"x", "y"};
    bool ok = dag.addJoin(
        500, kEdges, kNodes,
        std::make_unique<JoinTransform>("id", std::vector<JoinLookup>{s, d}));
    check(ok, "addJoin two-endpoint ok");
    check(dag.build(), "build ok");

    const ColumnSchema* sch = dag.schemaOf(500);
    check(sch && sch->columns.size() == 6 && sch->has("src.x") &&
              sch->has("src.y") && sch->has("dst.x") && sch->has("dst.y"),
          "edge schema = src,dst + src.x/src.y/dst.x/dst.y (4 position cols)");

    dag.markTableDirty(kEdges);
    dag.markTableDirty(kNodes);
    dag.evaluate();

    // edges: 0->2, 1->3, 2->0. node positions (id->x,y): 0:(0,0) 1:(10,1)
    // 2:(20,2) 3:(30,3).
    auto sx = dag.columns().viewF32(500, "src.x");
    auto sy = dag.columns().viewF32(500, "src.y");
    auto dx = dag.columns().viewF32(500, "dst.x");
    auto dy = dag.columns().viewF32(500, "dst.y");
    check(sx.valid() && sy.valid() && dx.valid() && dy.valid() &&
              sx.size() == 3,
          "all four endpoint columns present, 3 rows");
    // row0: src 0 -> (0,0), dst 2 -> (20,2)
    check(nearly(sx[0], 0.0) && nearly(sy[0], 0.0) && nearly(dx[0], 20.0) &&
              nearly(dy[0], 2.0),
          "edge 0->2 endpoints resolved");
    // row1: src 1 -> (10,1), dst 3 -> (30,3)
    check(nearly(sx[1], 10.0) && nearly(sy[1], 1.0) && nearly(dx[1], 30.0) &&
              nearly(dy[1], 3.0),
          "edge 1->3 endpoints resolved");
    // row2: src 2 -> (20,2), dst 0 -> (0,0)
    check(nearly(sx[2], 20.0) && nearly(sy[2], 2.0) && nearly(dx[2], 0.0) &&
              nearly(dy[2], 0.0),
          "edge 2->0 endpoints resolved");
  }

  // -------------------------------------------------------------------------
  // 3) MISS POLICY. Add an edge whose dst references a NON-EXISTENT node (code 9).
  //    Null -> keep the row, sentinel NaN in the missed columns.
  //    Drop -> the row disappears.
  // -------------------------------------------------------------------------
  {
    // append edge 0->9 (dst 9 has no node). edges now: 0->2,1->3,2->0,0->9.
    std::vector<std::uint8_t> batch;
    std::uint32_t eSrc[1] = {0};
    std::uint32_t eDst[1] = {9};
    appendRecordAt(batch, kEdgeSrc, 12, eSrc, sizeof(eSrc));  // append at row 3
    appendRecordAt(batch, kEdgeDst, 12, eDst, sizeof(eDst));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

    // --- Null policy ---
    {
      TransformDag dag(tables, src);
      dag.addSource(kEdges);
      dag.addSource(kNodes);
      JoinLookup d;
      d.leftKey = "dst"; d.prefix = "dst"; d.fields = {"x"};
      dag.addJoin(500, kEdges, kNodes,
                  std::make_unique<JoinTransform>(
                      "id", std::vector<JoinLookup>{d}, JoinMiss::Null));
      dag.build();
      dag.markTableDirty(kEdges);
      dag.markTableDirty(kNodes);
      dag.evaluate();
      auto dx = dag.columns().viewF32(500, "dst.x");
      check(dx.valid() && dx.size() == 4, "Null policy keeps all 4 rows");
      check(nearly(dx[0], 20.0) && std::isnan(dx[3]),
            "Null policy: hit row 0 resolved, missed row 3 is NaN sentinel");
    }
    // --- Drop policy ---
    {
      TransformDag dag(tables, src);
      dag.addSource(kEdges);
      dag.addSource(kNodes);
      JoinLookup d;
      d.leftKey = "dst"; d.prefix = "dst"; d.fields = {"x"};
      dag.addJoin(500, kEdges, kNodes,
                  std::make_unique<JoinTransform>(
                      "id", std::vector<JoinLookup>{d}, JoinMiss::Drop));
      dag.build();
      dag.markTableDirty(kEdges);
      dag.markTableDirty(kNodes);
      dag.evaluate();
      auto dx = dag.columns().viewF32(500, "dst.x");
      check(dx.valid() && dx.size() == 3,
            "Drop policy drops the missed row (4 -> 3)");
      // surviving dst.x: rows 0,1,2 -> dst 2,3,0 -> x 20,30,0
      check(nearly(dx[0], 20.0) && nearly(dx[1], 30.0) && nearly(dx[2], 0.0),
            "Drop policy keeps only fully-resolved rows");
      auto srcv = dag.columns().viewCat(500, "src");
      check(srcv.valid() && srcv.size() == 3,
            "Drop compacts left passthrough in lockstep");
    }
  }

  // -------------------------------------------------------------------------
  // 4) FAIL-FAST TYPING at addJoin (inferSchemaBinary).
  // -------------------------------------------------------------------------
  {
    // key dtype mismatch: a f32 "weight" left key resolved against a cat "id".
    const Id kWeight = 120;
    tables.addColumn(kEdges, "weight", DType::F32, kWeight);
    {
      std::vector<std::uint8_t> batch;
      float w[4] = {1.f, 2.f, 3.f, 4.f};
      appendRecordAt(batch, kWeight, 0, w, sizeof(w));
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }
    TransformDag dag(tables, src);
    dag.addSource(kEdges);
    dag.addSource(kNodes);
    JoinLookup bad;
    bad.leftKey = "weight"; bad.prefix = "w"; bad.fields = {"x"};
    bool ok = dag.addJoin(
        500, kEdges, kNodes,
        std::make_unique<JoinTransform>("id", std::vector<JoinLookup>{bad}));
    check(!ok, "REJECT key dtype mismatch (f32 left key vs cat right key)");

    // missing left key
    JoinLookup miss;
    miss.leftKey = "nope"; miss.prefix = "n"; miss.fields = {"x"};
    bool ok2 = dag.addJoin(
        501, kEdges, kNodes,
        std::make_unique<JoinTransform>("id", std::vector<JoinLookup>{miss}));
    check(!ok2, "REJECT missing left key column");

    // missing pulled right field
    JoinLookup mf;
    mf.leftKey = "src"; mf.prefix = "s"; mf.fields = {"nonesuch"};
    bool ok3 = dag.addJoin(
        502, kEdges, kNodes,
        std::make_unique<JoinTransform>("id", std::vector<JoinLookup>{mf}));
    check(!ok3, "REJECT missing pulled right field");

    // missing right key
    JoinLookup okLk;
    okLk.leftKey = "src"; okLk.prefix = "s"; okLk.fields = {"x"};
    bool ok4 = dag.addJoin(
        503, kEdges, kNodes,
        std::make_unique<JoinTransform>("badkey", std::vector<JoinLookup>{okLk}));
    check(!ok4, "REJECT missing right key column");
  }

  // -------------------------------------------------------------------------
  // 5) RECOMPUTE on a RIGHT-side change (the lookup table moved). Dirty gating:
  //    marking ONLY the nodes table dirty recomputes the join, and the new node
  //    position flows onto the edge rows.
  // -------------------------------------------------------------------------
  {
    // Fresh ingest so offsets are clean for this scenario.
    IngestProcessor ing2;
    TableStore t2;
    auto src2 = makeBufferByteSource(ing2);
    const Id nId = 100, nX = 101, nY = 102, nodes = 1;
    const Id eSrcB = 110, eDstB = 111, edges = 2;
    t2.defineTable(nodes, "nodes");
    t2.addColumn(nodes, "id", DType::Cat, nId);
    t2.addColumn(nodes, "x", DType::F32, nX);
    t2.addColumn(nodes, "y", DType::F32, nY);
    t2.defineTable(edges, "edges");
    t2.addColumn(edges, "src", DType::Cat, eSrcB);
    t2.addColumn(edges, "dst", DType::Cat, eDstB);
    {
      std::vector<std::uint8_t> b;
      std::uint32_t ids[2] = {0, 1};
      float xs[2] = {5.0f, 15.0f};
      float ys[2] = {0.0f, 0.0f};
      std::uint32_t s[1] = {0};
      std::uint32_t d[1] = {1};
      appendRecordAt(b, nId, 0, ids, sizeof(ids));
      appendRecordAt(b, nX, 0, xs, sizeof(xs));
      appendRecordAt(b, nY, 0, ys, sizeof(ys));
      appendRecordAt(b, eSrcB, 0, s, sizeof(s));
      appendRecordAt(b, eDstB, 0, d, sizeof(d));
      ing2.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
    }

    TransformDag dag(t2, src2);
    dag.addSource(edges);
    dag.addSource(nodes);
    JoinLookup s;
    s.leftKey = "src"; s.prefix = "src"; s.fields = {"x"};
    dag.addJoin(500, edges, nodes,
                std::make_unique<JoinTransform>("id", std::vector<JoinLookup>{s}));
    dag.build();

    dag.markTableDirty(edges);
    dag.markTableDirty(nodes);
    dag.evaluate();
    check(dag.recomputeCount(500) == 1, "join ran once initially");
    auto sx = dag.columns().viewF32(500, "src.x");
    check(sx.valid() && nearly(sx[0], 5.0), "edge src.x resolves to node 0 x=5");

    // No dirty -> no recompute (gating proof).
    auto ran = dag.evaluate();
    check(ran.empty() && dag.recomputeCount(500) == 1,
          "no dirty -> join does not recompute");

    // MOVE node 0: x 5 -> 99 (UPDATE_RANGE on the nodes.x buffer). Mark ONLY the
    // nodes-table buffers dirty -> the join recomputes from the RIGHT change.
    {
      std::vector<std::uint8_t> b;
      float nx[1] = {99.0f};
      updateRecordAt(b, nX, 0, nx, sizeof(nx));
      ing2.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
    }
    dag.markTouchedBuffers({nX});
    auto ran2 = dag.evaluate();
    check(ran2.size() == 1 && ran2[0] == 500,
          "RIGHT-side change recomputes the join");
    check(dag.recomputeCount(500) == 2, "join recompute count bumped to 2");
    auto sx2 = dag.columns().viewF32(500, "src.x");
    check(sx2.valid() && nearly(sx2[0], 99.0),
          "moved node position flows onto the edge row");
  }

  std::printf("=== ENC-616d join Results: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
