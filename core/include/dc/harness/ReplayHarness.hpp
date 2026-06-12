// ENC-620b (Epic ENC-620) — FEED→FRAMES REPLAY / PROPERTY HARNESS.
//
// WHAT THIS IS
// ------------
// The AI-authoring grading ORACLE (RESEARCH §6.5). A manifest is a PURE FUNCTION
// `feed → frames`: a stream of raw {t, streamKey, field, value} events drives
// PivotIngest → transforms → scales → encode → build(), producing DrawItems +
// byte-exact geometry. So a synthesized manifest can be GRADED without a human and
// without a GPU: replay it against a corpus of ADVERSARIAL synthetic feeds and
// assert structural INVARIANTS on the produced bytes. A violation is the localized
// signal an execution-guided repair loop (or a human reviewer) consumes; a clean
// pass over the whole corpus is the "this manifest is sound" oracle verdict.
//
// THE THREE PIECES (RESEARCH §6.5)
// --------------------------------
//   1. CORPUS GENERATOR — produce the adversarial feeds RESEARCH names: empty,
//      single-record, monotonic, all-equal / degenerate-domain, out-of-order,
//      with-NaN, and a large (1e5–1e6-record) burst. Each is a stream of long
//      {rowKey, field, value} records matching a manifest's declared fields.
//   2. REPLAY DRIVER — load + validate a manifest, feed one synthetic corpus case
//      through it (PivotIngest → optional derived columns → scales → encode →
//      build), and CAPTURE the produced DrawItems / geometry per "frame".
//   3. INVARIANT CHECKER — after a replay, assert (RESEARCH §6.5 invariant list):
//        * FiniteChannels   — no NaN/Inf written to a vertex/instance buffer.
//        * Ordering         — x0 ≤ x1 (rect) / low ≤ high (candle) where applicable.
//        * InPane           — packed positions within the pane/clip [-1,1] range.
//        * Stride           — geometry.format's stride == the packed byte length
//                             per record (the pipeline's required vertex format).
//        * DomainContains   — every scale's live domain contains the data it folded.
//        * NoCrash          — build() does not crash on any corpus case (degenerate
//                             domains handled) — observed by the driver completing.
//      Each violation names the (manifest, feed, mark, invariant) tuple — the
//      grading-oracle signal.
//
// HOW IT REUSES THE MERGED STACK
// ------------------------------
// The driver runs the REAL pipeline: dc::Manifest (load + build), dc::PivotIngest
// (long→wide), dc::IngestProcessor (the unchanged 13-byte feed), dc::Scale (the
// auto-domains build() re-folds), dc::EncodePass (the byte-exact packers behind
// build()). The harness owns NO new format and NO new ingest path — it only reads
// the produced EncodeResult bytes back out and reasons over them. This is exactly
// the seam ENC-606's proof used, generalized into a property oracle.
//
// SCOPE (ENC-620b only)
// ---------------------
// ONLY the replay driver + adversarial corpus + invariant checks. NO AI grammar-
// card / context-kit (ENC-620c); NO GPU / pixel comparison — every assertion is on
// the produced DrawItems + geometry BYTES, never pixels. Pure `dc` (C++17, no Dawn).
#pragma once

