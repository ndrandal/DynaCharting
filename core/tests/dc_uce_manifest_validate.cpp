// ENC-620a (Epic ENC-620) — Manifest LOAD-TIME VALIDATOR test.
//
// Covers EACH of the four §6.1 checks with a VALID manifest passing + a targeted
// MALFORMED manifest failing with the RIGHT localized error (the localized,
// node-keyed signal an AI repair loop consumes):
//   #1 ref resolution — dangling scale ref / dangling transform `in`.
//   #2 column-set + dtype — wrong dtype for a scale (band on f32, log on a ≤0
//      field); a channel referencing a missing column; transform-output column
//      inference (a scale/channel binding a column produced by a `window`).
//   #3 channel↔scale↔dtype↔pipeline matrix — a channel set not covering the
//      pipeline's required vertex/instance format; an illegal mark↔pipeline pin.
//   #4 DAG acyclicity — a cycle in the transform DAG; class-3 → perFrame downgrade
//      WARNING (not a hard error).
// The §6.2 candle+SMA and §6.3 treemap manifests are the VALID happy paths.
#include "dc/manifest/ManifestValidator.hpp"

#include <cstdio>
#include <string>

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

// Does any issue's message contain `needle` (and optionally match node id)?
static bool hasIssue(const dc::ValidationReport& r, dc::ValidationCheck check,
                     dc::Severity sev, const std::string& needle,
                     const std::string& nodeId = "") {
  for (const auto& i : r.issues) {
    if (i.check != check || i.severity != sev) continue;
    if (!nodeId.empty() && i.nodeId != nodeId) continue;
    if (i.message.find(needle) != std::string::npos) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// VALID §6.2-style: candlestick + SMA(20) over raw OHLC. The SMA column is a
// `window` transform output; the y scale binds [low,high]; the x scale binds the
// timestamp through a time scale. Both marks cover their pipeline's required set.
// ---------------------------------------------------------------------------
static const char* kCandleSma = R"JSON({
  "version":"dc-manifest/1","id":"candles-sma",
  "data":{"sources":[{
    "id":"ohlc","stream":{"rowKey":"t","columns":{
      "t":{"from":"rowKey","dtype":"timestamp"},
      "open":{"dtype":"f32"},"high":{"dtype":"f32"},
      "low":{"dtype":"f32"},"close":{"dtype":"f32"},"volume":{"dtype":"f32"}}}}]},
  "transforms":[
    {"id":"withSma","in":"ohlc","op":"window",
     "window":{"field":"close","frame":[-19,0],"agg":"mean","as":"sma20"},
     "stream":{"class":"incremental","cadence":"perFrame"}}],
  "scales":[
    {"id":"xt","type":"time","domainFrom":{"data":"ohlc","field":"t"},"range":"width"},
    {"id":"yp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},
     "range":"height","nice":true}],
  "coords":{"type":"cartesian"},
  "marks":[
    {"id":"candles","type":"candle","from":"withSma","pipeline":"instancedCandle@1",
     "encoding":{"x":{"scale":"xt","field":"t"},
       "open":{"scale":"yp","field":"open"},"close":{"scale":"yp","field":"close"},
       "high":{"scale":"yp","field":"high"},"low":{"scale":"yp","field":"low"},
       "size":{"value":0.7},
       "color":{"condition":{"value":"#26a69a"},"value":"#ef5350"}}},
    {"id":"smaLine","type":"line","from":"withSma","pipeline":"lineAA@1",
     "encoding":{"x":{"scale":"xt","field":"t"},"y":{"scale":"yp","field":"sma20"},
       "color":{"value":"#ffb300"}}}]
})JSON";

// ---------------------------------------------------------------------------
// VALID §6.3-style: a bin -> aggregate transform chain feeding a rect + the dtype
// matrix over a `cat` group key. Exercises multi-stage column-set inference (the
// aggregate's `size`/`o`/`c` outputs come from inferSchema, not declared columns).
// ---------------------------------------------------------------------------
static const char* kTreemapish = R"JSON({
  "version":"dc-manifest/1","id":"bucketed",
  "data":{"sources":[{
    "id":"bars","stream":{"rowKey":"t","columns":{
      "symbol":{"from":"field:streamKey","dtype":"cat"},
      "t":{"dtype":"f32"},"open":{"dtype":"f32"},
      "close":{"dtype":"f32"},"volume":{"dtype":"f32"}}}}]},
  "transforms":[
    {"id":"byBucket","in":"bars","op":"bin","bin":{"field":"t","maxbins":5,"as":"bucket"},
     "stream":{"class":"incremental","cadence":"perFrame"}},
    {"id":"leaves","in":"byBucket","op":"aggregate",
     "aggregate":{"groupBy":["bucket"],
       "ops":[{"field":"volume","agg":"sum","as":"size"},
              {"field":"close","agg":"last","as":"c"}]},
     "stream":{"class":"windowed","cadence":"onHop"}}],
  "scales":[
    {"id":"px","type":"linear","domain":[0,1],"range":"width"},
    {"id":"py","type":"linear","domainFrom":{"data":"leaves","field":"size"},
     "range":"height"}],
  "coords":{"type":"cartesian"},
  "marks":[
    {"id":"cells","type":"rect","from":"leaves","pipeline":"instancedRect@1",
     "encoding":{"x":{"scale":"px","field":"size"},"y":{"scale":"py","field":"size"},
       "x2":{"scale":"px","field":"c"},"y2":{"scale":"py","field":"c"}}}]
})JSON";

