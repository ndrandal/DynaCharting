// ENC-596 (P1.5) — Linear scale + streaming O(Δ) auto-domain tests.
//
// Covers, per the ticket:
//   * linear map + inverse correctness (round-trip, endpoints, inverted range),
//   * the streaming auto-domain matches a brute-force O(N) reference min/max over
//     a GROWING (append-only) f32 column, fed through the UNCHANGED 13-byte feed,
//   * edge cases: empty column, single value, all-equal degenerate domain,
//   * the nice()-ing hook (minimal — full NiceTicks is ENC-599).
#include "dc/scale/Scale.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
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

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) <= eps * (1.0 + std::fabs(a) + std::fabs(b));
}

// One 13-byte ingest APPEND record (op=1) — the EXACT existing wire format.
static void appendRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(1);  // op = APPEND
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);    // offset (ignored for append)
  u32(len);  // payloadBytes
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

int main() {
  std::printf("=== ENC-596 Linear scale + streaming auto-domain ===\n");

  // ----- linear map + inverse correctness -----------------------------------
  {
    // domain [0,100] -> range [0,1]
    dc::LinearScale s(dc::Domain{0.0, 100.0, false}, dc::Range{0.0, 1.0});
    check(approx(s.map(0.0), 0.0), "map domain-min -> range-r0");
    check(approx(s.map(100.0), 1.0), "map domain-max -> range-r1");
    check(approx(s.map(50.0), 0.5), "map midpoint");
    check(approx(s.map(25.0), 0.25), "map quarter");
    // map past the domain extrapolates linearly (no clamp)
    check(approx(s.map(150.0), 1.5), "map extrapolates above domain");
    check(approx(s.map(-10.0), -0.1), "map extrapolates below domain");

    // inverse round-trips
    check(approx(s.invert(0.0), 0.0), "invert r0 -> domain-min");
    check(approx(s.invert(1.0), 100.0), "invert r1 -> domain-max");
    check(approx(s.invert(s.map(42.0)), 42.0), "map/invert round-trip");
    check(approx(s.map(s.invert(0.73)), 0.73), "invert/map round-trip");
  }

  // ----- pixel-style range (offset + inverted) ------------------------------
  {
    // data [10,20] -> screen-y [480,20] (y grows DOWN: r0 > r1, a flipped axis)
    dc::LinearScale s(dc::Domain{10.0, 20.0, false}, dc::Range{480.0, 20.0});
    check(approx(s.map(10.0), 480.0), "inverted-range: domain-min -> top px");
    check(approx(s.map(20.0), 20.0), "inverted-range: domain-max -> bottom px");
    check(approx(s.map(15.0), 250.0), "inverted-range: midpoint px");
    check(approx(s.invert(250.0), 15.0), "inverted-range invert round-trip");
  }

  // ----- degenerate domains -------------------------------------------------
  {
    // empty domain -> everything parks at the range midpoint
    dc::LinearScale empty;
    empty.setRange(0.0, 200.0);
    check(empty.domain().empty, "default domain is empty");
    check(approx(empty.map(123.0), 100.0), "empty domain maps to range midpoint");
    check(approx(empty.invert(100.0), empty.domain().min),
          "empty domain invert -> domain min");

    // single-value / all-equal domain (span == 0) -> midpoint too
    dc::LinearScale flat(dc::Domain{7.0, 7.0, false}, dc::Range{0.0, 100.0});
    check(approx(flat.map(7.0), 50.0), "all-equal domain maps to midpoint");
    check(approx(flat.map(999.0), 50.0),
          "all-equal domain maps ANY value to midpoint");
    check(approx(flat.invert(50.0), 7.0), "all-equal domain invert -> the value");

    // degenerate (zero-width) range -> invert returns domain min, no div-by-zero
    dc::LinearScale zr(dc::Domain{0.0, 10.0, false}, dc::Range{5.0, 5.0});
    check(approx(zr.map(3.0), 5.0), "zero-width range maps to that point");
    check(approx(zr.invert(5.0), 0.0), "zero-width range invert -> domain min");
  }

  // ----- RunningDomain: O(Δ) reducer over a growing in-memory array ----------
  {
    dc::RunningDomain rd;
    check(rd.domain().empty, "fresh running domain is empty");
    check(rd.consumedCount() == 0, "fresh running domain consumed 0");

    std::vector<float> data;
    auto bruteMin = [&]() {
      double m = std::numeric_limits<double>::max();
      for (float v : data) m = std::min(m, static_cast<double>(v));
      return m;
    };
    auto bruteMax = [&]() {
      double m = std::numeric_limits<double>::lowest();
      for (float v : data) m = std::max(m, static_cast<double>(v));
      return m;
    };

    // Grow the array in irregular chunks; after each growth fold ONLY the tail
    // and assert the running [min,max] equals a brute-force O(N) rescan.
    const float chunks[][4] = {
        {3.0f, 1.0f, 4.0f, 1.0f},
        {5.0f, 9.0f, 2.0f, 6.0f},
        {-2.0f, 8.0f, 7.0f, 0.0f},
    };
    bool allMatch = true;
    std::size_t prevConsumed = 0;
    for (const auto& c : chunks) {
      for (float v : c) data.push_back(v);
      rd.reduceFrom(data.data(), data.size());
      if (!approx(rd.domain().min, bruteMin())) allMatch = false;
      if (!approx(rd.domain().max, bruteMax())) allMatch = false;
      if (rd.consumedCount() != data.size()) allMatch = false;
      // Δ-only: consumed advanced by exactly the chunk size each step.
      if (rd.consumedCount() - prevConsumed != 4) allMatch = false;
      prevConsumed = rd.consumedCount();
    }
    check(allMatch, "streaming min/max matches brute-force O(N) across growth");

    // Re-folding with NO new rows is a no-op (idempotent, truly O(Δ)=O(0)).
    double mn = rd.domain().min, mx = rd.domain().max;
    std::size_t cc = rd.consumedCount();
    rd.reduceFrom(data.data(), data.size());
    check(approx(rd.domain().min, mn) && approx(rd.domain().max, mx) &&
              rd.consumedCount() == cc,
          "re-fold with no new rows is a no-op");

    // A SHRINK (replaced column) resets + re-folds cleanly.
    std::vector<float> small = {100.0f, 50.0f};
    rd.reduceFrom(small.data(), small.size());
    check(approx(rd.domain().min, 50.0) && approx(rd.domain().max, 100.0) &&
              rd.consumedCount() == 2,
          "shrink resets and re-folds whole array");
  }

  // ----- streaming auto-domain over a GROWING ingest column (the real path) --
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);

    const dc::Id kBuf = 200;  // f32 price column
    const dc::Id kTable = 7;
    check(tables.defineTable(kTable, "series"), "defineTable for scale");
    check(tables.addColumn(kTable, "y", dc::DType::F32, kBuf), "addColumn y/f32");

    dc::LinearScale s;
    s.setRange(0.0, 1.0);
    s.bindColumn(kTable, "y");
    check(s.hasBoundColumn(), "scale has bound column");

    // No rows yet: updateDomain is a no-op and reports false.
    check(!s.updateDomain(tables, src), "updateDomain false on empty column");
    check(s.domain().empty, "domain still empty before any append");

    // Brute-force reference over the full logical column.
    std::vector<float> all;
    auto refMin = [&]() {
      double m = std::numeric_limits<double>::max();
      for (float v : all) m = std::min(m, static_cast<double>(v));
      return m;
    };
    auto refMax = [&]() {
      double m = std::numeric_limits<double>::lowest();
      for (float v : all) m = std::max(m, static_cast<double>(v));
      return m;
    };

    bool streamMatches = true;
    // Tick 1: append 3 rows.
    {
      float v[3] = {12.0f, 8.0f, 20.0f};
      for (float x : v) all.push_back(x);
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kBuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      check(s.updateDomain(tables, src), "updateDomain true after first append");
      if (!approx(s.domain().min, refMin())) streamMatches = false;
      if (!approx(s.domain().max, refMax())) streamMatches = false;
    }
    // Tick 2: append 2 more rows, one a new global min, one a new global max.
    {
      float v[2] = {-5.0f, 33.0f};
      for (float x : v) all.push_back(x);
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kBuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      s.updateDomain(tables, src);
      if (!approx(s.domain().min, refMin())) streamMatches = false;
      if (!approx(s.domain().max, refMax())) streamMatches = false;
    }
    // Tick 3: append rows entirely INSIDE the current range (must not change it).
    {
      float v[4] = {0.0f, 1.0f, 10.0f, 15.0f};
      for (float x : v) all.push_back(x);
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kBuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      s.updateDomain(tables, src);
      if (!approx(s.domain().min, refMin())) streamMatches = false;
      if (!approx(s.domain().max, refMax())) streamMatches = false;
    }
    check(streamMatches,
          "scale auto-domain tracks brute-force min/max across 3 ticks");
    check(s.runningDomain().consumedCount() == all.size(),
          "reducer consumed exactly the appended row count (O(Δ) high-water)");
    check(approx(s.domain().min, -5.0) && approx(s.domain().max, 33.0),
          "final auto-domain is [-5, 33]");

    // The scale now maps live data correctly against the streamed domain.
    check(approx(s.map(-5.0), 0.0) && approx(s.map(33.0), 1.0),
          "scale maps endpoints of the STREAMED domain");

    // Binding a non-f32 / missing column yields no domain update.
    dc::LinearScale bad;
    bad.bindColumn(kTable, "missing");
    check(!bad.updateDomain(tables, src),
          "updateDomain false for missing column");
  }

  // ----- nice()-ing hook (minimal; full NiceTicks is ENC-599) ----------------
  {
    dc::LinearScale s(dc::Domain{0.3, 9.7, false}, dc::Range{0.0, 1.0});
    s.nice(5);  // ~5 intervals over span 9.4 -> nice step 2 -> [0, 10]
    check(approx(s.domain().min, 0.0) && approx(s.domain().max, 10.0),
          "nice() rounds [0.3,9.7] outward to [0,10]");

    // nice() is a no-op on a degenerate domain (no crash, no NaN).
    dc::LinearScale flat(dc::Domain{5.0, 5.0, false}, dc::Range{0.0, 1.0});
    flat.nice();
    check(approx(flat.domain().min, 5.0) && approx(flat.domain().max, 5.0),
          "nice() no-op on degenerate domain");

    dc::LinearScale emptyNice;
    emptyNice.nice();
    check(emptyNice.domain().empty, "nice() no-op on empty domain");
  }

  std::printf("=== ENC-596 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