#include "dc/data/PivotIngest.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/manifest/Manifest.hpp"
#include "dc/manifest/ManifestValidator.hpp"
#include "dc/scale/Scale.hpp"
#include "dc/scene/Geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace dc {
namespace harness {

// ===========================================================================
// 1. SYNTHETIC FEED CORPUS
// ===========================================================================

// FeedRecord — ONE raw long-feed event {rowKey, field, value}, the only substrate
// a manifest may assume (RESEARCH §3). rowKey is the wide-row key (typically the
// timestamp t); field names the mapped column; value is the f32 datum. (Every
// corpus column here is f32 — the timestamp column receives the rowKey directly,
// see FeedSchema::timestampColumn.)
struct FeedRecord {
  std::int64_t rowKey{0};
  std::string field;
  float value{0.0f};
};

// SyntheticFeed — one adversarial corpus case: a named stream of long events plus
// the number of distinct wide rows it should pivot into (for the driver's row-count
// sanity). `expectEmpty` marks the cases that legitimately produce no rows (so the
// invariant checker treats an empty build as a PASS, not a miss).
struct SyntheticFeed {
  std::string name;                 // "empty" / "single" / "nan" / "burst" / ...
  std::vector<FeedRecord> records;  // the long events, in arrival order
  std::size_t rowCount{0};          // distinct wide rows expected
  bool expectEmpty{false};          // an intentionally row-less case (e.g. "empty")
  bool containsNonFinite{false};    // the case deliberately injects NaN/Inf data
};

// FeedSchema — the declarative description the corpus generator needs to emit feeds
// for a SPECIFIC manifest: the rowKey timestamp column + the ordered list of f32
// fields a wide row carries (e.g. open/high/low/close/volume). The generator emits
// one event per (row, field). rowKeyStart/rowKeyStep place the rows on a synthetic
// time grid (epoch-ms).
struct FeedSchema {
  std::vector<std::string> fields;       // f32 field names (open/high/low/close/…)
  std::int64_t rowKeyStart{1700000000000LL};  // first rowKey (epoch-ms)
  std::int64_t rowKeyStep{60000LL};      // step between rows (60 s default)
  std::size_t burstRows{100000};         // rows in the large-burst case (1e5 default)
};

// CorpusGenerator — produces the full adversarial corpus for a FeedSchema. Each
// generator is deterministic so a failing (manifest, feed) pair is reproducible.
class CorpusGenerator {
 public:
  explicit CorpusGenerator(FeedSchema schema) : schema_(std::move(schema)) {}

  // The whole adversarial corpus (RESEARCH §6.5 list).
  std::vector<SyntheticFeed> all() const {
    std::vector<SyntheticFeed> out;
    out.push_back(empty());
    out.push_back(single());
    out.push_back(monotonic(64));
    out.push_back(allEqual(32));
    out.push_back(outOfOrder(48));
    out.push_back(withNaN(40));
    out.push_back(burst());
    return out;
  }

  // ----- the individual adversarial cases -----------------------------------

  // EMPTY — no events at all. A valid manifest must build() to zero geometry
  // without crashing (degenerate/empty domain handled).
  SyntheticFeed empty() const {
    SyntheticFeed f;
    f.name = "empty";
    f.expectEmpty = true;
    f.rowCount = 0;
    return f;
  }

  // SINGLE-RECORD — exactly one wide row. Drives a DEGENERATE domain (span 0) for
  // every scale; the scale must "park it in the middle" without producing NaN.
  SyntheticFeed single() const {
    SyntheticFeed f;
    f.name = "single";
    emitRow(f, 0, /*base=*/100.0f);
    f.rowCount = 1;
    return f;
  }

  // MONOTONIC — N rows whose values strictly increase. The healthy baseline a
  // line/candle was designed for (auto-domain has a real positive span).
  SyntheticFeed monotonic(std::size_t n) const {
    SyntheticFeed f;
    f.name = "monotonic";
    for (std::size_t i = 0; i < n; ++i)
      emitRow(f, i, /*base=*/100.0f + static_cast<float>(i));
    f.rowCount = n;
    return f;
  }

  // ALL-EQUAL — N rows that all carry the SAME value: a DEGENERATE domain over many
  // rows (span 0). Exercises the divide-by-zero guard in every scale at scale.
  SyntheticFeed allEqual(std::size_t n) const {
    SyntheticFeed f;
    f.name = "all-equal";
    for (std::size_t i = 0; i < n; ++i) emitRow(f, i, /*base=*/42.0f);
    f.rowCount = n;
    return f;
  }

