// ENC-620b (Epic ENC-620) — FEED→FRAMES REPLAY / PROPERTY HARNESS test.
//
// THE GRADING ORACLE (RESEARCH §6.5): a manifest is a PURE function feed→frames, so
// replay it against an ADVERSARIAL synthetic-feed corpus and assert structural
// INVARIANTS on the produced DrawItems + geometry BYTES (no GPU, no pixels). This
// test proves both directions of the oracle:
//
//   (A) KNOWN-GOOD manifests (the §6.2 candle+SMA, and a bar/rect chart) pass EVERY
//       invariant across the WHOLE finite-data corpus — empty, single-record,
//       all-equal / degenerate-domain, monotonic, out-of-order, and a 1e5-record
//       burst — and NEVER crash build() on any case (incl. the NaN feed + burst).
//
//   (B) The ADVERSARIAL probe IS CAUGHT by the RIGHT invariant: the NaN feed drives
//       a value into a vertex buffer and the FiniteChannels invariant flags exactly
//       that (manifest, feed, mark); a deliberately MIS-SET manifest (a too-narrow
//       literal scale domain that pushes positions off the pane) is caught by the
//       InPane invariant. A clean manifest never trips these on finite data.
#include "dc/harness/ReplayHarness.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace dc;
using namespace dc::harness;

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

// Count violations of a given invariant in an outcome.
static std::size_t countInv(const ReplayOutcome& o, Invariant inv) {
  std::size_t n = 0;
  for (const auto& v : o.violations)
    if (v.invariant == inv) ++n;
  return n;
}

// True if the outcome has ANY violation of `inv`.
static bool hasInv(const ReplayOutcome& o, Invariant inv) {
  return countInv(o, inv) > 0;
}

// The trivial CPU rolling-mean SMA helper (mirrors the ENC-606 proof): read the
// pivoted `close` column and patch the pre-existing `sma20` column in place (op=2)
// so the table stays in lockstep. Wired as the harness DerivedHook.
static void smaDerivedHook(Manifest& m, IngestProcessor& ingest,
                           const BufferByteSource& src) {
  auto tid = m.tableId("ohlc");
  if (!tid.has_value()) return;
  auto closeCol = m.tables().viewF32(*tid, "close", src);
  if (!closeCol.valid() || closeCol.count == 0) return;
  const int W = 20;
  std::vector<float> sma(closeCol.count, 0.0f);
  for (std::size_t i = 0; i < closeCol.count; ++i) {
    const std::size_t lo =
        (i + 1 >= static_cast<std::size_t>(W)) ? i + 1 - W : 0;
    double sum = 0.0;
    for (std::size_t j = lo; j <= i; ++j) sum += closeCol[j];
    sma[i] = static_cast<float>(sum / static_cast<double>(i - lo + 1));
  }
  auto buf = m.columnBufferId("ohlc", "sma20");
  if (!buf) return;
  // op=2 UPDATE_RANGE record (13-byte header + payload) — the unchanged feed.
  std::vector<std::uint8_t> batch;
  auto u32 = [&batch](std::uint32_t v) {
    batch.push_back(static_cast<std::uint8_t>(v & 0xFF));
    batch.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    batch.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    batch.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  batch.push_back(2);  // op = UPDATE_RANGE
  u32(static_cast<std::uint32_t>(*buf));
  u32(0);  // offset
  u32(static_cast<std::uint32_t>(sma.size() * sizeof(float)));
  const auto* p = reinterpret_cast<const std::uint8_t*>(sma.data());
  batch.insert(batch.end(), p, p + sma.size() * sizeof(float));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// ---------------------------------------------------------------------------
// MANIFEST A — the §6.2 candle + SMA: TimeScale x (epoch-ms `t`) + LinearScale y,
// a candle mark (instancedCandle@1) + an SMA line (line2d@1). The KNOWN-GOOD spec.
// ---------------------------------------------------------------------------
static const char* kCandleSma = R"JSON(
{
  "version": "dc-manifest/1", "id": "candles-sma",
  "data": { "sources": [{
    "id": "ohlc", "kind": "stream",
    "stream": {
      "rowKey": "t",
      "columns": {
        "t":      {"from":"rowKey","dtype":"timestamp","role":"time"},
        "open":   {"from":"field:open","dtype":"f32"},
        "high":   {"from":"field:high","dtype":"f32"},
        "low":    {"from":"field:low","dtype":"f32"},
        "close":  {"from":"field:close","dtype":"f32"},
        "volume": {"from":"field:volume","dtype":"f32"},
        "sma20":  {"from":"field:sma20","dtype":"f32"}
      } } }] },
  "scales": [
    { "id":"xt","type":"time","domainFrom":{"data":"ohlc","field":"t"},"range":"width" },
    { "id":"yp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},"range":"height" }
  ],
  "coords": { "type":"cartesian" },
  "marks": [
    { "id":"candles","type":"candle","from":"ohlc","pipeline":"instancedCandle@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},
        "yOpen":{"scale":"yp","field":"open"},"yClose":{"scale":"yp","field":"close"},
        "yHigh":{"scale":"yp","field":"high"},"yLow":{"scale":"yp","field":"low"},
        "width":{"value":0.01},
        "color":{"condition":{"value":"#26a69a"},"value":"#ef5350"} } },
    { "id":"smaLine","type":"line","from":"ohlc","pipeline":"line2d@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},"y":{"scale":"yp","field":"sma20"},
        "color":{"value":"#ffb300"} } }
  ]
}
)JSON";

