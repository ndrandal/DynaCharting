// D25.1: Headless showcase render validation test.
// Renders the multi-chart showcase (line + pie + table) via OSMesa,
// samples key pixels to verify rendering correctness, and writes
// a PNG for visual inspection.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/export/ChartSnapshot.hpp"
#include "dc/math/Normalize.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Minimal helpers (same as showcase_server.cpp)
// ---------------------------------------------------------------------------

static void uploadBuf(dc::IngestProcessor& ingest, dc::Id bufId,
                      const float* data, std::uint32_t bytes) {
  ingest.ensureBuffer(bufId);
  ingest.setBufferData(bufId,
    reinterpret_cast<const std::uint8_t*>(data), bytes);
}

static void uploadBuf(dc::IngestProcessor& ingest, dc::Id bufId,
                      const std::vector<float>& data) {
  uploadBuf(ingest, bufId, data.data(),
            static_cast<std::uint32_t>(data.size() * sizeof(float)));
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

struct Rng {
  std::uint32_t seed{42};
  float next() {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  }
};

// ---------------------------------------------------------------------------
// Command helpers (correct 4-step protocol)
// ---------------------------------------------------------------------------

static void cmd(dc::CommandProcessor& cp, const char* json) {
  cp.applyJsonText(json);
}

static void createDrawItem(dc::CommandProcessor& cp, dc::Id id, dc::Id layerId,
                           dc::Id geomId, const char* pipeline,
                           float r, float g, float b, float a,
                           dc::Id transformId = 0) {
  char buf[384];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"createDrawItem","id":%u,"layerId":%u})",
    static_cast<unsigned>(id), static_cast<unsigned>(layerId));
  cp.applyJsonText(buf);
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"%s","geometryId":%u})",
    static_cast<unsigned>(id), pipeline, static_cast<unsigned>(geomId));
  cp.applyJsonText(buf);
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":%u,"r":%.4g,"g":%.4g,"b":%.4g,"a":%.4g})",
    static_cast<unsigned>(id),
    static_cast<double>(r), static_cast<double>(g), static_cast<double>(b), static_cast<double>(a));
  cp.applyJsonText(buf);
  if (transformId != 0) {
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"attachTransform","drawItemId":%u,"transformId":%u})",
      static_cast<unsigned>(id), static_cast<unsigned>(transformId));
    cp.applyJsonText(buf);
  }
}

static void setDrawItemStyle(dc::CommandProcessor& cp, dc::Id id,
                             float lineWidth, float pointSize) {
  char buf[128];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemStyle","drawItemId":%u,"lineWidth":%.4g,"pointSize":%.4g})",
    static_cast<unsigned>(id),
    static_cast<double>(lineWidth), static_cast<double>(pointSize));
  cp.applyJsonText(buf);
}

// Helpers for geometry
static void addHLine(std::vector<float>& v, float x0, float x1, float y) {
  v.push_back(x0); v.push_back(y);
  v.push_back(x1); v.push_back(y);
}
static void addVLine(std::vector<float>& v, float x, float y0, float y1) {
  v.push_back(x); v.push_back(y0);
  v.push_back(x); v.push_back(y1);
}