  // OUT-OF-ORDER — rows whose rowKeys arrive shuffled (a real out-of-order stream).
  // PivotIngest's auto-flush is time-ordered, so this stresses the pivot's row
  // accumulation AND the final flushAll(). Domain/geometry must stay correct.
  SyntheticFeed outOfOrder(std::size_t n) const {
    SyntheticFeed f;
    f.name = "out-of-order";
    // A fixed deterministic shuffle: odd indices first, then even.
    for (std::size_t i = 1; i < n; i += 2)
      emitRow(f, i, /*base=*/100.0f + std::sin(0.3f * static_cast<float>(i)));
    for (std::size_t i = 0; i < n; i += 2)
      emitRow(f, i, /*base=*/100.0f + std::sin(0.3f * static_cast<float>(i)));
    f.rowCount = n;
    return f;
  }

  // WITH-NaN — a feed that injects a NaN into ONE field of ONE row. This is the
  // adversarial "drive a value to NaN" case: a sound encode pipeline must EITHER
  // keep the NaN out of the buffer or the FiniteChannels invariant CATCHES it —
  // either way the oracle reports the truth. Marked containsNonFinite so the test
  // can assert the catch.
  SyntheticFeed withNaN(std::size_t n) const {
    SyntheticFeed f;
    f.name = "with-nan";
    const std::size_t bad = n / 2;
    for (std::size_t i = 0; i < n; ++i) {
      const float base = 100.0f + static_cast<float>(i);
      emitRow(f, i, base);
      if (i == bad && !schema_.fields.empty()) {
        // Overwrite the FIRST field of the bad row with a NaN.
        f.records.push_back(
            FeedRecord{rowKeyAt(i), schema_.fields.front(),
                       std::numeric_limits<float>::quiet_NaN()});
      }
    }
    f.rowCount = n;
    f.containsNonFinite = true;
    return f;
  }

  // BURST — a large (schema_.burstRows, default 1e5) feed. Proves build() does not
  // crash / blow up on a realistic high-cardinality stream and that O(Δ) auto-domain
  // + the byte packers scale. Values are bounded so a real domain forms.
  SyntheticFeed burst() const {
    SyntheticFeed f;
    f.name = "burst";
    f.records.reserve(schema_.burstRows * (schema_.fields.size() + 0));
    for (std::size_t i = 0; i < schema_.burstRows; ++i) {
      const float base =
          100.0f + 10.0f * std::sin(0.001f * static_cast<float>(i));
      emitRow(f, i, base);
    }
    f.rowCount = schema_.burstRows;
    return f;
  }

  const FeedSchema& schema() const { return schema_; }

 private:
  std::int64_t rowKeyAt(std::size_t i) const {
    return schema_.rowKeyStart +
           static_cast<std::int64_t>(i) * schema_.rowKeyStep;
  }

  // Emit one wide row: every declared field gets an event. OHLC values sit in a
  // TIGHT band around `base` (low ≤ open/close ≤ high) so that a y-scale which
  // auto-domains over a SINGLE channel (the merged stack folds the first domainFrom
  // field) still BRACKETS the whole candle to within a small, bounded overshoot —
  // the band is deliberately narrow vs. the inter-row variation. A plain line just
  // reads field[0].
  void emitRow(SyntheticFeed& f, std::size_t i, float base) const {
    const std::int64_t rk = rowKeyAt(i);
    for (std::size_t fi = 0; fi < schema_.fields.size(); ++fi) {
      const std::string& name = schema_.fields[fi];
      float v = base;
      if (name == "open") v = base;
      else if (name == "close") v = base + ((i % 3 == 0) ? 0.15f : -0.12f);
      else if (name == "high") v = base + 0.3f;
      else if (name == "low") v = base - 0.3f;
      else if (name == "volume") v = 1000.0f + static_cast<float>(i);
      else v = base + 0.1f * static_cast<float>(fi);
      f.records.push_back(FeedRecord{rk, name, v});
    }
  }

