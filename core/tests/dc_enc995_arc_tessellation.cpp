// ENC-995 — arc/wedge tessellation: the chord count is ANGULAR, not per-wedge.
//
// Before ENC-995 every Mark::Arc wedge got a FIXED 24 chords no matter how much
// of the circle it covered, so the chord angle — and with it the deviation from
// the true arc — scaled with the wedge's span. Measured through this same
// EncodePass at a 427.5 px outer radius (clip r=0.95 on a 900 px viewport):
//
//   shape                      old (24/wedge)      new (72/turn)
//   1 wedge spanning 360 deg     3.6573 px           0.4069 px
//   1 wedge spanning 270 deg     2.0585 px           0.4069 px
//   4-slice pie                  0.2289 px           0.4069 px
//   12-slice pie                 0.0254 px           0.4069 px
//   32-slice radial              0.0036 px           0.2289 px
//
// i.e. the old rule spread the error over three orders of magnitude depending on
// something the viewer cannot see (how the table splits the circle) and spent its
// geometry in inverse proportion to where the error was. This test pins the new
// contract: the deviation depends on the RADIUS and on segmentsPerTurn, and on
// nothing else.
//
// Each check below FAILS on the pre-ENC-995 code:
//   [1] the analytic chord-angle bound            (arcSegmentsFor did not exist)
//   [3] deviation <= bound, every shape           (3.66 px vs a 0.41 px bound)
//   [4] deviation independent of the slice count  (0.0254 px vs 3.66 px)
//   [5] a widening append repacks the buffer      (stale bytes at the old stride)
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/render/CpuBufferStore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool cond, const std::string& name) {
  if (cond) { std::printf("  PASS: %s\n", name.c_str()); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name.c_str()); ++failed; }
}

static const double kPi = 3.14159265358979323846;
static const double kTwoPi = 6.283185307179586;

// ---- the same 13-byte APPEND ingest record the other encode tests use --------
static void appendRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  };
  out.push_back(1);
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}
static void appendF32(dc::IngestProcessor& ing, dc::Id buf,
                      const std::vector<float>& v) {
  std::vector<std::uint8_t> b;
  appendRecord(b, buf, v.data(), static_cast<std::uint32_t>(v.size() * 4));
  ing.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
}
static void appendI32(dc::IngestProcessor& ing, dc::Id buf,
                      const std::vector<std::int32_t>& v) {
  std::vector<std::uint8_t> b;
  appendRecord(b, buf, v.data(), static_cast<std::uint32_t>(v.size() * 4));
  ing.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
}
static float f32At(const std::vector<std::uint8_t>& b, std::size_t off) {
  float v = 0.0f; std::memcpy(&v, b.data() + off, 4); return v;
}

// ---- a table of `wedges` equal slices covering `total` radians ---------------
struct Fixture {
  dc::IngestProcessor ingest;
  dc::TableStore tables;
  dc::Encoding enc;
  static constexpr dc::Id kTable = 1, kT0 = 10, kR0 = 11, kT1 = 12,
                         kR1 = 13, kCol = 14;

  Fixture() {
    tables.defineTable(kTable, "wedges");
    tables.addColumn(kTable, "t0", dc::DType::F32, kT0);
    tables.addColumn(kTable, "r0", dc::DType::F32, kR0);
    tables.addColumn(kTable, "t1", dc::DType::F32, kT1);
    tables.addColumn(kTable, "r1", dc::DType::F32, kR1);
    tables.addColumn(kTable, "col", dc::DType::I32, kCol);
    enc.field(dc::Channel::X, "t0").field(dc::Channel::Y, "r0")
       .field(dc::Channel::X2, "t1").field(dc::Channel::Y2, "r1");
    enc.setColorField("col");
  }
  // Append one wedge [a0,a1] with outer radius rOuter (inner 0).
  void add(double a0, double a1, double rOuter) {
    appendF32(ingest, kT0, {static_cast<float>(a0)});
    appendF32(ingest, kR0, {0.0f});
    appendF32(ingest, kT1, {static_cast<float>(a1)});
    appendF32(ingest, kR1, {static_cast<float>(rOuter)});
    appendI32(ingest, kCol, {static_cast<std::int32_t>(0xFF0000FFu)});
  }
  void addEqualSlices(int wedges, double total, double rOuter) {
    for (int i = 0; i < wedges; ++i)
      add(total * i / wedges, total * (i + 1) / wedges, rOuter);
  }
};

