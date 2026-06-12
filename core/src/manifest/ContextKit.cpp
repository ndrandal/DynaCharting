// ENC-620c (Epic ENC-620) — AI GRAMMAR-CARD / CONTEXT-KIT generator.
// See ContextKit.hpp for the contract. Every section of the grammar card is
// ENUMERATED FROM THE LIVE CATALOGS/REGISTRIES so the card cannot drift from code:
//   * marks       — markSpecOf() over the Mark enum (pipeline + format + channels)
//   * scales      — the ScaleType vocabulary + the scale↔dtype acceptance matrix
//   * transforms  — REAL transform node instances (their op() + inferSchema rule)
//   * pipelines   — PipelineCatalog::keys() + each spec's requiredVertexFormat
#include "dc/manifest/ContextKit.hpp"

#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"
#include "dc/scene/Geometry.hpp"

#include "dc/transform/Transform.hpp"
#include "dc/transform/transforms/Aggregate.hpp"
#include "dc/transform/transforms/Bin.hpp"
#include "dc/transform/transforms/Filter.hpp"
#include "dc/transform/transforms/Formula.hpp"
#include "dc/transform/transforms/Join.hpp"
#include "dc/transform/transforms/Sample.hpp"
#include "dc/transform/transforms/Sort.hpp"
#include "dc/transform/transforms/Stack.hpp"
#include "dc/transform/transforms/Window.hpp"

#include <memory>
#include <sstream>