  FeedSchema schema_;
};

// ===========================================================================
// 2. INVARIANT MODEL — what an assertion failure looks like
// ===========================================================================

// Invariant — which structural property a violation came from. Mirrors the
// RESEARCH §6.5 invariant list; lets the oracle bucket failures by category.
enum class Invariant : std::uint8_t {
  FiniteChannels,  // no NaN/Inf written to a vertex/instance buffer
  Ordering,        // x0 ≤ x1 / low ≤ high where the format defines an order
  InPane,          // packed positions within the pane/clip [-1,1] range
  Stride,          // geometry.format's stride == packed bytes per record
  DomainContains,  // a scale's live domain contains the data it folded
  BuildCrashed,    // build() failed / threw on a corpus case
};

inline const char* toString(Invariant inv) {
  switch (inv) {
    case Invariant::FiniteChannels: return "FiniteChannels";
    case Invariant::Ordering: return "Ordering";
    case Invariant::InPane: return "InPane";
    case Invariant::Stride: return "Stride";
    case Invariant::DomainContains: return "DomainContains";
    case Invariant::BuildCrashed: return "BuildCrashed";
  }
  return "unknown";
}

// Violation — ONE failed invariant, fully localized to the (manifest, feed, mark)
// tuple plus a human-readable reason. This IS the grading-oracle signal.
struct Violation {
  std::string manifestId;
  std::string feedName;
  std::string markId;     // the offending mark ("" for a build-level failure)
  Invariant invariant{Invariant::FiniteChannels};
  std::string message;    // localized reason (which value / record / channel)
};

// ReplayOutcome — the result of replaying ONE (manifest, feed) pair: whether the
// build succeeded, how many marks compiled, and every invariant violation found.
struct ReplayOutcome {
  std::string manifestId;
  std::string feedName;
  bool built{false};         // build() returned ok (or legitimately empty)
  std::size_t markCount{0};  // compiled marks inspected
  std::size_t rowCount{0};   // table rows the feed pivoted into
  std::vector<Violation> violations;

  bool clean() const { return violations.empty(); }
};

// ===========================================================================
// 3. REPLAY DRIVER + INVARIANT CHECKER
// ===========================================================================

// PaneBounds — the clip-space extent positions must fall inside (RESEARCH §6.5
// "positions in-pane"). The encode pass packs into clip space; a full-viewport pane
// is [-1,1] in both axes. `pad` admits floating-point slop / half-extents that ride
// just past the edge (a candle half-width, a point radius) without false positives.
struct PaneBounds {
  float minX{-1.0f}, maxX{1.0f};
  float minY{-1.0f}, maxY{1.0f};
  float pad{0.05f};
};

// ReplayHarness — loads a manifest ONCE, then replays each synthetic feed through
// the real PivotIngest → build pipeline and checks the invariants on the produced
// bytes. Reusable across a corpus and across many candidate manifests.
class ReplayHarness {
 public:
  // A hook to populate DERIVED columns after the pivot lands the raw rows but
  // before build() (e.g. the ENC-606 trivial SMA). Receives the manifest + the live
  // byte source + ingest so it can read a pivoted column and patch a derived one via
  // the unchanged feed (op=2). Default: no derived columns.
  using DerivedHook = std::function<void(Manifest&, IngestProcessor&,
                                         const BufferByteSource&)>;

  ReplayHarness() = default;

  // Configure the manifest source id + its pivot field→column map (matching the
  // manifest's data.sources[].stream.columns). `tsColumn` is the rowKey timestamp
  // column. `fieldColumns` maps each long-feed field to its destination column
  // (usually identity — field "open" → column "open").
  void configure(std::string sourceId, std::string tsColumn,
                 std::vector<std::pair<std::string, std::string>> fieldColumns) {
    sourceId_ = std::move(sourceId);
    tsColumn_ = std::move(tsColumn);
    fieldColumns_ = std::move(fieldColumns);
  }

  void setDerivedHook(DerivedHook hook) { derived_ = std::move(hook); }
  void setPaneBounds(PaneBounds pb) { pane_ = pb; }

  // Register a DomainContains check: scale `scaleId`'s live domain must bracket the
  // finite values of every listed column (after the feed folds). E.g. the y-scale
  // "yp" must contain {"low","high"}; the time x-scale "xt" must contain {"t"}.
  // (The time column is read as a timestamp; everything else as f32.)
  void addDomainCheck(std::string scaleId, std::vector<std::string> columns,
                      bool timestampColumn = false) {
    domainChecks_.push_back(
        DomainCheck{std::move(scaleId), std::move(columns), timestampColumn});
  }

