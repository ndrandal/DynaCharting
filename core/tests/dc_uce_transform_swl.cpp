// ENC-616c — Transform tier: stack + window/rolling + sample/LOD (RESEARCH §5.1).
//
// Each transform is exercised as a DAG NODE (inferSchema fail-fast typing + the
// topo evaluate path), mirroring dc_uce_transform_dag.cpp. Covers:
//   * stack — cumulative band (y0/y1) correctness, grouped stacking, the normalize
//             and wiggle offset modes, and the class-3/4 baseline-policy REJECTION
//             (normalize/wiggle without a policy is refused at inferSchema; zero
//             with a policy is refused).
//   * window — rolling mean / sum / min / max over a fixed frame, and EMA via the
//              existing O(1) Ema helper (value-for-value vs computeEma).
//   * sample — M4/min-max LOD reduces to the target budget while PRESERVING the
//              global min/max extremes a naive stride would drop.
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/math/Ema.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Sample.hpp"
#include "dc/transform/transforms/Stack.hpp"
#include "dc/transform/transforms/Window.hpp"

#include <algorithm>
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
  std::printf("=== ENC-616c Transforms: stack + window + sample ===\n");

  // ---------------------------------------------------------------------------
  // Source table: f32 `value`, i32 `grp` (x-position group), f32 `x`, f32 `yy`.
  // We append a wide row set so each transform has enough to chew on.
  // ---------------------------------------------------------------------------
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);
  const Id kBufVal = 200, kBufGrp = 201, kBufX = 202, kBufY = 203, kTable = 1;
  tables.defineTable(kTable, "series");
  tables.addColumn(kTable, "value", DType::F32, kBufVal);
  tables.addColumn(kTable, "grp", DType::I32, kBufGrp);
  tables.addColumn(kTable, "x", DType::F32, kBufX);
  tables.addColumn(kTable, "yy", DType::F32, kBufY);

  auto append = [&](const std::vector<float>& vals,
                    const std::vector<std::int32_t>& grps,
                    const std::vector<float>& xs,
                    const std::vector<float>& ys) {
    std::vector<std::uint8_t> b;
    appendRecord(b, kBufVal, vals.data(),
                 static_cast<std::uint32_t>(vals.size() * 4));
    appendRecord(b, kBufGrp, grps.data(),
                 static_cast<std::uint32_t>(grps.size() * 4));
    appendRecord(b, kBufX, xs.data(),
                 static_cast<std::uint32_t>(xs.size() * 4));
    appendRecord(b, kBufY, ys.data(),
                 static_cast<std::uint32_t>(ys.size() * 4));
    ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
  };

  // ---------------------------------------------------------------------------
  // STACK — cumulative band + grouped stacking.
  // Rows: (value, grp). grp is the x-position; within a grp the series stack.
  //   grp 0: 3, 2   -> bands [0,3],[3,5]
  //   grp 1: 4, 1   -> bands [0,4],[4,5]
  // ---------------------------------------------------------------------------
  append({3, 2, 4, 1}, {0, 0, 1, 1}, {0, 1, 2, 3}, {0, 0, 0, 0});
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    check(dag.addTransform(
              100, kTable,
              std::make_unique<StackTransform>("value", "grp", StackOffset::Zero)),
          "stack(zero) added (no policy required)");
    check(dag.build(), "stack build ok");
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->has("y0") && sch->has("y1"),
          "stack output schema = inputs + y0,y1");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto y0 = dag.columns().viewF32(100, "y0");
    auto y1 = dag.columns().viewF32(100, "y1");
    check(y0.valid() && y0.size() == 4, "stack 4 bands");
    check(nearly(y0[0], 0) && nearly(y1[0], 3), "grp0 row0 band [0,3]");
    check(nearly(y0[1], 3) && nearly(y1[1], 5), "grp0 row1 band [3,5] (cumulative)");
    check(nearly(y0[2], 0) && nearly(y1[2], 4), "grp1 row0 band [0,4] (group reset)");
    check(nearly(y0[3], 4) && nearly(y1[3], 5), "grp1 row1 band [4,5]");
  }

  // STACK normalize — each group rescaled to total 1. With a baseline policy.
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    BaselinePolicy pol;
    pol.kind = BaselinePolicy::Kind::FixedEpoch;
    pol.fixedEpochMs = 1700000000000LL;
    check(dag.addTransform(
              100, kTable,
              std::make_unique<StackTransform>("value", "grp",
                                               StackOffset::Normalize, pol)),
          "stack(normalize) added WITH baseline policy");
    check(dag.build(), "stack normalize build ok");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto y1 = dag.columns().viewF32(100, "y1");
    // grp0 total 5: tops 3/5, 5/5. grp1 total 5: tops 4/5, 5/5.
    check(nearly(y1[0], 0.6) && nearly(y1[1], 1.0), "normalize grp0 -> 0.6,1.0");
    check(nearly(y1[2], 0.8) && nearly(y1[3], 1.0), "normalize grp1 -> 0.8,1.0");
  }

  // STACK wiggle — group centered (shift down by half the group total).
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    BaselinePolicy pol;
    pol.kind = BaselinePolicy::Kind::Decaying;
    pol.decay = 0.5;
    check(dag.addTransform(
              100, kTable,
              std::make_unique<StackTransform>("value", "grp",
                                               StackOffset::Wiggle, pol)),
          "stack(wiggle) added WITH decaying policy");
    check(dag.build(), "stack wiggle build ok");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto y0 = dag.columns().viewF32(100, "y0");
    auto y1 = dag.columns().viewF32(100, "y1");
    // grp0 total 5, shift 2.5: row0 [0,3]->[-2.5,0.5]; row1 [3,5]->[0.5,2.5].
    check(nearly(y0[0], -2.5) && nearly(y1[0], 0.5), "wiggle grp0 row0 centered");
    check(nearly(y0[1], 0.5) && nearly(y1[1], 2.5), "wiggle grp0 row1 centered");
  }

  // STACK class-3/4 REJECTION — normalize/wiggle without a policy is refused; zero
  // WITH a policy is refused (no drift to pin).
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    check(!dag.addTransform(
              100, kTable,
              std::make_unique<StackTransform>("value", "grp",
                                               StackOffset::Normalize)),
          "REJECT normalize without baseline policy (class-3 drift)");
    check(!dag.addTransform(
              101, kTable,
              std::make_unique<StackTransform>("value", "grp",
                                               StackOffset::Wiggle)),
          "REJECT wiggle without baseline policy (class-4 drift)");
    BaselinePolicy zero;
    check(!dag.addTransform(
              102, kTable,
              std::make_unique<StackTransform>("value", "grp", StackOffset::Zero,
                                               zero)),
          "REJECT zero offset carrying a baseline policy (no drift)");
    // a malformed policy (decaying decay=0) is also refused
    BaselinePolicy bad;
    bad.kind = BaselinePolicy::Kind::Decaying;
    bad.decay = 0.0;
    check(!dag.addTransform(
              103, kTable,
              std::make_unique<StackTransform>("value", "grp",
                                               StackOffset::Normalize, bad)),
          "REJECT malformed decaying policy (decay out of (0,1])");
    // unknown value column rejected
    check(!dag.addTransform(
              104, kTable,
              std::make_unique<StackTransform>("nope", "grp", StackOffset::Zero)),
          "REJECT stack on unknown value column");
  }

  // ---------------------------------------------------------------------------
  // WINDOW — rolling mean/sum/min/max over a frame of 3, and EMA via the helper.
  // value column over the 4 rows is {3,2,4,1}.
  // ---------------------------------------------------------------------------
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable, std::make_unique<WindowTransform>(
                                      "value", WindowAgg::Sum, 3, "rsum"));
    dag.addTransform(101, kTable, std::make_unique<WindowTransform>(
                                      "value", WindowAgg::Mean, 3, "rmean"));
    dag.addTransform(102, kTable, std::make_unique<WindowTransform>(
                                      "value", WindowAgg::Min, 3, "rmin"));
    dag.addTransform(103, kTable, std::make_unique<WindowTransform>(
                                      "value", WindowAgg::Max, 3, "rmax"));
    check(dag.build(), "window build ok");
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->has("rsum"), "window output schema adds derived col");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto s = dag.columns().viewF32(100, "rsum");
    auto m = dag.columns().viewF32(101, "rmean");
    auto mn = dag.columns().viewF32(102, "rmin");
    auto mx = dag.columns().viewF32(103, "rmax");
    // frames (clamped at head): [3],[3,2],[3,2,4],[2,4,1]
    check(nearly(s[0], 3) && nearly(s[1], 5) && nearly(s[2], 9) && nearly(s[3], 7),
          "rolling SUM window=3");
    check(nearly(m[0], 3) && nearly(m[1], 2.5) && nearly(m[2], 3) &&
              nearly(m[3], 7.0 / 3.0),
          "rolling MEAN window=3");
    check(nearly(mn[0], 3) && nearly(mn[1], 2) && nearly(mn[2], 2) &&
              nearly(mn[3], 1),
          "rolling MIN window=3");
    check(nearly(mx[0], 3) && nearly(mx[1], 3) && nearly(mx[2], 4) &&
              nearly(mx[3], 4),
          "rolling MAX window=3");
  }

  // WINDOW EMA — must equal computeEma value-for-value (the O(1) §5.1 reuse).
  {
    TransformDag dag(tables, src);
    dag.addSource(kTable);
    dag.addTransform(100, kTable, std::make_unique<WindowTransform>(
                                      "value", WindowAgg::Ema, 2, "ema"));
    check(dag.build(), "window EMA build ok");
    dag.markTableDirty(kTable);
    dag.evaluate();
    auto e = dag.columns().viewF32(100, "ema");
    float input[4] = {3, 2, 4, 1};
    float expect[4] = {0};
    computeEma(input, expect, 4, 2);
    bool eq = e.valid() && e.size() == 4;
    for (std::size_t i = 0; i < 4 && eq; ++i) eq = nearly(e[i], expect[i]);
    check(eq, "EMA matches computeEma value-for-value");
    // reject window < 1
    check(!dag.addTransform(200, kTable, std::make_unique<WindowTransform>(
                                             "value", WindowAgg::Mean, 0, "z")),
          "REJECT window period < 1");
  }

  // ---------------------------------------------------------------------------
  // SAMPLE / LOD (M4) — reduce a big series to a budget while keeping extremes.
  // Build a fresh table: a long ramp with a sharp spike and dip planted mid-run.
  // ---------------------------------------------------------------------------
  {
    const Id kBufSx = 300, kBufSy = 301, kTableS = 2;
    tables.defineTable(kTableS, "big");
    tables.addColumn(kTableS, "sx", DType::F32, kBufSx);
    tables.addColumn(kTableS, "sy", DType::F32, kBufSy);

    const int N = 1000;
    std::vector<float> sx(N), sy(N);
    for (int i = 0; i < N; ++i) {
      sx[i] = static_cast<float>(i);
      sy[i] = std::sin(static_cast<float>(i) * 0.05f);
    }
    // Plant a global MAX spike and global MIN dip at known interior rows.
    const int spikeRow = 421, dipRow = 733;
    sy[spikeRow] = 100.0f;
    sy[dipRow] = -100.0f;
    {
      std::vector<std::uint8_t> b;
      appendRecord(b, kBufSx, sx.data(), static_cast<std::uint32_t>(N * 4));
      appendRecord(b, kBufSy, sy.data(), static_cast<std::uint32_t>(N * 4));
      ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
    }

    TransformDag dag(tables, src);
    dag.addSource(kTableS);
    const std::uint32_t budget = 100;
    check(dag.addTransform(
              100, kTableS,
              std::make_unique<SampleTransform>("sx", "sy", budget)),
          "sample added");
    check(dag.build(), "sample build ok");
    const ColumnSchema* sch = dag.schemaOf(100);
    check(sch && sch->columns.size() == 2 && sch->has("sy"),
          "sample preserves schema (compaction)");
    dag.markTableDirty(kTableS);
    dag.evaluate();

    auto ox = dag.columns().viewF32(100, "sx");
    auto oy = dag.columns().viewF32(100, "sy");
    check(ox.valid() && oy.valid(), "sample produced columns");
    check(oy.size() < static_cast<std::size_t>(N), "sample reduced row count");
    check(oy.size() <= budget + 4, "sample at/under target budget (+bucket slack)");

    // The min/max EXTREMES must survive (the property M4 guarantees, stride loses).
    float omin = oy[0], omax = oy[0];
    bool keptSpikeX = false, keptDipX = false;
    for (std::size_t i = 0; i < oy.size(); ++i) {
      omin = std::min(omin, oy[i]);
      omax = std::max(omax, oy[i]);
      if (nearly(ox[i], spikeRow)) keptSpikeX = true;
      if (nearly(ox[i], dipRow)) keptDipX = true;
    }
    check(nearly(omax, 100.0), "sample preserved global MAX (+100 spike)");
    check(nearly(omin, -100.0), "sample preserved global MIN (-100 dip)");
    check(keptSpikeX && keptDipX, "sample kept the exact extreme rows' x");

    // Output x is monotone non-decreasing (input-order compaction).
    bool mono = true;
    for (std::size_t i = 1; i < ox.size(); ++i)
      if (ox[i] < ox[i - 1]) mono = false;
    check(mono, "sample output is x-monotone");

    // Below budget: nothing dropped (passthrough). 3-row table.
    const Id kBufTx = 400, kBufTy = 401, kTableT = 3;
    tables.defineTable(kTableT, "tiny");
    tables.addColumn(kTableT, "tx", DType::F32, kBufTx);
    tables.addColumn(kTableT, "ty", DType::F32, kBufTy);
    {
      std::vector<std::uint8_t> b;
      float txs[3] = {0, 1, 2}, tys[3] = {5, 9, 1};
      appendRecord(b, kBufTx, txs, sizeof(txs));
      appendRecord(b, kBufTy, tys, sizeof(tys));
      ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
    }
    TransformDag dag2(tables, src);
    dag2.addSource(kTableT);
    dag2.addTransform(100, kTableT,
                      std::make_unique<SampleTransform>("tx", "ty", 100));
    dag2.build();
    dag2.markTableDirty(kTableT);
    dag2.evaluate();
    check(dag2.columns().viewF32(100, "ty").size() == 3,
          "sample under budget keeps all rows");
    // reject budget < 2
    check(!dag2.addTransform(200, kTableT,
                             std::make_unique<SampleTransform>("tx", "ty", 1)),
          "REJECT sample budget < 2");
  }

  std::printf("=== ENC-616c Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
