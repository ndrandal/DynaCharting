// ENC-605 (P1.14) — Manifest v0 parser (data / scales / coords / marks /
// encoding). STRUCTURAL validation (the pixel render proof is ENC-606).
//
// This asserts that a valid §6.2-style manifest (a CANDLE mark + a LINE mark over
// linear scales bound to columns) parses and, driven through the EXISTING encode
// compiler, produces the EXPECTED DrawItems + geometry: the right marks/pipelines,
// the right per-channel scaling, and BYTE-CORRECT vertex/instance geometry. It
// then asserts the §6.1 anti-nonsense typing checks REJECT malformed manifests
// with clear errors: a dangling scale ref, a wrong-dtype scale<->field binding,
// and a channel set that does not cover the pipeline's required format.
//
// THE TIME COLUMN / ENCODE-PASS TIME PATH (ENC-606 Part A — the gap is now WIRED)
// -----------------------------------------------------------------------------
// RESEARCH §6.2's candle binds x = time(t) over a `timestamp` column. ENC-605
// surfaced that the Phase-1 encode pass read numeric channels through the f32
// column view ONLY — a `timestamp` column has no f32 view by design (epoch-ms
// overflows f32) — so a time-scaled channel was REJECTED at build(). ENC-606 Part
// A wires the timestamp→encode path minimally + additively: when a position
// channel binds a TimeScale to a Timestamp(i64 epoch-ms) column, Encoding::resolve
// reads the i64 column losslessly into f64, normalizes it to a small RELATIVE f32
// offset against the scale's CPU base epoch (TimeScale::normalizedOffsetF32), and
// maps that — so a real time x-axis renders with NO f32 epoch-ms overflow. The
// byte-exact happy-path test below still uses an f32 `t` column with a linear x
// scale; a dedicated case now asserts the timestamp+time path BUILDS and packs the
// correct scaled bytes (was: asserted rejected).
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/manifest/Manifest.hpp"
#include "dc/scene/Geometry.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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