  // Replay every feed in `corpus` against `manifestJson`, returning one outcome per
  // feed. A load/validate failure short-circuits with a single BuildCrashed-style
  // outcome carrying the reason (so a malformed manifest is itself a clear signal).
  std::vector<ReplayOutcome> replayCorpus(
      const std::string& manifestJson,
      const std::vector<SyntheticFeed>& corpus) const {
    std::vector<ReplayOutcome> out;
    out.reserve(corpus.size());
    for (const auto& feed : corpus)
      out.push_back(replayOne(manifestJson, feed));
    return out;
  }

  // Replay ONE feed. Loads + validates the manifest fresh (a manifest is stateless
  // feed→frames, so each replay is independent), pivots the feed, runs the derived
  // hook, builds, and checks the invariants on every compiled mark.
  ReplayOutcome replayOne(const std::string& manifestJson,
                          const SyntheticFeed& feed) const {
    ReplayOutcome r;
    r.feedName = feed.name;

    // ---- load + validate (a bad manifest is its own grading signal) ----
    Manifest m;
    auto lr = m.load(manifestJson);
    r.manifestId = m.id();
    if (!lr.ok()) {
      r.violations.push_back(Violation{
          r.manifestId, feed.name, "", Invariant::BuildCrashed,
          std::string("manifest load failed: ") + lr.message});
      return r;
    }
    {
      ManifestValidator validator;
      auto report = validator.validate(manifestJson);
      if (!report.valid()) {
        const auto* e = report.firstError();
        r.violations.push_back(Violation{
            r.manifestId, feed.name, e ? e->nodeId : "",
            Invariant::BuildCrashed,
            std::string("manifest invalid: ") + (e ? e->message : "")});
        return r;
      }
    }

    auto tid = m.tableId(sourceId_);
    if (!tid.has_value()) {
      r.violations.push_back(Violation{
          r.manifestId, feed.name, "", Invariant::BuildCrashed,
          "source '" + sourceId_ + "' did not resolve to a table"});
      return r;
    }

    // ---- pivot the synthetic long feed into the manifest's table ----
    IngestProcessor ingest;
    auto src = makeBufferByteSource(ingest);
    PivotIngest pivot(m.tables(), ingest);
    pivot.setTable(*tid);
    pivot.setRowKeyColumn(tsColumn_);
    for (const auto& fc : fieldColumns_) pivot.mapField(fc.first, fc.second);

    for (const auto& rec : feed.records)
      pivot.pushEvent(rec.rowKey, rec.field, pvF32(rec.value));
    pivot.flushAll();
    r.rowCount = m.tables().rowCount(*tid, src);

    // ---- derived columns (e.g. SMA) through the unchanged feed ----
    if (derived_) derived_(m, ingest, src);

    // ---- build(): re-fold auto-domains + run the encode pass per mark ----
    auto br = m.build(src);
    r.built = br.ok();
    if (!br.ok()) {
      // An empty feed legitimately produces no geometry; some marks accept that
      // (ok with empty bytes). A genuine build error is a violation.
      r.violations.push_back(Violation{
          r.manifestId, feed.name, "", Invariant::BuildCrashed,
          std::string("build() rejected: ") + br.message});
      return r;
    }

    r.markCount = m.compiledMarks().size();

    // ---- invariant checks on every compiled mark's bytes ----
    for (const auto& cm : m.compiledMarks())
      checkMark(m, cm, feed, r);

    // ---- scale domain-contains-values over the live columns ----
    checkScaleDomains(m, feed, src, r);

    return r;
  }

