// ENC-606 (P1.15) — THE HEADLINE PROOF: candlestick + SMA(20) from a RAW OHLC
// feed, end to end through the JSON manifest path, with ZERO upstream precompute.
// This proves the inversion: ship the RAW numbers + a declarative spec, and the
// engine does data→pixels (RESEARCH §6.2, worked manifest A).
//
// THE FULL PROOF CHAIN (no precomputed geometry anywhere)
// -------------------------------------------------------
//   1. RAW long feed  {t, streamKey, field, value}  (the only substrate, §3)
//   2. PivotIngest    long events -> wide OHLC rows in the manifest's TableStore,
//                     landed through the UNCHANGED 13-byte ingest feed (op 1).
//   3. SMA(20)        a TRIVIAL CPU rolling-mean helper computes the sma20 column
//                     from the pivoted `close` column and feeds it back through the
//                     same ingest feed (full transforms are Phase 3; a small local
//                     helper is in-scope here per the ticket).
//   4. Manifest       a §6.2 spec: TimeScale x (epoch-ms `t`) + LinearScale y, a
//                     `candle` mark (instancedCandle@1) over OHLC, a `line` mark
//                     (line2d@1) over sma20.
//   5. build()        re-folds the scales' auto-domains over the live columns and
//                     runs the encode pass per mark -> DrawItems + byte-exact
//                     Candle6 + LineList geometry. The engine did data→pixels.
//
// This test asserts the EXPECTED DrawItems + BYTE-CORRECT geometry (Candle6 +
// line) from the raw feed, and that the TIME x-axis (Part A: timestamp→encode)
// packs the correct relative-offset-scaled bytes. The Dawn render proof is the
// sibling dc_enc606_dawn_candle_sma render test.
#include "dc/data/PivotIngest.hpp"
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

static float f32At(const std::vector<std::uint8_t>& bytes, std::size_t off) {
  float v = 0.0f;
  std::memcpy(&v, bytes.data() + off, sizeof(float));
  return v;
}

static bool eqf(float a, float b) {
  return std::fabs(a - b) <= 1e-4f * (1.0f + std::fabs(a));
}

// One 13-byte ingest APPEND record (op=1) — the EXACT existing wire format. Used
// to feed the computed SMA column back through the unchanged feed (the SMA column
// is, to the engine, just another appended buffer — no special path).
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

// One 13-byte ingest UPDATE_RANGE record (op=2) — patches an existing slice in
// place at `offsetBytes`. The derived SMA column already has rows (the pivot wrote
// the MISSING sentinel into every declared column at flush); op=2 overwrites those
// sentinels with the computed rolling means WITHOUT growing the row count, keeping
// the table in lockstep. This IS the streaming derived-column model.
static void updateRangeRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
                              std::uint32_t offsetBytes, const void* bytes,
                              std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(2);  // op = UPDATE_RANGE
  u32(static_cast<std::uint32_t>(bufferId));
  u32(offsetBytes);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