// ---------------------------------------------------------------------------
// MANIFEST B — a BAR / rect chart: instancedRect@1 over (low,high) so each bar's
// x0<x1 and y0/y1 span the candle range. A second KNOWN-GOOD spec exercising the
// Rect4 format + the x0≤x1 ORDERING invariant.
// ---------------------------------------------------------------------------
static const char* kBars = R"JSON(
{
  "version": "dc-manifest/1", "id": "bars",
  "data": { "sources": [{
    "id": "ohlc", "kind": "stream",
    "stream": {
      "rowKey": "t",
      "columns": {
        "t":     {"from":"rowKey","dtype":"f32","role":"x"},
        "low":   {"from":"field:low","dtype":"f32"},
        "high":  {"from":"field:high","dtype":"f32"}
      } } }] },
  "scales": [
    { "id":"xp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},"range":"width" },
    { "id":"yp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},"range":"height" }
  ],
  "coords": { "type":"cartesian" },
  "marks": [
    { "id":"bars","type":"rect","from":"ohlc","pipeline":"instancedRect@1",
      "encoding":{ "x":{"scale":"xp","field":"low"},"y":{"scale":"yp","field":"low"},
        "x2":{"scale":"xp","field":"high"},"y2":{"scale":"yp","field":"high"},
        "color":{"value":"#42a5f5"} } }
  ]
}
)JSON";

// ---------------------------------------------------------------------------
// MANIFEST C — a deliberately MIS-SET manifest: a LITERAL y-domain [100,100.5]
// that is far too narrow for the data (~98..122), so the linear y-scale maps the
// data WAY past the [0,1] range -> positions land off the pane. A sound oracle
// catches this with InPane on plain finite data (no NaN required).
// ---------------------------------------------------------------------------
static const char* kBrokenDomain = R"JSON(
{
  "version": "dc-manifest/1", "id": "broken-domain",
  "data": { "sources": [{
    "id": "ohlc", "kind": "stream",
    "stream": {
      "rowKey": "t",
      "columns": {
        "t":     {"from":"rowKey","dtype":"f32","role":"x"},
        "low":   {"from":"field:low","dtype":"f32"},
        "high":  {"from":"field:high","dtype":"f32"},
        "close": {"from":"field:close","dtype":"f32"}
      } } }] },
  "scales": [
    { "id":"xp","type":"linear","domainFrom":{"data":"ohlc","field":"low"},"range":"width" },
    { "id":"yp","type":"linear","domain":[100.0,100.5],"range":[0.0,1.0] }
  ],
  "coords": { "type":"cartesian" },
  "marks": [
    { "id":"line","type":"line","from":"ohlc","pipeline":"line2d@1",
      "encoding":{ "x":{"scale":"xp","field":"low"},"y":{"scale":"yp","field":"close"},
        "color":{"value":"#ffffff"} } }
  ]
}
)JSON";

