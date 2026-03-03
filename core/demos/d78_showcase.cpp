// D78 — Modern Chart Styling Showcase
//
// Renders 4 theme variants of a 2-pane candle+volume chart:
// 1. Midnight theme — borders + separators
// 2. Neon theme — dashed grid + bright colors
// 3. Pastel theme — soft colors + reduced opacity grid
// 4. Bloomberg theme — classic terminal with dashed grid

#include "dc/style/Theme.hpp"
#include "dc/style/ThemeManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/BuiltinEffects.hpp"
#include "dc/gl/PostProcessPass.hpp"

#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// ---- Binary ingest helpers ----

static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

static void appendRecord(std::vector<std::uint8_t>& batch,
                          std::uint8_t op, std::uint32_t bufferId,
                          std::uint32_t offset, const void* payload, std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

static void writePPM(const char* path, int w, int h, const std::vector<std::uint8_t>& rgba) {
  std::ofstream f(path, std::ios::binary);
  f << "P6\n" << w << " " << h << "\n255\n";
  for (int y = h - 1; y >= 0; --y) {
    for (int x = 0; x < w; ++x) {
      std::size_t idx = static_cast<std::size_t>((y * w + x) * 4);
      f.put(static_cast<char>(rgba[idx + 0]));
      f.put(static_cast<char>(rgba[idx + 1]));
      f.put(static_cast<char>(rgba[idx + 2]));
    }
  }
  std::printf("Wrote %s\n", path);
}

// ---- Render a themed scene ----

static void renderThemed(const char* outputPath, const dc::Theme& theme,
                          int W, int H) {
#ifdef DC_HAS_OSMESA
  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Failed to init OSMesa for %s\n", theme.name.c_str());
    return;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  dc::GpuBufferManager gpuBuf;
  dc::Renderer renderer;
  renderer.init();

  // Set render style from theme
  dc::RenderStyle rs;
  std::memcpy(rs.paneBorderColor, theme.paneBorderColor, sizeof(rs.paneBorderColor));
  rs.paneBorderWidth = theme.paneBorderWidth;
  std::memcpy(rs.separatorColor, theme.separatorColor, sizeof(rs.separatorColor));
  rs.separatorWidth = theme.separatorWidth;
  renderer.setRenderStyle(rs);

  // Create 2 panes (price + volume)
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.0,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Volume"})");
  cp.applyJsonText(R"({"cmd":"setPaneRegion","id":2,"clipYMin":-0.95,"clipYMax":-0.08,"clipXMin":-0.95,"clipXMax":0.95})");

  // Set pane clear colors from theme
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setPaneClearColor","id":1,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    theme.backgroundColor[0], theme.backgroundColor[1],
    theme.backgroundColor[2], theme.backgroundColor[3]);
  cp.applyJsonText(buf);
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setPaneClearColor","id":2,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    theme.backgroundColor[0], theme.backgroundColor[1],
    theme.backgroundColor[2], theme.backgroundColor[3]);
  cp.applyJsonText(buf);

  // Create layers + candle draw item
  cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Data"})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":2,"name":"Volume"})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":101,"vertexBufferId":100,"format":"candle6","vertexCount":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":200,"layerId":10,"name":"Candles"})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":200,"pipeline":"instancedCandle@1","geometryId":101})");
  cp.applyJsonText(R"({"cmd":"createTransform","id":50})");
  cp.applyJsonText(R"({"cmd":"setTransform","id":50,"tx":-0.0,"ty":0.0,"sx":1.0,"sy":1.0})");
  cp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":200,"transformId":50})");

  // Volume bars
  cp.applyJsonText(R"({"cmd":"createBuffer","id":110,"byteLength":0})");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":111,"vertexBufferId":110,"format":"rect4","vertexCount":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":210,"layerId":20,"name":"VolBars"})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":210,"pipeline":"instancedRect@1","geometryId":111})");

  // Apply theme colors to candle draw item
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":200,"colorUpR":%.9g,"colorUpG":%.9g,"colorUpB":%.9g,"colorUpA":%.9g,"colorDownR":%.9g,"colorDownG":%.9g,"colorDownB":%.9g,"colorDownA":%.9g})",
    theme.candleUp[0], theme.candleUp[1], theme.candleUp[2], theme.candleUp[3],
    theme.candleDown[0], theme.candleDown[1], theme.candleDown[2], theme.candleDown[3]);
  cp.applyJsonText(buf);

  // Volume bar color
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":210,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g})",
    theme.volumeUp[0], theme.volumeUp[1], theme.volumeUp[2], theme.volumeUp[3]);
  cp.applyJsonText(buf);

  // Grid lines (horizontal) for price pane
  cp.applyJsonText(R"({"cmd":"createBuffer","id":120,"byteLength":0})");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":121,"vertexBufferId":120,"format":"rect4","vertexCount":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":220,"layerId":10,"name":"HGrid"})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":220,"pipeline":"lineAA@1","geometryId":121})");

  // Apply grid style from theme
  float gridAlpha = theme.gridColor[3] * theme.gridOpacity;
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":220,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
    theme.gridColor[0], theme.gridColor[1], theme.gridColor[2], gridAlpha,
    theme.gridLineWidth);
  cp.applyJsonText(buf);
  if (theme.gridDashLength > 0.0f) {
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":220,"dashLength":%.9g,"gapLength":%.9g})",
      theme.gridDashLength, theme.gridGapLength);
    cp.applyJsonText(buf);
  }

  // Generate candle data (10 candles)
  const int N = 10;
  float candles[N * 6];
  float volumes[N * 4];
  float hw = 0.08f;
  float xStep = 1.7f / static_cast<float>(N);
  float basePrice = 0.0f;
  for (int i = 0; i < N; ++i) {
    float cx = -0.8f + static_cast<float>(i) * xStep;
    float open = basePrice + (i % 3 == 0 ? 0.1f : -0.05f);
    float close = open + (i % 2 == 0 ? 0.15f : -0.1f);
    float high = std::max(open, close) + 0.08f;
    float low = std::min(open, close) - 0.06f;
    candles[i * 6 + 0] = cx;
    candles[i * 6 + 1] = open;
    candles[i * 6 + 2] = high;
    candles[i * 6 + 3] = low;
    candles[i * 6 + 4] = close;
    candles[i * 6 + 5] = hw * 0.5f;
    basePrice = close * 0.5f;

    // Volume bar
    float vHeight = 0.3f + (i % 3) * 0.2f;
    volumes[i * 4 + 0] = cx - hw * 0.4f;
    volumes[i * 4 + 1] = -0.9f;
    volumes[i * 4 + 2] = cx + hw * 0.4f;
    volumes[i * 4 + 3] = -0.9f + vHeight;
  }

  // Grid lines (5 horizontal)
  float gridLines[5 * 4];
  for (int i = 0; i < 5; ++i) {
    float y = -0.8f + static_cast<float>(i) * 0.4f;
    gridLines[i * 4 + 0] = -0.9f;
    gridLines[i * 4 + 1] = y;
    gridLines[i * 4 + 2] = 0.9f;
    gridLines[i * 4 + 3] = y;
  }

  // Ingest
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, candles, sizeof(candles));
  appendRecord(batch, 1, 110, 0, volumes, sizeof(volumes));
  appendRecord(batch, 1, 120, 0, gridLines, sizeof(gridLines));
  auto ir = ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

  // Update vertex counts
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":%d})", N);
  cp.applyJsonText(buf);
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setGeometryVertexCount","geometryId":111,"vertexCount":%d})", N);
  cp.applyJsonText(buf);
  cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":121,"vertexCount":5})");

  // Upload to GPU
  for (dc::Id bid : ir.touchedBufferIds) {
    gpuBuf.setCpuData(bid, ingest.getBufferData(bid), ingest.getBufferSize(bid));
  }
  gpuBuf.uploadDirty();

  // Render
  renderer.render(scene, gpuBuf, W, H);
  auto pixels = ctx.readPixels();
  writePPM(outputPath, W, H, pixels);

  std::printf("Theme '%s' rendered successfully.\n", theme.name.c_str());
#else
  (void)outputPath; (void)theme; (void)W; (void)H;
#endif
}

int main() {
#ifndef DC_HAS_OSMESA
  std::printf("D78 showcase requires OSMesa — skipping.\n");
  return 0;
#else
  const int W = 640, H = 480;

  dc::ThemeManager mgr;

  renderThemed("d78_midnight.ppm", dc::midnightTheme(), W, H);
  renderThemed("d78_neon.ppm", dc::neonTheme(), W, H);
  renderThemed("d78_pastel.ppm", dc::pastelTheme(), W, H);
  renderThemed("d78_bloomberg.ppm", dc::bloombergTheme(), W, H);

  std::printf("\nD78 showcase completed — 4 themed charts rendered.\n");
  return 0;
#endif
}