static void updateF32(dc::IngestProcessor& ingest, dc::Id buf,
                      const std::vector<float>& vals) {
  std::vector<std::uint8_t> batch;
  updateRangeRecord(batch, buf, 0, vals.data(),
                    static_cast<std::uint32_t>(vals.size() * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// ---------------------------------------------------------------------------
// The TRIVIAL CPU rolling-mean SMA helper (ENC-606 scope: a small local helper;
// the full window transform is Phase 3). Min-periods 1 so early rows are defined:
// sma[i] = mean(close[max(0,i-W+1) .. i]). O(W) per row, fine for the proof.
// ---------------------------------------------------------------------------
static std::vector<float> rollingMean(const std::vector<float>& x, int window) {
  std::vector<float> out(x.size(), 0.0f);
  for (std::size_t i = 0; i < x.size(); ++i) {
    const std::size_t lo = (i + 1 >= static_cast<std::size_t>(window))
                               ? i + 1 - static_cast<std::size_t>(window)
                               : 0;
    double sum = 0.0;
    for (std::size_t j = lo; j <= i; ++j) sum += x[j];
    out[i] = static_cast<float>(sum / static_cast<double>(i - lo + 1));
  }
  return out;
}

// The §6.2 manifest: a candle mark + an SMA line, TimeScale x over the `t`
// timestamp column, LinearScale y over low/high (and the SMA shares it). `t` is a
// real `timestamp` (i64 epoch-ms) bound to a `time` scale — exercising the ENC-606
// Part A timestamp→encode path. The SMA column is a PRE-EXISTING column (the
// trivial helper fills it; the manifest never computes a transform).
static const char* kManifest = R"JSON(
{
  "version": "dc-manifest/1", "id": "candles-sma",
  "data": { "sources": [{
    "id": "ohlc", "kind": "stream",
    "stream": {
      "match": { "streamKey": ["AAPL"], "field": ["open","high","low","close","volume"] },
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
        "width":{"value":0.4},
        "color":{"condition":{"value":"#26a69a"},"value":"#ef5350"} } },
    { "id":"smaLine","type":"line","from":"ohlc","pipeline":"line2d@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},"y":{"scale":"yp","field":"sma20"},
        "color":{"value":"#ffb300"},"strokeWidth":{"value":1.5} } }
  ]
}
)JSON";

int main() {
  std::printf("=== ENC-606 PROOF: candlestick + SMA(20) from raw OHLC via manifest ===\n");

  // ----- the RAW OHLC feed (long/tidy events; epoch-ms minute bars) -----------
  // 25 one-minute bars so SMA(20) has both warm-up (min-periods) and a steady
  // 20-wide window. A simple sawtooth-ish close so the SMA visibly trails it.
  const int N = 25;
  const std::int64_t t0 = 1700000000000LL;  // a real epoch-ms (far past the f32 limit)
  const std::int64_t dtMs = 60000LL;        // 60 s bars
  std::vector<std::int64_t> ts(N);
  std::vector<float> open(N), high(N), low(N), close(N), vol(N);
  for (int i = 0; i < N; ++i) {
    ts[i] = t0 + static_cast<std::int64_t>(i) * dtMs;
    const float base = 100.0f + 5.0f * std::sin(0.4f * static_cast<float>(i));
    open[i]  = base;
    close[i] = base + ((i % 3 == 0) ? 1.5f : -1.2f);  // mix of up/down bars
    high[i]  = std::max(open[i], close[i]) + 1.0f;
    low[i]   = std::min(open[i], close[i]) - 1.0f;
    vol[i]   = 1000.0f + 10.0f * static_cast<float>(i);
  }

  // ----- load the manifest (it owns the TableStore schema) --------------------
  dc::Manifest m;
  check(m.load(kManifest).ok(), "load: §6.2 candle+SMA manifest parses");
  auto tid = m.tableId("ohlc");
  check(tid.has_value(), "load: 'ohlc' resolves to a table");
  check(m.scale("xt") != nullptr &&
            m.scale("xt")->type() == dc::ScaleType::Time,
        "load: x scale 'xt' is a TIME scale (over the timestamp column)");

  // ----- PivotIngest the RAW long events into the manifest's table ------------
  // PivotIngest shares the manifest's TableStore (schema) + the ingest feed
  // (bytes). The manifest reads the same bytes through the BufferByteSource.
  dc::IngestProcessor ingest;
  auto src = dc::makeBufferByteSource(ingest);
  dc::PivotIngest pivot(m.tables(), ingest);
  check(pivot.setTable(*tid), "pivot: bound to the ohlc table");
  check(pivot.setRowKeyColumn("t"), "pivot: rowKey column = t (timestamp)");
  pivot.mapField("open", "open");
  pivot.mapField("high", "high");
  pivot.mapField("low", "low");
  pivot.mapField("close", "close");
  pivot.mapField("volume", "volume");

  // Push the long feed one (field,value) event at a time, exactly as a raw stream
  // delivers them. Auto-flush-on-new-rowKey lands each completed bar.
  for (int i = 0; i < N; ++i) {
    pivot.pushEvent(ts[i], "open",   dc::pvF32(open[i]));
    pivot.pushEvent(ts[i], "high",   dc::pvF32(high[i]));
    pivot.pushEvent(ts[i], "low",    dc::pvF32(low[i]));
    pivot.pushEvent(ts[i], "close",  dc::pvF32(close[i]));
    pivot.pushEvent(ts[i], "volume", dc::pvF32(vol[i]));
  }
  pivot.flushAll();  // flush the final open bar
  check(pivot.rowsAppended() == static_cast<std::size_t>(N),
        "pivot: all 25 OHLC rows pivoted from the raw long feed");

  // The pivot wrote the i64 timestamp + f32 OHLC into the manifest's columns.
  check(m.tables().rowCount(*tid, src) == static_cast<std::size_t>(N),
        "pivot: table row count == 25 (raw events -> wide rows)");

  // ----- compute SMA(20) with the trivial CPU helper, feed it back ------------
  // Read the pivoted `close` column straight back out (it lives in the ingest
  // feed) and compute the rolling mean, then append it as the sma20 column. No
  // precomputed geometry — just a derived data column.
  dc::ColumnView<float> closeCol = m.tables().viewF32(*tid, "close", src);
  check(closeCol.valid() && closeCol.count == static_cast<std::size_t>(N),
        "sma: read the pivoted close column back from the feed");
  std::vector<float> closeVec(closeCol.begin(), closeCol.end());
  std::vector<float> sma = rollingMean(closeVec, 20);
  // Patch the pre-existing sma20 column (the pivot filled it with the missing
  // sentinel) in place via op=2 — keeps the table in lockstep at 25 rows.
  updateF32(ingest, *m.columnBufferId("ohlc", "sma20"), sma);
  check(m.tables().rowCountConsistent(*tid, src),
        "sma: table stays in lockstep after the derived sma20 patch (op=2)");

  // sma[0] == close[0] (min-periods 1); sma[24] is the mean of the last 20 closes.
  check(eqf(sma[0], close[0]), "sma: sma20[0] == close[0] (min-periods 1)");
  {
    double s = 0.0;
    for (int j = 5; j <= 24; ++j) s += close[j];  // 20-wide window ending at 24
    check(eqf(sma[24], static_cast<float>(s / 20.0)),
          "sma: sma20[24] == mean of the last 20 closes");
  }

  // =========================================================================
  // build() — re-fold the auto-domains + run the encode pass per mark.
  // =========================================================================
  auto br = m.build(src);
  check(br.ok(), "build: encode pass ran for every mark (data->geometry)");
  check(m.compiledMarks().size() == 2, "build: 2 marks compiled (candle + SMA line)");

  // The TIME scale auto-domained over the epoch-ms column [t0, t0+24*dt].
  const dc::Scale* xt = m.scale("xt");
  const dc::Scale* yp = m.scale("yp");
  check(xt->domain().min == static_cast<double>(t0),
        "build: xt auto-domain min == t0 (epoch-ms, f64, no overflow)");
  check(xt->domain().max == static_cast<double>(ts[N - 1]),
        "build: xt auto-domain max == last epoch-ms");

  // ----- CANDLE mark: instancedCandle@1, Candle6, byte-exact ------------------
  const dc::CompiledMark* candle = m.compiledMark("candles");
  check(candle != nullptr && candle->pipeline == "instancedCandle@1" &&
            candle->result.geometry.format == dc::VertexFormat::Candle6,
        "candle: -> instancedCandle@1 / Candle6");
  check(candle->result.bytes.size() == static_cast<std::size_t>(N) * 24u,
        "candle: 25 instances * 24B (exact Candle6 stride)");

  // Verify EVERY candle's bytes are byte-exact: x = TimeScale.map(epoch-ms) (the
  // Part-A relative-offset path), open/high/low/close = yp.map(...), hw = 0.4.
  bool candleOk = true;
  for (int i = 0; i < N; ++i) {
    const std::size_t b = static_cast<std::size_t>(i) * 24;
    if (!eqf(f32At(candle->result.bytes, b + 0),
             static_cast<float>(xt->map(static_cast<double>(ts[i]))))) candleOk = false;
    if (!eqf(f32At(candle->result.bytes, b + 4),
             static_cast<float>(yp->map(open[i]))))  candleOk = false;
    if (!eqf(f32At(candle->result.bytes, b + 8),
             static_cast<float>(yp->map(high[i]))))  candleOk = false;
    if (!eqf(f32At(candle->result.bytes, b + 12),
             static_cast<float>(yp->map(low[i]))))   candleOk = false;
    if (!eqf(f32At(candle->result.bytes, b + 16),
             static_cast<float>(yp->map(close[i])))) candleOk = false;
    if (!eqf(f32At(candle->result.bytes, b + 20), 0.4f)) candleOk = false;
  }
  check(candleOk,
        "candle: ALL 25 instances byte-exact [x(time),open,high,low,close,hw]");

  // Distinct epoch-ms minute bars produce distinct x (the f32 epoch-ms overflow
  // trap that a naive cast would hit is avoided — bars 0 and 1 differ).
  check(!eqf(f32At(candle->result.bytes, 0),
             f32At(candle->result.bytes, 24)),
        "candle: bar 0 and bar 1 have DISTINCT x (time path, no f32 collapse)");

  // The up/down colors routed onto the DrawItem (not into the per-instance buffer).
  check(eqf(candle->result.drawItem.colorUp[1], 0xa6 / 255.0f) &&
            eqf(candle->result.drawItem.colorDown[0], 0xef / 255.0f),
        "candle: #26a69a up / #ef5350 down on the draw item");

  // ----- SMA LINE mark: line2d@1, Pos2_Clip LineList, byte-exact --------------
  const dc::CompiledMark* line = m.compiledMark("smaLine");
  check(line != nullptr && line->pipeline == "line2d@1" &&
            line->result.geometry.format == dc::VertexFormat::Pos2_Clip,
        "sma-line: -> line2d@1 / Pos2_Clip");
  // 25 rows -> 24 segments -> 48 LineList vertices (8B each).
  check(line->result.bytes.size() == static_cast<std::size_t>(2 * (N - 1)) * 8u,
        "sma-line: 24 segments -> 48 verts * 8B");

  // Verify every segment endpoint: x = time(t), y = yp.map(sma20). Each segment k
  // is (row k -> row k+1).
  bool lineOk = true;
  for (int k = 0; k < N - 1; ++k) {
    const std::size_t b = static_cast<std::size_t>(k) * 16;  // 2 verts * 8B
    if (!eqf(f32At(line->result.bytes, b + 0),
             static_cast<float>(xt->map(static_cast<double>(ts[k]))))) lineOk = false;
    if (!eqf(f32At(line->result.bytes, b + 4),
             static_cast<float>(yp->map(sma[k])))) lineOk = false;
    if (!eqf(f32At(line->result.bytes, b + 8),
             static_cast<float>(xt->map(static_cast<double>(ts[k + 1]))))) lineOk = false;
    if (!eqf(f32At(line->result.bytes, b + 12),
             static_cast<float>(yp->map(sma[k + 1])))) lineOk = false;
  }
  check(lineOk,
        "sma-line: ALL 24 segments byte-exact [x(time), y=yp.map(sma20)]");
  check(eqf(line->result.drawItem.color[0], 0xff / 255.0f) &&
            eqf(line->result.drawItem.color[2], 0x00 / 255.0f),
        "sma-line: #ffb300 color on the draw item");

  std::printf("=== ENC-606 PROOF: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