// Max distance, in pixels, from the TRUE outer arc of wedge 0 to the chord
// polyline the compile actually emitted. `sx`/`sy` are the clip->pixel scales.
static double outerDeviationPx(const std::vector<std::uint8_t>& bytes, int segs,
                               double a0, double a1, double rOuter,
                               double sx, double sy) {
  struct P { double x, y; };
  std::vector<P> poly;
  for (int s = 0; s < segs; ++s) {
    // vertex layout per slice: inner_a, OUTER_a, OUTER_b, inner_a, outer_b, inner_b
    const std::size_t base = static_cast<std::size_t>(s) * 6u * 24u;
    P a{f32At(bytes, base + 1 * 24 + 0) * sx, f32At(bytes, base + 1 * 24 + 4) * sy};
    P b{f32At(bytes, base + 2 * 24 + 0) * sx, f32At(bytes, base + 2 * 24 + 4) * sy};
    if (poly.empty()) poly.push_back(a);
    poly.push_back(b);
  }
  auto distSeg = [](P q, P a, P b) {
    const double vx = b.x - a.x, vy = b.y - a.y;
    const double wx = q.x - a.x, wy = q.y - a.y;
    const double L2 = vx * vx + vy * vy;
    double t = (L2 > 0.0) ? (wx * vx + wy * vy) / L2 : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(q.x - (a.x + t * vx), q.y - (a.y + t * vy));
  };
  double maxDev = 0.0;
  const int N = 8000;
  for (int i = 0; i <= N; ++i) {
    const double th = a0 + (a1 - a0) * static_cast<double>(i) / N;
    P q{rOuter * std::cos(th) * sx, rOuter * std::sin(th) * sy};
    double best = 1e300;
    for (std::size_t k = 0; k + 1 < poly.size(); ++k)
      best = std::min(best, distSeg(q, poly[k], poly[k + 1]));
    maxDev = std::max(maxDev, best);
  }
  return maxDev;
}