 private:
  // --- per-mark byte invariants (Finite / Stride / Ordering / InPane) -------
  void checkMark(const Manifest& m, const CompiledMark& cm,
                 const SyntheticFeed& feed, ReplayOutcome& r) const {
    const auto& geo = cm.result.geometry;
    const auto& bytes = cm.result.bytes;
    const std::uint32_t stride = strideOf(geo.format);

    // STRIDE — the produced byte count must be a whole multiple of the format's
    // stride (the pipeline's required vertex format). A non-multiple means the
    // packer wrote a partial record — the exact-stride contract the renderer trusts.
    if (stride == 0 || (bytes.size() % stride) != 0) {
      r.violations.push_back(Violation{
          m.id(), feed.name, cm.id, Invariant::Stride,
          "byte length " + std::to_string(bytes.size()) +
              " is not a multiple of format stride " + std::to_string(stride) +
              " (" + toString(geo.format) + ")"});
      return;  // can't safely walk records if the stride is broken
    }
    if (bytes.empty()) return;  // nothing to walk (an empty/row-less frame)

    const std::size_t records = bytes.size() / stride;

    // Decode the per-record float lanes that are POSITIONS (clip space) vs the
    // lanes that are sizes/half-extents (NOT bounded by the pane). We check finiteness
    // on EVERY float lane, but InPane only on position lanes.
    for (std::size_t rec = 0; rec < records; ++rec) {
      const std::size_t base = rec * stride;
      checkRecord(m, cm, feed, r, bytes, base);
    }
  }