int main() {
  dc::ManifestValidator V;

  // ===== VALID happy paths =================================================
  {
    auto r = V.validate(kCandleSma);
    check(r.valid(), "candle+SMA (§6.2) is valid");
    check(r.firstError() == nullptr, "candle+SMA has no error");
    check(!r.hasWarnings(), "candle+SMA has no warnings");
  }
  {
    auto r = V.validate(kTreemapish);
    if (!r.valid()) std::fprintf(stderr, "%s", r.toString().c_str());
    check(r.valid(), "bin->aggregate->rect (§6.3) is valid");
  }

  // ===== CHECK 1 — reference resolution ====================================
  {
    // dangling scale ref: channel y binds scale 'NOPE'.
    std::string m = kCandleSma;
    auto pos = m.find("\"scale\":\"yp\",\"field\":\"sma20\"");
    m.replace(pos, std::string("\"scale\":\"yp\"").size(), "\"scale\":\"NOPE\"");
    auto r = V.validate(m);
    check(!r.valid(), "dangling scale ref invalidates");
    check(hasIssue(r, dc::ValidationCheck::RefResolution, dc::Severity::Error,
                   "scale 'NOPE' does not resolve", "smaLine"),
          "dangling scale ref localized to mark 'smaLine'");
  }
  {
    // dangling transform `in`: window reads a source that does not exist.
    std::string m = kCandleSma;
    auto pos = m.find("\"in\":\"ohlc\"");
    m.replace(pos, std::string("\"in\":\"ohlc\"").size(), "\"in\":\"ghost\"");
    auto r = V.validate(m);
    check(!r.valid(), "dangling transform 'in' invalidates");
    check(hasIssue(r, dc::ValidationCheck::RefResolution, dc::Severity::Error,
                   "in 'ghost' does not resolve", "withSma"),
          "dangling 'in' localized to transform 'withSma'");
  }

  // ===== CHECK 2 — column-set + dtype inference ============================
  {
    // wrong dtype for a scale: a BAND scale over the f32 'close' column.
    std::string m = kCandleSma;
    auto pos = m.find("{\"id\":\"yp\",\"type\":\"linear\"");
    m.replace(pos, std::string("{\"id\":\"yp\",\"type\":\"linear\"").size(),
              "{\"id\":\"yp\",\"type\":\"band\"");
    auto r = V.validate(m);
    check(!r.valid(), "band scale over f32 invalidates");
    check(hasIssue(r, dc::ValidationCheck::ColumnSet, dc::Severity::Error,
                   "rejects field", "yp"),
          "band-on-f32 localized to scale 'yp'");
  }
  {
    // log on a possibly-≤0 field: a LOG scale over the f32 'close' column gives the
    // §6.5-style positivity hint.
    std::string m = kCandleSma;
    auto pos = m.find("{\"id\":\"yp\",\"type\":\"linear\"");
    m.replace(pos, std::string("{\"id\":\"yp\",\"type\":\"linear\"").size(),
              "{\"id\":\"yp\",\"type\":\"log\"");
    auto r = V.validate(m);
    check(!r.valid(), "log scale over a possibly-<=0 field invalidates");
    check(hasIssue(r, dc::ValidationCheck::ColumnSet, dc::Severity::Error,
                   "strictly-positive domain", "yp"),
          "log positivity hint localized to scale 'yp'");
  }
  {
    // missing column: a channel binds field 'ghostCol' not in the source.
    std::string m = kCandleSma;
    auto pos = m.find("\"y\":{\"scale\":\"yp\",\"field\":\"sma20\"}");
    m.replace(pos, std::string("\"y\":{\"scale\":\"yp\",\"field\":\"sma20\"}").size(),
              "\"y\":{\"scale\":\"yp\",\"field\":\"ghostCol\"}");
    auto r = V.validate(m);
    check(!r.valid(), "missing channel column invalidates");
    check(hasIssue(r, dc::ValidationCheck::ColumnSet, dc::Severity::Error,
                   "field 'ghostCol' is not a column", "smaLine"),
          "missing column localized to mark 'smaLine'");
  }
  {
    // transform typing failure: window over a non-existent source column. The
    // transform's OWN inferSchema rejects it; the validator surfaces it localized.
    std::string m = kCandleSma;
    auto pos = m.find("\"field\":\"close\",\"frame\"");
    m.replace(pos, std::string("\"field\":\"close\"").size(),
              "\"field\":\"nope\"");
    auto r = V.validate(m);
    check(!r.valid(), "window over a missing column invalidates");
    check(hasIssue(r, dc::ValidationCheck::ColumnSet, dc::Severity::Error,
                   "window source column 'nope' not found", "withSma"),
          "window typing failure localized to transform 'withSma'");
  }

  // ===== CHECK 3 — channel↔scale↔dtype↔pipeline matrix =====================
  {
    // channel set not covering the format: drop the candle's required 'low' channel.
    std::string m = kCandleSma;
    auto pos = m.find("\"low\":{\"scale\":\"yp\",\"field\":\"low\"},");
    m.erase(pos, std::string("\"low\":{\"scale\":\"yp\",\"field\":\"low\"},").size());
    auto r = V.validate(m);
    check(!r.valid(), "uncovered required channel invalidates");
    check(hasIssue(r, dc::ValidationCheck::ChannelMatrix, dc::Severity::Error,
                   "does not cover required channel", "candles"),
          "format-coverage failure localized to mark 'candles'");
  }
  {
    // illegal mark<->pipeline pin: a candle pinned to lineAA@1.
    std::string m = kCandleSma;
    auto pos = m.find("\"pipeline\":\"instancedCandle@1\"");
    m.replace(pos, std::string("\"pipeline\":\"instancedCandle@1\"").size(),
              "\"pipeline\":\"lineAA@1\"");
    auto r = V.validate(m);
    check(!r.valid(), "illegal mark<->pipeline pin invalidates");
    check(hasIssue(r, dc::ValidationCheck::ChannelMatrix, dc::Severity::Error,
                   "manifest requested 'lineAA@1'", "candles"),
          "mark<->pipeline mismatch localized to mark 'candles'");
  }

  // ===== CHECK 4 — DAG acyclicity + streaming-class coherence ==============
  {
    // a cycle: add a second transform B that reads A, and make A read B.
    const char* cyc = R"JSON({
      "id":"cyc",
      "data":{"sources":[{"id":"src","columns":{"v":{"dtype":"f32"}}}]},
      "transforms":[
        {"id":"a","in":"b","op":"formula","formula":{"expr":"v*2","as":"a2"}},
        {"id":"b","in":"a","op":"formula","formula":{"expr":"a2*2","as":"b2"}}],
      "scales":[{"id":"sx","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"mk","type":"point","from":"a","pipeline":"points@1",
        "encoding":{"x":{"scale":"sx","field":"v"},"y":{"scale":"sx","field":"a2"}}}]
    })JSON";
    auto r = V.validate(cyc);
    check(!r.valid(), "transform DAG cycle invalidates");
    check(hasIssue(r, dc::ValidationCheck::DagCoherence, dc::Severity::Error,
                   "has a cycle"),
          "cycle reported as a DAG coherence error");
  }
  {
    // class-3 (globalRecompute) feeding a perFrame mark: a WARNING (downgrade), not
    // an error. A `sort` transform marked globalRecompute with a throttled cadence
    // feeds a point mark — the mark cannot render perFrame off it.
    const char* downgrade = R"JSON({
      "id":"dg",
      "data":{"sources":[{"id":"src","columns":{
        "v":{"dtype":"f32"},"k":{"dtype":"f32"}}}]},
      "transforms":[
        {"id":"ranked","in":"src","op":"sort","sort":{"field":"v","as":"rank"},
         "stream":{"class":"globalRecompute","cadence":{"debounceMs":100}}}],
      "scales":[{"id":"sx","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"dots","type":"point","from":"ranked","pipeline":"points@1",
        "encoding":{"x":{"scale":"sx","field":"v"},"y":{"scale":"sx","field":"k"}}}]
    })JSON";
    auto r = V.validate(downgrade);
    check(r.valid(), "class-3 -> perFrame mark stays VALID (warning, not error)");
    check(r.hasWarnings(), "class-3 -> perFrame produces a warning");
    check(hasIssue(r, dc::ValidationCheck::DagCoherence, dc::Severity::Warning,
                   "DOWNGRADED", "dots"),
          "class-3 downgrade warning localized to mark 'dots'");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