static void appendF32(dc::IngestProcessor& ingest, dc::Id buf,
                      const std::vector<float>& vals) {
  std::vector<std::uint8_t> batch;
  appendRecord(batch, buf, vals.data(),
               static_cast<std::uint32_t>(vals.size() * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

static float f32At(const std::vector<std::uint8_t>& bytes, std::size_t off) {
  float v = 0.0f;
  std::memcpy(&v, bytes.data() + off, sizeof(float));
  return v;
}

static bool eqf(float a, float b) {
  return std::fabs(a - b) <= 1e-5f * (1.0f + std::fabs(a));
}

// A §6.2-style candle + SMA-line manifest. `t` is an f32 column + a LINEAR x scale
// (the encode-pass-supported path; see the file header on the timestamp seam). The
// SMA column `sma20` is treated as a PRE-EXISTING column (no transform computed
// here — that's Phase 3 / the ENC-606 demo's trivial CPU helper).
static const char* kCandleManifest = R"JSON(
{
  "version": "dc-manifest/1", "id": "candles-sma",
  "data": { "sources": [{
    "id": "ohlc", "kind": "stream",
    "stream": {
      "rowKey": "t",
      "columns": {
        "t":     {"from":"rowKey","dtype":"f32","role":"time"},
        "open":  {"from":"field:open","dtype":"f32"},
        "high":  {"from":"field:high","dtype":"f32"},
        "low":   {"from":"field:low","dtype":"f32"},
        "close": {"from":"field:close","dtype":"f32"},
        "sma20": {"from":"field:sma20","dtype":"f32"}
      } } }] },
  "scales": [
    { "id":"xt","type":"linear","domainFrom":{"data":"ohlc","field":"t"},"range":"width" },
    { "id":"yp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},"range":"height" }
  ],
  "coords": { "type":"cartesian" },
  "marks": [
    { "id":"candles","type":"candle","from":"ohlc","pipeline":"instancedCandle@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},
        "yOpen":{"scale":"yp","field":"open"},"yClose":{"scale":"yp","field":"close"},
        "yHigh":{"scale":"yp","field":"high"},"yLow":{"scale":"yp","field":"low"},
        "width":{"value":0.4},
        "color":{"condition":{"value":"#26a69a"},"value":"#ef5350"} } },
    { "id":"smaLine","type":"line","from":"ohlc","pipeline":"line2d@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},"y":{"scale":"yp","field":"sma20"},
        "color":{"value":"#ffb300"},"strokeWidth":{"value":1.5} } }
  ]
}
)JSON";

int main() {
  std::printf("=== ENC-605 Manifest v0 parser ===\n");

  // =========================================================================
  // VALID MANIFEST — parses + drives the encode compiler to byte-exact geometry.
  // =========================================================================
  {
    dc::Manifest m;
    auto lr = m.load(kCandleManifest);
    check(lr.ok(), "valid: load parses the candle+SMA manifest");
    check(m.id() == "candles-sma", "valid: manifest id parsed");

    // The data section built a table with the six declared columns.
    auto tid = m.tableId("ohlc");
    check(tid.has_value(), "valid: 'ohlc' source resolves to a table id");
    check(m.tables().column(*tid, "close") != nullptr &&
              m.tables().column(*tid, "sma20") != nullptr,
          "valid: all declared columns exist (incl pre-existing sma20)");

    // Both scales parsed (linear) with their bindings.
    check(m.scale("xt") != nullptr && m.scale("xt")->type() == dc::ScaleType::Linear,
          "valid: scale 'xt' is a linear scale");
    check(m.scale("yp") != nullptr, "valid: scale 'yp' resolves");

    // Feed two rows of OHLC + SMA through the UNCHANGED ingest feed against the
    // manifest's declared column buffers.
    dc::IngestProcessor ingest;
    auto src = dc::makeBufferByteSource(ingest);
    auto buf = [&](const char* c) { return *m.columnBufferId("ohlc", c); };
    // Two candles: one up (close>open), one down. y domain spans low..high = 9..21.
    appendF32(ingest, buf("t"),     {0.0f, 1.0f});
    appendF32(ingest, buf("open"),  {10.0f, 20.0f});
    appendF32(ingest, buf("high"),  {12.0f, 21.0f});
    appendF32(ingest, buf("low"),   {9.0f, 16.0f});
    appendF32(ingest, buf("close"), {11.0f, 17.0f});
    appendF32(ingest, buf("sma20"), {10.5f, 18.5f});

    auto br = m.build(src);
    check(br.ok(), "valid: build runs the encode pass for every mark");
    check(m.compiledMarks().size() == 2, "valid: 2 marks compiled (candle + line)");

    // Auto-domains folded the live columns: xt over t=[0,1], yp over low/high.
    check(eqf(static_cast<float>(m.scale("xt")->domain().min), 0.0f) &&
              eqf(static_cast<float>(m.scale("xt")->domain().max), 1.0f),
          "valid: xt auto-domain folded t -> [0,1]");
    // yp binds the FIRST domain field (low): low column is {9,16} -> [9,16].
    check(eqf(static_cast<float>(m.scale("yp")->domain().min), 9.0f),
          "valid: yp auto-domain folded its bound column min (9)");

    // ----- the CANDLE mark: instancedCandle@1, Candle6 byte-exact -----
    const dc::CompiledMark* candle = m.compiledMark("candles");
    check(candle != nullptr, "valid: 'candles' mark compiled");
    check(candle->pipeline == "instancedCandle@1" &&
              candle->result.geometry.format == dc::VertexFormat::Candle6,
          "valid: candle -> instancedCandle@1 / Candle6");
    check(candle->result.bytes.size() == 2u * 24u,
          "valid: candle is 2 * 24B (exact Candle6 stride)");

    // Candle6 = (x, open, high, low, close, halfWidth). x is xt.map(t),
    // open/high/low/close are yp.map(...), halfWidth is the constant 0.4.
    const dc::Scale* xt = m.scale("xt");
    const dc::Scale* yp = m.scale("yp");
    const float xs[2] = {static_cast<float>(xt->map(0.0)),
                         static_cast<float>(xt->map(1.0))};
    const float open[2]  = {10.0f, 20.0f}, high[2] = {12.0f, 21.0f};
    const float low[2]   = {9.0f, 16.0f},  close[2] = {11.0f, 17.0f};
    bool candleOk = true;
    for (int i = 0; i < 2; ++i) {
      const std::size_t base = i * 24;
      if (!eqf(f32At(candle->result.bytes, base + 0), xs[i])) candleOk = false;
      if (!eqf(f32At(candle->result.bytes, base + 4),
               static_cast<float>(yp->map(open[i])))) candleOk = false;
      if (!eqf(f32At(candle->result.bytes, base + 8),
               static_cast<float>(yp->map(high[i])))) candleOk = false;
      if (!eqf(f32At(candle->result.bytes, base + 12),
               static_cast<float>(yp->map(low[i])))) candleOk = false;
      if (!eqf(f32At(candle->result.bytes, base + 16),
               static_cast<float>(yp->map(close[i])))) candleOk = false;
      if (!eqf(f32At(candle->result.bytes, base + 20), 0.4f)) candleOk = false;  // halfWidth
    }
    check(candleOk,
          "valid: candle bytes are byte-exact [x,open,high,low,close,hw] (scaled)");

    // The candle's up/down colors routed onto the DrawItem (NOT into the buffer).
    check(eqf(candle->result.drawItem.colorUp[1], 0xa6 / 255.0f) &&
              eqf(candle->result.drawItem.colorDown[0], 0xef / 255.0f),
          "valid: candle color condition -> colorUp/colorDown on the draw item");
    check(candle->result.drawItem.pipeline == "instancedCandle@1",
          "valid: candle draw item carries the pipeline key");

    // ----- the LINE mark: line2d@1, Pos2_Clip LineList byte-exact -----
    const dc::CompiledMark* line = m.compiledMark("smaLine");
    check(line != nullptr && line->pipeline == "line2d@1" &&
              line->result.geometry.format == dc::VertexFormat::Pos2_Clip,
          "valid: smaLine -> line2d@1 / Pos2_Clip");
    // 2 rows -> 1 segment -> 2 LineList vertices (8B each).
    check(line->result.bytes.size() == 2u * 8u,
          "valid: line is 2 verts (1 segment) * 8B");
    // seg0: (xt.map(0), yp.map(sma0=10.5)) -> (xt.map(1), yp.map(sma1=18.5)).
    bool lineOk =
        eqf(f32At(line->result.bytes, 0), static_cast<float>(xt->map(0.0))) &&
        eqf(f32At(line->result.bytes, 4), static_cast<float>(yp->map(10.5))) &&
        eqf(f32At(line->result.bytes, 8), static_cast<float>(xt->map(1.0))) &&
        eqf(f32At(line->result.bytes, 12), static_cast<float>(yp->map(18.5)));
    check(lineOk, "valid: line bytes are byte-exact scaled LineList endpoints");
    check(eqf(line->result.drawItem.color[2], 0x00 / 255.0f) &&
              eqf(line->result.drawItem.color[0], 0xff / 255.0f),
          "valid: line color (#ffb300) routed onto the draw item");
  }

  // =========================================================================
  // INCREMENTAL re-build — a second build after appending more rows re-folds the
  // domains and re-packs, byte-exact to the grown table.
  // =========================================================================
  {
    dc::Manifest m;
    check(m.load(kCandleManifest).ok(), "incr: load");
    dc::IngestProcessor ingest;
    auto src = dc::makeBufferByteSource(ingest);
    auto buf = [&](const char* c) { return *m.columnBufferId("ohlc", c); };
    appendF32(ingest, buf("t"), {0.0f}); appendF32(ingest, buf("open"), {10.0f});
    appendF32(ingest, buf("high"), {12.0f}); appendF32(ingest, buf("low"), {9.0f});
    appendF32(ingest, buf("close"), {11.0f}); appendF32(ingest, buf("sma20"), {10.5f});
    check(m.build(src).ok(), "incr: build tick1 (1 row)");
    check(m.compiledMark("candles")->result.bytes.size() == 24u,
          "incr: 1 candle after tick1");

    appendF32(ingest, buf("t"), {1.0f, 2.0f});
    appendF32(ingest, buf("open"), {20.0f, 5.0f});
    appendF32(ingest, buf("high"), {21.0f, 6.0f});
    appendF32(ingest, buf("low"), {16.0f, 4.0f});
    appendF32(ingest, buf("close"), {17.0f, 5.5f});
    appendF32(ingest, buf("sma20"), {18.5f, 5.0f});
    check(m.build(src).ok(), "incr: build tick2 (now 3 rows)");
    check(m.compiledMark("candles")->result.bytes.size() == 3u * 24u,
          "incr: 3 candles after tick2 (whole table re-packed)");
    // yp domain extended to cover the new low (4).
    check(eqf(static_cast<float>(m.scale("yp")->domain().min), 4.0f),
          "incr: yp auto-domain extended to the new min (4)");
  }

  // =========================================================================
  // §6.1 #1 — REFERENCE RESOLUTION: a dangling scale ref is rejected at load.
  // =========================================================================
  {
    const char* bad = R"JSON(
    { "id":"x", "data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"},"b":{"dtype":"f32"}}}]},
      "scales":[{"id":"sy","type":"linear","domainFrom":{"data":"d","field":"b"},"range":"height"}],
      "coords":{"type":"cartesian"},
      "marks":[{"id":"pts","type":"point","from":"d",
        "encoding":{"x":{"field":"a"},"y":{"scale":"NOPE","field":"b"}}}] }
    )JSON";
    dc::Manifest m;
    auto r = m.load(bad);
    check(r.status == dc::ManifestStatus::DanglingRef,
          "reject: dangling scale ref -> DanglingRef");
    check(r.message.find("NOPE") != std::string::npos,
          "reject: dangling ref error names the missing scale");
  }

  // dangling FIELD ref (a channel field that is not a column).
  {
    const char* bad = R"JSON(
    { "id":"x", "data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"}}}]},
      "scales":[{"id":"sx","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"pts","type":"point","from":"d",
        "encoding":{"x":{"field":"a"},"y":{"field":"ghost"}}}] }
    )JSON";
    dc::Manifest m;
    auto r = m.load(bad);
    check(r.status == dc::ManifestStatus::DanglingRef &&
              r.message.find("ghost") != std::string::npos,
          "reject: dangling field ref -> DanglingRef naming the field");
  }

  // dangling mark `from`.
  {
    const char* bad = R"JSON(
    { "id":"x", "data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"}}}]},
      "scales":[{"id":"sx","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"pts","type":"point","from":"missingSource",
        "encoding":{"x":{"field":"a"},"y":{"field":"a"}}}] }
    )JSON";
    dc::Manifest m;
    check(m.load(bad).status == dc::ManifestStatus::DanglingRef,
          "reject: dangling mark 'from' -> DanglingRef");
  }

  // =========================================================================
  // §6.1 #2 — DTYPE MISMATCH: a scale type rejects a column dtype.
  //   A `time` scale bound to an f32 column (time needs timestamp).
  // =========================================================================
  {
    const char* bad = R"JSON(
    { "id":"x", "data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"},"b":{"dtype":"f32"}}}]},
      "scales":[{"id":"xt","type":"time","domainFrom":{"data":"d","field":"a"},"range":"width"}],
      "marks":[{"id":"pts","type":"point","from":"d",
        "encoding":{"x":{"scale":"xt","field":"a"},"y":{"field":"b"}}}] }
    )JSON";
    dc::Manifest m;
    auto r = m.load(bad);
    check(r.status == dc::ManifestStatus::DTypeMismatch,
          "reject: time scale over an f32 column -> DTypeMismatch");
    check(r.message.find("time") != std::string::npos,
          "reject: dtype error names the offending scale type");
  }

  // a band scale bound to an f32 column (band needs cat) via a channel binding.
  {
    const char* bad = R"JSON(
    { "id":"x", "data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"},"b":{"dtype":"f32"}}}]},
      "scales":[{"id":"sx","type":"band"}],
      "marks":[{"id":"pts","type":"point","from":"d",
        "encoding":{"x":{"scale":"sx","field":"a"},"y":{"field":"b"}}}] }
    )JSON";
    dc::Manifest m;
    check(m.load(bad).status == dc::ManifestStatus::DTypeMismatch,
          "reject: band scale over an f32 field -> DTypeMismatch");
  }

  // =========================================================================
  // §6.1 #3 — CHANNEL SET vs PIPELINE: a rect missing the x2 channel does not
  // cover instancedRect@1's required format. Rejected (structural pre-check).
  // =========================================================================
  {
    const char* bad = R"JSON(
    { "id":"x", "data":{"sources":[{"id":"d","columns":{
        "x0":{"dtype":"f32"},"y0":{"dtype":"f32"},"y1":{"dtype":"f32"}}}]},
      "scales":[{"id":"s","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"r","type":"rect","from":"d",
        "encoding":{"x":{"field":"x0"},"y":{"field":"y0"},"y2":{"field":"y1"}}}] }
    )JSON";
    dc::Manifest m;
    auto r = m.load(bad);
    check(r.status == dc::ManifestStatus::EncodeRejected,
          "reject: rect missing x2 channel -> EncodeRejected");
    check(r.message.find("x2") != std::string::npos,
          "reject: channel-coverage error names the missing channel (x2)");
  }

  // =========================================================================
  // TIMESTAMP + TIME scale (ENC-606 Part A) — passes §6.1 typing (time<-timestamp)
  // AND now BUILDS: the encode pass routes a time-scaled timestamp channel through
  // the i64 -> f64 -> relative-f32-offset path (no epoch-ms->f32 overflow) and
  // packs the correct scaled x bytes. Was rejected pre-ENC-606.
  // =========================================================================
  {
    const char* tsManifest = R"JSON(
    { "id":"ts", "data":{"sources":[{"id":"d","stream":{"rowKey":"t","columns":{
        "t":{"from":"rowKey","dtype":"timestamp","role":"time"},
        "v":{"from":"field:v","dtype":"f32"}}}}]},
      "scales":[
        {"id":"xt","type":"time","domainFrom":{"data":"d","field":"t"},"range":"width"},
        {"id":"yv","type":"linear","domainFrom":{"data":"d","field":"v"},"range":"height"}],
      "coords":{"type":"cartesian"},
      "marks":[{"id":"ln","type":"line","from":"d",
        "encoding":{"x":{"scale":"xt","field":"t"},"y":{"scale":"yv","field":"v"}}}] }
    )JSON";
    dc::Manifest m;
    auto lr = m.load(tsManifest);
    check(lr.ok(),
          "ts-time: timestamp+time manifest PASSES §6.1 typing at load");
    dc::IngestProcessor ingest;
    auto src = dc::makeBufferByteSource(ingest);
    // append a couple of timestamp (i64 epoch-ms) + f32 rows. Two epoch-ms values
    // 60 s apart — both far past the f32 mantissa limit, so a naive f32 cast would
    // collapse them to the SAME value. The relative-offset path keeps them apart.
    const std::int64_t t0 = 1700000000000LL, t1 = 1700000060000LL;
    {
      std::vector<std::int64_t> ts = {t0, t1};
      std::vector<std::uint8_t> batch;
      appendRecord(batch, *m.columnBufferId("d", "t"), ts.data(),
                   static_cast<std::uint32_t>(ts.size() * sizeof(std::int64_t)));
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }
    appendF32(ingest, *m.columnBufferId("d", "v"), {1.0f, 2.0f});
    auto br = m.build(src);
    check(br.ok(),
          "ts-time: build NOW SUCCEEDS (time-on-timestamp channel is wired)");

    // The line packs one segment (2 LineList verts, 8B each). x is the TimeScale
    // mapping of the epoch-ms (via the relative f32 offset); over the auto-domain
    // [t0,t1] the endpoints map to the range extents (width => [0,1]).
    const dc::CompiledMark* ln = m.compiledMark("ln");
    check(ln != nullptr && ln->result.bytes.size() == 2u * 8u,
          "ts-time: line packs 1 segment (2 * 8B) from the timestamp x channel");
    const dc::Scale* xt = m.scale("xt");
    const dc::Scale* yv = m.scale("yv");
    bool tsOk =
        eqf(f32At(ln->result.bytes, 0),
            static_cast<float>(xt->map(static_cast<double>(t0)))) &&
        eqf(f32At(ln->result.bytes, 4), static_cast<float>(yv->map(1.0))) &&
        eqf(f32At(ln->result.bytes, 8),
            static_cast<float>(xt->map(static_cast<double>(t1)))) &&
        eqf(f32At(ln->result.bytes, 12), static_cast<float>(yv->map(2.0)));
    check(tsOk,
          "ts-time: x bytes == TimeScale.map(epoch-ms) (relative-offset path, no overflow)");
    // The two distinct epoch-ms 60 s apart produce DISTINCT x (the overflow trap
    // a naive f32 epoch-ms cast would fall into is avoided).
    check(!eqf(f32At(ln->result.bytes, 0), f32At(ln->result.bytes, 8)),
          "ts-time: distinct epoch-ms -> distinct x (no f32 epoch-ms collapse)");
  }

  // =========================================================================
  // MALFORMED structure — bad JSON, missing sections, unknown enums.
  // =========================================================================
  {
    dc::Manifest m;
    check(m.load("not json").status == dc::ManifestStatus::BadJson,
          "reject: non-JSON -> BadJson");
    check(m.load(R"({"id":"x"})").status == dc::ManifestStatus::MissingSection,
          "reject: missing data section -> MissingSection");

    const char* badCoords = R"JSON(
    { "id":"x","data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"}}}]},
      "scales":[{"id":"s","type":"linear","domain":[0,1],"range":"width"}],
      "coords":{"type":"polar"},
      "marks":[{"id":"p","type":"point","from":"d","encoding":{"x":{"field":"a"},"y":{"field":"a"}}}] }
    )JSON";
    check(m.load(badCoords).status == dc::ManifestStatus::UnknownCoords,
          "reject: polar coords -> UnknownCoords (v0 cartesian only)");

    const char* badDtype = R"JSON(
    { "id":"x","data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f64"}}}]},
      "scales":[{"id":"s","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"p","type":"point","from":"d","encoding":{"x":{"field":"a"},"y":{"field":"a"}}}] }
    )JSON";
    check(m.load(badDtype).status == dc::ManifestStatus::UnknownDType,
          "reject: unknown column dtype (f64) -> UnknownDType");

    const char* badMark = R"JSON(
    { "id":"x","data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"}}}]},
      "scales":[{"id":"s","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"p","type":"area","from":"d","encoding":{"x":{"field":"a"}}}] }
    )JSON";
    check(m.load(badMark).status == dc::ManifestStatus::UnknownMarkType,
          "reject: unknown mark type (area) -> UnknownMarkType");

    const char* badScale = R"JSON(
    { "id":"x","data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"}}}]},
      "scales":[{"id":"s","type":"quantile","domain":[0,1],"range":"width"}],
      "marks":[{"id":"p","type":"point","from":"d","encoding":{"x":{"field":"a"},"y":{"field":"a"}}}] }
    )JSON";
    check(m.load(badScale).status == dc::ManifestStatus::UnknownScaleType,
          "reject: unknown scale type (quantile) -> UnknownScaleType");

    const char* dupId = R"JSON(
    { "id":"x","data":{"sources":[{"id":"d","columns":{"a":{"dtype":"f32"}}}]},
      "scales":[{"id":"s","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[
        {"id":"p","type":"point","from":"d","encoding":{"x":{"field":"a"},"y":{"field":"a"}}},
        {"id":"p","type":"point","from":"d","encoding":{"x":{"field":"a"},"y":{"field":"a"}}}] }
    )JSON";
    check(m.load(dupId).status == dc::ManifestStatus::DuplicateId,
          "reject: duplicate mark id -> DuplicateId");

    // a candle pinned to the wrong pipeline (lineAA@1) -> pipeline mismatch.
    const char* badPipe = R"JSON(
    { "id":"x","data":{"sources":[{"id":"d","columns":{
        "t":{"dtype":"f32"},"o":{"dtype":"f32"},"h":{"dtype":"f32"},
        "l":{"dtype":"f32"},"c":{"dtype":"f32"}}}]},
      "scales":[{"id":"s","type":"linear","domain":[0,1],"range":"width"}],
      "marks":[{"id":"k","type":"candle","from":"d","pipeline":"lineAA@1",
        "encoding":{"x":{"field":"t"},"yOpen":{"field":"o"},"yHigh":{"field":"h"},
          "yLow":{"field":"l"},"yClose":{"field":"c"},"width":{"value":0.4}}}] }
    )JSON";
    check(m.load(badPipe).status == dc::ManifestStatus::UnknownPipeline,
          "reject: candle pinned to lineAA@1 -> UnknownPipeline (fixed mapping)");
  }

  std::printf("=== ENC-605 Manifest Results: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
