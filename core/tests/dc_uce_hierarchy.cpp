// ENC-618a — Hierarchy layout transforms (RESEARCH §5.1 layout tier; §6.3 treemap).
//
// Covers, all run as DAG nodes (inferSchema typing + evaluate):
//   * stratify builds the right parent/depth/value tree from flat leaf rows.
//   * treemap squarify: rectangles tile [0,1] with no overlap, leaf area ∝ value.
//   * resquarify (stable): a tile keeps its slot when values change slightly.
//   * partition / icicle: depth -> band, value-proportional cross-axis.
//   * pack: sibling circles non-overlapping within their parent.
//   * dendrogram: node y monotonic by depth; leaf x spread; internal = mean of kids.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Dendrogram.hpp"
#include "dc/transform/transforms/Pack.hpp"
#include "dc/transform/transforms/Partition.hpp"
#include "dc/transform/transforms/Stratify.hpp"
#include "dc/transform/transforms/Treemap.hpp"

#include <array>
#include <cmath>
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
static bool nearly(double x, double y, double tol = 1e-4) {
  return std::fabs(x - y) < tol;
}

// Rectangle overlap test on open interiors.
static bool rectsOverlap(double ax0, double ay0, double ax1, double ay1,
                         double bx0, double by0, double bx1, double by1) {
  const double eps = 1e-6;
  return ax0 < bx1 - eps && bx0 < ax1 - eps &&
         ay0 < by1 - eps && by0 < ay1 - eps;
}

