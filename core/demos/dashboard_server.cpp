// Market Command Center — 5-pane dashboard demo
// Demonstrates heavy engine usage: 300 candles, SMA overlay, area fill,
// dashed level lines, volume bars, AA pie chart, performance bars,
// 6 sparklines. Interactive pan/zoom on candle+volume panes.
//
// Uses the TEXT/FRME protocol (same as live_server.cpp / showcase_server.cpp).
//
// Layout (1200x800):
//   ┌─────────────────────┬──────────────┐
//   │                     │   Pie Chart  │
//   │  300 OHLC Candles   │  (8 sectors) │
//   │  + SMA(20) overlay  ├──────────────┤
//   │  + Area fill        │  Perf Bars   │
//   │  + Level lines      │  (8 ranked)  │
//   │  + Grid / spine     │              │
//   ├─────────────────────┼──────────────┤
//   │  Volume bars (300)  │  Sparklines  │
//   │  colored by dir     │  (6 assets)  │
//   └─────────────────────┴──────────────┘

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/math/Normalize.hpp"

#include <rapidjson/document.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Protocol helpers (TEXT/FRME over stdout, commands from stdin)
// ---------------------------------------------------------------------------

static std::string readLine() {
  std::string line;
  int c;
  while ((c = std::fgetc(stdin)) != EOF && c != '\n')
    line += static_cast<char>(c);
  if (c == EOF && line.empty()) return {};
  return line;
}

static void writeFrame(const std::uint8_t* pixels, int w, int h) {
  std::fwrite("FRME", 1, 4, stdout);
  std::uint32_t width  = static_cast<std::uint32_t>(w);
  std::uint32_t height = static_cast<std::uint32_t>(h);
  std::fwrite(&width, 4, 1, stdout);
  std::fwrite(&height, 4, 1, stdout);
  for (int y = h - 1; y >= 0; y--)
    std::fwrite(pixels + y * w * 4, 1, static_cast<std::size_t>(w) * 4, stdout);
  std::fflush(stdout);
}

static void writeTextJson(const std::string& json) {
  std::fwrite("TEXT", 1, 4, stdout);
  std::uint32_t len = static_cast<std::uint32_t>(json.size());
  std::fwrite(&len, 4, 1, stdout);
  std::fwrite(json.data(), 1, json.size(), stdout);
}

// ---------------------------------------------------------------------------
// Engine helpers
// ---------------------------------------------------------------------------

static void uploadBuf(dc::IngestProcessor& ingest, dc::Id bufId,
                      const float* data, std::uint32_t bytes) {
  ingest.ensureBuffer(bufId);
  ingest.setBufferData(bufId, reinterpret_cast<const std::uint8_t*>(data), bytes);
}
static void uploadBuf(dc::IngestProcessor& ingest, dc::Id bufId,
                      const std::vector<float>& data) {
  uploadBuf(ingest, bufId, data.data(),
            static_cast<std::uint32_t>(data.size() * sizeof(float)));
}
static void syncGpuBuf(dc::IngestProcessor& ingest,
                       dc::GpuBufferManager& gpuBufs, dc::Id bufId) {
  const auto* data = ingest.getBufferData(bufId);
  auto size = ingest.getBufferSize(bufId);
  if (data && size > 0) gpuBufs.setCpuData(bufId, data, size);
}

// JSON command helpers
static void cmd(dc::CommandProcessor& cp, const char* fmt, ...) {
  char buf[512];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  cp.applyJsonText(buf);
}

// Simple LCG RNG
struct Rng {
  std::uint32_t seed{42};
  float next() {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  }
};

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

static void addHLine(std::vector<float>& v, float x0, float x1, float y) {
  v.push_back(x0); v.push_back(y);
  v.push_back(x1); v.push_back(y);
}
static void addVLine(std::vector<float>& v, float x, float y0, float y1) {
  v.push_back(x); v.push_back(y0);
  v.push_back(x); v.push_back(y1);
}

// lineAA segments from N data points
static std::vector<float> makeLineSegments(const float* xs, const float* ys, int n) {
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>((n - 1)) * 4);
  for (int i = 0; i + 1 < n; i++) {
    segs.push_back(xs[i]);  segs.push_back(ys[i]);
    segs.push_back(xs[i+1]); segs.push_back(ys[i+1]);
  }
  return segs;
}

// Area fill (triSolid) from data points to baseline
static std::vector<float> makeAreaFill(const float* xs, const float* ys,
                                        int n, float baseline) {
  std::vector<float> verts;
  verts.reserve(static_cast<std::size_t>((n - 1)) * 12);
  for (int i = 0; i + 1 < n; i++) {
    // Triangle 1
    verts.push_back(xs[i]);   verts.push_back(ys[i]);
    verts.push_back(xs[i]);   verts.push_back(baseline);
    verts.push_back(xs[i+1]); verts.push_back(ys[i+1]);
    // Triangle 2
    verts.push_back(xs[i+1]); verts.push_back(ys[i+1]);
    verts.push_back(xs[i]);   verts.push_back(baseline);
    verts.push_back(xs[i+1]); verts.push_back(baseline);
  }
  return verts;
}

