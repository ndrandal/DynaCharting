// ENC-606 (P1.15) — THE HEADLINE PROOF, RENDERED: candlestick + SMA(20) from a RAW
// OHLC feed, end to end through the JSON manifest path, rasterized on Dawn. The
// pixel companion to dc_uce_enc606_proof: it takes the EXACT same chain — raw long
// {t,field,value} events -> PivotIngest -> trivial CPU SMA(20) -> a §6.2 manifest
// (TimeScale x + LinearScale y, candle mark + line mark) -> build() -> byte-exact
// geometry — and feeds the compiled DrawItems + geometry straight into the headless
// DawnDevice, then asserts a NON-BLANK, PLAUSIBLE chart: green up-candle pixels, red
// down-candle pixels, and the amber SMA line, with real coverage (no precomputed
// geometry — the engine did data→pixels).
//
// CLIP-SPACE RANGES: the manifest scales here use explicit clip-space ranges
//   x: [-0.9, 0.9]   y: [0.9, -0.9]  (higher price -> clip-up; the WGSL y-flip then
// lands higher price near the framebuffer TOP). map() output is the clip coord the
// candle/line vertex shaders read directly. This is the only render-specific tweak;
// the data path, pivot, SMA, encode, and byte layout are identical to the fast test.
//
// On this headless box the only Vulkan backend may be lavapipe (software). Force:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/data/PivotIngest.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/manifest/Manifest.hpp"

#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedCandleBackend.hpp"
#include "dc/gpu/DawnLineAABackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx,
                 r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

// One 13-byte ingest record (op 1 APPEND / op 2 UPDATE_RANGE) — the exact wire.
static void record(std::vector<std::uint8_t>& out, std::uint8_t op, dc::Id buf,
                   std::uint32_t offset, const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(op);
  u32(static_cast<std::uint32_t>(buf));
  u32(offset);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

// Trivial CPU rolling-mean SMA (min-periods 1) — the same helper as the fast test.
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

// The §6.2 manifest with CLIP-SPACE ranges (see file header) for direct rasterize.
static const char* kManifest = R"JSON(
{
  "version": "dc-manifest/1", "id": "candles-sma-render",
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
    { "id":"xt","type":"time","domainFrom":{"data":"ohlc","field":"t"},"range":[-0.85,0.85] },
    { "id":"yp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},"range":[0.85,-0.85] }
  ],
  "coords": { "type":"cartesian" },
  "marks": [
    { "id":"candles","type":"candle","from":"ohlc","pipeline":"instancedCandle@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},
        "yOpen":{"scale":"yp","field":"open"},"yClose":{"scale":"yp","field":"close"},
        "yHigh":{"scale":"yp","field":"high"},"yLow":{"scale":"yp","field":"low"},
        "width":{"value":0.025},
        "color":{"condition":{"value":"#26a69a"},"value":"#ef5350"} } },
    { "id":"smaLine","type":"line","from":"ohlc","pipeline":"lineAA@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},"y":{"scale":"yp","field":"sma20"},
        "color":{"value":"#ffb300"} } }
  ]
}
)JSON";

