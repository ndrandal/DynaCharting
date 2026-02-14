// Live rendering server for DynaCharting
// Reads JSON commands from stdin, writes RGBA frames to stdout.
// Protocol:
//   stdin:  newline-delimited JSON (mouse, scroll, key, resize)
//   stdout: "TEXT" + uint32 jsonLen + JSON,  then  "FRME" + uint32 w + uint32 h + RGBA pixels

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/VolumeRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/layout/LayoutManager.hpp"
#include "dc/style/Theme.hpp"
#include "dc/math/NiceTicks.hpp"
#include "dc/math/NiceTimeTicks.hpp"
#include "dc/math/TimeFormat.hpp"
#include "dc/math/Normalize.hpp"

#include <rapidjson/document.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string readLine() {
  std::string line;
  int c;
  while ((c = std::fgetc(stdin)) != EOF && c != '\n') {
    line += static_cast<char>(c);
  }
  if (c == EOF && line.empty()) return {};
  return line;
}

static void writeFrame(const std::uint8_t* pixels, int w, int h) {
  std::fwrite("FRME", 1, 4, stdout);
  std::uint32_t width  = static_cast<std::uint32_t>(w);
  std::uint32_t height = static_cast<std::uint32_t>(h);
  std::fwrite(&width, 4, 1, stdout);
  std::fwrite(&height, 4, 1, stdout);
  // OSMesa readPixels is bottom-up; write top-down
  for (int y = h - 1; y >= 0; y--) {
    std::fwrite(pixels + y * w * 4, 1, static_cast<std::size_t>(w) * 4, stdout);
  }
  std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// Write text overlay for HTML rendering (sent before each FRME)
// ---------------------------------------------------------------------------
static void writeTextOverlay(
    const dc::Viewport& priceVp, const dc::Viewport& volVp,
    const dc::AxisRecipeConfig& priceAxisCfg,
    const dc::AxisRecipeConfig& volAxisCfg,
    int W, int H) {

  std::string json = R"({"fontSize":)";
  int fontPx = 13;
  json += std::to_string(fontPx);

  // Label color from theme (convert float RGBA to hex)
  auto toHex = [](const float c[4]) {
    char buf[10];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
      static_cast<int>(c[0] * 255.0f),
      static_cast<int>(c[1] * 255.0f),
      static_cast<int>(c[2] * 255.0f));
    return std::string(buf);
  };
  json += R"(,"color":")" + toHex(priceAxisCfg.labelColor) + R"(","labels":[)";

  bool first = true;
  auto addLabel = [&](float clipX, float clipY, const char* text, const char* align) {
    float px = (clipX + 1.0f) / 2.0f * static_cast<float>(W);
    float py = (1.0f - clipY) / 2.0f * static_cast<float>(H);
    if (!first) json += ",";
    first = false;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"x":%.1f,"y":%.1f,"t":"%s","a":"%s"})",
      static_cast<double>(px), static_cast<double>(py), text, align);
    json += buf;
  };

  // ---- Price pane Y-axis labels ----
  auto pdr = priceVp.dataRange();
  auto priceClip = priceVp.clipRegion();
  dc::TickSet yTicks = dc::computeNiceTicks(
    static_cast<float>(pdr.yMin), static_cast<float>(pdr.yMax), 5);
  const char* yFmt = dc::AxisRecipe::chooseFormat(yTicks.step);
  for (float val : yTicks.values) {
    float clipY = dc::normalizeToClip(val,
      static_cast<float>(pdr.yMin), static_cast<float>(pdr.yMax),
      priceClip.clipYMin, priceClip.clipYMax);
    if (clipY < priceClip.clipYMin || clipY > priceClip.clipYMax) continue;
    char buf[32];
    std::snprintf(buf, sizeof(buf), yFmt, static_cast<double>(val));
    addLabel(priceAxisCfg.yAxisClipX + 0.01f, clipY, buf, "l");
  }

  // ---- Price pane X-axis labels (time) ----
  dc::TimeTickSet xTicks = dc::computeNiceTimeTicks(
    static_cast<float>(pdr.xMin), static_cast<float>(pdr.xMax), 5);
  for (float val : xTicks.values) {
    float clipX = dc::normalizeToClip(val,
      static_cast<float>(pdr.xMin), static_cast<float>(pdr.xMax),
      priceClip.clipXMin, priceAxisCfg.yAxisClipX);
    if (clipX < priceClip.clipXMin || clipX > priceAxisCfg.yAxisClipX) continue;
    const char* timeFmt = dc::chooseTimeFormat(xTicks.stepSeconds);
    std::string labelStr = dc::formatTimestamp(val, timeFmt, priceAxisCfg.useUTC);
    addLabel(clipX, priceAxisCfg.xAxisClipY - 0.04f, labelStr.c_str(), "c");
  }

  // ---- Volume pane Y-axis labels ----
  auto vdr = volVp.dataRange();
  auto volClip = volVp.clipRegion();
  dc::TickSet volYTicks = dc::computeNiceTicks(
    static_cast<float>(vdr.yMin), static_cast<float>(vdr.yMax), 5);
  const char* volYFmt = dc::AxisRecipe::chooseFormat(volYTicks.step);
  for (float val : volYTicks.values) {
    float clipY = dc::normalizeToClip(val,
      static_cast<float>(vdr.yMin), static_cast<float>(vdr.yMax),
      volClip.clipYMin, volClip.clipYMax);
    if (clipY < volClip.clipYMin || clipY > volClip.clipYMax) continue;
    char buf[32];
    std::snprintf(buf, sizeof(buf), volYFmt, static_cast<double>(val));
    addLabel(volAxisCfg.yAxisClipX + 0.01f, clipY, buf, "l");
  }

  json += "]}";

  // Write TEXT message
  std::fwrite("TEXT", 1, 4, stdout);
  std::uint32_t len = static_cast<std::uint32_t>(json.size());
  std::fwrite(&len, 4, 1, stdout);
  std::fwrite(json.data(), 1, json.size(), stdout);
}

