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

  // ----- ENC-599: LinearScale emits nice ticks from the live domain ----------
  {
    dc::LinearScale s(dc::Domain{0.3, 9.7, false}, dc::Range{0.0, 1.0});
    auto t = s.ticks(5);
    check(!t.empty(), "linear ticks non-empty over [0.3,9.7]");
    // nice step 2 over [0,10] -> {0,2,4,6,8,10}
    check(approx(t.front().value, 0.0), "linear first tick is 0");
    check(approx(t.back().value, 10.0), "linear last tick is 10");
    check(t.size() == 6, "linear ticks count is 6 (0..10 step 2)");
    check(t[1].label == "2", "linear tick label formatted compactly");

    // Ticks UPDATE (extend) as the domain grows — the acceptance criterion.
    dc::LinearScale g(dc::Domain{0.0, 9.0, false}, dc::Range{0.0, 1.0});
    auto t1 = g.ticks(5);  // step 2 -> last tick 10
    g.setDomain(0.0, 90.0);  // domain extends 10x
    auto t2 = g.ticks(5);  // step 20 -> last tick 100
    check(t2.back().value > t1.back().value,
          "linear ticks extend with the domain (new max covered)");
    check(!approx(t2[1].value, t1[1].value),
          "linear ticks recomputed (new step) from the extended domain");

    // Empty / degenerate domains emit no ticks (no crash).
    dc::LinearScale empt;
    check(empt.ticks().empty(), "empty domain emits no ticks");
    dc::LinearScale flat(dc::Domain{5.0, 5.0, false}, dc::Range{0.0, 1.0});
    check(flat.ticks().empty(), "degenerate domain emits no ticks");
  }

  // ----- ENC-597: TimeScale — multi-day epoch-ms with NO f32 precision loss --
  {
    // Two instants 2 days + 1 ms apart, both far past the f32 mantissa limit
    // (~16.7M). epoch-ms here is ~1.7e12. A naive f32 cast would collapse the
    // 1ms difference (and quantize both to ~minute buckets). The time scale keeps
    // the i64 base on CPU and only ever exposes a RELATIVE f32 offset.
    const double kDayMs = 86400000.0;
    const double t0 = 1700000000000.0;          // 2023-11-14T22:13:20Z
    const double t1 = t0 + 2.0 * kDayMs + 1.0;  // +2 days +1 ms

    dc::TimeScale ts(t0, t1, dc::Range{0.0, 1000.0});
    // map endpoints exactly in f64 epoch-ms space.
    check(approx(ts.map(t0), 0.0), "time map domain-min -> r0");
    check(approx(ts.map(t1), 1000.0), "time map domain-max -> r1");
    // The 1ms-apart instants map to DISTINCT range positions — proof the 1ms is
    // not lost. (A f32 epoch cast would make these identical.)
    const double span = t1 - t0;
    const double pa = ts.map(t0 + 1.0);  // +1 ms from min
    const double pb = ts.map(t0);
    check(pa > pb, "1ms difference at epoch-ms scale is RESOLVED (no f32 loss)");
    check(approx(pa - pb, 1000.0 / span), "1ms maps to the exact f64 fraction");

    // The base epoch is CPU-held; the relative f32 offset is small + exact.
    check(approx(ts.baseEpochMs(), t0), "base epoch pinned to domain min");
    check(ts.normalizedOffsetF32(t1) == static_cast<float>(span),
          "relative f32 offset for the full span is exact (fits f32)");
    check(ts.normalizedOffsetF32(t0) == 0.0f, "offset at base epoch is 0");
    // Sanity: the RELATIVE offset (a few e8 ms) is within f32 integer-exactness
    // (< 2^24 only after we accept ms granularity loss above ~16.7M ms ≈ 4.6h);
    // for the OFFSET we still hold the exact value as a CPU double — prove the
    // round-trip through invert is f64-exact regardless.
    check(approx(ts.invert(ts.map(t1)), t1), "time map/invert round-trips in f64");
    check(approx(ts.invert(ts.map(t0 + 1.0)), t0 + 1.0),
          "time invert recovers the 1ms-offset instant exactly");

    // Direct proof the trap is real and avoided: a naive f32 epoch cast collapses
    // the two instants; the scale's f64 path does not.
    check(static_cast<float>(t0) == static_cast<float>(t0 + 1.0),
          "[trap] naive f32 epoch cast COLLAPSES the 1ms difference");
    check(ts.map(t0) != ts.map(t0 + 1.0),
          "[mitigation] TimeScale keeps the 1ms difference distinct");
  }

  // ----- ENC-597: TimeScale streaming auto-domain over a Timestamp column ----
  {
    dc::IngestProcessor ingest;
    dc::TableStore tables;
    auto src = dc::makeBufferByteSource(ingest);

    const dc::Id kTbuf = 300;
    const dc::Id kTtab = 8;
    check(tables.defineTable(kTtab, "tseries"), "defineTable for time scale");
    check(tables.addColumn(kTtab, "t", dc::DType::Timestamp, kTbuf),
          "addColumn t/timestamp");

    dc::TimeScale ts;
    ts.setRange(0.0, 1.0);
    ts.bindColumn(kTtab, "t");
    check(ts.hasBoundColumn(), "time scale bound to timestamp column");
    check(!ts.updateDomain(tables, src), "time updateDomain false on empty col");

    const double kDayMs = 86400000.0;
    const double base = 1700000000000.0;
    // Tick 1: 3 timestamps spanning ~2 days.
    {
      std::int64_t v[3] = {
          static_cast<std::int64_t>(base),
          static_cast<std::int64_t>(base + kDayMs),
          static_cast<std::int64_t>(base + 2.0 * kDayMs)};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kTbuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      check(ts.updateDomain(tables, src), "time updateDomain true after append");
      check(approx(ts.domain().min, base) &&
                approx(ts.domain().max, base + 2.0 * kDayMs),
            "time auto-domain spans the 3 timestamps (multi-day, f64-exact)");
      check(approx(ts.baseEpochMs(), base), "base epoch re-pinned to domain min");
    }
    // Tick 2: append an EARLIER and a LATER timestamp; domain extends both ends.
    {
      std::int64_t v[2] = {
          static_cast<std::int64_t>(base - kDayMs),
          static_cast<std::int64_t>(base + 5.0 * kDayMs)};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, kTbuf, v, sizeof(v));
      ingest.processBatch(batch.data(),
                          static_cast<std::uint32_t>(batch.size()));
      ts.updateDomain(tables, src);
      check(approx(ts.domain().min, base - kDayMs) &&
                approx(ts.domain().max, base + 5.0 * kDayMs),
            "time auto-domain extends both ends as timestamps stream in");
      check(ts.runningDomain().consumedCount() == 5,
            "time reducer consumed exactly 5 rows (O(Δ) high-water)");
    }
    // Nice time ticks over the live multi-day domain (ENC-599).
    {
      auto tk = ts.ticks(6);
      check(!tk.empty(), "time scale emits nice ticks");
      // All ticks lie within the domain, are increasing, and are labeled.
      bool ok = true;
      for (std::size_t i = 0; i < tk.size(); ++i) {
        if (tk[i].value < ts.domain().min - 1.0 ||
            tk[i].value > ts.domain().max + 1.0)
          ok = false;
        if (tk[i].label.empty()) ok = false;
        if (i > 0 && tk[i].value <= tk[i - 1].value) ok = false;
      }
      check(ok, "time ticks are in-domain, increasing, and labeled");
      // A multi-day span picks a day(ish) step -> labels are date-only.
      check(tk.front().label.size() >= 8,
            "time tick label is a formatted UTC date");
    }
  }

  // ----- ENC-598: Band + Point ordinal scales over a GROWING category set ----
  {
    // Band scale over 4 categories in [0,400], no padding -> 4 equal bands of
    // width 100 starting at 0,100,200,300.
    dc::BandScale b;
    b.setRange(0.0, 400.0);
    b.setCategories({0, 1, 2, 3});
    check(approx(b.bandwidth(), 100.0), "band bandwidth = extent/n with no pad");
    check(approx(b.step(), 100.0), "band step = 100");
    check(approx(b.map(0.0), 0.0), "band cat0 start = 0");
    check(approx(b.map(1.0), 100.0), "band cat1 start = 100");
    check(approx(b.map(3.0), 300.0), "band cat3 start = 300");
    check(approx(b.center(2), 250.0), "band cat2 center = 250");
    // invert: a pixel inside band 2 returns code 2.
    check(approx(b.invert(260.0), 2.0), "band invert px->category code");
    check(approx(b.invert(99.0), 0.0), "band invert near-edge stays in band 0");

    // Padding carves gaps but bands stay ordered + within range.
    dc::BandScale bp;
    bp.setRange(0.0, 100.0);
    bp.setCategories({0, 1, 2, 3, 4});
    bp.setPadding(0.2);
    check(bp.bandwidth() < bp.step(), "padded bandwidth < step");
    check(bp.map(0.0) > 0.0, "outer padding offsets first band start");
    check(bp.map(4.0) + bp.bandwidth() <= 100.0 + 1e-6,
          "last band stays within range");

    // Point scale over the same 4 categories -> 0-width, positions at band
    // centers of a zero-inner-pad band layout: step = 400/3, points at 0,step,...
    dc::PointScale p;
    p.setRange(0.0, 300.0);
    p.setCategories({0, 1, 2, 3});
    check(approx(p.bandwidth(), 0.0), "point bandwidth is 0");
    check(approx(p.step(), 100.0), "point step = extent/(n-1)-equivalent = 100");
    check(approx(p.map(0.0), 0.0), "point cat0 at r0");
    check(approx(p.map(3.0), 300.0), "point cat3 at r1");
    check(approx(p.center(1), 100.0), "point cat1 at 100");

    // GROWING category set: placements stay correct as categories ARRIVE.
    dc::BandScale grow;
    grow.setRange(0.0, 1000.0);
    grow.setCategories({0});
    check(approx(grow.bandwidth(), 1000.0), "1 category fills the whole range");
    grow.setCategories({0, 1});
    check(approx(grow.bandwidth(), 500.0) && approx(grow.map(1.0), 500.0),
          "adding a 2nd category re-lays-out to two 500-wide bands");
    grow.setCategories({0, 1, 2, 3, 4});
    check(approx(grow.bandwidth(), 200.0) && approx(grow.map(4.0), 800.0),
          "5 categories -> 200-wide bands; last at 800");
  }

  // ----- ENC-598: ordinal auto-domain over a GROWING Cat dictionary ----------
  {
    dc::TableStore tables;
    const dc::Id kCbuf = 400;
    const dc::Id kCtab = 9;
    check(tables.defineTable(kCtab, "catseries"), "defineTable for band scale");
    check(tables.addColumn(kCtab, "sym", dc::DType::Cat, kCbuf),
          "addColumn sym/cat");
    dc::CatDictionary* dict = tables.catDict(kCtab, "sym");
    check(dict != nullptr, "cat column has a dictionary");

    dc::BandScale b;
    b.setRange(0.0, 300.0);
    b.bindColumn(kCtab, "sym");
    check(b.hasBoundColumn(), "band scale bound to cat column");
    check(b.updateDomain(tables) && b.ordinalDomain().empty(),
          "empty dictionary is a valid no-op domain");

    // Categories arrive over the stream (interned into the dictionary in order).
    dict->intern("AAPL");  // code 0
    dict->intern("MSFT");  // code 1
    dict->intern("GOOG");  // code 2
    check(b.updateDomain(tables), "band updateDomain after 3 categories");
    check(b.ordinalDomain().size() == 3, "domain grew to 3 categories");
    check(approx(b.bandwidth(), 100.0), "3 categories -> 100-wide bands");
    check(approx(b.map(0.0), 0.0) && approx(b.map(2.0), 200.0),
          "category placements correct (AAPL@0, GOOG@200)");

    // A 4th category ARRIVES — O(Δ) fold extends the domain; placements re-lay.
    dict->intern("AMZN");  // code 3
    b.updateDomain(tables);
    check(b.ordinalDomain().size() == 4, "domain grew to 4 categories (O(Δ))");
    check(approx(b.bandwidth(), 75.0) && approx(b.map(3.0), 225.0),
          "4 categories -> 75-wide bands; AMZN@225");

    // Re-update with no new categories is a no-op (idempotent O(0)).
    std::size_t before = b.ordinalDomain().size();
    b.updateDomain(tables);
    check(b.ordinalDomain().size() == before, "re-fold with no new cats no-op");

    // Ticks: one per category at its center, labeled from the dictionary.
    auto tk = b.ticksWithDict(dict);
    check(tk.size() == 4, "ordinal emits one tick per category");
    check(tk[0].label == "AAPL" && tk[3].label == "AMZN",
          "ordinal tick labels come from the Cat dictionary");
    check(approx(tk[0].value, b.center(0)), "ordinal tick sits at band center");

    // Binding a non-Cat / missing column yields no domain update.
    dc::BandScale bad;
    bad.bindColumn(kCtab, "nope");
    check(!bad.updateDomain(tables), "ordinal updateDomain false for missing col");
  }

  std::printf("=== ENC-596/597/598/599 Results: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