  // Inspect one packed record per the geometry format.
  void checkRecord(const Manifest& m, const CompiledMark& cm,
                   const SyntheticFeed& feed, ReplayOutcome& r,
                   const std::vector<std::uint8_t>& bytes,
                   std::size_t base) const {
    switch (cm.result.geometry.format) {
      case VertexFormat::Pos2_Clip: {  // points@1 / line2d@1: (x, y)
        const float x = f32(bytes, base + 0);
        const float y = f32(bytes, base + 4);
        finite(m, cm, feed, r, x, "x");
        finite(m, cm, feed, r, y, "y");
        inPaneX(m, cm, feed, r, x, "x");
        inPaneY(m, cm, feed, r, y, "y");
        break;
      }
      case VertexFormat::Rect4: {  // instancedRect@1: (x0, y0, x1, y1)
        const float x0 = f32(bytes, base + 0);
        const float y0 = f32(bytes, base + 4);
        const float x1 = f32(bytes, base + 8);
        const float y1 = f32(bytes, base + 12);
        finite(m, cm, feed, r, x0, "x0");
        finite(m, cm, feed, r, y0, "y0");
        finite(m, cm, feed, r, x1, "x1");
        finite(m, cm, feed, r, y1, "y1");
        inPaneX(m, cm, feed, r, x0, "x0");
        inPaneX(m, cm, feed, r, x1, "x1");
        inPaneY(m, cm, feed, r, y0, "y0");
        inPaneY(m, cm, feed, r, y1, "y1");
        // ORDERING — a rect's left edge must not exceed its right edge.
        if (std::isfinite(x0) && std::isfinite(x1) && x0 > x1)
          r.violations.push_back(Violation{
              m.id(), feed.name, cm.id, Invariant::Ordering,
              "rect x0 (" + std::to_string(x0) + ") > x1 (" +
                  std::to_string(x1) + ")"});
        break;
      }
      case VertexFormat::Candle6: {  // (x, open, high, low, close, halfWidth)
        const float x = f32(bytes, base + 0);
        const float open = f32(bytes, base + 4);
        const float high = f32(bytes, base + 8);
        const float low = f32(bytes, base + 12);
        const float close = f32(bytes, base + 16);
        const float hw = f32(bytes, base + 20);
        finite(m, cm, feed, r, x, "x");
        finite(m, cm, feed, r, open, "open");
        finite(m, cm, feed, r, high, "high");
        finite(m, cm, feed, r, low, "low");
        finite(m, cm, feed, r, close, "close");
        finite(m, cm, feed, r, hw, "halfWidth");
        inPaneX(m, cm, feed, r, x, "x");
        inPaneY(m, cm, feed, r, open, "open");
        inPaneY(m, cm, feed, r, high, "high");
        inPaneY(m, cm, feed, r, low, "low");
        inPaneY(m, cm, feed, r, close, "close");
        // ORDERING — high ≥ low (the candle wick must not be inverted). Note clip-y
        // may be flipped (range r0>r1), so compare in whichever direction maps
        // "high" above "low": we assert the raw data invariant high≥low is preserved
        // by the (monotone) scale, i.e. map(high) and map(low) keep a consistent
        // order with open/close between them.
        if (std::isfinite(high) && std::isfinite(low)) {
          const float hi = std::max(high, low);
          const float lo = std::min(high, low);
          // open & close must lie within [lo, hi] (the body sits inside the wick).
          if (std::isfinite(open) && (open < lo - 1e-3f || open > hi + 1e-3f))
            r.violations.push_back(Violation{
                m.id(), feed.name, cm.id, Invariant::Ordering,
                "candle open outside [low,high] wick"});
          if (std::isfinite(close) && (close < lo - 1e-3f || close > hi + 1e-3f))
            r.violations.push_back(Violation{
                m.id(), feed.name, cm.id, Invariant::Ordering,
                "candle close outside [low,high] wick"});
        }
        break;
      }
      case VertexFormat::Rect4Color: {  // (x0,y0,x1,y1, rgba8, scalar)
        const float x0 = f32(bytes, base + 0);
        const float y0 = f32(bytes, base + 4);
        const float x1 = f32(bytes, base + 8);
        const float y1 = f32(bytes, base + 12);
        finite(m, cm, feed, r, x0, "x0");
        finite(m, cm, feed, r, y0, "y0");
        finite(m, cm, feed, r, x1, "x1");
        finite(m, cm, feed, r, y1, "y1");
        inPaneX(m, cm, feed, r, x0, "x0");
        inPaneX(m, cm, feed, r, x1, "x1");
        inPaneY(m, cm, feed, r, y0, "y0");
        inPaneY(m, cm, feed, r, y1, "y1");
        if (std::isfinite(x0) && std::isfinite(x1) && x0 > x1)
          r.violations.push_back(Violation{
              m.id(), feed.name, cm.id, Invariant::Ordering,
              "rectColor x0 > x1"});
        break;
      }
      case VertexFormat::Point4Color: {  // (x, y, rgba8, size)
        const float x = f32(bytes, base + 0);
        const float y = f32(bytes, base + 4);
        finite(m, cm, feed, r, x, "x");
        finite(m, cm, feed, r, y, "y");
        inPaneX(m, cm, feed, r, x, "x");
        inPaneY(m, cm, feed, r, y, "y");
        break;
      }
      default:
        // Other formats (text/gradient/uv): only the position lanes (x,y) at offset
        // 0 are pane-bounded; we conservatively finite-check the first vec2.
        finite(m, cm, feed, r, f32(bytes, base + 0), "x");
        finite(m, cm, feed, r, f32(bytes, base + 4), "y");
        break;
    }
  }