// Simple LCG random number generator
struct Rng {
  std::uint32_t seed{42};
  float next() {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  }
};

// ---------------------------------------------------------------------------
// Sync a viewport transform to the scene via CommandProcessor
// ---------------------------------------------------------------------------
static void syncTransform(dc::CommandProcessor& cp, dc::Id transformId,
                          const dc::Viewport& vp) {
  auto tp = vp.computeTransformParams();
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setTransform","id":%u,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
    static_cast<unsigned>(transformId),
    static_cast<double>(tp.sx), static_cast<double>(tp.sy),
    static_cast<double>(tp.tx), static_cast<double>(tp.ty));
  cp.applyJsonText(buf);
}

// ---------------------------------------------------------------------------
// Upload helpers
// ---------------------------------------------------------------------------
static void uploadBuf(dc::IngestProcessor& ingest, dc::Id bufId,
                      const std::vector<float>& data) {
  ingest.ensureBuffer(bufId);
  ingest.setBufferData(bufId,
    reinterpret_cast<const std::uint8_t*>(data.data()),
    static_cast<std::uint32_t>(data.size() * sizeof(float)));
}

static void uploadBuf(dc::IngestProcessor& ingest, dc::Id bufId,
                      const float* data, std::uint32_t bytes) {
  ingest.ensureBuffer(bufId);
  ingest.setBufferData(bufId,
    reinterpret_cast<const std::uint8_t*>(data), bytes);
}

static void setVertexCount(dc::CommandProcessor& cp, dc::Id geomId,
                           std::uint32_t count) {
  char buf[128];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%u})",
    static_cast<unsigned>(geomId), count);
  cp.applyJsonText(buf);
}

static void syncGpuBuf(dc::IngestProcessor& ingest,
                       dc::GpuBufferManager& gpuBufs, dc::Id bufId) {
  const auto* data = ingest.getBufferData(bufId);
  auto size = ingest.getBufferSize(bufId);
  if (data && size > 0) gpuBufs.setCpuData(bufId, data, size);
}