static std::vector<float> tessellatePieSliceAA(
    float cx, float cy, float radiusX, float radiusY,
    float fringePixels,
    float startAngle, float sweepAngle, int segments,
    int viewW, int viewH) {
  std::vector<float> verts;
  float fpx = fringePixels * 2.0f / static_cast<float>(viewW);
  float fpy = fringePixels * 2.0f / static_cast<float>(viewH);
  for (int i = 0; i < segments; i++) {
    float a0 = startAngle + sweepAngle * static_cast<float>(i) / static_cast<float>(segments);
    float a1 = startAngle + sweepAngle * static_cast<float>(i + 1) / static_cast<float>(segments);
    float cos0 = std::cos(a0), sin0 = std::sin(a0);
    float cos1 = std::cos(a1), sin1 = std::sin(a1);
    float ox0 = cx + radiusX * cos0, oy0 = cy + radiusY * sin0;
    float ox1 = cx + radiusX * cos1, oy1 = cy + radiusY * sin1;
    float ix0 = ox0 - fpx * cos0, iy0 = oy0 - fpy * sin0;
    float ix1 = ox1 - fpx * cos1, iy1 = oy1 - fpy * sin1;
    // Fill
    verts.push_back(cx);  verts.push_back(cy);  verts.push_back(1.0f);
    verts.push_back(ix0); verts.push_back(iy0); verts.push_back(1.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    // Fringe T1
    verts.push_back(ix0); verts.push_back(iy0); verts.push_back(1.0f);
    verts.push_back(ox0); verts.push_back(oy0); verts.push_back(0.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    // Fringe T2
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    verts.push_back(ox0); verts.push_back(oy0); verts.push_back(0.0f);
    verts.push_back(ox1); verts.push_back(oy1); verts.push_back(0.0f);
  }
  return verts;
}

// ---------------------------------------------------------------------------
// Pixel sampling helper
// ---------------------------------------------------------------------------
struct RGBA {
  std::uint8_t r, g, b, a;
};

static RGBA samplePixel(const std::vector<std::uint8_t>& pixels, int w, int h, int px, int py) {
  // OSMesa readPixels is bottom-up, so row 0 = bottom of image.
  // But we want top-down coordinates (like screen pixels).
  int flippedY = h - 1 - py;
  int idx = (flippedY * w + px) * 4;
  return {pixels[idx], pixels[idx+1], pixels[idx+2], pixels[idx+3]};
}

static bool isClose(std::uint8_t a, std::uint8_t b, int tolerance = 20) {
  return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
}

// ---------------------------------------------------------------------------
// Main test
// ---------------------------------------------------------------------------
#define CHECK(cond, msg) do { \
  if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while(0)

int main() {
  const int W = 900, H = 600;

  // ---- Create context ----
  dc::OsMesaContext ctx;
  CHECK(ctx.init(W, H), "OSMesa init");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // ---- Panes ----
  cmd(cp, R"({"cmd":"createPane","id":1})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.025,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":-0.025})");
  cmd(cp, R"({"cmd":"setPaneClearColor","id":1,"r":0.11,"g":0.12,"b":0.14,"a":1})");

  cmd(cp, R"({"cmd":"createPane","id":2})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":2,"clipYMin":0.025,"clipYMax":0.95,"clipXMin":0.025,"clipXMax":0.95})");
  cmd(cp, R"({"cmd":"setPaneClearColor","id":2,"r":0.11,"g":0.12,"b":0.14,"a":1})");

  cmd(cp, R"({"cmd":"createPane","id":3})");
  cmd(cp, R"({"cmd":"setPaneRegion","id":3,"clipYMin":-0.95,"clipYMax":-0.025,"clipXMin":-0.95,"clipXMax":0.95})");
  cmd(cp, R"({"cmd":"setPaneClearColor","id":3,"r":0.13,"g":0.14,"b":0.16,"a":1})");

  // ---- Layers ----
  cmd(cp, R"({"cmd":"createLayer","id":10,"paneId":1})"); // line chart grid
  cmd(cp, R"({"cmd":"createLayer","id":11,"paneId":1})"); // line chart data
  cmd(cp, R"({"cmd":"createLayer","id":20,"paneId":2})"); // pie
  cmd(cp, R"({"cmd":"createLayer","id":30,"paneId":3})"); // table bg
  cmd(cp, R"({"cmd":"createLayer","id":31,"paneId":3})"); // table grid

  // ---- Transform ----
  cmd(cp, R"({"cmd":"createTransform","id":50})");

  // ---- Line chart data ----
  constexpr int N = 30;
  float xs[N], ys[N];
  Rng rng;
  float y = 50.0f;
  for (int i = 0; i < N; i++) {
    xs[i] = static_cast<float>(i);
    y += (rng.next() - 0.45f) * 8.0f;
    if (y < 10.0f) y = 10.0f;
    ys[i] = y;
  }
  float yMin = ys[0], yMax = ys[0];
  for (int i = 1; i < N; i++) {
    if (ys[i] < yMin) yMin = ys[i];
    if (ys[i] > yMax) yMax = ys[i];
  }
  float margin = (yMax - yMin) * 0.1f;
  yMin -= margin; yMax += margin;

  // Transform: data → clip
  float clipXMin = -0.87f, clipXMax = -0.065f;
  float clipYMin = 0.105f, clipYMax = 0.89f;
  {
    float sx = (clipXMax - clipXMin) / 29.0f;
    float sy = (clipYMax - clipYMin) / (yMax - yMin);
    float tx = clipXMin;
    float ty = clipYMin - sy * yMin;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setTransform","id":50,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
      static_cast<double>(sx), static_cast<double>(sy),
      static_cast<double>(tx), static_cast<double>(ty));
    cp.applyJsonText(buf);
  }

  // Line segments (lineAA@1)
  {
    std::vector<float> segs;
    for (int i = 0; i + 1 < N; i++) {
      segs.push_back(xs[i]); segs.push_back(ys[i]);
      segs.push_back(xs[i+1]); segs.push_back(ys[i+1]);
    }
    cmd(cp, R"({"cmd":"createBuffer","id":106,"byteLength":0})");
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"createGeometry","id":107,"vertexBufferId":106,"format":"rect4","vertexCount":%u})",
      static_cast<unsigned>(segs.size() / 4));
    cp.applyJsonText(buf);
    createDrawItem(cp, 108, 11, 107, "lineAA@1", 0.31f, 0.58f, 0.98f, 1.0f, 50);
    setDrawItemStyle(cp, 108, 2.5f, 1.0f);
    uploadBuf(ingest, 106, segs);
  }

  // Data points (points@1)
  {
    std::vector<float> pts;
    for (int i = 0; i < N; i++) {
      pts.push_back(xs[i]); pts.push_back(ys[i]);
    }
    cmd(cp, R"({"cmd":"createBuffer","id":109,"byteLength":0})");
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"createGeometry","id":110,"vertexBufferId":109,"format":"pos2_clip","vertexCount":%d})", N);
    cp.applyJsonText(buf);
    createDrawItem(cp, 111, 11, 110, "points@1", 0.31f, 0.58f, 0.98f, 1.0f, 50);
    setDrawItemStyle(cp, 111, 1.0f, 5.0f);
    uploadBuf(ingest, 109, pts);
  }

  // ---- Pie chart ----
  float pieValues[] = {0.35f, 0.25f, 0.20f, 0.12f, 0.08f};
  float pieColors[][3] = {
    {0.306f, 0.475f, 0.655f},
    {0.949f, 0.557f, 0.169f},
    {0.882f, 0.341f, 0.349f},
    {0.463f, 0.718f, 0.698f},
    {0.349f, 0.631f, 0.310f},
  };
  constexpr int PIE_N = 5;
  float pieCx = 0.4875f, pieCy = 0.5375f;
  float piePixelR = 110.0f;
  float pieRX = piePixelR * 2.0f / static_cast<float>(W);
  float pieRY = piePixelR * 2.0f / static_cast<float>(H);

  constexpr float FRINGE_PX = 2.5f;
  constexpr int SEGS = 64;
  float angle = static_cast<float>(M_PI) / 2.0f;
  for (int i = 0; i < PIE_N; i++) {
    dc::Id bufId  = 200 + static_cast<dc::Id>(i) * 3;
    dc::Id geomId = 201 + static_cast<dc::Id>(i) * 3;
    dc::Id diId   = 202 + static_cast<dc::Id>(i) * 3;
    float sweep = pieValues[i] * 2.0f * static_cast<float>(M_PI);
    auto verts = tessellatePieSliceAA(pieCx, pieCy, pieRX, pieRY,
                                      FRINGE_PX, angle, sweep,
                                      SEGS, W, H);
    std::uint32_t vertCount = static_cast<std::uint32_t>(SEGS) * 9;

    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"createBuffer","id":%u,"byteLength":0})", static_cast<unsigned>(bufId));
    cp.applyJsonText(buf);
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"pos2_alpha","vertexCount":%u})",
      static_cast<unsigned>(geomId), static_cast<unsigned>(bufId), vertCount);
    cp.applyJsonText(buf);
    createDrawItem(cp, diId, 20, geomId, "triAA@1",
                   pieColors[i][0], pieColors[i][1], pieColors[i][2], 1.0f);
    uploadBuf(ingest, bufId, verts);
    angle += sweep;
  }

  // ---- Table ----
  // Header background
  {
    std::vector<float> hdr = { -0.95f, -0.145f, 0.95f, -0.045f };
    cmd(cp, R"({"cmd":"createBuffer","id":300,"byteLength":0})");
    cmd(cp, R"({"cmd":"createGeometry","id":301,"vertexBufferId":300,"format":"rect4","vertexCount":1})");
    createDrawItem(cp, 302, 30, 301, "instancedRect@1", 0.18f, 0.20f, 0.25f, 1.0f);
    uploadBuf(ingest, 300, hdr);
  }

  // Table grid
  {
    std::vector<float> grid;
    addHLine(grid, -0.95f, 0.95f, -0.145f);
    addHLine(grid, -0.95f, 0.95f, -0.255f);
    addHLine(grid, -0.95f, 0.95f, -0.365f);
    for (int c = 1; c < 4; c++) {
      float x = -0.95f + static_cast<float>(c) * 0.475f;
      addVLine(grid, x, -0.95f, -0.045f);
    }
    cmd(cp, R"({"cmd":"createBuffer","id":306,"byteLength":0})");
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"createGeometry","id":307,"vertexBufferId":306,"format":"rect4","vertexCount":%u})",
      static_cast<unsigned>(grid.size() / 4));
    cp.applyJsonText(buf);
    createDrawItem(cp, 308, 31, 307, "lineAA@1", 0.25f, 0.27f, 0.30f, 0.7f);
    setDrawItemStyle(cp, 308, 1.0f, 1.0f);
    uploadBuf(ingest, 306, grid);
  }

  // ---- Sync to GPU + render ----
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  renderer.init();

  // Sync all buffers
  syncGpuBuf(ingest, gpuBufs, 106); // line segments
  syncGpuBuf(ingest, gpuBufs, 109); // points
  for (int i = 0; i < PIE_N; i++)
    syncGpuBuf(ingest, gpuBufs, 200 + static_cast<dc::Id>(i) * 3);
  syncGpuBuf(ingest, gpuBufs, 300); // table header
  syncGpuBuf(ingest, gpuBufs, 306); // table grid
  gpuBufs.uploadDirty();

  renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  auto pixels = ctx.readPixels();

  // ---- Export PNG for visual inspection ----
  dc::writePNGFlipped("showcase_test.png", pixels.data(), W, H);
  std::fprintf(stderr, "Wrote showcase_test.png (%dx%d)\n", W, H);

  // ---- Pixel sampling validation ----

  // 1. Pane 1 background (top-left, should be dark ~(28,31,36))
  {
    // Sample inside pane 1, away from any content
    // Pane 1 clip: X [-0.95, -0.025], Y [0.025, 0.95]
    // Pixel for clip(-0.5, 0.8) ≈ (225, 60)
    RGBA bg = samplePixel(pixels, W, H, 100, 50);
    CHECK(bg.r < 50 && bg.g < 50 && bg.b < 50, "Pane 1 bg should be dark");
  }

  // 2. Pane 2 background (top-right)
  {
    // Sample a corner of pane 2 away from pie
    // clip(0.9, 0.9) → pixel ≈ (855, 30)
    RGBA bg = samplePixel(pixels, W, H, 855, 30);
    CHECK(bg.r < 50 && bg.g < 50 && bg.b < 50, "Pane 2 bg should be dark");
  }

  // 3. Pie chart center should have the first slice color (blue ~(78, 121, 167))
  {
    // Pie center clip (0.4875, 0.5375) → pixel
    int px = static_cast<int>((pieCx + 1.0f) / 2.0f * W);
    int py = static_cast<int>((1.0f - pieCy) / 2.0f * H);
    RGBA pie = samplePixel(pixels, W, H, px, py);
    // Should NOT be the dark background
    CHECK(pie.r > 50 || pie.g > 50 || pie.b > 50,
          "Pie center should not be background color");
  }

  // 4. Pie chart — sample each slice at its angular midpoint
  {
    float ang = static_cast<float>(M_PI) / 2.0f;
    int slicesFound = 0;
    for (int i = 0; i < PIE_N; i++) {
      float sweep = pieValues[i] * 2.0f * static_cast<float>(M_PI);
      float midAngle = ang + sweep / 2.0f;
      float cx = pieCx + pieRX * 0.6f * std::cos(midAngle);
      float cy = pieCy + pieRY * 0.6f * std::sin(midAngle);
      int px = static_cast<int>((cx + 1.0f) / 2.0f * W);
      int py = static_cast<int>((1.0f - cy) / 2.0f * H);
      RGBA s = samplePixel(pixels, W, H, px, py);
      std::uint8_t expR = static_cast<std::uint8_t>(pieColors[i][0] * 255.0f);
      std::uint8_t expG = static_cast<std::uint8_t>(pieColors[i][1] * 255.0f);
      std::uint8_t expB = static_cast<std::uint8_t>(pieColors[i][2] * 255.0f);
      if (isClose(s.r, expR, 30) && isClose(s.g, expG, 30) && isClose(s.b, expB, 30))
        slicesFound++;
      ang += sweep;
    }
    CHECK(slicesFound >= 4, "At least 4 of 5 pie slices should have correct color");
  }

  // 5. Table header should be slightly lighter than pane 3 background
  {
    // Header clip region: Y ≈ [-0.145, -0.045], X center ≈ 0
    // clip(0, -0.1) → pixel (450, 330)
    int py = static_cast<int>((1.0f - (-0.1f)) / 2.0f * H);
    RGBA hdr = samplePixel(pixels, W, H, 450, py);
    // Pane 3 bg is (0.13, 0.14, 0.16) ≈ (33, 36, 41)
    // Header is (0.18, 0.20, 0.25) ≈ (46, 51, 64)
    CHECK(hdr.r > 35, "Table header should be lighter than pane bg");
  }

  // 6. Line chart — sample a point on the line near its midpoint
  {
    // The 15th data point: xs[15]=15, ys[15] = some value
    // Transform to clip space and then to pixel
    float sx = (clipXMax - clipXMin) / 29.0f;
    float sy = (clipYMax - clipYMin) / (yMax - yMin);
    float tx = clipXMin;
    float ty = clipYMin - sy * yMin;
    float cx = sx * xs[15] + tx;
    float cy = sy * ys[15] + ty;
    int px = static_cast<int>((cx + 1.0f) / 2.0f * W);
    int py = static_cast<int>((1.0f - cy) / 2.0f * H);
    // Sample near the data point — should be blue-ish (not background)
    RGBA pt = samplePixel(pixels, W, H, px, py);
    CHECK(pt.b > 100, "Line chart data point should be blue");
  }

  // 7. Outside all panes (overall clear color, typically black)
  {
    // Pixel at very top-left corner, outside all panes
    RGBA corner = samplePixel(pixels, W, H, 5, 5);
    CHECK(corner.r < 20 && corner.g < 20 && corner.b < 20,
          "Area outside panes should be near-black");
  }

  std::fprintf(stderr, "All D25.1 showcase validation checks passed.\n");
  return 0;
}