int main() {
  std::printf("=== ENC-606 Dawn render: candlestick + SMA(20) from raw OHLC ===\n");

  constexpr std::uint32_t W = 256;
  constexpr std::uint32_t H = 256;

  // ----- the RAW OHLC feed (long events; epoch-ms minute bars) ----------------
  const int N = 20;
  const std::int64_t t0 = 1700000000000LL;
  const std::int64_t dtMs = 60000LL;
  std::vector<std::int64_t> ts(N);
  std::vector<float> open(N), high(N), low(N), close(N), vol(N);
  for (int i = 0; i < N; ++i) {
    ts[i] = t0 + static_cast<std::int64_t>(i) * dtMs;
    // A clear up-trend with alternating up/down bars so the SMA visibly trails the
    // price and both candle colors appear.
    const float base = 100.0f + 1.4f * static_cast<float>(i) +
                       2.0f * std::sin(0.7f * static_cast<float>(i));
    open[i]  = base;
    close[i] = base + ((i % 2 == 0) ? 1.4f : -1.4f);  // even=up, odd=down
    high[i]  = std::max(open[i], close[i]) + 0.8f;
    low[i]   = std::min(open[i], close[i]) - 0.8f;
    vol[i]   = 1000.0f;
  }

  // ----- manifest + pivot the raw events --------------------------------------
  dc::Manifest m;
  if (!m.load(kManifest).ok()) {
    std::fprintf(stderr, "manifest load failed\n");
    return 1;
  }
  const dc::Id tid = *m.tableId("ohlc");
  dc::IngestProcessor ingest;
  auto src = dc::makeBufferByteSource(ingest);
  dc::PivotIngest pivot(m.tables(), ingest);
  pivot.setTable(tid);
  pivot.setRowKeyColumn("t");
  pivot.mapField("open", "open");
  pivot.mapField("high", "high");
  pivot.mapField("low", "low");
  pivot.mapField("close", "close");
  pivot.mapField("volume", "volume");
  for (int i = 0; i < N; ++i) {
    pivot.pushEvent(ts[i], "open",   dc::pvF32(open[i]));
    pivot.pushEvent(ts[i], "high",   dc::pvF32(high[i]));
    pivot.pushEvent(ts[i], "low",    dc::pvF32(low[i]));
    pivot.pushEvent(ts[i], "close",  dc::pvF32(close[i]));
    pivot.pushEvent(ts[i], "volume", dc::pvF32(vol[i]));
  }
  pivot.flushAll();

  // ----- trivial CPU SMA(20), patched into the pre-existing sma20 column -------
  dc::ColumnView<float> closeCol = m.tables().viewF32(tid, "close", src);
  std::vector<float> closeVec(closeCol.begin(), closeCol.end());
  std::vector<float> sma = rollingMean(closeVec, 20);
  {
    std::vector<std::uint8_t> batch;
    record(batch, 2, *m.columnBufferId("ohlc", "sma20"), 0, sma.data(),
           static_cast<std::uint32_t>(sma.size() * sizeof(float)));
    ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  }

  // ----- build(): the engine compiles data -> geometry ------------------------
  if (!m.build(src).ok()) {
    std::fprintf(stderr, "manifest build failed\n");
    return 1;
  }
  const dc::CompiledMark* candle = m.compiledMark("candles");
  const dc::CompiledMark* line = m.compiledMark("smaLine");
  if (!candle || !line) {
    std::fprintf(stderr, "compiled marks missing\n");
    return 1;
  }

  // ----- bring up Dawn + the two backends -------------------------------------
  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: set "
                 "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "to force lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  dc::DawnInstancedCandleBackend candleBe;
  dc::DawnLineAABackend lineBe;
  if (!candleBe.init(dev) || !lineBe.init(dev)) {
    std::fprintf(stderr, "backend init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &candleBe);
  backends.registerBackend(dc::DeviceKind::Dawn, &lineBe);

  // ----- wire the compiled geometry into a Scene + CpuBufferStore -------------
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // Helper: register one compiled mark (buffer bytes + geometry + draw item).
  auto wireMark = [&](const dc::CompiledMark* cm, int diId, int bufId, int geoId) {
    const auto& g = cm->result.geometry;
    char cmd[256];
    std::snprintf(cmd, sizeof(cmd),
                  R"({"cmd":"createDrawItem","id":%d,"layerId":2})", diId);
    requireOk(cp.applyJsonText(cmd), "di");
    std::snprintf(cmd, sizeof(cmd),
                  R"({"cmd":"createBuffer","id":%d,"byteLength":%zu})", bufId,
                  cm->result.bytes.size());
    requireOk(cp.applyJsonText(cmd), "buf");
    store.setCpuData(static_cast<dc::Id>(bufId), cm->result.bytes.data(),
                     static_cast<std::uint32_t>(cm->result.bytes.size()));
    std::snprintf(cmd, sizeof(cmd),
                  R"({"cmd":"createGeometry","id":%d,"vertexBufferId":%d,)"
                  R"("vertexCount":%u,"format":"%s"})",
                  geoId, bufId, g.vertexCount, toString(g.format));
    requireOk(cp.applyJsonText(cmd), "geom");
    std::snprintf(cmd, sizeof(cmd),
                  R"({"cmd":"bindDrawItem","drawItemId":%d,"pipeline":"%s",)"
                  R"("geometryId":%d})",
                  diId, cm->pipeline.c_str(), geoId);
    requireOk(cp.applyJsonText(cmd), "bind");
  };

  wireMark(candle, 3, 10, 100);
  wireMark(line, 4, 11, 101);

  // Candle up/down colors from the encode (DrawItem.colorUp/.colorDown) onto style.
  {
    char cmd[256];
    std::snprintf(cmd, sizeof(cmd),
                  R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
                  R"("colorUpR":%f,"colorUpG":%f,"colorUpB":%f,"colorUpA":1,)"
                  R"("colorDownR":%f,"colorDownG":%f,"colorDownB":%f,"colorDownA":1})",
                  candle->result.drawItem.colorUp[0],
                  candle->result.drawItem.colorUp[1],
                  candle->result.drawItem.colorUp[2],
                  candle->result.drawItem.colorDown[0],
                  candle->result.drawItem.colorDown[1],
                  candle->result.drawItem.colorDown[2]);
    requireOk(cp.applyJsonText(cmd), "candle-style");
    // SMA line color (#ffb300 amber) from the encode, drawn THICK via lineAA@1
    // (Rect4 quad expansion) so it reads as a visible amber band.
    std::snprintf(cmd, sizeof(cmd),
                  R"({"cmd":"setDrawItemStyle","drawItemId":4,)"
                  R"("colorR":%f,"colorG":%f,"colorB":%f,"colorA":1,)"
                  R"("lineWidth":3.0})",
                  line->result.drawItem.color[0], line->result.drawItem.color[1],
                  line->result.drawItem.color[2]);
    requireOk(cp.applyJsonText(cmd), "line-style");
  }

  // ----- render: candles then the SMA line ------------------------------------
  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  dev.beginRenderPass(rp);
  std::uint32_t calls = 0;
  for (int diId : {3, 4}) {
    const dc::DrawItem* di = scene.getDrawItem(diId);
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
    if (be)
      calls += be->renderDrawItem(dev, scene, store, *di, static_cast<int>(W),
                                  static_cast<int>(H)).drawCalls;
  }
  dev.endRenderPass();
  check(calls == 2, "render: 2 draw calls (candles + SMA line)");

  // ----- read back the whole framebuffer + classify pixels --------------------
  std::size_t greenPx = 0, redPx = 0, amberPx = 0, litPx = 0;
  for (std::uint32_t y = 0; y < H; ++y) {
    for (std::uint32_t x = 0; x < W; ++x) {
      std::uint8_t p[4];
      dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), p);
      const int r = p[0], g = p[1], b = p[2];
      if (r > 24 || g > 24 || b > 24) ++litPx;
      // up candle #26a69a ~ (38,166,154): green-dominant teal — green is the top
      // channel, red is low, and (being a teal) blue is high but stays below green.
      if (g > 110 && r < 90 && g > r + 40 && g > b + 8) ++greenPx;
      // down candle #ef5350 ~ (239,83,80): red-dominant.
      else if (r > 150 && g < 130 && b < 130 && r > g + 50 && r > b + 50) ++redPx;
      // SMA line #ffb300 ~ (255,179,0): red+green high, blue ~0.
      else if (r > 180 && g > 110 && g < 215 && b < 70) ++amberPx;
    }
  }
  std::printf("  pixels: lit=%zu green(up)=%zu red(down)=%zu amber(sma)=%zu\n",
              litPx, greenPx, redPx, amberPx);

  // NON-BLANK + PLAUSIBLE: real coverage, both candle colors, and the SMA line.
  check(litPx > 400, "plausible: chart is non-blank (substantial lit coverage)");
  check(greenPx > 30, "plausible: UP candles rendered (green pixels)");
  check(redPx > 30, "plausible: DOWN candles rendered (red pixels)");
  check(amberPx > 20, "plausible: SMA(20) line rendered (amber pixels)");
  check(greenPx > 0 && redPx > 0 && amberPx > 0,
        "plausible: all three encodings present (up + down candles + SMA line)");

  std::printf("=== ENC-606 Dawn render: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