int main() {
  std::printf("=== ENC-620b feed->frames replay / property harness ===\n");

  // The OHLC feed schema the corpus generator emits for both known-good manifests.
  FeedSchema schema;
  schema.fields = {"open", "high", "low", "close", "volume"};
  schema.burstRows = 100000;  // 1e5-record burst (RESEARCH §6.5)
  CorpusGenerator gen(schema);
  auto corpus = gen.all();
  check(corpus.size() == 7,
        "corpus: 7 adversarial cases generated "
        "(empty/single/monotonic/all-equal/out-of-order/nan/burst)");
  // Sanity: the burst really is large.
  {
    const SyntheticFeed* burst = nullptr;
    for (const auto& f : corpus) if (f.name == "burst") burst = &f;
    check(burst && burst->rowCount == 100000,
          "corpus: burst case is 1e5 rows");
  }

  // =========================================================================
  // (A) KNOWN-GOOD: candle + SMA passes EVERY invariant on finite-data feeds.
  // =========================================================================
  {
    ReplayHarness h;
    h.configure("ohlc", "t",
                {{"open", "open"}, {"high", "high"}, {"low", "low"},
                 {"close", "close"}, {"volume", "volume"}});
    h.setDerivedHook(smaDerivedHook);
    h.addDomainCheck("xt", {"t"}, /*timestampColumn=*/true);
    // yp auto-domains over `low` (the merged stack folds the FIRST domainFrom
    // field); DomainContains is asserted over exactly that folded column. The
    // candle's high/low ride a TIGHT band, so map() keeps them within the pane pad.
    h.addDomainCheck("yp", {"low"});
    PaneBounds pb;            // clip pane [0,1] for "width"/"height" ranges, padded
    pb.minX = 0.0f; pb.maxX = 1.0f; pb.minY = 0.0f; pb.maxY = 1.0f; pb.pad = 0.35f;
    h.setPaneBounds(pb);

    auto outcomes = h.replayCorpus(kCandleSma, corpus);
    check(outcomes.size() == corpus.size(),
          "candle: one outcome per corpus feed");

    bool noCrash = true;
    std::size_t cleanFinite = 0;
    for (const auto& o : outcomes) {
      // NoCrash invariant: build() must NEVER crash/reject on ANY case, incl. NaN
      // and the 1e5 burst (a degenerate/empty domain is handled, not fatal).
      if (hasInv(o, Invariant::BuildCrashed)) {
        noCrash = false;
        std::fprintf(stderr, "    build crash on feed '%s': %s\n",
                     o.feedName.c_str(),
                     o.violations.empty() ? "" : o.violations.front().message.c_str());
      }
      // Every FINITE-data feed must be perfectly clean (zero violations).
      const bool finiteFeed = (o.feedName != "with-nan");
      if (finiteFeed && o.clean()) ++cleanFinite;
      else if (finiteFeed && !o.clean())
        std::fprintf(stderr, "    DIRTY finite feed '%s': %s\n",
                     o.feedName.c_str(), o.violations.front().message.c_str());
    }
    check(noCrash, "candle: build() NEVER crashed on any corpus case (incl burst)");
    check(cleanFinite == 6,
          "candle: ALL 6 finite-data feeds pass EVERY invariant "
          "(empty/single/monotonic/all-equal/out-of-order/burst)");

    // The single-record + all-equal cases drove DEGENERATE domains (span 0); the
    // scale parked them mid-range -> still finite, in-pane, no domain violation.
    for (const auto& o : outcomes) {
      if (o.feedName == "single")
        check(o.clean() && o.rowCount == 1,
              "candle: single-record (degenerate domain) is clean");
      if (o.feedName == "all-equal")
        check(o.clean(),
              "candle: all-equal (degenerate domain, many rows) is clean");
      if (o.feedName == "empty")
        check(o.clean() && o.built,
              "candle: empty feed builds clean to zero geometry");
    }
  }

  // =========================================================================
  // (B1) ADVERSARIAL NaN probe IS CAUGHT by FiniteChannels (the right invariant).
  // =========================================================================
  {
    ReplayHarness h;
    h.configure("ohlc", "t",
                {{"open", "open"}, {"high", "high"}, {"low", "low"},
                 {"close", "close"}, {"volume", "volume"}});
    h.setDerivedHook(smaDerivedHook);

    // Replay ONLY the NaN feed.
    SyntheticFeed nan;
    for (const auto& f : corpus) if (f.name == "with-nan") nan = f;
    auto o = h.replayOne(kCandleSma, nan);

    check(!hasInv(o, Invariant::BuildCrashed),
          "nan-probe: build() did NOT crash on the NaN feed (graceful)");
    check(hasInv(o, Invariant::FiniteChannels),
          "nan-probe: FiniteChannels CAUGHT the NaN written to a vertex buffer");
    // It must be caught by FiniteChannels, not mis-attributed to another invariant.
    check(!hasInv(o, Invariant::Stride),
          "nan-probe: the catch is FiniteChannels, not a spurious Stride failure");
    // The violation names the offending mark (the candle whose open got the NaN).
    bool named = false;
    for (const auto& v : o.violations)
      if (v.invariant == Invariant::FiniteChannels && v.markId == "candles")
        named = true;
    check(named, "nan-probe: violation localized to the 'candles' mark");
  }

  // =========================================================================
  // (A2) KNOWN-GOOD bar/rect chart: x0<x1 ORDERING holds; whole corpus clean.
  // =========================================================================
  {
    ReplayHarness h;
    h.configure("ohlc", "t",
                {{"low", "low"}, {"high", "high"}});
    // Both scales auto-domain over `low` (first domainFrom field); the rect's
    // high edge rides the tight band within the pane pad.
    h.addDomainCheck("xp", {"low"});
    h.addDomainCheck("yp", {"low"});
    PaneBounds pb;
    pb.minX = 0.0f; pb.maxX = 1.0f; pb.minY = 0.0f; pb.maxY = 1.0f; pb.pad = 0.35f;
    h.setPaneBounds(pb);

    auto outcomes = h.replayCorpus(kBars, corpus);
    std::size_t cleanFinite = 0;
    bool noCrash = true;
    for (const auto& o : outcomes) {
      if (hasInv(o, Invariant::BuildCrashed)) noCrash = false;
      if (o.feedName != "with-nan" && o.clean()) ++cleanFinite;
      // Ordering must never trip: the generator's low<high guarantees x0<x1.
      check(!hasInv(o, Invariant::Ordering),
            ("bars: no Ordering violation on feed '" + o.feedName + "'").c_str());
    }
    check(noCrash, "bars: build() never crashed (incl burst + NaN)");
    check(cleanFinite == 6, "bars: all 6 finite-data feeds pass every invariant");
  }

  // =========================================================================
  // (B2) DELIBERATELY MIS-SET manifest (too-narrow literal y-domain) is CAUGHT by
  //      InPane on plain finite data — the "manifest mis-set" grading signal.
  // =========================================================================
  {
    ReplayHarness h;
    h.configure("ohlc", "t",
                {{"low", "low"}, {"high", "high"}, {"close", "close"}});
    // Tighten the pane to clip space [-1,1]; the mis-set domain maps data to ~20x.
    PaneBounds pb;
    pb.minX = -1.0f; pb.maxX = 1.0f; pb.minY = -1.0f; pb.maxY = 1.0f; pb.pad = 0.05f;
    h.setPaneBounds(pb);

    // Use the monotonic (healthy, finite) feed — the failure is the MANIFEST, not
    // the data: a sound manifest would pass this exact feed.
    SyntheticFeed mono;
    for (const auto& f : corpus) if (f.name == "monotonic") mono = f;
    auto o = h.replayOne(kBrokenDomain, mono);

    check(!hasInv(o, Invariant::BuildCrashed),
          "broken-domain: build() did not crash (the encode pass still ran)");
    check(!hasInv(o, Invariant::FiniteChannels),
          "broken-domain: values are FINITE (it is a domain mis-set, not a NaN)");
    check(hasInv(o, Invariant::InPane),
          "broken-domain: InPane CAUGHT positions pushed off the pane by the "
          "too-narrow literal y-domain");
  }

  std::printf("=== ENC-620b harness: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