int main() {
  std::printf("=== ENC-995 arc/wedge tessellation is angular, not per-wedge ===\n");
  dc::EncodePass pass;

  // A 900x900 viewport: clip r=1.0 is 450 px, so the shapes below sit at 427.5 px.
  const double kSx = 450.0, kSy = 450.0;
  const double kROuter = 0.95;
  const double kRadiusPx = kROuter * kSx;

  // =======================================================================
  // [1] arcSegmentsFor bounds the CHORD ANGLE, whatever the span.
  // =======================================================================
  {
    std::printf("-- [1] derived count bounds the chord angle --\n");
    for (int perTurn : {12, 36, 72, 180}) {
      dc::ArcOptions ao; ao.segmentsPerTurn = perTurn;
      const double bound = kTwoPi / perTurn;
      bool allWithin = true;
      double worst = 0.0;
      // every span from a sliver to a full turn
      for (int d = 1; d <= 360; ++d) {
        const double span = kPi * d / 180.0;
        const int segs = dc::arcSegmentsFor(ao, span);
        if (segs < 1) { allWithin = false; break; }
        const double chord = span / segs;
        worst = std::max(worst, chord);
        if (chord > bound * (1.0 + 1e-6)) allWithin = false;
      }
      check(allWithin,
            "segmentsPerTurn=" + std::to_string(perTurn) +
                ": every span 1..360 deg keeps the chord <= " +
                std::to_string(bound * 180.0 / kPi) + " deg (worst " +
                std::to_string(worst * 180.0 / kPi) + ")");
    }
    // an exact full turn costs exactly segmentsPerTurn chords, not one more
    dc::ArcOptions d72;
    check(dc::arcSegmentsFor(d72, kTwoPi) == 72,
          "a full turn derives exactly segmentsPerTurn (72) chords");
    check(dc::arcSegmentsFor(d72, 0.0) == 1,
          "a degenerate zero span derives 1 chord, never 0");
  }

  // =======================================================================
  // [2] BACK-COMPAT: a positive segmentsPerArc is still an exact override.
  // =======================================================================
  {
    std::printf("-- [2] explicit segmentsPerArc overrides exactly --\n");
    dc::ArcOptions ao; ao.segmentsPerArc = 4;
    check(dc::arcSegmentsFor(ao, kTwoPi) == 4 &&
              dc::arcSegmentsFor(ao, 0.01) == 4,
          "segmentsPerArc=4 is returned verbatim for every span");

    Fixture f; f.addEqualSlices(2, kTwoPi, 0.8);
    auto src = dc::makeBufferByteSource(f.ingest);
    auto res = pass.compile(dc::Mark::Arc, f.enc, f.tables, Fixture::kTable, src,
                            100, 200, 300, nullptr, dc::LineStyle::Line2d, ao);
    check(res.ok && res.geometry.vertexCount == 2u * 4u * 6u,
          "compile honours segmentsPerArc=4 (2 wedges * 4 chords * 6 verts)");
  }

  // =======================================================================
  // [3] END TO END: the deviation of the COMPILED geometry is under the
  //     analytic bound for every shape. This is the check the fixed 24 fails
  //     (a full-turn wedge landed 3.66 px out against a 0.41 px bound).
  // =======================================================================
  {
    std::printf("-- [3] compiled deviation <= analytic bound, every shape --\n");
    dc::ArcOptions ao;  // derived, segmentsPerTurn = 72
    const double bound = kRadiusPx * (1.0 - std::cos(kPi / ao.segmentsPerTurn));
    std::printf("     bound at R=%.1f px, %d chords/turn: %.4f px\n",
                kRadiusPx, ao.segmentsPerTurn, bound);
    struct Shape { const char* name; int wedges; double total; };
    const Shape shapes[] = {
        {"1 wedge, 360 deg (full ring)", 1, kTwoPi},
        {"1 wedge, 270 deg (gauge)",     1, 1.5 * kPi},
        {"1 wedge, 180 deg",             1, kPi},
        {"4-slice pie",                  4, kTwoPi},
        {"8-slice pie",                  8, kTwoPi},
        {"12-slice pie",                12, kTwoPi},
        {"32-slice radial",             32, kTwoPi},
    };
    for (const auto& sh : shapes) {
      Fixture f; f.addEqualSlices(sh.wedges, sh.total, kROuter);
      auto src = dc::makeBufferByteSource(f.ingest);
      auto res = pass.compile(dc::Mark::Arc, f.enc, f.tables, Fixture::kTable,
                              src, 100, 200, 300, nullptr, dc::LineStyle::Line2d,
                              ao);
      if (!res.ok) { check(false, std::string(sh.name) + ": compile ok"); continue; }
      const int segs =
          static_cast<int>(res.geometry.vertexCount / (6u * static_cast<std::uint32_t>(sh.wedges)));
      const double span = sh.total / sh.wedges;
      const double dev = outerDeviationPx(res.bytes, segs, 0.0, span, kROuter,
                                          kSx, kSy);
      char msg[256];
      std::snprintf(msg, sizeof(msg),
                    "%-30s segs=%3d  maxDev=%.4f px  (bound %.4f)", sh.name,
                    segs, dev, bound);
      check(dev <= bound * 1.02, msg);
    }
  }

  // =======================================================================
  // [4] The deviation does NOT depend on how the table splits the circle.
  //     Under the fixed 24 the same ring measured 3.6573 px as one wedge and
  //     0.0254 px as twelve — a 144x spread driven by nothing visible.
  // =======================================================================
  {
    std::printf("-- [4] deviation is independent of the slice count --\n");
    dc::ArcOptions ao;
    double devs[3] = {0, 0, 0};
    const int splits[3] = {1, 4, 12};
    for (int i = 0; i < 3; ++i) {
      Fixture f; f.addEqualSlices(splits[i], kTwoPi, kROuter);
      auto src = dc::makeBufferByteSource(f.ingest);
      auto res = pass.compile(dc::Mark::Arc, f.enc, f.tables, Fixture::kTable,
                              src, 100, 200, 300, nullptr, dc::LineStyle::Line2d,
                              ao);
      const int segs = static_cast<int>(
          res.geometry.vertexCount / (6u * static_cast<std::uint32_t>(splits[i])));
      devs[i] = outerDeviationPx(res.bytes, segs, 0.0, kTwoPi / splits[i],
                                 kROuter, kSx, kSy);
      std::printf("     %2d slice(s): segs/wedge=%2d  maxDev=%.4f px\n",
                  splits[i], segs, devs[i]);
    }
    const double lo = std::min({devs[0], devs[1], devs[2]});
    const double hi = std::max({devs[0], devs[1], devs[2]});
    char msg[192];
    std::snprintf(msg, sizeof(msg),
                  "1, 4 and 12 slices of the same ring deviate within 5%% of each "
                  "other (%.4f .. %.4f px)", lo, hi);
    check(hi <= lo * 1.05, msg);
  }

  // =======================================================================
  // [5] INCREMENTAL: appending a WIDER wedge raises the derived count, which
  //     retroactively changes the stride. compileInto must repack the whole
  //     buffer rather than append onto a layout that no longer exists.
  // =======================================================================
  {
    std::printf("-- [5] a widening append repacks instead of appending --\n");
    Fixture f;
    // three narrow wedges (20 deg each) -> derived 4 chords/wedge
    for (int i = 0; i < 3; ++i)
      f.add(kPi * (20.0 * i) / 180.0, kPi * (20.0 * (i + 1)) / 180.0, kROuter);
    auto src = dc::makeBufferByteSource(f.ingest);
    dc::ArcOptions ao;
    dc::CpuBufferStore store;

    auto r1 = pass.compileInto(dc::Mark::Arc, f.enc, f.tables, Fixture::kTable,
                               src, store, 100, 200, 300, /*fromRow=*/0, nullptr,
                               dc::LineStyle::Line2d, ao);
    const std::uint32_t narrowSegs = r1.geometry.vertexCount / (6u * 3u);
    check(r1.ok && narrowSegs > 0,
          "3 narrow wedges compiled at " + std::to_string(narrowSegs) +
              " chords/wedge");
    const std::uint32_t sizeAfter3 = store.getCpuDataSize(300);

    // now append ONE wedge spanning 300 degrees — far wider than any before it
    f.add(kPi * 20.0 / 180.0, kPi * 320.0 / 180.0, kROuter);
    auto r2 = pass.compileInto(dc::Mark::Arc, f.enc, f.tables, Fixture::kTable,
                               src, store, 100, 200, 300, /*fromRow=*/3, nullptr,
                               dc::LineStyle::Line2d, ao);
    const std::uint32_t wideSegs = r2.geometry.vertexCount / (6u * 4u);
    check(r2.ok && wideSegs > narrowSegs,
          "the wide append RAISED the derived count " +
              std::to_string(narrowSegs) + " -> " + std::to_string(wideSegs));
    check(store.getCpuDataSize(300) != sizeAfter3 + (sizeAfter3 / 3),
          "the store is NOT the old 3 wedges plus one appended tail");

    // the store must now be byte-identical to a clean full compile
    auto full = pass.compile(dc::Mark::Arc, f.enc, f.tables, Fixture::kTable, src,
                             100, 200, 300, nullptr, dc::LineStyle::Line2d, ao);
    const std::uint32_t have = store.getCpuDataSize(300);
    const bool sameLen = (have >= full.bytes.size());
    const bool sameBytes =
        sameLen && std::memcmp(store.getCpuData(300), full.bytes.data(),
                               full.bytes.size()) == 0;
    check(sameBytes,
          "after the widening append the store matches a full compile byte for "
          "byte (" + std::to_string(full.bytes.size()) + " B)");
    check(r2.geometry.vertexCount == full.geometry.vertexCount,
          "and reports the same vertexCount as the full compile");
  }

  std::printf("\n=== %d passed, %d failed ===\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