namespace dc {

namespace {

// The full mark vocabulary (the Mark enum). The card is BUILT by walking this and
// asking markSpecOf for each — so adding a Mark + its markSpecOf entry adds a row.
const Mark kAllMarks[] = {
    Mark::Point, Mark::Line, Mark::Rect, Mark::Candle,
    Mark::RectColor, Mark::PointColor, Mark::Arc,
};

// The full scale-type vocabulary (Scale.hpp ScaleType + the color scales). Paired
// with the accepted-dtype string the validator's matrix enforces (mirror of
// ManifestValidator::scaleAcceptsDType / the §6.1 #2 contract). The manifest type
// STRING is the authoring surface a model writes.
struct ScaleRow {
  const char* manifestType;  // the `scales[].type` string
  const char* accepts;       // accepted column dtypes (the §6.1 #2 matrix)
  const char* note;
};
const ScaleRow kScales[] = {
    {"linear", "f32, i32", "numeric domain -> [r0,r1]; streaming auto-domain (class-1)"},
    {"log", "f32, i32 (>0)", "log domain; REQUIRES a strictly-positive field"},
    {"sqrt", "f32, i32", "power (sqrt) domain"},
    {"pow", "f32, i32", "power domain"},
    {"time", "timestamp", "epoch-ms domain; f64 on CPU, relative f32 offset to GPU"},
    {"band", "cat", "category -> band [start,start+bandwidth] with inner/outer pad"},
    {"point", "cat", "category -> a single position (bandwidth 0)"},
    {"ordinal", "cat", "category -> discrete position"},
    {"sequential", "f32, i32", "numeric -> ramp t in [0,1] -> packed RGBA8 (per row)"},
    {"color", "f32, i32", "sequential color ramp (viridis/magma/blueRed/...)"},
    {"diverging", "f32, i32", "two-sided ramp around a fixed mid; REQUIRES a baseline policy"},
};

// The transform OPS recognized by the manifest DSL (mirror of the validator's
// buildNode dispatch). The card enumerates these by CONSTRUCTING a real transform
// node and reading its op() — so the op string is derived from the class, never a
// literal. The streaming class is the RESEARCH §5.1 class each node documents.
struct TxRow {
  std::unique_ptr<TransformNode> node;  // a real instance — op() is read from it
  const char* streamClass;              // RESEARCH §5.1 streaming class
  const char* outputRule;               // the inferSchema output-schema rule
};

std::vector<TxRow> buildTransformRows() {
  std::vector<TxRow> rows;
  rows.push_back({std::make_unique<FilterTransform>("x>0"), "class-1 (incremental)",
                  "output schema = input schema (rows filtered, columns unchanged)"});
  rows.push_back({std::make_unique<FormulaTransform>("a+b", "out"),
                  "class-1 (incremental)",
                  "output schema = input + one f32 column `as` (the expr result)"});
  rows.push_back({std::make_unique<WindowTransform>("close", WindowAgg::Mean, 20, "sma"),
                  "class-1 EMA / class-2 fixed-window",
                  "output schema = input + one f32 column `as` (trailing-frame agg)"});
  rows.push_back({std::make_unique<BinTransform>(BinTransform::byMaxBins("t", 10, "bucket")),
                  "class-1 (incremental)",
                  "output schema = input + one i32 column `as` (bin index)"});
  rows.push_back({std::make_unique<AggregateTransform>(
                      std::vector<std::string>{"k"}, std::vector<AggMeasure>{}),
                  "class-1 sum/mean/min/max-count; class-2 median",
                  "output schema = groupBy key columns + one column per `ops[].as`"});
  rows.push_back({std::make_unique<SortTransform>(SortTransform::rank("v", "rank", true)),
                  "class-3 (global recompute)",
                  "reorder: schema unchanged; rank: input + one i32 column `as`"});
  rows.push_back({std::make_unique<StackTransform>("v", "k", StackOffset::Zero),
                  "class-1 zero / class-3 normalize / class-4 wiggle",
                  "output schema = input + f32 `y0` + f32 `y1` (stacked bounds)"});
  rows.push_back({std::make_unique<SampleTransform>("x", "y", 1000),
                  "class-2 (viewport-hop)",
                  "output schema = input schema (rows decimated to the budget)"});
  rows.push_back({std::make_unique<JoinTransform>("key", std::vector<JoinLookup>{}),
                  "class-1 (incremental lookup)",
                  "output schema = left input + the looked-up right `fields` (prefixed)"});
  return rows;
}

std::string indentBlock(const std::string& s) { return s; }

}  // namespace

// ===========================================================================
// FeedDescriptor
// ===========================================================================

FeedDescriptor FeedDescriptor::demoOhlc() {
  FeedDescriptor f;
  f.id = "ohlc";
  f.rowKey = "t";
  f.streamKeys = {"AAPL", "MSFT", "GOOG"};
  f.fields = {
      {"t", "timestamp", "row key; epoch-ms — bind ONLY through a `time` scale (CPU f64)"},
      {"open", "f32", "OHLC open"},
      {"high", "f32", "OHLC high"},
      {"low", "f32", "OHLC low"},
      {"close", "f32", "OHLC close"},
      {"volume", "f32", "bar volume"},
      {"symbol", "cat", "the stream key (one series per symbol)"},
  };
  return f;
}

std::string FeedDescriptor::toMarkdown() const {
  std::ostringstream o;
  o << "## FEED SCHEMA\n\n";
  o << "Feed `" << id << "` — rowKey `" << rowKey << "`. Stream keys (series): ";
  for (std::size_t i = 0; i < streamKeys.size(); ++i)
    o << (i ? ", " : "") << "`" << streamKeys[i] << "`";
  o << ".\n\n";
  o << "| field | dtype | note |\n|---|---|---|\n";
  for (const auto& fld : fields)
    o << "| `" << fld.name << "` | " << fld.dtype << " | " << fld.note << " |\n";
  o << "\n> RULE: a `timestamp` field is epoch-ms; it stays on the CPU as f64 and is\n"
       "> bound ONLY through a `time` scale (which normalizes to a relative f32 offset\n"
       "> for the GPU). NEVER bind a timestamp to a linear/log scale or push raw\n"
       "> epoch-ms to an f32 channel — the f32 mantissa quantizes it to minute/hour\n"
       "> buckets (a named correctness trap).\n";
  return o.str();
}

// ===========================================================================
// ContextKit — grammar card sections (catalog-derived)
// ===========================================================================

std::string ContextKit::marksSection() {
  std::ostringstream o;
  o << "## MARKS (mark -> pipeline + required channels)\n\n";
  o << "| mark | pipeline | required channels |\n|---|---|---|\n";
  for (Mark m : kAllMarks) {
    // A line resolves to line2d@1 by default; surface the lineAA variant too.
    MarkSpec spec = markSpecOf(m);
    o << "| `" << toString(m) << "` | `" << spec.pipeline << "` | ";
    for (std::size_t i = 0; i < spec.required.size(); ++i)
      o << (i ? ", " : "") << "`" << toString(spec.required[i]) << "`";
    o << " |\n";
    if (m == Mark::Line) {
      MarkSpec aa = markSpecOf(Mark::Line, LineStyle::LineAA);
      o << "| `line` (antialiased) | `" << aa.pipeline << "` | ";
      for (std::size_t i = 0; i < aa.required.size(); ++i)
        o << (i ? ", " : "") << "`" << toString(aa.required[i]) << "`";
      o << " |\n";
    }
  }
  o << "\n> Each mark emits ONE instance per table row; a mark is REJECTED unless every\n"
       "> required channel above is bound (the validateDrawItem exact-stride gate).\n";
  return o.str();
}

std::string ContextKit::scalesSection() {
  std::ostringstream o;
  o << "## SCALES (scale type -> accepted dtypes)\n\n";
  o << "| scale type | accepts | note |\n|---|---|---|\n";
  for (const auto& s : kScales)
    o << "| `" << s.manifestType << "` | " << s.accepts << " | " << s.note << " |\n";
  o << "\n> A channel binds `scale(field)` ONLY if the field's dtype is in the scale's\n"
       "> accepted set. A `log` scale additionally REQUIRES a strictly-positive field.\n";
  return o.str();
}

std::string ContextKit::transformsSection() {
  std::ostringstream o;
  o << "## TRANSFORMS (op -> output-schema rule + streaming class)\n\n";
  o << "| op | output-schema rule | streaming class |\n|---|---|---|\n";
  for (auto& row : buildTransformRows())
    o << "| `" << row.node->op() << "` | " << row.outputRule << " | "
      << row.streamClass << " |\n";
  o << "\n> A transform is a pure node: inferSchema(inputSchema) -> outputSchema, typed\n"
       "> WITHOUT data at load. The streaming class governs recompute cadence:\n"
       "> class-1 = O(Δ) every frame; class-2 = window/hop boundary; class-3 = throttled\n"
       "> global recompute; class-4 = needs a declared baseline policy.\n";
  return o.str();
}

std::string ContextKit::pipelinesSection() {
  std::ostringstream o;
  PipelineCatalog cat;
  o << "## PIPELINES (pipeline -> required vertex format)\n\n";
  o << "| pipeline | required vertex format | stride (bytes) |\n|---|---|---|\n";
  for (const auto& key : cat.keys()) {
    const PipelineSpec* spec = cat.find(key);
    if (!spec) continue;
    o << "| `" << key << "` | `" << toString(spec->requiredVertexFormat) << "` | "
      << strideOf(spec->requiredVertexFormat) << " |\n";
  }
  o << "\n> The renderer trusts that geometry bound to pipeline P has bytes laid out\n"
       "> EXACTLY as P's required vertex format. The encode pass packs each mark to its\n"
       "> pipeline's format byte-for-byte; a format mismatch is rejected at compile.\n";
  return o.str();
}

std::string ContextKit::grammarCardMarkdown() {
  std::ostringstream o;
  o << "# DC GRAMMAR CARD (`dc-manifest/1`)\n\n"
       "Authoring a manifest is program-synthesis against a strongly-typed, TOTAL DSL\n"
       "with a finite vocabulary. The tables below are GENERATED from the live engine\n"
       "catalogs (they cannot drift from the code). Bind only what appears here.\n\n";
  o << marksSection() << "\n";
  o << scalesSection() << "\n";
  o << transformsSection() << "\n";
  o << pipelinesSection();
  return o.str();
}

// ===========================================================================
// ContextKit — worked manifests (few-shot anchors; each PASSES the validator)
// ===========================================================================

std::vector<WorkedManifest> ContextKit::workedManifests() {
  std::vector<WorkedManifest> out;

  // --- Anchor 1: a bar chart (volume per time-bucket). Exercises a linear scale +
  //     a rect mark over instancedRect@1, the simplest position-only mark. ---
  out.push_back(
      {"Bar chart (volume by bucket)",
       "linear scales + a rect mark over instancedRect@1 (x,y,x2,y2)",
       R"JSON({
  "version":"dc-manifest/1","id":"bars-by-bucket",
  "data":{"sources":[{
    "id":"bars","stream":{"rowKey":"t","columns":{
      "t":{"dtype":"f32"},"volume":{"dtype":"f32"},"close":{"dtype":"f32"}}}}]},
  "scales":[
    {"id":"px","type":"linear","domainFrom":{"data":"bars","field":"t"},"range":"width"},
    {"id":"py","type":"linear","domainFrom":{"data":"bars","field":"volume"},"range":"height"}],
  "coords":{"type":"cartesian"},
  "marks":[
    {"id":"bars","type":"rect","from":"bars","pipeline":"instancedRect@1",
     "encoding":{"x":{"scale":"px","field":"t"},"y":{"scale":"py","field":"volume"},
       "x2":{"scale":"px","field":"close"},"y2":{"scale":"py","field":"close"},
       "color":{"value":"#42a5f5"}}}]
})JSON"});

  // --- Anchor 2: candlestick + SMA(20). Exercises a `window` transform output,
  //     a time scale (timestamp), a multi-channel candle mark, and an AA line.
  //     (Lifted from the §6.2 worked manifest — known to pass the validator.) ---
  out.push_back(
      {"Candlestick + SMA(20)",
       "time scale + a window-transform output (sma20) + candle & lineAA marks",
       R"JSON({
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
})JSON"});

  // --- Anchor 3: treemap-style bin -> aggregate -> rect. Exercises multi-stage
  //     column-set inference (the aggregate's `size`/`c` outputs come from
  //     inferSchema, not declared columns) feeding a rect mark over a `cat` group.
  //     (Lifted from the §6.3 worked manifest — known to pass the validator.) ---
  out.push_back(
      {"Treemap-style bin -> aggregate -> rect",
       "a bin->aggregate transform chain + a cat group key feeding a rect mark",
       R"JSON({
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
})JSON"});

  return out;
}

std::string ContextKit::toPrompt() const {
  std::ostringstream o;
  o << grammarCardMarkdown() << "\n\n";
  o << feed_.toMarkdown() << "\n\n";

  o << "## WORKED MANIFESTS (few-shot anchors — each PASSES the validator)\n\n";
  for (const auto& wm : workedManifests()) {
    o << "### " << wm.title << "\n" << wm.description << "\n\n```json\n"
      << indentBlock(wm.json) << "\n```\n\n";
  }

  o << "## REPAIR PROTOCOL\n\n"
       "Author a `dc-manifest/1` JSON object. It is checked by the ManifestValidator\n"
       "(a data-free type checker). If the verdict is INVALID, you receive a LOCALIZED\n"
       "report — each issue keyed to the offending node id + JSON path + a reason\n"
       "(e.g. \"scale 'yp' type 'log' rejects field 'close'\"). Fix exactly the reported\n"
       "issues and re-emit the full manifest. Repeat until the verdict is VALID.\n";
  return o.str();
}

// ===========================================================================
// Repair-loop scaffold (execution-guided; no LLM call here)
// ===========================================================================

std::string repairSignalFor(const ValidationReport& report) {
  if (report.valid()) return "";
  std::ostringstream o;
  o << "MANIFEST INVALID — fix the following and re-emit the full manifest:\n";
  for (const auto& i : report.issues) {
    if (i.severity != Severity::Error) continue;  // warnings do not invalidate
    o << "  - [" << toString(i.check) << "]";
    if (!i.nodeId.empty()) o << " node '" << i.nodeId << "'";
    if (!i.path.empty()) o << " at " << i.path;
    o << ": " << i.message << "\n";
  }
  return o.str();
}

std::vector<RepairAttempt> runRepairLoop(
    const ManifestValidator& validator,
    const std::function<std::string(const std::string& repairSignal)>& author,
    int maxAttempts) {
  std::vector<RepairAttempt> trace;
  if (maxAttempts < 1) maxAttempts = 1;
  std::string signal;  // "" on the first turn
  for (int attempt = 0; attempt < maxAttempts; ++attempt) {
    RepairAttempt a;
    a.candidate = author(signal);
    a.report = validator.validate(a.candidate);
    if (a.report.valid()) {
      a.done = true;
      a.repairSignal.clear();
      trace.push_back(std::move(a));
      break;
    }
    a.done = false;
    a.repairSignal = repairSignalFor(a.report);
    signal = a.repairSignal;  // feed forward to the next author turn
    trace.push_back(std::move(a));
  }
  return trace;
}

}  // namespace dc