  // --- scale DomainContains check -------------------------------------------
  // Each registered scale's live [min,max] must CONTAIN the finite values of the
  // columns it auto-domains over (RESEARCH §6.5 "domain-contains-values"). We re-read
  // the raw columns and assert domain.min ≤ min(finite col) and max(finite col) ≤
  // domain.max. NaNs are skipped — RunningDomain never folds them, so a NaN in a
  // column legitimately does NOT widen the domain (and the FiniteChannels invariant
  // catches it downstream if it reaches a buffer). A degenerate (single / all-equal)
  // domain has min==max and still contains its (single) value, so this holds there
  // too. An empty feed yields an empty domain and no column values — vacuously true.
  void checkScaleDomains(const Manifest& m, const SyntheticFeed& feed,
                         const BufferByteSource& src, ReplayOutcome& r) const {
    auto tid = m.tableId(sourceId_);
    if (!tid.has_value()) return;

    for (const auto& dc : domainChecks_) {
      const Scale* s = m.scale(dc.scaleId);
      if (s == nullptr) continue;
      const Domain& dom = s->domain();

      double lo = 0.0, hi = 0.0;
      bool any = false;
      for (const auto& col : dc.columns) {
        if (dc.timestampColumn) {
          auto v = m.tables().viewTimestamp(*tid, col, src);
          for (std::size_t i = 0; i < v.count; ++i)
            foldFinite(static_cast<double>(v[i]), lo, hi, any);
        } else {
          auto v = m.tables().viewF32(*tid, col, src);
          for (std::size_t i = 0; i < v.count; ++i)
            foldFinite(static_cast<double>(v[i]), lo, hi, any);
        }
      }
      if (!any) continue;  // no finite data folded — nothing to contain

      if (dom.empty) {
        r.violations.push_back(Violation{
            m.id(), feed.name, "", Invariant::DomainContains,
            "scale '" + dc.scaleId +
                "' domain is EMPTY but its columns carry finite data"});
        continue;
      }
      const double eps = 1e-3 * (1.0 + std::fabs(dom.max) + std::fabs(dom.min));
      if (lo < dom.min - eps || hi > dom.max + eps)
        r.violations.push_back(Violation{
            m.id(), feed.name, "", Invariant::DomainContains,
            "scale '" + dc.scaleId + "' domain [" + std::to_string(dom.min) +
                "," + std::to_string(dom.max) + "] does not contain data [" +
                std::to_string(lo) + "," + std::to_string(hi) + "]"});
    }
  }

  static void foldFinite(double v, double& lo, double& hi, bool& any) {
    if (!std::isfinite(v)) return;
    if (!any) { lo = hi = v; any = true; }
    else { lo = std::min(lo, v); hi = std::max(hi, v); }
  }

  // --- primitives -----------------------------------------------------------
  static float f32(const std::vector<std::uint8_t>& b, std::size_t off) {
    float v = 0.0f;
    std::memcpy(&v, b.data() + off, sizeof(float));
    return v;
  }

  void finite(const Manifest& m, const CompiledMark& cm, const SyntheticFeed& feed,
              ReplayOutcome& r, float v, const char* lane) const {
    if (!std::isfinite(v))
      r.violations.push_back(Violation{
          m.id(), feed.name, cm.id, Invariant::FiniteChannels,
          std::string("non-finite ") + lane + " written to buffer (" +
              std::to_string(v) + ")"});
  }

  void inPaneX(const Manifest& m, const CompiledMark& cm, const SyntheticFeed& feed,
               ReplayOutcome& r, float v, const char* lane) const {
    if (!std::isfinite(v)) return;  // finiteness reported separately
    if (v < pane_.minX - pane_.pad || v > pane_.maxX + pane_.pad)
      r.violations.push_back(Violation{
          m.id(), feed.name, cm.id, Invariant::InPane,
          std::string(lane) + " (" + std::to_string(v) + ") outside pane x [" +
              std::to_string(pane_.minX) + "," + std::to_string(pane_.maxX) + "]"});
  }

  void inPaneY(const Manifest& m, const CompiledMark& cm, const SyntheticFeed& feed,
               ReplayOutcome& r, float v, const char* lane) const {
    if (!std::isfinite(v)) return;
    if (v < pane_.minY - pane_.pad || v > pane_.maxY + pane_.pad)
      r.violations.push_back(Violation{
          m.id(), feed.name, cm.id, Invariant::InPane,
          std::string(lane) + " (" + std::to_string(v) + ") outside pane y [" +
              std::to_string(pane_.minY) + "," + std::to_string(pane_.maxY) + "]"});
  }

  struct DomainCheck {
    std::string scaleId;
    std::vector<std::string> columns;
    bool timestampColumn{false};
  };

  std::string sourceId_{"ohlc"};
  std::string tsColumn_{"t"};
  std::vector<std::pair<std::string, std::string>> fieldColumns_;
  std::vector<DomainCheck> domainChecks_;
  DerivedHook derived_;
  PaneBounds pane_;
};

}  // namespace harness
}  // namespace dc