// ---------------------------------------------------------------------------
// Recompute axis data and upload to both ingest and GPU
// ---------------------------------------------------------------------------
static void recomputeAndUploadAxis(
    const dc::AxisRecipe& axisRecipe,
    const dc::GlyphAtlas& atlas,
    dc::IngestProcessor& ingest,
    dc::CommandProcessor& cp,
    dc::GpuBufferManager& gpuBufs,
    float yMin, float yMax, float xMin, float xMax,
    float clipYMin, float clipYMax,
    float clipXMin, float clipXMax) {

  auto axData = axisRecipe.computeAxisDataV2(atlas,
    yMin, yMax, xMin, xMax,
    clipYMin, clipYMax,
    clipXMin, clipXMax,
    48.0f, 0.08f);

  // Upload all axis sub-buffers to ingest + set vertex counts
  uploadBuf(ingest, axisRecipe.yTickBufferId(), axData.yTickVerts);
  setVertexCount(cp, axisRecipe.yTickGeomId(), axData.yTickVertexCount);
  uploadBuf(ingest, axisRecipe.xTickBufferId(), axData.xTickVerts);
  setVertexCount(cp, axisRecipe.xTickGeomId(), axData.xTickVertexCount);
  uploadBuf(ingest, axisRecipe.labelBufferId(), axData.labelInstances);
  setVertexCount(cp, axisRecipe.labelGeomId(), axData.labelGlyphCount);

  if (axisRecipe.config().enableGrid) {
    uploadBuf(ingest, axisRecipe.hGridBufferId(), axData.hGridVerts);
    setVertexCount(cp, axisRecipe.hGridGeomId(), axData.hGridLineCount);
    uploadBuf(ingest, axisRecipe.vGridBufferId(), axData.vGridVerts);
    setVertexCount(cp, axisRecipe.vGridGeomId(), axData.vGridLineCount);
  }

  if (axisRecipe.config().enableAALines) {
    uploadBuf(ingest, axisRecipe.yTickAABufferId(), axData.yTickAAVerts);
    setVertexCount(cp, axisRecipe.yTickAAGeomId(), axData.yTickAAVertexCount);
    uploadBuf(ingest, axisRecipe.xTickAABufferId(), axData.xTickAAVerts);
    setVertexCount(cp, axisRecipe.xTickAAGeomId(), axData.xTickAAVertexCount);
  }

  if (axisRecipe.config().enableSpine) {
    uploadBuf(ingest, axisRecipe.spineBufferId(), axData.spineVerts);
    setVertexCount(cp, axisRecipe.spineGeomId(), axData.spineLineCount);
  }

  // Sync all axis buffers to GPU
  syncGpuBuf(ingest, gpuBufs, axisRecipe.yTickBufferId());
  syncGpuBuf(ingest, gpuBufs, axisRecipe.xTickBufferId());
  syncGpuBuf(ingest, gpuBufs, axisRecipe.labelBufferId());

  if (axisRecipe.config().enableGrid) {
    syncGpuBuf(ingest, gpuBufs, axisRecipe.hGridBufferId());
    syncGpuBuf(ingest, gpuBufs, axisRecipe.vGridBufferId());
  }
  if (axisRecipe.config().enableAALines) {
    syncGpuBuf(ingest, gpuBufs, axisRecipe.yTickAABufferId());
    syncGpuBuf(ingest, gpuBufs, axisRecipe.xTickAABufferId());
  }
  if (axisRecipe.config().enableSpine) {
    syncGpuBuf(ingest, gpuBufs, axisRecipe.spineBufferId());
  }
}