// Pie slice tessellation with edge-fringe AA
static std::vector<float> tessellatePieSliceAA(
    float cx, float cy, float rx, float ry,
    float fringePixels, float startAngle, float sweepAngle,
    int segments, int viewW, int viewH) {
  std::vector<float> verts;
  float fpx = fringePixels * 2.0f / static_cast<float>(viewW);
  float fpy = fringePixels * 2.0f / static_cast<float>(viewH);
  verts.reserve(static_cast<std::size_t>(segments) * 27);

  for (int i = 0; i < segments; i++) {
    float a0 = startAngle + sweepAngle * static_cast<float>(i) / static_cast<float>(segments);
    float a1 = startAngle + sweepAngle * static_cast<float>(i + 1) / static_cast<float>(segments);
    float c0 = std::cos(a0), s0 = std::sin(a0);
    float c1 = std::cos(a1), s1 = std::sin(a1);
    float ox0 = cx + rx * c0, oy0 = cy + ry * s0;
    float ox1 = cx + rx * c1, oy1 = cy + ry * s1;
    float ix0 = ox0 - fpx * c0, iy0 = oy0 - fpy * s0;
    float ix1 = ox1 - fpx * c1, iy1 = oy1 - fpy * s1;

    // Fill triangle
    verts.push_back(cx);  verts.push_back(cy);  verts.push_back(1.0f);
    verts.push_back(ix0); verts.push_back(iy0); verts.push_back(1.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    // Fringe quad
    verts.push_back(ix0); verts.push_back(iy0); verts.push_back(1.0f);
    verts.push_back(ox0); verts.push_back(oy0); verts.push_back(0.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    verts.push_back(ox0); verts.push_back(oy0); verts.push_back(0.0f);
    verts.push_back(ox1); verts.push_back(oy1); verts.push_back(0.0f);
  }
  return verts;
}

// Donut: same as pie but with inner radius cutout (inner fill + outer fringe)
static std::vector<float> tessellateDonutSliceAA(
    float cx, float cy, float rx, float ry,
    float innerRatio, // 0..1, ratio of inner radius to outer
    float fringePixels, float startAngle, float sweepAngle,
    int segments, int viewW, int viewH) {
  std::vector<float> verts;
  float fpx = fringePixels * 2.0f / static_cast<float>(viewW);
  float fpy = fringePixels * 2.0f / static_cast<float>(viewH);
  float irx = rx * innerRatio, iry = ry * innerRatio;
  verts.reserve(static_cast<std::size_t>(segments) * 36);

  for (int i = 0; i < segments; i++) {
    float a0 = startAngle + sweepAngle * static_cast<float>(i) / static_cast<float>(segments);
    float a1 = startAngle + sweepAngle * static_cast<float>(i + 1) / static_cast<float>(segments);
    float c0 = std::cos(a0), s0 = std::sin(a0);
    float c1 = std::cos(a1), s1 = std::sin(a1);

    // Outer edge
    float ox0 = cx + rx * c0, oy0 = cy + ry * s0;
    float ox1 = cx + rx * c1, oy1 = cy + ry * s1;
    // Outer inner fringe
    float oix0 = ox0 - fpx * c0, oiy0 = oy0 - fpy * s0;
    float oix1 = ox1 - fpx * c1, oiy1 = oy1 - fpy * s1;
    // Inner edge
    float inx0 = cx + irx * c0, iny0 = cy + iry * s0;
    float inx1 = cx + irx * c1, iny1 = cy + iry * s1;
    // Inner outer fringe
    float iox0 = inx0 + fpx * c0, ioy0 = iny0 + fpy * s0;
    float iox1 = inx1 + fpx * c1, ioy1 = iny1 + fpy * s1;

    // Solid band: two triangles between inner-fringe and outer-fringe
    verts.push_back(iox0); verts.push_back(ioy0); verts.push_back(1.0f);
    verts.push_back(oix0); verts.push_back(oiy0); verts.push_back(1.0f);
    verts.push_back(iox1); verts.push_back(ioy1); verts.push_back(1.0f);
    verts.push_back(iox1); verts.push_back(ioy1); verts.push_back(1.0f);
    verts.push_back(oix0); verts.push_back(oiy0); verts.push_back(1.0f);
    verts.push_back(oix1); verts.push_back(oiy1); verts.push_back(1.0f);

    // Outer fringe
    verts.push_back(oix0); verts.push_back(oiy0); verts.push_back(1.0f);
    verts.push_back(ox0);  verts.push_back(oy0);  verts.push_back(0.0f);
    verts.push_back(oix1); verts.push_back(oiy1); verts.push_back(1.0f);
    verts.push_back(oix1); verts.push_back(oiy1); verts.push_back(1.0f);
    verts.push_back(ox0);  verts.push_back(oy0);  verts.push_back(0.0f);
    verts.push_back(ox1);  verts.push_back(oy1);  verts.push_back(0.0f);
  }
  return verts;
}

// ---------------------------------------------------------------------------
// Track all buffer IDs for re-sync on resize
// ---------------------------------------------------------------------------
static std::vector<dc::Id> g_allBufferIds;

static void trackBuf(dc::Id id) {
  g_allBufferIds.push_back(id);
}

// ---------------------------------------------------------------------------
// Text overlay builder
// ---------------------------------------------------------------------------

struct TextOverlayBuilder {
  std::string json;
  bool first = true;
  int W, H;
  float dpr = 1.0f;

  TextOverlayBuilder(int w, int h) : W(w), H(h) {
    json = R"({"fontSize":12,"color":"#9ea0a5","labels":[)";
  }

  void label(float clipX, float clipY, const char* text, const char* align,
             const char* color = nullptr, int fontSize = 0) {
    float px = (clipX + 1.0f) / 2.0f * static_cast<float>(W);
    float py = (1.0f - clipY) / 2.0f * static_cast<float>(H);
    if (!first) json += ",";
    first = false;
    char buf[512];
    if (color && fontSize > 0) {
      std::snprintf(buf, sizeof(buf),
        R"({"x":%.1f,"y":%.1f,"t":"%s","a":"%s","c":"%s","fs":%d})",
        static_cast<double>(px), static_cast<double>(py), text, align, color, fontSize);
    } else if (color) {
      std::snprintf(buf, sizeof(buf),
        R"({"x":%.1f,"y":%.1f,"t":"%s","a":"%s","c":"%s"})",
        static_cast<double>(px), static_cast<double>(py), text, align, color);
    } else {
      std::snprintf(buf, sizeof(buf),
        R"({"x":%.1f,"y":%.1f,"t":"%s","a":"%s"})",
        static_cast<double>(px), static_cast<double>(py), text, align);
    }
    json += buf;
  }

  std::string finish() { json += "]}"; return json; }
};

// Color helpers
static std::string toHex(float r, float g, float b) {
  char buf[10];
  std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
    static_cast<int>(r * 255.0f),
    static_cast<int>(g * 255.0f),
    static_cast<int>(b * 255.0f));
  return std::string(buf);
}

// ---------------------------------------------------------------------------
// Viewport helper (simplified — just data range + clip region)
// ---------------------------------------------------------------------------
struct SimpleViewport {
  double xMin, xMax, yMin, yMax;
  float clipXMin, clipXMax, clipYMin, clipYMax;
  int pixW, pixH;

  void computeTransform(float& sx, float& sy, float& tx, float& ty) const {
    sx = static_cast<float>((clipXMax - clipXMin) / (xMax - xMin));
    sy = static_cast<float>((clipYMax - clipYMin) / (yMax - yMin));
    tx = static_cast<float>(clipXMin - xMin * sx);
    ty = static_cast<float>(clipYMin - yMin * sy);
  }

  void syncTransform(dc::CommandProcessor& cp, dc::Id id) const {
    float sx, sy, tx, ty;
    computeTransform(sx, sy, tx, ty);
    cmd(cp, R"({"cmd":"setTransform","id":%u,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
        static_cast<unsigned>(id),
        static_cast<double>(sx), static_cast<double>(sy),
        static_cast<double>(tx), static_cast<double>(ty));
  }

  bool containsPixel(double px, double py) const {
    float cx = static_cast<float>(px / pixW * 2.0 - 1.0);
    float cy = static_cast<float>(1.0 - py / pixH * 2.0);
    return cx >= clipXMin && cx <= clipXMax && cy >= clipYMin && cy <= clipYMax;
  }

  void pan(double dxPx, double dyPx) {
    double dxData = -dxPx / pixW * (xMax - xMin) * 2.0 / (clipXMax - clipXMin);
    double dyData =  dyPx / pixH * (yMax - yMin) * 2.0 / (clipYMax - clipYMin);
    xMin += dxData; xMax += dxData;
    yMin += dyData; yMax += dyData;
  }

  void zoom(double factor, double px, double py) {
    double scale = std::exp(-factor * 0.1);
    double cx = (px / pixW * 2.0 - 1.0);
    double cy = (1.0 - py / pixH * 2.0);
    // Convert pixel to data space
    float sx, sy, tx, ty;
    computeTransform(sx, sy, tx, ty);
    double dataX = (cx - tx) / sx;
    double dataY = (cy - ty) / sy;
    // Zoom around data point
    xMin = dataX + (xMin - dataX) * scale;
    xMax = dataX + (xMax - dataX) * scale;
    yMin = dataY + (yMin - dataY) * scale;
    yMax = dataY + (yMax - dataY) * scale;
  }
};

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

// Pie chart colors (8 sectors — Tableau-like palette)
static const float PIE_COLORS[][3] = {
  {0.306f, 0.475f, 0.655f}, // blue
  {0.949f, 0.557f, 0.169f}, // orange
  {0.882f, 0.341f, 0.349f}, // red
  {0.463f, 0.718f, 0.698f}, // teal
  {0.349f, 0.631f, 0.310f}, // green
  {0.667f, 0.467f, 0.714f}, // purple
  {0.992f, 0.722f, 0.388f}, // gold
  {0.639f, 0.459f, 0.373f}, // brown
};

static const char* PIE_NAMES[] = {
  "Tech", "Finance", "Health", "Energy",
  "Consumer", "Industry", "Comms", "Materials"
};

static const float PIE_VALUES[] = {
  0.28f, 0.18f, 0.14f, 0.12f, 0.10f, 0.08f, 0.06f, 0.04f
};

// Performance bar data (sorted by return)
static const char* PERF_NAMES[] = {
  "Tech", "Health", "Consumer", "Comms",
  "Industry", "Materials", "Finance", "Energy"
};
static const float PERF_VALUES[] = {
  24.5f, 18.2f, 12.7f, 8.3f, 4.1f, -2.6f, -5.8f, -11.3f
};

// Sparkline asset names
static const char* SPARK_NAMES[] = {
  "BTC", "ETH", "SOL", "AAPL", "GOOGL", "TSLA"
};

constexpr int PIE_N = 8;
constexpr int PERF_N = 8;
constexpr int SPARK_N = 6;
constexpr int SPARK_PTS = 50;
constexpr int CANDLE_COUNT = 120;
constexpr int SMA_PERIOD = 20;

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  std::freopen("/dev/null", "w", stderr);

  int W = 1200, H = 800;

  // ---- 1. Create OSMesa context ----
  auto ctx = std::make_unique<dc::OsMesaContext>();
  if (!ctx->init(W, H)) return 1;

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // ========================================================================
  // PANE LAYOUT
  // ========================================================================
  // Left column: candle pane (top 70%) + volume pane (bottom 30%)
  // Right column: pie (top 40%) + perf bars (mid 25%) + sparklines (bottom 35%)

  const float LEFT_MAX = 0.28f;   // right edge of left column
  const float RIGHT_MIN = 0.33f;  // left edge of right column
  const float MID_Y = 0.03f;      // split between top and bottom of left col

  // Pane 1: Candles (large, top-left)
  cmd(cp, R"({"cmd":"createPane","id":1,"name":"Candles"})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":1,"clipYMin":%g,"clipYMax":%g,"clipXMin":%g,"clipXMax":%g})",
      MID_Y + 0.02, 0.98, -0.98, LEFT_MAX);
  cmd(cp, R"({"cmd":"setPaneClearColor","id":1,"r":0.07,"g":0.075,"b":0.09,"a":1})");

  // Pane 2: Volume (bottom-left)
  cmd(cp, R"({"cmd":"createPane","id":2,"name":"Volume"})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":2,"clipYMin":%g,"clipYMax":%g,"clipXMin":%g,"clipXMax":%g})",
      -0.98, MID_Y - 0.02, -0.98, LEFT_MAX);
  cmd(cp, R"({"cmd":"setPaneClearColor","id":2,"r":0.065,"g":0.07,"b":0.085,"a":1})");

  // Pane 3: Pie chart (top-right)
  cmd(cp, R"({"cmd":"createPane","id":3,"name":"Allocation"})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":3,"clipYMin":%g,"clipYMax":%g,"clipXMin":%g,"clipXMax":%g})",
      0.25, 0.98, RIGHT_MIN, 0.98);
  cmd(cp, R"({"cmd":"setPaneClearColor","id":3,"r":0.08,"g":0.085,"b":0.10,"a":1})");

  // Pane 4: Performance bars (mid-right)
  cmd(cp, R"({"cmd":"createPane","id":4,"name":"Performance"})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":4,"clipYMin":%g,"clipYMax":%g,"clipXMin":%g,"clipXMax":%g})",
      -0.28, 0.20, RIGHT_MIN, 0.98);
  cmd(cp, R"({"cmd":"setPaneClearColor","id":4,"r":0.075,"g":0.08,"b":0.095,"a":1})");

  // Pane 5: Sparklines (bottom-right)
  cmd(cp, R"({"cmd":"createPane","id":5,"name":"Sparklines"})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":5,"clipYMin":%g,"clipYMax":%g,"clipXMin":%g,"clipXMax":%g})",
      -0.98, -0.33, RIGHT_MIN, 0.98);
  cmd(cp, R"({"cmd":"setPaneClearColor","id":5,"r":0.07,"g":0.075,"b":0.09,"a":1})");

  // ========================================================================
  // LAYERS
  // ========================================================================
  // Pane 1 (candles): grid(10), area(11), candles(12), sma(13), levels(14)
  cmd(cp, R"({"cmd":"createLayer","id":10,"paneId":1})");
  cmd(cp, R"({"cmd":"createLayer","id":11,"paneId":1})");
  cmd(cp, R"({"cmd":"createLayer","id":12,"paneId":1})");
  cmd(cp, R"({"cmd":"createLayer","id":13,"paneId":1})");
  cmd(cp, R"({"cmd":"createLayer","id":14,"paneId":1})");
  // Pane 2 (volume): grid(20), bars(21)
  cmd(cp, R"({"cmd":"createLayer","id":20,"paneId":2})");
  cmd(cp, R"({"cmd":"createLayer","id":21,"paneId":2})");
  // Pane 3 (pie): slices(30)
  cmd(cp, R"({"cmd":"createLayer","id":30,"paneId":3})");
  // Pane 4 (perf bars): bg(40), bars(41)
  cmd(cp, R"({"cmd":"createLayer","id":40,"paneId":4})");
  cmd(cp, R"({"cmd":"createLayer","id":41,"paneId":4})");
  // Pane 5 (sparklines): bg(55), lines(56)
  cmd(cp, R"({"cmd":"createLayer","id":55,"paneId":5})");
  cmd(cp, R"({"cmd":"createLayer","id":56,"paneId":5})");

  // ========================================================================
  // TRANSFORMS (IDs must not collide with any other resource)
  // ========================================================================
  cmd(cp, R"({"cmd":"createTransform","id":60})");  // candle
  cmd(cp, R"({"cmd":"createTransform","id":61})");  // volume

  // ========================================================================
  // GENERATE DATA
  // ========================================================================
  Rng rng;

  // --- Candle data (300 candles, 1-hour intervals) ---
  float startTs = 1700000000.0f;
  float interval = 3600.0f;
  std::vector<float> candles(static_cast<std::size_t>(CANDLE_COUNT) * 6);
  std::vector<float> volumes(CANDLE_COUNT);
  float price = 42000.0f;
  float priceMin = 1e9f, priceMax = -1e9f;
  float volMax = 0.0f;

  for (int i = 0; i < CANDLE_COUNT; i++) {
    float x = startTs + static_cast<float>(i) * interval;
    float open = price;
    float change = (rng.next() - 0.48f) * 400.0f; // slight upward bias
    price += change;
    float close = price;
    float high = std::max(open, close) + rng.next() * 200.0f;
    float low  = std::min(open, close) - rng.next() * 200.0f;
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

    volumes[static_cast<std::size_t>(i)] = 1000.0f + rng.next() * 8000.0f;
    if (volumes[static_cast<std::size_t>(i)] > volMax)
      volMax = volumes[static_cast<std::size_t>(i)];
  }

  float priceMargin = (priceMax - priceMin) * 0.05f;
  priceMin -= priceMargin;
  priceMax += priceMargin;

  // --- SMA(20) ---
  std::vector<float> smaXs, smaYs;
  for (int i = SMA_PERIOD - 1; i < CANDLE_COUNT; i++) {
    float sum = 0.0f;
    for (int j = 0; j < SMA_PERIOD; j++) {
      sum += candles[static_cast<std::size_t>(i - j) * 6 + 4]; // close
    }
    smaXs.push_back(candles[static_cast<std::size_t>(i) * 6]); // x timestamp
    smaYs.push_back(sum / static_cast<float>(SMA_PERIOD));
  }
  int smaN = static_cast<int>(smaXs.size());

  // --- Volume bars (candle6 format, colored by direction) ---
  std::vector<float> volBars(static_cast<std::size_t>(CANDLE_COUNT) * 6);
  for (int i = 0; i < CANDLE_COUNT; i++) {
    std::size_t base = static_cast<std::size_t>(i) * 6;
    float x = candles[base + 0];
    float vol = volumes[static_cast<std::size_t>(i)];
    bool isUp = candles[base + 4] >= candles[base + 1]; // close >= open
    volBars[base + 0] = x;
    volBars[base + 1] = isUp ? 0.0f : vol;   // open
    volBars[base + 2] = vol;                   // high
    volBars[base + 3] = 0.0f;                  // low
    volBars[base + 4] = isUp ? vol : 0.0f;    // close
    volBars[base + 5] = interval * 0.35f;      // hw
  }

  // --- Sparkline data (6 assets, 50 points each) ---
  float sparkData[SPARK_N][SPARK_PTS];
  for (int s = 0; s < SPARK_N; s++) {
    float val = 100.0f + rng.next() * 50.0f;
    for (int p = 0; p < SPARK_PTS; p++) {
      val += (rng.next() - 0.47f) * 5.0f;
      if (val < 20.0f) val = 20.0f;
      sparkData[s][p] = val;
    }
  }

  // ========================================================================
  // PANE 1: CANDLE CHART
  // ========================================================================

  // Data ranges
  float xMin = startTs - interval * 2;
  float xMax = startTs + static_cast<float>(CANDLE_COUNT + 1) * interval;

  SimpleViewport candleVp;
  candleVp.xMin = xMin; candleVp.xMax = xMax;
  candleVp.yMin = priceMin; candleVp.yMax = priceMax;
  candleVp.clipXMin = -0.98f; candleVp.clipXMax = LEFT_MAX;
  candleVp.clipYMin = MID_Y + 0.02f; candleVp.clipYMax = 0.98f;
  candleVp.pixW = W; candleVp.pixH = H;
  candleVp.syncTransform(cp, 60);

  SimpleViewport volVp;
  volVp.xMin = xMin; volVp.xMax = xMax;
  volVp.yMin = 0; volVp.yMax = volMax * 1.15;
  volVp.clipXMin = -0.98f; volVp.clipXMax = LEFT_MAX;
  volVp.clipYMin = -0.98f; volVp.clipYMax = MID_Y - 0.02f;
  volVp.pixW = W; volVp.pixH = H;
  volVp.syncTransform(cp, 61);

  // --- Grid lines ---
  dc::Id GRID_BUF = 100, GRID_GEOM = 101, GRID_DI = 102;
  {
    std::vector<float> gridSegs;
    int numH = 5;
    for (int i = 1; i <= numH; i++) {
      float t = static_cast<float>(i) / static_cast<float>(numH + 1);
      float y = priceMin + t * (priceMax - priceMin);
      addHLine(gridSegs, static_cast<float>(xMin), static_cast<float>(xMax), y);
    }
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", GRID_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
        GRID_GEOM, GRID_BUF, static_cast<unsigned>(gridSegs.size() / 4));
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":14})", GRID_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"lineAA@1","geometryId":%u})", GRID_DI, GRID_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":60})", GRID_DI);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.25,"g":0.27,"b":0.30,"a":0.35,"lineWidth":1.0})", GRID_DI);
    uploadBuf(ingest, GRID_BUF, gridSegs);
    trackBuf(GRID_BUF);
  }

  // --- Area fill under SMA ---
  dc::Id AREA_BUF = 103, AREA_GEOM = 104, AREA_DI = 105;
  {
    auto areaVerts = makeAreaFill(smaXs.data(), smaYs.data(), smaN, priceMin);
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", AREA_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"pos2_clip","vertexCount":%u})",
        AREA_GEOM, AREA_BUF, static_cast<unsigned>(areaVerts.size() / 2));
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":11})", AREA_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"triSolid@1","geometryId":%u})", AREA_DI, AREA_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":60})", AREA_DI);
    cmd(cp, R"({"cmd":"setDrawItemColor","drawItemId":%u,"r":0.2,"g":0.4,"b":0.8,"a":0.08})", AREA_DI);
    uploadBuf(ingest, AREA_BUF, areaVerts);
    trackBuf(AREA_BUF);
  }

  // --- Candles ---
  dc::Id CNDL_BUF = 106, CNDL_GEOM = 107, CNDL_DI = 108;
  {
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", CNDL_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"candle6","vertexCount":%u})",
        CNDL_GEOM, CNDL_BUF, CANDLE_COUNT);
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":12})", CNDL_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"instancedCandle@1","geometryId":%u})", CNDL_DI, CNDL_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":60})", CNDL_DI);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"colorUpR":0.16,"colorUpG":0.78,"colorUpB":0.35,"colorUpA":1,"colorDownR":0.91,"colorDownG":0.22,"colorDownB":0.27,"colorDownA":1})",
        CNDL_DI);
    uploadBuf(ingest, CNDL_BUF, candles);
    trackBuf(CNDL_BUF);
  }

  // --- SMA line ---
  dc::Id SMA_BUF = 109, SMA_GEOM = 110, SMA_DI = 111;
  {
    auto smaSegs = makeLineSegments(smaXs.data(), smaYs.data(), smaN);
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", SMA_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
        SMA_GEOM, SMA_BUF, static_cast<unsigned>(smaSegs.size() / 4));
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":13})", SMA_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"lineAA@1","geometryId":%u})", SMA_DI, SMA_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":60})", SMA_DI);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":1.0,"g":0.65,"b":0.0,"a":0.9,"lineWidth":2.0})", SMA_DI);
    uploadBuf(ingest, SMA_BUF, smaSegs);
    trackBuf(SMA_BUF);
  }

  // --- Dashed level lines (support + resistance) ---
  dc::Id LVL_BUF = 112, LVL_GEOM = 113, LVL_DI = 114;
  {
    float support = priceMin + (priceMax - priceMin) * 0.25f;
    float resistance = priceMin + (priceMax - priceMin) * 0.78f;
    std::vector<float> lvlSegs;
    addHLine(lvlSegs, static_cast<float>(xMin), static_cast<float>(xMax), support);
    addHLine(lvlSegs, static_cast<float>(xMin), static_cast<float>(xMax), resistance);
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", LVL_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
        LVL_GEOM, LVL_BUF, static_cast<unsigned>(lvlSegs.size() / 4));
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":14})", LVL_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"lineAA@1","geometryId":%u})", LVL_DI, LVL_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":60})", LVL_DI);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.5,"g":0.5,"b":0.9,"a":0.7,"lineWidth":1.5,"dashLength":10,"gapLength":6})", LVL_DI);
    uploadBuf(ingest, LVL_BUF, lvlSegs);
    trackBuf(LVL_BUF);
  }

  // ========================================================================
  // PANE 2: VOLUME BARS
  // ========================================================================
  dc::Id VOL_BUF = 120, VOL_GEOM = 121, VOL_DI = 122;
  {
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", VOL_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"candle6","vertexCount":%u})",
        VOL_GEOM, VOL_BUF, CANDLE_COUNT);
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":21})", VOL_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"instancedCandle@1","geometryId":%u})", VOL_DI, VOL_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":61})", VOL_DI);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"colorUpR":0.16,"colorUpG":0.6,"colorUpB":0.35,"colorUpA":0.6,"colorDownR":0.7,"colorDownG":0.22,"colorDownB":0.27,"colorDownA":0.6})",
        VOL_DI);
    uploadBuf(ingest, VOL_BUF, volBars);
    trackBuf(VOL_BUF);
  }

  // Volume grid lines
  dc::Id VGRID_BUF = 123, VGRID_GEOM = 124, VGRID_DI = 125;
  {
    std::vector<float> vgridSegs;
    for (int i = 1; i <= 3; i++) {
      float y = volMax * 1.15f * static_cast<float>(i) / 4.0f;
      addHLine(vgridSegs, static_cast<float>(xMin), static_cast<float>(xMax), y);
    }
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", VGRID_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
        VGRID_GEOM, VGRID_BUF, static_cast<unsigned>(vgridSegs.size() / 4));
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":20})", VGRID_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"lineAA@1","geometryId":%u})", VGRID_DI, VGRID_GEOM);
    cmd(cp, R"({"cmd":"attachTransform","drawItemId":%u,"transformId":61})", VGRID_DI);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.25,"g":0.27,"b":0.30,"a":0.3,"lineWidth":1.0})", VGRID_DI);
    uploadBuf(ingest, VGRID_BUF, vgridSegs);
    trackBuf(VGRID_BUF);
  }

  // ========================================================================
  // PANE 3: DONUT CHART (8 sectors)
  // ========================================================================
  float pieCx = (RIGHT_MIN + 0.98f) / 2.0f;
  float pieCy = (0.25f + 0.98f) / 2.0f + 0.04f;
  float piePixelRadius = 80.0f;
  float pieRx = piePixelRadius * 2.0f / static_cast<float>(W);
  float pieRy = piePixelRadius * 2.0f / static_cast<float>(H);
  constexpr int PIE_SEGS = 64;
  constexpr float FRINGE_PX = 2.5f;

  float angle = static_cast<float>(M_PI) / 2.0f;
  for (int i = 0; i < PIE_N; i++) {
    dc::Id bufId  = 200 + static_cast<dc::Id>(i) * 3;
    dc::Id geomId = 201 + static_cast<dc::Id>(i) * 3;
    dc::Id diId   = 202 + static_cast<dc::Id>(i) * 3;

    float sweep = PIE_VALUES[i] * 2.0f * static_cast<float>(M_PI);
    auto verts = tessellateDonutSliceAA(pieCx, pieCy, pieRx, pieRy,
                                         0.55f, FRINGE_PX, angle, sweep,
                                         PIE_SEGS, W, H);
    std::uint32_t vertCount = static_cast<std::uint32_t>(verts.size() / 3);

    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", bufId);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"pos2_alpha","vertexCount":%u})",
        geomId, bufId, vertCount);
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":30})", diId);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"triAA@1","geometryId":%u})", diId, geomId);
    cmd(cp, R"({"cmd":"setDrawItemColor","drawItemId":%u,"r":%g,"g":%g,"b":%g,"a":1})",
        diId,
        static_cast<double>(PIE_COLORS[i][0]),
        static_cast<double>(PIE_COLORS[i][1]),
        static_cast<double>(PIE_COLORS[i][2]));
    uploadBuf(ingest, bufId, verts);
    trackBuf(bufId);
    angle += sweep;
  }

  // ========================================================================
  // PANE 4: PERFORMANCE BARS (horizontal, ranked)
  // ========================================================================
  // Each bar: rect from center to value, color green if positive, red if negative
  {
    float barRegionXMin = RIGHT_MIN + 0.05f;
    float barRegionXMax = 0.98f - 0.05f;
    float barRegionYMin = -0.28f + 0.08f;
    float barRegionYMax = 0.20f - 0.05f;
    float barHeight = (barRegionYMax - barRegionYMin) / static_cast<float>(PERF_N);
    float barGap = barHeight * 0.15f;
    float barH = barHeight - barGap;

    // Find max absolute value for scaling
    float maxAbs = 0.0f;
    for (int i = 0; i < PERF_N; i++) {
      float a = std::fabs(PERF_VALUES[i]);
      if (a > maxAbs) maxAbs = a;
    }

    // Center line X
    float centerX = (barRegionXMin + barRegionXMax) / 2.0f;
    float halfRange = (barRegionXMax - barRegionXMin) / 2.0f;

    // Bar backgrounds (subtle guide rectangles)
    dc::Id BG_BUF = 250, BG_GEOM = 251, BG_DI = 252;
    std::vector<float> bgRects;
    for (int i = 0; i < PERF_N; i++) {
      float top = barRegionYMax - static_cast<float>(i) * barHeight;
      float bot = top - barH;
      bgRects.push_back(barRegionXMin);
      bgRects.push_back(bot);
      bgRects.push_back(barRegionXMax);
      bgRects.push_back(top);
    }
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", BG_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
        BG_GEOM, BG_BUF, PERF_N);
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":40})", BG_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"instancedRect@1","geometryId":%u})", BG_DI, BG_GEOM);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.12,"g":0.13,"b":0.16,"a":0.6,"cornerRadius":3})", BG_DI);
    uploadBuf(ingest, BG_BUF, bgRects);
    trackBuf(BG_BUF);

    // Actual value bars
    dc::Id BAR_BUF = 253, BAR_GEOM = 254, BAR_DI_POS = 255, BAR_DI_NEG = 256;
    std::vector<float> posRects, negRects;
    for (int i = 0; i < PERF_N; i++) {
      float top = barRegionYMax - static_cast<float>(i) * barHeight;
      float bot = top - barH;
      float barEnd = centerX + (PERF_VALUES[i] / maxAbs) * halfRange * 0.85f;
      if (PERF_VALUES[i] >= 0) {
        posRects.push_back(centerX);
        posRects.push_back(bot);
        posRects.push_back(barEnd);
        posRects.push_back(top);
      } else {
        negRects.push_back(barEnd);
        negRects.push_back(bot);
        negRects.push_back(centerX);
        negRects.push_back(top);
      }
    }
    // Positive bars (green)
    if (!posRects.empty()) {
      cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", BAR_BUF);
      cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
          BAR_GEOM, BAR_BUF, static_cast<unsigned>(posRects.size() / 4));
      cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":41})", BAR_DI_POS);
      cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"instancedRect@1","geometryId":%u})", BAR_DI_POS, BAR_GEOM);
      cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.16,"g":0.72,"b":0.35,"a":0.85,"cornerRadius":3})", BAR_DI_POS);
      uploadBuf(ingest, BAR_BUF, posRects);
      trackBuf(BAR_BUF);
    }
    // Negative bars (red)
    dc::Id NEG_BUF = 257, NEG_GEOM = 258;
    if (!negRects.empty()) {
      cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", NEG_BUF);
      cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
          NEG_GEOM, NEG_BUF, static_cast<unsigned>(negRects.size() / 4));
      cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":41})", BAR_DI_NEG);
      cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"instancedRect@1","geometryId":%u})", BAR_DI_NEG, NEG_GEOM);
      cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.85,"g":0.22,"b":0.27,"a":0.85,"cornerRadius":3})", BAR_DI_NEG);
      uploadBuf(ingest, NEG_BUF, negRects);
      trackBuf(NEG_BUF);
    }

    // Center line
    dc::Id CTR_BUF = 259, CTR_GEOM = 260, CTR_DI = 261;
    std::vector<float> ctrLine;
    addVLine(ctrLine, centerX, barRegionYMin - 0.02f, barRegionYMax + 0.02f);
    cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", CTR_BUF);
    cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":1})",
        CTR_GEOM, CTR_BUF);
    cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":41})", CTR_DI);
    cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"lineAA@1","geometryId":%u})", CTR_DI, CTR_GEOM);
    cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.5,"g":0.5,"b":0.55,"a":0.5,"lineWidth":1.5})", CTR_DI);
    uploadBuf(ingest, CTR_BUF, ctrLine);
    trackBuf(CTR_BUF);
  }

  // ========================================================================
  // PANE 5: SPARKLINES (6 mini charts in a 2x3 grid)
  // ========================================================================
  {
    float spRegionXMin = RIGHT_MIN + 0.02f;
    float spRegionXMax = 0.98f - 0.02f;
    float spRegionYMin = -0.98f + 0.04f;
    float spRegionYMax = -0.33f - 0.04f;
    int cols = 3, rows = 2;
    float cellW = (spRegionXMax - spRegionXMin) / static_cast<float>(cols);
    float cellH = (spRegionYMax - spRegionYMin) / static_cast<float>(rows);
    float cellPad = 0.015f;

    for (int s = 0; s < SPARK_N; s++) {
      int col = s % cols, row = s / cols;
      float cxMin = spRegionXMin + static_cast<float>(col) * cellW + cellPad;
      float cxMax = spRegionXMin + static_cast<float>(col + 1) * cellW - cellPad;
      float cyMin = spRegionYMax - static_cast<float>(row + 1) * cellH + cellPad;
      float cyMax = spRegionYMax - static_cast<float>(row) * cellH - cellPad;

      // Background rectangle
      dc::Id bgBuf  = 300 + static_cast<dc::Id>(s) * 10;
      dc::Id bgGeom = 301 + static_cast<dc::Id>(s) * 10;
      dc::Id bgDi   = 302 + static_cast<dc::Id>(s) * 10;
      std::vector<float> bgRect = { cxMin, cyMin, cxMax, cyMax };
      cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", bgBuf);
      cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":1})",
          bgGeom, bgBuf);
      cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":55})", bgDi);
      cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"instancedRect@1","geometryId":%u})", bgDi, bgGeom);
      cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.10,"g":0.11,"b":0.13,"a":0.7,"cornerRadius":4})", bgDi);
      uploadBuf(ingest, bgBuf, bgRect);
      trackBuf(bgBuf);

      // Sparkline data → line segments in clip space (pre-transformed)
      dc::Id lineBuf  = 303 + static_cast<dc::Id>(s) * 10;
      dc::Id lineGeom = 304 + static_cast<dc::Id>(s) * 10;
      dc::Id lineDi   = 305 + static_cast<dc::Id>(s) * 10;

      // Find min/max for this sparkline
      float sMin = sparkData[s][0], sMax = sparkData[s][0];
      for (int p = 1; p < SPARK_PTS; p++) {
        if (sparkData[s][p] < sMin) sMin = sparkData[s][p];
        if (sparkData[s][p] > sMax) sMax = sparkData[s][p];
      }
      float sRange = sMax - sMin;
      if (sRange < 0.01f) sRange = 1.0f;
      float sMargin = sRange * 0.1f;
      sMin -= sMargin; sMax += sMargin;

      // Convert to clip-space line segments
      float innerPad = 0.008f;
      float lineXMin = cxMin + innerPad, lineXMax = cxMax - innerPad;
      float lineYMin = cyMin + innerPad, lineYMax = cyMax - innerPad;

      std::vector<float> lineSegs;
      for (int p = 0; p + 1 < SPARK_PTS; p++) {
        float t0 = static_cast<float>(p) / static_cast<float>(SPARK_PTS - 1);
        float t1 = static_cast<float>(p + 1) / static_cast<float>(SPARK_PTS - 1);
        float x0 = lineXMin + t0 * (lineXMax - lineXMin);
        float x1 = lineXMin + t1 * (lineXMax - lineXMin);
        float y0 = lineYMin + ((sparkData[s][p] - sMin) / (sMax - sMin)) * (lineYMax - lineYMin);
        float y1 = lineYMin + ((sparkData[s][p + 1] - sMin) / (sMax - sMin)) * (lineYMax - lineYMin);
        lineSegs.push_back(x0); lineSegs.push_back(y0);
        lineSegs.push_back(x1); lineSegs.push_back(y1);
      }

      cmd(cp, R"({"cmd":"createBuffer","id":%u,"byteLength":0})", lineBuf);
      cmd(cp, R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"rect4","vertexCount":%u})",
          lineGeom, lineBuf, static_cast<unsigned>(lineSegs.size() / 4));
      cmd(cp, R"({"cmd":"createDrawItem","id":%u,"layerId":56})", lineDi);
      cmd(cp, R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"lineAA@1","geometryId":%u})", lineDi, lineGeom);

      // Color: green if up, red if down
      bool sparkUp = sparkData[s][SPARK_PTS - 1] > sparkData[s][0];
      if (sparkUp)
        cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.16,"g":0.78,"b":0.35,"a":0.9,"lineWidth":1.5})", lineDi);
      else
        cmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":%u,"r":0.91,"g":0.32,"b":0.27,"a":0.9,"lineWidth":1.5})", lineDi);

      uploadBuf(ingest, lineBuf, lineSegs);
      trackBuf(lineBuf);
    }
  }

  // ========================================================================
  // SYNC ALL BUFFERS TO GPU + RENDER
  // ========================================================================
  auto gpuBufs = std::make_unique<dc::GpuBufferManager>();
  auto renderer = std::make_unique<dc::Renderer>();
  renderer->init();

  for (dc::Id bid : g_allBufferIds)
    syncGpuBuf(ingest, *gpuBufs, bid);
  gpuBufs->uploadDirty();

  // ========================================================================
  // TEXT OVERLAY
  // ========================================================================
  auto buildTextOverlay = [&]() -> std::string {
    TextOverlayBuilder tb(W, H);

    // --- Pane titles ---
    tb.label(-0.35f, 0.98f + 0.015f, "BTCUSD 1H", "l", "#e8e8e8", 15);
    tb.label((RIGHT_MIN + 0.98f) / 2.0f, 0.98f + 0.015f, "Sector Allocation", "c", "#e8e8e8", 13);
    tb.label((RIGHT_MIN + 0.98f) / 2.0f, 0.22f, "YTD Performance (%)", "c", "#e8e8e8", 13);
    tb.label((RIGHT_MIN + 0.98f) / 2.0f, -0.31f, "Asset Sparklines", "c", "#e8e8e8", 13);

    // --- Candle pane Y-axis labels ---
    {
      auto pdr = candleVp;
      int numTicks = 5;
      for (int i = 0; i <= numTicks; i++) {
        float t = static_cast<float>(i) / static_cast<float>(numTicks);
        float val = static_cast<float>(pdr.yMin + t * (pdr.yMax - pdr.yMin));
        float clipY = pdr.clipYMin + t * (pdr.clipYMax - pdr.clipYMin);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(val));
        tb.label(LEFT_MAX + 0.01f, clipY, buf, "l");
      }
    }

    // --- Volume pane Y-axis labels ---
    {
      int numTicks = 3;
      for (int i = 0; i <= numTicks; i++) {
        float t = static_cast<float>(i) / static_cast<float>(numTicks);
        float val = static_cast<float>(volVp.yMin + t * (volVp.yMax - volVp.yMin));
        float clipY = volVp.clipYMin + t * (volVp.clipYMax - volVp.clipYMin);
        char buf[32];
        if (val >= 1000.0f)
          std::snprintf(buf, sizeof(buf), "%.1fK", static_cast<double>(val / 1000.0f));
        else
          std::snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(val));
        tb.label(LEFT_MAX + 0.01f, clipY, buf, "l");
      }
    }

    // --- Pie chart legend ---
    {
      float legendXStart = pieCx - 0.25f;
      for (int i = 0; i < PIE_N; i++) {
        float y = 0.25f + 0.05f + static_cast<float>(PIE_N - 1 - i) * 0.05f;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %.0f%%", PIE_NAMES[i],
                      static_cast<double>(PIE_VALUES[i] * 100.0f));
        std::string col = toHex(PIE_COLORS[i][0], PIE_COLORS[i][1], PIE_COLORS[i][2]);
        tb.label(legendXStart, y, buf, "l", col.c_str(), 11);
      }
    }

    // --- Performance bar labels ---
    {
      float barRegionYMax = 0.20f - 0.05f;
      float barRegionYMin = -0.28f + 0.08f;
      float barHeight = (barRegionYMax - barRegionYMin) / static_cast<float>(PERF_N);
      float barGap = barHeight * 0.15f;
      float barH = barHeight - barGap;
      for (int i = 0; i < PERF_N; i++) {
        float top = barRegionYMax - static_cast<float>(i) * barHeight;
        float mid = top - barH / 2.0f;
        // Name on left
        tb.label(RIGHT_MIN + 0.02f, mid, PERF_NAMES[i], "l", nullptr, 11);
        // Value on right
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%+.1f%%", static_cast<double>(PERF_VALUES[i]));
        const char* valCol = PERF_VALUES[i] >= 0 ? "#4caf50" : "#ef5350";
        tb.label(0.98f - 0.02f, mid, buf, "r", valCol, 11);
      }
    }

    // --- Sparkline asset names ---
    {
      float spRegionXMin = RIGHT_MIN + 0.02f;
      float spRegionXMax = 0.98f - 0.02f;
      float spRegionYMin = -0.98f + 0.04f;
      float spRegionYMax = -0.33f - 0.04f;
      int cols = 3, rows = 2;
      float cellW = (spRegionXMax - spRegionXMin) / static_cast<float>(cols);
      float cellH = (spRegionYMax - spRegionYMin) / static_cast<float>(rows);
      for (int s = 0; s < SPARK_N; s++) {
        int col = s % cols, row = s / cols;
        float cxMin = spRegionXMin + static_cast<float>(col) * cellW + 0.015f;
        float cyMax = spRegionYMax - static_cast<float>(row) * cellH - 0.015f;
        bool up = sparkData[s][SPARK_PTS - 1] > sparkData[s][0];
        float pctChange = (sparkData[s][SPARK_PTS - 1] / sparkData[s][0] - 1.0f) * 100.0f;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s %+.1f%%", SPARK_NAMES[s], static_cast<double>(pctChange));
        tb.label(cxMin + 0.005f, cyMax - 0.005f, buf, "l",
                 up ? "#4caf50" : "#ef5350", 10);
      }
    }

    return tb.finish();
  };

  // ========================================================================
  // INITIAL RENDER
  // ========================================================================
  renderer->render(scene, *gpuBufs, W, H);
  ctx->swapBuffers();
  auto pixels = ctx->readPixels();
  std::string textJson = buildTextOverlay();
  writeTextJson(textJson);
  writeFrame(pixels.data(), W, H);

  // ========================================================================
  // INPUT LOOP (pan/zoom on candle+volume panes)
  // ========================================================================
  bool dragging = false;
  double lastMouseX = 0.0, lastMouseY = 0.0;

  while (true) {
    std::string line = readLine();
    if (line.empty()) break;

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    if (doc.HasParseError() || !doc.IsObject()) continue;
    if (!doc.HasMember("cmd")) continue;

    std::string cmdStr = doc["cmd"].GetString();
    bool needsRender = false;

    if (cmdStr == "mouse") {
      double px = doc["x"].GetDouble();
      double py = doc["y"].GetDouble();
      int buttons = doc.HasMember("buttons") ? doc["buttons"].GetInt() : 0;
      std::string type = doc.HasMember("type") ? doc["type"].GetString() : "";

      if (type == "down") {
        dragging = (buttons & 1) != 0;
        lastMouseX = px; lastMouseY = py;
      } else if (type == "up") {
        dragging = false;
      } else if (type == "move" && (buttons & 1) != 0) {
        if (!dragging) { dragging = true; lastMouseX = px; lastMouseY = py; }
        double dx = px - lastMouseX;
        double dy = py - lastMouseY;
        lastMouseX = px; lastMouseY = py;

        if (std::fabs(dx) > 0.001 || std::fabs(dy) > 0.001) {
          bool inCandle = candleVp.containsPixel(px, py);
          bool inVol    = volVp.containsPixel(px, py);

          // Linked X-axis: always pan X on both
          candleVp.pan(dx, 0);
          volVp.pan(dx, 0);
          // Pan Y only for active pane
          if (inCandle) candleVp.pan(0, dy);
          else if (inVol) volVp.pan(0, dy);

          candleVp.syncTransform(cp, 60);
          volVp.syncTransform(cp, 61);
          needsRender = true;
        }
      }
    } else if (cmdStr == "scroll") {
      double px = doc["x"].GetDouble();
      double py = doc["y"].GetDouble();
      double dy = doc.HasMember("dy") ? doc["dy"].GetDouble() : 0.0;

      if (std::fabs(dy) > 0.001) {
        double factor = dy * 0.1;
        candleVp.zoom(factor, px, py);
        volVp.zoom(factor, px, py);
        candleVp.syncTransform(cp, 60);
        volVp.syncTransform(cp, 61);
        needsRender = true;
      }
    } else if (cmdStr == "key") {
      std::string code = doc.HasMember("code") ? doc["code"].GetString() : "";
      double panAmt = 30.0;
      if (code == "ArrowRight" || code == "ArrowLeft") {
        double d = (code == "ArrowRight") ? -panAmt : panAmt;
        candleVp.pan(d, 0); volVp.pan(d, 0);
        candleVp.syncTransform(cp, 60); volVp.syncTransform(cp, 61);
        needsRender = true;
      } else if (code == "ArrowUp" || code == "ArrowDown") {
        double cx = static_cast<double>(W) / 2.0;
        double cy = static_cast<double>(H) / 2.0;
        double f = (code == "ArrowUp") ? 0.2 : -0.2;
        candleVp.zoom(f, cx, cy); volVp.zoom(f, cx, cy);
        candleVp.syncTransform(cp, 60); volVp.syncTransform(cp, 61);
        needsRender = true;
      } else if (code == "Home") {
        candleVp.xMin = xMin; candleVp.xMax = xMax;
        candleVp.yMin = priceMin; candleVp.yMax = priceMax;
        volVp.xMin = xMin; volVp.xMax = xMax;
        volVp.yMin = 0; volVp.yMax = volMax * 1.15;
        candleVp.syncTransform(cp, 60); volVp.syncTransform(cp, 61);
        needsRender = true;
      }
    } else if (cmdStr == "resize") {
      int newW = doc.HasMember("w") ? doc["w"].GetInt() : W;
      int newH = doc.HasMember("h") ? doc["h"].GetInt() : H;
      if (newW != W || newH != H) {
        W = newW; H = newH;
        ctx = std::make_unique<dc::OsMesaContext>();
        if (!ctx->init(W, H)) break;
        renderer = std::make_unique<dc::Renderer>();
        renderer->init();
        gpuBufs = std::make_unique<dc::GpuBufferManager>();

        candleVp.pixW = W; candleVp.pixH = H;
        volVp.pixW = W; volVp.pixH = H;
        candleVp.syncTransform(cp, 60);
        volVp.syncTransform(cp, 61);

        // Re-tessellate pie for new aspect ratio
        {
          float newRx = piePixelRadius * 2.0f / static_cast<float>(W);
          float newRy = piePixelRadius * 2.0f / static_cast<float>(H);
          float a = static_cast<float>(M_PI) / 2.0f;
          for (int i = 0; i < PIE_N; i++) {
            dc::Id bufId = 200 + static_cast<dc::Id>(i) * 3;
            dc::Id geomId = 201 + static_cast<dc::Id>(i) * 3;
            float sweep = PIE_VALUES[i] * 2.0f * static_cast<float>(M_PI);
            auto verts = tessellateDonutSliceAA(pieCx, pieCy, newRx, newRy,
                                                 0.55f, FRINGE_PX, a, sweep,
                                                 PIE_SEGS, W, H);
            std::uint32_t vertCount = static_cast<std::uint32_t>(verts.size() / 3);
            uploadBuf(ingest, bufId, verts);
            cmd(cp, R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%u})",
                geomId, vertCount);
            a += sweep;
          }
        }

        for (dc::Id bid : g_allBufferIds)
          syncGpuBuf(ingest, *gpuBufs, bid);
        needsRender = true;
      }
    } else if (cmdStr == "render") {
      needsRender = true;
    }

    if (needsRender) {
      gpuBufs->uploadDirty();
      renderer->render(scene, *gpuBufs, W, H);
      ctx->swapBuffers();
      pixels = ctx->readPixels();
      textJson = buildTextOverlay();
      writeTextJson(textJson);
      writeFrame(pixels.data(), W, H);
    }
  }

  return 0;
}