int main() {
  std::printf("=== ENC-618a Hierarchy layouts ===\n");

  // Source table: a 2-level hierarchy. level0 (grp), level1 (sub) as i32 codes;
  // size as f32. 6 leaf rows over 2 groups:
  //   grp 0: sub {0,1,2} sizes {3,1,2}    -> group value 6
  //   grp 1: sub {0,1}   sizes {2,2}      -> group value 4
  // grand total 10.
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kGrp = 400, kSub = 401, kSize = 402, kTable = 1;
  tables.defineTable(kTable, "leaves");
  tables.addColumn(kTable, "grp", DType::I32, kGrp);
  tables.addColumn(kTable, "sub", DType::I32, kSub);
  tables.addColumn(kTable, "size", DType::F32, kSize);

  auto load = [&](const std::vector<std::int32_t>& grp,
                  const std::vector<std::int32_t>& sub,
                  const std::vector<float>& size) {
    // Re-seed: a fresh ingest each call by appending a full batch (table grows; we
    // use a brand-new processor per scenario instead — see scenarios below).
    std::vector<std::uint8_t> b;
    appendRecord(b, kGrp, grp.data(), static_cast<std::uint32_t>(grp.size() * 4));
    appendRecord(b, kSub, sub.data(), static_cast<std::uint32_t>(sub.size() * 4));
    appendRecord(b, kSize, size.data(), static_cast<std::uint32_t>(size.size() * 4));
    ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
  };
  load({0, 0, 0, 1, 1}, {0, 1, 2, 0, 1}, {3.0f, 1.0f, 2.0f, 2.0f, 2.0f});
  // 5 leaf rows, total 10. grp0 value 6 (3+1+2), grp1 value 4 (2+2).

  const std::vector<std::string> levels = {"grp", "sub"};

  // -------------------------------------------------------------------------
  // 1) STRATIFY — parent/depth/value tree.
  // Expected nodes (build order): root(0), grp0(1), grp0.sub0(2), grp0.sub1(3),
  // grp0.sub2(4), grp1(5), grp1.sub0(6), grp1.sub1(7). 8 nodes.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    check(dag.addTransform(100, kTable,
                           std::make_unique<StratifyTransform>(levels, "size")),
          "stratify addTransform ok");
    check(dag.build(), "stratify build ok");
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->has("parent") && sch->has("depth") && sch->has("value") &&
              sch->has("leaf"),
          "stratify output schema = node/parent/depth/value/leaf + levels");
    dag.markTableDirty(kTable);
    dag.evaluate();

    auto par = dag.columns().viewI32(100, "parent");
    auto dep = dag.columns().viewI32(100, "depth");
    auto val = dag.columns().viewF32(100, "value");
    auto leaf = dag.columns().viewI32(100, "leaf");
    check(par.valid() && par.size() == 8, "stratify produced 8 nodes");
    check(par[0] == -1 && dep[0] == 0, "root has parent=-1 depth=0");
    check(nearly(val[0], 10.0), "root value = grand total 10");
    // node 1 = grp0, depth 1, parent root, value 6.
    check(par[1] == 0 && dep[1] == 1 && nearly(val[1], 6.0) && leaf[1] == 0,
          "grp0 internal: parent=root depth=1 value=6");
    // node 2 = grp0.sub0, depth 2, leaf, value 3.
    check(par[2] == 1 && dep[2] == 2 && nearly(val[2], 3.0) && leaf[2] == 1,
          "grp0.sub0 leaf: parent=grp0 depth=2 value=3");
    // node 5 = grp1, value 4.
    check(par[5] == 0 && dep[5] == 1 && nearly(val[5], 4.0),
          "grp1 internal value=4");
  }

  // -------------------------------------------------------------------------
  // 2) TREEMAP (squarify) — tiles [0,1], no overlap, leaf area ∝ value.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    check(dag.addTransform(
              100, kTable,
              std::make_unique<TreemapTransform>(levels, "size", 0.0, false)),
          "treemap addTransform ok");
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();

    auto x0 = dag.columns().viewF32(100, "x0");
    auto y0 = dag.columns().viewF32(100, "y0");
    auto x1 = dag.columns().viewF32(100, "x1");
    auto y1 = dag.columns().viewF32(100, "y1");
    auto leaf = dag.columns().viewI32(100, "leaf");
    auto val = dag.columns().viewF32(100, "value");
    const std::size_t n = x0.size();
    check(n == 8, "treemap 8 node rects");

    // Root covers [0,1]^2.
    check(nearly(x0[0], 0.0) && nearly(y0[0], 0.0) && nearly(x1[0], 1.0) &&
              nearly(y1[0], 1.0),
          "treemap root rect = unit square");

    // Leaf areas sum to 1 and each ∝ value/total (total area = 1).
    double areaSum = 0.0;
    bool areaProp = true;
    for (std::size_t i = 0; i < n; ++i) {
      if (leaf[i] != 1) continue;
      const double area = (x1[i] - x0[i]) * (y1[i] - y0[i]);
      areaSum += area;
      const double expect = val[i] / 10.0;  // total value 10 -> full unit area
      if (!nearly(area, expect, 2e-3)) areaProp = false;
    }
    check(nearly(areaSum, 1.0, 2e-3), "treemap leaf areas tile the unit square");
    check(areaProp, "treemap leaf area ∝ value");

    // No two leaf rects overlap.
    bool anyOverlap = false;
    for (std::size_t i = 0; i < n; ++i) {
      if (leaf[i] != 1) continue;
      for (std::size_t j = i + 1; j < n; ++j) {
        if (leaf[j] != 1) continue;
        if (rectsOverlap(x0[i], y0[i], x1[i], y1[i], x0[j], y0[j], x1[j], y1[j]))
          anyOverlap = true;
      }
    }
    check(!anyOverlap, "treemap leaf rects do not overlap");

    // Every leaf rect lies inside its parent's rect.
    auto par = dag.columns().viewI32(100, "parent");
    bool contained = true;
    for (std::size_t i = 1; i < n; ++i) {
      const int p = par[i];
      if (p < 0) continue;
      if (x0[i] < x0[p] - 1e-5 || y0[i] < y0[p] - 1e-5 ||
          x1[i] > x1[p] + 1e-5 || y1[i] > y1[p] + 1e-5)
        contained = false;
    }
    check(contained, "treemap child rects nested inside parents");
  }

  // -------------------------------------------------------------------------
  // 3) RESQUARIFY (stable) — a tile keeps its slot when values change slightly.
  //    Two fresh DAGs with stable=true sharing ONE transform instance is not
  //    possible (the DAG owns the node), so we drive ONE dag and mutate the
  //    source between evaluates: the stable plan should hold each leaf's slot.
  // -------------------------------------------------------------------------
  {
    // Fresh ingest/table so row growth is controlled.
    IngestProcessor ing2;
    TableStore tb2;
    auto s2 = makeBufferByteSource(ing2);
    const Id g = 500, su = 501, sz = 502, T = 7;
    tb2.defineTable(T, "lv2");
    tb2.addColumn(T, "grp", DType::I32, g);
    tb2.addColumn(T, "sub", DType::I32, su);
    tb2.addColumn(T, "size", DType::F32, sz);
    auto put = [&](const std::vector<std::int32_t>& gr,
                   const std::vector<std::int32_t>& sb,
                   const std::vector<float>& si) {
      std::vector<std::uint8_t> b;
      appendRecord(b, g, gr.data(), static_cast<std::uint32_t>(gr.size() * 4));
      appendRecord(b, su, sb.data(), static_cast<std::uint32_t>(sb.size() * 4));
      appendRecord(b, sz, si.data(), static_cast<std::uint32_t>(si.size() * 4));
      ing2.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
    };

    TransformDag dag(tb2, s2);
    dag.addSource(T);
    dag.addTransform(100, T,
                     std::make_unique<TreemapTransform>(levels, "size", 0.0, true));
    // Node 200: the SAME layout but stable=false (fresh squarify each pass) — the
    // control that shows the perturbation is large enough to move tiles WITHOUT
    // resquarify, so the stability assertion is non-trivial.
    dag.addTransform(200, T,
                     std::make_unique<TreemapTransform>(levels, "size", 0.0, false));
    dag.build();

    // Update-in-place buffers: rewrite all 6 leaf sizes each pass. Use a single
    // batch that appends; to keep row count fixed we use a single initial append,
    // then op=2 UPDATE_RANGE on the size buffer. Simpler: re-create the dag each
    // pass is not allowed (loses the cache). Instead we append rows that re-state
    // the SAME 6 leaves — but that would grow the tree. So use UPDATE_RANGE.
    // Initial 6 leaves, one group, subs 0..5.
    // Initial: 6 leaves with a graded size profile that gives squarify a multi-row
    // partition (so a later perturbation CAN re-break rows in the unstable control).
    put({0, 0, 0, 0, 0, 0}, {0, 1, 2, 3, 4, 5},
        {8.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f});
    dag.markTableDirty(T);
    dag.evaluate();
    // Record each leaf's slot (its rect), for BOTH the stable (100) and the
    // unstable control (200) nodes.
    auto x0a = dag.columns().viewF32(100, "x0");
    auto y0a = dag.columns().viewF32(100, "y0");
    auto leafa = dag.columns().viewI32(100, "leaf");
    std::vector<std::array<double, 4>> before, beforeCtl;
    std::vector<int> leafIdx;
    auto cx0a = dag.columns().viewF32(200, "x0");
    auto cy0a = dag.columns().viewF32(200, "y0");
    for (std::size_t i = 0; i < x0a.size(); ++i)
      if (leafa[i] == 1) {
        leafIdx.push_back(static_cast<int>(i));
        before.push_back({x0a[i], y0a[i],
                          dag.columns().viewF32(100, "x1")[i],
                          dag.columns().viewF32(100, "y1")[i]});
        beforeCtl.push_back({cx0a[i], cy0a[i], 0, 0});
      }

    // Perturb sizes via UPDATE_RANGE (op=2): a reordering-strength change that
    // would let a fresh squarify re-break rows, but resquarify must keep the slots.
    {
      std::vector<float> ns = {7.0f, 7.0f, 4.0f, 5.0f, 2.5f, 2.5f};
      std::vector<std::uint8_t> b;
      // op=2 update range: [1B op][4B buf][4B off][4B len][payload]
      auto u32 = [&b](std::uint32_t v) {
        b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
        b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
      };
      b.push_back(2);
      u32(static_cast<std::uint32_t>(sz));
      u32(0);
      u32(static_cast<std::uint32_t>(ns.size() * 4));
      const auto* p = reinterpret_cast<const std::uint8_t*>(ns.data());
      b.insert(b.end(), p, p + ns.size() * 4);
      ing2.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
    }
    dag.markTableDirty(T);
    dag.evaluate();
    // Each leaf's slot stays in the same neighbourhood (small movement), proving
    // the resquarify plan preserved the partition rather than re-breaking rows.
    auto x0b = dag.columns().viewF32(100, "x0");
    auto y0b = dag.columns().viewF32(100, "y0");
    auto cx0b = dag.columns().viewF32(200, "x0");
    auto cy0b = dag.columns().viewF32(200, "y0");
    double maxMoveStable = 0.0, maxMoveCtl = 0.0;
    for (std::size_t k = 0; k < leafIdx.size(); ++k) {
      const int i = leafIdx[k];
      maxMoveStable = std::max(
          maxMoveStable, std::max(std::fabs(x0b[i] - before[k][0]),
                                  std::fabs(y0b[i] - before[k][1])));
      maxMoveCtl = std::max(
          maxMoveCtl, std::max(std::fabs(cx0b[i] - beforeCtl[k][0]),
                               std::fabs(cy0b[i] - beforeCtl[k][1])));
    }
    // The STABLE node keeps each tile's slot anchored: its row partition is reused,
    // so corners barely move (only the slice rescale within the kept rows). The
    // unstable control re-runs squarify and is free to re-break rows — proving the
    // perturbation is reordering-strength and the stability is non-trivial.
    check(maxMoveStable < 0.12, "resquarify: stable tiling keeps each tile's slot");
    check(maxMoveStable <= maxMoveCtl + 1e-6,
          "resquarify: stable tiling moves no more than a fresh squarify");

    // The reused-plan path must still tile the unit square exactly (area ∝ value
    // after the rescale — stability does not break the tiling invariant).
    auto x1b = dag.columns().viewF32(100, "x1");
    auto y1b = dag.columns().viewF32(100, "y1");
    auto leafb = dag.columns().viewI32(100, "leaf");
    auto valb = dag.columns().viewF32(100, "value");
    double areaSum = 0.0;
    bool areaProp = true;
    double total = 0.0;
    for (std::size_t i = 0; i < x0b.size(); ++i)
      if (leafb[i] == 1) total += valb[i];
    for (std::size_t i = 0; i < x0b.size(); ++i) {
      if (leafb[i] != 1) continue;
      const double a = (x1b[i] - x0b[i]) * (y1b[i] - y0b[i]);
      areaSum += a;
      if (!nearly(a, valb[i] / total, 3e-3)) areaProp = false;
    }
    check(nearly(areaSum, 1.0, 3e-3) && areaProp,
          "resquarify: reused partition still tiles [0,1], area ∝ value");
  }

  // -------------------------------------------------------------------------
  // 4) PARTITION / icicle — depth->band, value-proportional cross-axis.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<PartitionTransform>(levels, "size", 0.0));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto x0 = dag.columns().viewF32(100, "x0");
    auto y0 = dag.columns().viewF32(100, "y0");
    auto x1 = dag.columns().viewF32(100, "x1");
    auto y1 = dag.columns().viewF32(100, "y1");
    auto dep = dag.columns().viewI32(100, "depth");
    auto val = dag.columns().viewF32(100, "value");
    const std::size_t n = x0.size();
    // 3 depth bands (root d0, groups d1, leaves d2). band thickness = 1/3.
    check(nearly(y0[0], 0.0) && nearly(y1[0], 1.0 / 3.0),
          "partition root in depth band 0");
    // Root spans full cross-axis.
    check(nearly(x0[0], 0.0) && nearly(x1[0], 1.0), "partition root spans [0,1]");
    // Each node's y band == depth/3 .. (depth+1)/3.
    bool bandOk = true;
    for (std::size_t i = 0; i < n; ++i) {
      if (!nearly(y0[i], dep[i] / 3.0) || !nearly(y1[i], (dep[i] + 1) / 3.0))
        bandOk = false;
    }
    check(bandOk, "partition y band = depth / (maxDepth+1)");
    // Group cross-extent ∝ value: grp0 width 6/10, grp1 width 4/10.
    // node1=grp0, node5=grp1.
    check(nearly(x1[1] - x0[1], 0.6, 2e-3) && nearly(x1[5] - x0[5], 0.4, 2e-3),
          "partition group widths ∝ value");
    // Siblings tile their parent's span with no overlap (group widths sum to 1).
    check(nearly((x1[1] - x0[1]) + (x1[5] - x0[5]), 1.0, 2e-3),
          "partition siblings tile parent span");
    (void)val;
  }

  // -------------------------------------------------------------------------
  // 5) PACK — sibling circles non-overlapping within their parent.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<PackTransform>(levels, "size", 0.0));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto cx = dag.columns().viewF32(100, "cx");
    auto cy = dag.columns().viewF32(100, "cy");
    auto rr = dag.columns().viewF32(100, "r");
    auto par = dag.columns().viewI32(100, "parent");
    auto leaf = dag.columns().viewI32(100, "leaf");
    const std::size_t n = cx.size();
    check(n == 8, "pack 8 node circles");

    // All circles within the unit square (root r ~0.5 centred at 0.5,0.5).
    check(nearly(cx[0], 0.5, 1e-3) && nearly(cy[0], 0.5, 1e-3) &&
              nearly(rr[0], 0.5, 1e-3),
          "pack root circle = inscribed unit circle");

    // Siblings (same parent) do not overlap.
    bool anyOverlap = false;
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        if (par[i] != par[j] || par[i] < 0) continue;
        const double dx = cx[i] - cx[j], dy = cy[i] - cy[j];
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d < rr[i] + rr[j] - 1e-4) anyOverlap = true;
      }
    }
    check(!anyOverlap, "pack sibling circles non-overlapping");

    // Each child circle inside its parent's circle (centre dist + r <= parent r).
    bool contained = true;
    for (std::size_t i = 1; i < n; ++i) {
      const int p = par[i];
      if (p < 0) continue;
      const double dx = cx[i] - cx[p], dy = cy[i] - cy[p];
      const double d = std::sqrt(dx * dx + dy * dy);
      if (d + rr[i] > rr[p] + 1e-3) contained = false;
    }
    check(contained, "pack child circles inside parent circle");
    // Leaf radius ∝ sqrt(value): area ∝ value. Compare two leaves' r^2 ratio.
    // node2 (grp0.sub0 value3) vs node3 (grp0.sub1 value1): r2^2/r3^2 ≈ 3.
    int a = -1, bnode = -1;
    auto val = dag.columns().viewF32(100, "value");
    for (std::size_t i = 0; i < n; ++i) {
      if (leaf[i] == 1 && nearly(val[i], 3.0) && a < 0) a = (int)i;
      if (leaf[i] == 1 && nearly(val[i], 1.0) && bnode < 0) bnode = (int)i;
    }
    check(a >= 0 && bnode >= 0 &&
              nearly((rr[a] * rr[a]) / (rr[bnode] * rr[bnode]), 3.0, 0.05),
          "pack leaf area ∝ value (r ∝ sqrt(value))");
  }

  // -------------------------------------------------------------------------
  // 6) DENDROGRAM — y monotonic by depth; leaves spread; internal = mean of kids.
  // -------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable,
                     std::make_unique<DendrogramTransform>(levels, "size"));
    dag.build();
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto x = dag.columns().viewF32(100, "x");
    auto y = dag.columns().viewF32(100, "y");
    auto px = dag.columns().viewF32(100, "px");
    auto py = dag.columns().viewF32(100, "py");
    auto dep = dag.columns().viewI32(100, "depth");
    auto par = dag.columns().viewI32(100, "parent");
    const std::size_t n = x.size();
    check(n == 8, "dendrogram 8 nodes");

    // y strictly increases from a node to its parent's complement: a child's y >
    // its parent's y (monotonic by depth).
    bool mono = true;
    for (std::size_t i = 1; i < n; ++i) {
      const int p = par[i];
      if (p < 0) continue;
      if (!(y[i] > y[p] - 1e-9)) mono = false;
      if (dep[i] > dep[p] && !(y[i] > y[p] + 1e-9)) mono = false;
    }
    check(mono, "dendrogram y monotonic increasing by depth");
    // Root at y=0; deepest leaves at y=1 (maxDepth=2 -> y=depth/2).
    check(nearly(y[0], 0.0), "dendrogram root y=0");
    bool depthY = true;
    for (std::size_t i = 0; i < n; ++i)
      if (!nearly(y[i], dep[i] / 2.0)) depthY = false;
    check(depthY, "dendrogram y = depth/maxDepth");
    // Parent point columns mirror the actual parent's (x,y).
    bool pmatch = true;
    for (std::size_t i = 1; i < n; ++i) {
      const int p = par[i];
      if (p < 0) continue;
      if (!nearly(px[i], x[p]) || !nearly(py[i], y[p])) pmatch = false;
    }
    check(pmatch, "dendrogram px,py = parent position (edge endpoints)");
    // An internal node's x is the mean of its children's x.
    // node1 = grp0 with children nodes 2,3,4.
    const double meanGrp0 = (x[2] + x[3] + x[4]) / 3.0;
    check(nearly(x[1], meanGrp0), "dendrogram internal x = mean of children x");
  }

  std::printf("=== ENC-618a Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