// Sync all known ingest buffers to GPU (used after resize / context recreate)
static void syncAllBuffers(
    dc::IngestProcessor& ingest,
    dc::GpuBufferManager& gpuBufs,
    const dc::CandleRecipe& candleRecipe,
    const dc::VolumeRecipe& volRecipe,
    const dc::AxisRecipe& priceAxisRecipe,
    const dc::AxisRecipe& volAxisRecipe) {

  syncGpuBuf(ingest, gpuBufs, candleRecipe.bufferId());
  syncGpuBuf(ingest, gpuBufs, volRecipe.bufferId());

  // Price axis buffers
  syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.yTickBufferId());
  syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.xTickBufferId());
  syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.labelBufferId());
  if (priceAxisRecipe.config().enableGrid) {
    syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.hGridBufferId());
    syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.vGridBufferId());
  }
  if (priceAxisRecipe.config().enableAALines) {
    syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.yTickAABufferId());
    syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.xTickAABufferId());
  }
  if (priceAxisRecipe.config().enableSpine) {
    syncGpuBuf(ingest, gpuBufs, priceAxisRecipe.spineBufferId());
  }

  // Volume axis buffers
  syncGpuBuf(ingest, gpuBufs, volAxisRecipe.yTickBufferId());
  syncGpuBuf(ingest, gpuBufs, volAxisRecipe.xTickBufferId());
  syncGpuBuf(ingest, gpuBufs, volAxisRecipe.labelBufferId());
  if (volAxisRecipe.config().enableGrid) {
    syncGpuBuf(ingest, gpuBufs, volAxisRecipe.hGridBufferId());
    syncGpuBuf(ingest, gpuBufs, volAxisRecipe.vGridBufferId());
  }
  if (volAxisRecipe.config().enableAALines) {
    syncGpuBuf(ingest, gpuBufs, volAxisRecipe.yTickAABufferId());
    syncGpuBuf(ingest, gpuBufs, volAxisRecipe.xTickAABufferId());
  }
  if (volAxisRecipe.config().enableSpine) {
    syncGpuBuf(ingest, gpuBufs, volAxisRecipe.spineBufferId());
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  // Redirect stderr to /dev/null to prevent debug output from corrupting protocol
  std::freopen("/dev/null", "w", stderr);

  int W = 900, H = 600;

  // ---- 1. Create OSMesa context ----
  auto ctx = std::make_unique<dc::OsMesaContext>();
  if (!ctx->init(W, H)) return 1;

  // ---- 2. Load font ----
  dc::GlyphAtlas atlas;
  bool fontOk = atlas.loadFontFile("third_party/test_font.ttf");
  if (!fontOk) {
    // Try absolute fallback
    fontOk = atlas.loadFontFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
  }
  atlas.setUseSdf(false);  // raw alpha — pixel-perfect text
  if (fontOk) atlas.ensureAscii();

  // ---- 3. Set up engine ----
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  if (fontOk) cp.setGlyphAtlas(&atlas);

  // ---- 4. Create panes ----
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createPane","id":2})");

  dc::LayoutManager layout;
  layout.addPane(1, 0.7f);
  layout.addPane(2, 0.3f);
  layout.applyLayout(cp);

  // ---- 5. Create layers ----
  // Price pane layers: grid(5), ticks(10), data(15), labels(20)
  cp.applyJsonText(R"({"cmd":"createLayer","id":5,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":15,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":1})");
  // Volume pane layers: data(30), labels(35)
  cp.applyJsonText(R"({"cmd":"createLayer","id":30,"paneId":2})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":35,"paneId":2})");

  // ---- 6. Shared data transforms ----
  cp.applyJsonText(R"({"cmd":"createTransform","id":50})");  // price
  cp.applyJsonText(R"({"cmd":"createTransform","id":51})");  // volume

  // ---- 7. Dark theme: set pane clear colors ----
  dc::Theme theme = dc::darkTheme();
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setPaneClearColor","id":1,"r":%.4g,"g":%.4g,"b":%.4g,"a":1})",
      static_cast<double>(theme.backgroundColor[0]),
      static_cast<double>(theme.backgroundColor[1]),
      static_cast<double>(theme.backgroundColor[2]));
    cp.applyJsonText(buf);
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setPaneClearColor","id":2,"r":%.4g,"g":%.4g,"b":%.4g,"a":1})",
      static_cast<double>(theme.backgroundColor[0]),
      static_cast<double>(theme.backgroundColor[1]),
      static_cast<double>(theme.backgroundColor[2]));
    cp.applyJsonText(buf);
  }

  // ---- 8. Generate candle data ----
  constexpr int CANDLE_COUNT = 100;
  float startTs = 1700000000.0f;
  float interval = 3600.0f; // 1-hour candles

  std::vector<float> candles(static_cast<std::size_t>(CANDLE_COUNT) * 6);
  std::vector<float> volumes(CANDLE_COUNT);

  Rng rng;
  float price = 100.0f;
  float priceMin = 1e9f, priceMax = -1e9f;
  float volMax = 0.0f;

  for (int i = 0; i < CANDLE_COUNT; i++) {
    float x = startTs + static_cast<float>(i) * interval;
    float open = price;
    float high = price + rng.next() * 3.0f;
    float low  = price - rng.next() * 3.0f;
    price += (rng.next() - 0.5f) * 4.0f;
    float close = price;
    float hw = interval * 0.35f;

    std::size_t base = static_cast<std::size_t>(i) * 6;
    candles[base + 0] = x;
    candles[base + 1] = open;
    candles[base + 2] = high;
    candles[base + 3] = low;
    candles[base + 4] = close;
    candles[base + 5] = hw;

    if (low < priceMin) priceMin = low;
    if (high > priceMax) priceMax = high;

    volumes[static_cast<std::size_t>(i)] = 500.0f + rng.next() * 3000.0f;
    if (volumes[static_cast<std::size_t>(i)] > volMax)
      volMax = volumes[static_cast<std::size_t>(i)];
  }

  // ---- 9. CandleRecipe ----
  dc::CandleRecipeConfig candleCfg;
  candleCfg.paneId = 1;
  candleCfg.layerId = 15;
  candleCfg.name = "BTCUSD";
  candleCfg.createTransform = false;
  candleCfg.colorUp[0]   = theme.candleUp[0];
  candleCfg.colorUp[1]   = theme.candleUp[1];
  candleCfg.colorUp[2]   = theme.candleUp[2];
  candleCfg.colorUp[3]   = theme.candleUp[3];
  candleCfg.colorDown[0] = theme.candleDown[0];
  candleCfg.colorDown[1] = theme.candleDown[1];
  candleCfg.colorDown[2] = theme.candleDown[2];
  candleCfg.colorDown[3] = theme.candleDown[3];

  dc::CandleRecipe candleRecipe(100, candleCfg);
  auto candleBuild = candleRecipe.build();
  for (auto& cmd : candleBuild.createCommands)
    cp.applyJsonText(cmd);

  cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":)" +
    std::to_string(candleRecipe.drawItemId()) +
    R"(,"transformId":50})");

  // Upload candle data
  uploadBuf(ingest, candleRecipe.bufferId(),
            candles.data(), static_cast<std::uint32_t>(candles.size() * sizeof(float)));
  setVertexCount(cp, candleRecipe.geometryId(), CANDLE_COUNT);

  // ---- 10. VolumeRecipe ----
  dc::VolumeRecipeConfig volCfg;
  volCfg.paneId = 2;
  volCfg.layerId = 30;
  volCfg.name = "Volume";
  volCfg.createTransform = false;
  volCfg.colorUp[0]   = theme.volumeUp[0];
  volCfg.colorUp[1]   = theme.volumeUp[1];
  volCfg.colorUp[2]   = theme.volumeUp[2];
  volCfg.colorUp[3]   = theme.volumeUp[3];
  volCfg.colorDown[0] = theme.volumeDown[0];
  volCfg.colorDown[1] = theme.volumeDown[1];
  volCfg.colorDown[2] = theme.volumeDown[2];
  volCfg.colorDown[3] = theme.volumeDown[3];

  dc::VolumeRecipe volRecipe(200, volCfg);
  auto volBuild = volRecipe.build();
  for (auto& cmd : volBuild.createCommands)
    cp.applyJsonText(cmd);

  cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":)" +
    std::to_string(volRecipe.drawItemId()) +
    R"(,"transformId":51})");

  // Compute volume bars
  auto volData = volRecipe.computeVolumeBars(
    candles.data(), volumes.data(), CANDLE_COUNT, interval * 0.35f);

  uploadBuf(ingest, volRecipe.bufferId(),
            volData.candle6.data(),
            static_cast<std::uint32_t>(volData.candle6.size() * sizeof(float)));
  setVertexCount(cp, volRecipe.geometryId(), volData.barCount);

  // ---- 11. AxisRecipe (price pane) ----
  // Derive axis positions from actual pane regions
  const auto& regions = layout.regions();
  const auto& priceReg = regions[0]; // pane 1
  const auto& volReg   = regions[1]; // pane 2

  dc::AxisRecipeConfig axisCfg;
  axisCfg.paneId = 1;
  axisCfg.tickLayerId = 10;
  axisCfg.labelLayerId = 20;
  axisCfg.gridLayerId = 5;
  axisCfg.name = "priceAxis";
  axisCfg.xAxisIsTime = true;
  axisCfg.useUTC = true;
  axisCfg.enableGrid = true;
  axisCfg.enableAALines = true;
  axisCfg.enableSpine = true;
  axisCfg.yAxisClipX = priceReg.clipXMax - 0.12f;
  axisCfg.xAxisClipY = priceReg.clipYMin + 0.12f;
  // Theme colors
  axisCfg.gridColor[0] = theme.gridColor[0]; axisCfg.gridColor[1] = theme.gridColor[1];
  axisCfg.gridColor[2] = theme.gridColor[2]; axisCfg.gridColor[3] = theme.gridColor[3];
  axisCfg.tickColor[0] = theme.tickColor[0]; axisCfg.tickColor[1] = theme.tickColor[1];
  axisCfg.tickColor[2] = theme.tickColor[2]; axisCfg.tickColor[3] = theme.tickColor[3];
  axisCfg.labelColor[0] = theme.labelColor[0]; axisCfg.labelColor[1] = theme.labelColor[1];
  axisCfg.labelColor[2] = theme.labelColor[2]; axisCfg.labelColor[3] = theme.labelColor[3];
  axisCfg.gridLineWidth = theme.gridLineWidth;
  axisCfg.tickLineWidth = theme.tickLineWidth;
  axisCfg.spineLineWidth = 2.0f;

  dc::AxisRecipe priceAxisRecipe(500, axisCfg);
  auto priceAxisBuild = priceAxisRecipe.build();
  for (auto& cmd : priceAxisBuild.createCommands)
    cp.applyJsonText(cmd);

  // ---- 12. AxisRecipe (volume pane) ----
  dc::AxisRecipeConfig volAxisCfg;
  volAxisCfg.paneId = 2;
  volAxisCfg.tickLayerId = 35;
  volAxisCfg.labelLayerId = 35;
  volAxisCfg.gridLayerId = 30;
  volAxisCfg.name = "volAxis";
  volAxisCfg.xAxisIsTime = true;
  volAxisCfg.useUTC = true;
  volAxisCfg.enableGrid = true;
  volAxisCfg.enableAALines = false;
  volAxisCfg.enableSpine = true;
  volAxisCfg.yAxisClipX = volReg.clipXMax - 0.12f;
  volAxisCfg.xAxisClipY = volReg.clipYMin + 0.12f;
  volAxisCfg.gridColor[0] = theme.gridColor[0]; volAxisCfg.gridColor[1] = theme.gridColor[1];
  volAxisCfg.gridColor[2] = theme.gridColor[2]; volAxisCfg.gridColor[3] = theme.gridColor[3];
  volAxisCfg.tickColor[0] = theme.tickColor[0]; volAxisCfg.tickColor[1] = theme.tickColor[1];
  volAxisCfg.tickColor[2] = theme.tickColor[2]; volAxisCfg.tickColor[3] = theme.tickColor[3];
  volAxisCfg.labelColor[0] = theme.labelColor[0]; volAxisCfg.labelColor[1] = theme.labelColor[1];
  volAxisCfg.labelColor[2] = theme.labelColor[2]; volAxisCfg.labelColor[3] = theme.labelColor[3];
  volAxisCfg.gridLineWidth = theme.gridLineWidth;
  volAxisCfg.tickLineWidth = theme.tickLineWidth;
  volAxisCfg.spineLineWidth = 2.0f;

  dc::AxisRecipe volAxisRecipe(600, volAxisCfg);
  auto volAxisBuild = volAxisRecipe.build();
  for (auto& cmd : volAxisBuild.createCommands)
    cp.applyJsonText(cmd);

  // ---- 13. Set up viewports ----
  float dataMargin = 2.0f;
  float xMin = startTs - interval;
  float xMax = startTs + static_cast<float>(CANDLE_COUNT) * interval + interval;
  float yMin = priceMin - dataMargin;
  float yMax = priceMax + dataMargin;

  dc::Viewport priceVp;
  priceVp.setPixelViewport(W, H);
  const auto* pricePane = scene.getPane(1);
  if (pricePane) priceVp.setClipRegion(pricePane->region);
  priceVp.setDataRange(xMin, xMax, yMin, yMax);

  dc::Viewport volVp;
  volVp.setPixelViewport(W, H);
  const auto* volPane = scene.getPane(2);
  if (volPane) volVp.setClipRegion(volPane->region);
  volVp.setDataRange(xMin, xMax, 0.0, static_cast<double>(volMax) * 1.1);

  // Sync transforms
  syncTransform(cp, 50, priceVp);
  syncTransform(cp, 51, volVp);

  // ---- 14. Initialize renderer and GPU buffers ----
  auto gpuBufs = std::make_unique<dc::GpuBufferManager>();
  auto renderer = std::make_unique<dc::Renderer>();
  renderer->init();
  if (fontOk) renderer->setGlyphAtlas(&atlas);

  // ---- 15. Compute and upload axis data (grid/ticks/spine only, no GL text) ----
  if (fontOk) {
    const auto& priceClip = priceVp.clipRegion();
    recomputeAndUploadAxis(priceAxisRecipe, atlas, ingest, cp, *gpuBufs,
      static_cast<float>(yMin), static_cast<float>(yMax),
      static_cast<float>(xMin), static_cast<float>(xMax),
      priceClip.clipYMin, priceClip.clipYMax,
      priceClip.clipXMin, axisCfg.yAxisClipX);
    // Suppress GL text — HTML overlay handles labels
    setVertexCount(cp, priceAxisRecipe.labelGeomId(), 0);

    const auto& volClip = volVp.clipRegion();
    recomputeAndUploadAxis(volAxisRecipe, atlas, ingest, cp, *gpuBufs,
      0.0f, static_cast<float>(volMax * 1.1f),
      static_cast<float>(xMin), static_cast<float>(xMax),
      volClip.clipYMin, volClip.clipYMax,
      volClip.clipXMin, volAxisCfg.yAxisClipX);
    setVertexCount(cp, volAxisRecipe.labelGeomId(), 0);
  }

  // Sync candle + volume data to GPU
  syncGpuBuf(ingest, *gpuBufs, candleRecipe.bufferId());
  syncGpuBuf(ingest, *gpuBufs, volRecipe.bufferId());

  gpuBufs->uploadDirty();

  // ---- 16. Render initial frame ----
  renderer->render(scene, *gpuBufs, W, H);
  ctx->swapBuffers();
  auto pixels = ctx->readPixels();
  writeTextOverlay(priceVp, volVp, axisCfg, volAxisCfg, W, H);
  writeFrame(pixels.data(), W, H);

  // ---- 17. Input loop ----
  bool dragging = false;
  double lastMouseX = 0.0, lastMouseY = 0.0;

  while (true) {
    std::string line = readLine();
    if (line.empty()) break; // EOF

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    if (doc.HasParseError() || !doc.IsObject()) continue;
    if (!doc.HasMember("cmd")) continue;

    std::string cmd = doc["cmd"].GetString();
    bool needsRender = false;

    if (cmd == "mouse") {
      double px = doc["x"].GetDouble();
      double py = doc["y"].GetDouble();
      int buttons = doc.HasMember("buttons") ? doc["buttons"].GetInt() : 0;
      std::string type = doc.HasMember("type") ? doc["type"].GetString() : "";

      if (type == "down") {
        dragging = (buttons & 1) != 0;
        lastMouseX = px;
        lastMouseY = py;
      }
      else if (type == "up") {
        dragging = false;
      }
      else if (type == "move" && (buttons & 1) != 0) {
        // Start drag if not already dragging (handles missing "down" event)
        if (!dragging) {
          dragging = true;
          lastMouseX = px;
          lastMouseY = py;
        }
        double dx = px - lastMouseX;
        double dy = py - lastMouseY;
        lastMouseX = px;
        lastMouseY = py;

        if (std::fabs(dx) > 0.001 || std::fabs(dy) > 0.001) {
          // Determine which pane the cursor is in
          bool inPrice = priceVp.containsPixel(px, py);
          bool inVol   = volVp.containsPixel(px, py);

          // Always pan X on both viewports (linked X-axis)
          priceVp.pan(dx, 0);
          volVp.pan(dx, 0);

          // Pan Y only for the active pane
          if (inPrice) priceVp.pan(0, dy);
          else if (inVol) volVp.pan(0, dy);

          syncTransform(cp, 50, priceVp);
          syncTransform(cp, 51, volVp);
          needsRender = true;
        }
      }
    }
    else if (cmd == "scroll") {
      double px = doc["x"].GetDouble();
      double py = doc["y"].GetDouble();
      double dy = doc.HasMember("dy") ? doc["dy"].GetDouble() : 0.0;

      if (std::fabs(dy) > 0.001) {
        // Zoom: positive dy = scroll up = zoom in
        double factor = dy * 0.1;

        // Zoom both viewports at cursor position
        priceVp.zoom(factor, px, py);
        volVp.zoom(factor, px, py);

        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);
        needsRender = true;
      }
    }
    else if (cmd == "key") {
      std::string code = doc.HasMember("code") ? doc["code"].GetString() : "";

      double panAmount = 30.0; // pixels

      if (code == "ArrowRight") {
        priceVp.pan(-panAmount, 0);
        volVp.pan(-panAmount, 0);
        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);
        needsRender = true;
      }
      else if (code == "ArrowLeft") {
        priceVp.pan(panAmount, 0);
        volVp.pan(panAmount, 0);
        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);
        needsRender = true;
      }
      else if (code == "ArrowUp") {
        double cx = static_cast<double>(W) / 2.0;
        double cy = static_cast<double>(H) / 2.0;
        priceVp.zoom(0.2, cx, cy);
        volVp.zoom(0.2, cx, cy);
        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);
        needsRender = true;
      }
      else if (code == "ArrowDown") {
        double cx = static_cast<double>(W) / 2.0;
        double cy = static_cast<double>(H) / 2.0;
        priceVp.zoom(-0.2, cx, cy);
        volVp.zoom(-0.2, cx, cy);
        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);
        needsRender = true;
      }
      else if (code == "Home") {
        // Reset to initial view
        priceVp.setDataRange(xMin, xMax, yMin, yMax);
        volVp.setDataRange(xMin, xMax, 0.0, static_cast<double>(volMax) * 1.1);
        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);
        needsRender = true;
      }
    }
    else if (cmd == "resize") {
      int newW = doc.HasMember("w") ? doc["w"].GetInt() : W;
      int newH = doc.HasMember("h") ? doc["h"].GetInt() : H;

      if (newW != W || newH != H) {
        W = newW;
        H = newH;

        // Recreate OSMesa context at new size
        ctx = std::make_unique<dc::OsMesaContext>();
        if (!ctx->init(W, H)) break;

        // Re-init renderer (new GL context invalidates old programs/VAOs)
        renderer = std::make_unique<dc::Renderer>();
        renderer->init();
        if (fontOk) renderer->setGlyphAtlas(&atlas);

        // Re-create GPU buffers (old VBOs invalidated by new context)
        gpuBufs = std::make_unique<dc::GpuBufferManager>();

        // Update pixel dimensions on viewports
        priceVp.setPixelViewport(W, H);
        volVp.setPixelViewport(W, H);
        syncTransform(cp, 50, priceVp);
        syncTransform(cp, 51, volVp);

        // Re-upload all data to new GPU context
        syncAllBuffers(ingest, *gpuBufs,
                       candleRecipe, volRecipe,
                       priceAxisRecipe, volAxisRecipe);

        needsRender = true;
      }
    }

    if (needsRender) {
      // Recompute axis data on viewport change
      if (fontOk) {
        auto pdr = priceVp.dataRange();
        const auto& priceClip = priceVp.clipRegion();
        recomputeAndUploadAxis(priceAxisRecipe, atlas, ingest, cp, *gpuBufs,
          static_cast<float>(pdr.yMin), static_cast<float>(pdr.yMax),
          static_cast<float>(pdr.xMin), static_cast<float>(pdr.xMax),
          priceClip.clipYMin, priceClip.clipYMax,
          priceClip.clipXMin, axisCfg.yAxisClipX);
        setVertexCount(cp, priceAxisRecipe.labelGeomId(), 0);

        auto vdr = volVp.dataRange();
        const auto& volClip = volVp.clipRegion();
        recomputeAndUploadAxis(volAxisRecipe, atlas, ingest, cp, *gpuBufs,
          static_cast<float>(vdr.yMin), static_cast<float>(vdr.yMax),
          static_cast<float>(vdr.xMin), static_cast<float>(vdr.xMax),
          volClip.clipYMin, volClip.clipYMax,
          volClip.clipXMin, volAxisCfg.yAxisClipX);
        setVertexCount(cp, volAxisRecipe.labelGeomId(), 0);
      }

      gpuBufs->uploadDirty();
      renderer->render(scene, *gpuBufs, W, H);
      ctx->swapBuffers();
      pixels = ctx->readPixels();
      writeTextOverlay(priceVp, volVp, axisCfg, volAxisCfg, W, H);
      writeFrame(pixels.data(), W, H);
    }
  }

  return 0;
}
