// Multi-chart showcase: Line Chart + Pie Chart + Table
// Demonstrates that DynaCharting is a general-purpose drawing engine.
// Uses the same TEXT/FRME protocol as live_server.cpp.
//
// Layout (900x600):
//   Pane 1 (top-left):  Line chart  — lineAA + points
//   Pane 2 (top-right): Pie chart   — triAA (edge-fringe AA, one DrawItem per slice)
//   Pane 3 (bottom):    Data table   — instancedRect + lineAA

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/math/Normalize.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Helpers (same protocol as live_server)
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
  for (int y = h - 1; y >= 0; y--) {
    std::fwrite(pixels + y * w * 4, 1, static_cast<std::size_t>(w) * 4, stdout);
  }
  std::fflush(stdout);
}

static void writeTextJson(const std::string& json) {
  std::fwrite("TEXT", 1, 4, stdout);
  std::uint32_t len = static_cast<std::uint32_t>(json.size());
  std::fwrite(&len, 4, 1, stdout);
  std::fwrite(json.data(), 1, json.size(), stdout);
}

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

// Simple LCG random number generator
struct Rng {
  std::uint32_t seed{42};
  float next() {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  }
};

// ---------------------------------------------------------------------------
// Helpers: create scene resources via JSON commands
// ---------------------------------------------------------------------------
static void createPane(dc::CommandProcessor& cp, dc::Id id) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), R"({"cmd":"createPane","id":%u})", static_cast<unsigned>(id));
  cp.applyJsonText(buf);
}

static void setPaneRegion(dc::CommandProcessor& cp, dc::Id id,
                          float clipYMin, float clipYMax, float clipXMin, float clipXMax) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setPaneRegion","id":%u,"clipYMin":%.6g,"clipYMax":%.6g,"clipXMin":%.6g,"clipXMax":%.6g})",
    static_cast<unsigned>(id),
    static_cast<double>(clipYMin), static_cast<double>(clipYMax),
    static_cast<double>(clipXMin), static_cast<double>(clipXMax));
  cp.applyJsonText(buf);
}

static void setPaneClearColor(dc::CommandProcessor& cp, dc::Id id,
                              float r, float g, float b, float a) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setPaneClearColor","id":%u,"r":%.4g,"g":%.4g,"b":%.4g,"a":%.4g})",
    static_cast<unsigned>(id),
    static_cast<double>(r), static_cast<double>(g), static_cast<double>(b), static_cast<double>(a));
  cp.applyJsonText(buf);
}

static void createLayer(dc::CommandProcessor& cp, dc::Id id, dc::Id paneId) {
  char buf[128];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"createLayer","id":%u,"paneId":%u})",
    static_cast<unsigned>(id), static_cast<unsigned>(paneId));
  cp.applyJsonText(buf);
}

static void createTransform(dc::CommandProcessor& cp, dc::Id id) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), R"({"cmd":"createTransform","id":%u})", static_cast<unsigned>(id));
  cp.applyJsonText(buf);
}

static void createBuffer(dc::CommandProcessor& cp, dc::Id id) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), R"({"cmd":"createBuffer","id":%u,"byteLength":0})", static_cast<unsigned>(id));
  cp.applyJsonText(buf);
}

static void createGeometry(dc::CommandProcessor& cp, dc::Id id, dc::Id bufId,
                           const char* format, std::uint32_t vertexCount) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,"format":"%s","vertexCount":%u})",
    static_cast<unsigned>(id), static_cast<unsigned>(bufId), format, vertexCount);
  cp.applyJsonText(buf);
}

static void createDrawItem(dc::CommandProcessor& cp, dc::Id id, dc::Id layerId,
                           dc::Id geomId, const char* pipeline,
                           float r, float g, float b, float a,
                           dc::Id transformId = 0) {
  char buf[384];
  // Step 1: create the draw item (only id, layerId, name)
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"createDrawItem","id":%u,"layerId":%u})",
    static_cast<unsigned>(id), static_cast<unsigned>(layerId));
  cp.applyJsonText(buf);

  // Step 2: bind pipeline + geometry
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"bindDrawItem","drawItemId":%u,"pipeline":"%s","geometryId":%u})",
    static_cast<unsigned>(id), pipeline, static_cast<unsigned>(geomId));
  cp.applyJsonText(buf);

  // Step 3: set color
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":%u,"r":%.4g,"g":%.4g,"b":%.4g,"a":%.4g})",
    static_cast<unsigned>(id),
    static_cast<double>(r), static_cast<double>(g), static_cast<double>(b), static_cast<double>(a));
  cp.applyJsonText(buf);

  // Step 4: attach transform (if non-zero)
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

static void setTransform(dc::CommandProcessor& cp, dc::Id id,
                         float sx, float sy, float tx, float ty) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setTransform","id":%u,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
    static_cast<unsigned>(id),
    static_cast<double>(sx), static_cast<double>(sy),
    static_cast<double>(tx), static_cast<double>(ty));
  cp.applyJsonText(buf);
}

// ---------------------------------------------------------------------------
// Line-chart lineAA segments: convert N data points to (N-1) rect4 line segments
// Each rect4 = {x0, y0, x1, y1} in data space.
// ---------------------------------------------------------------------------
static std::vector<float> makeLineSegments(const float* xs, const float* ys, int n) {
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>((n - 1)) * 4);
  for (int i = 0; i + 1 < n; i++) {
    segs.push_back(xs[i]);
    segs.push_back(ys[i]);
    segs.push_back(xs[i + 1]);
    segs.push_back(ys[i + 1]);
  }
  return segs;
}

// ---------------------------------------------------------------------------
// Pie chart tessellation with edge-fringe AA (aspect-corrected).
// Uses separate radiusX/radiusY for circular appearance on non-square viewports.
// Fringe offset is computed per-vertex in screen-space pixels for uniform width.
// Returns Pos2Alpha vertices (3 floats per vertex: x, y, alpha).
// ---------------------------------------------------------------------------
static std::vector<float> tessellatePieSliceAA(
    float cx, float cy, float radiusX, float radiusY,
    float fringePixels,
    float startAngle, float sweepAngle, int segments,
    int viewW, int viewH) {
  std::vector<float> verts;
  // Per-vertex fringe offset: fringePixels converted to clip-space per axis
  float fpx = fringePixels * 2.0f / static_cast<float>(viewW);
  float fpy = fringePixels * 2.0f / static_cast<float>(viewH);
  verts.reserve(static_cast<std::size_t>(segments) * 9 * 3);

  for (int i = 0; i < segments; i++) {
    float a0 = startAngle + sweepAngle * static_cast<float>(i) / static_cast<float>(segments);
    float a1 = startAngle + sweepAngle * static_cast<float>(i + 1) / static_cast<float>(segments);
    float cos0 = std::cos(a0), sin0 = std::sin(a0);
    float cos1 = std::cos(a1), sin1 = std::sin(a1);

    // Outer vertices at full radius
    float ox0 = cx + radiusX * cos0, oy0 = cy + radiusY * sin0;
    float ox1 = cx + radiusX * cos1, oy1 = cy + radiusY * sin1;
    // Inner vertices: inset by fringePixels in screen space along the normal
    float ix0 = ox0 - fpx * cos0, iy0 = oy0 - fpy * sin0;
    float ix1 = ox1 - fpx * cos1, iy1 = oy1 - fpy * sin1;

    // Fill triangle: center(a=1) + inner0(a=1) + inner1(a=1)
    verts.push_back(cx);  verts.push_back(cy);  verts.push_back(1.0f);
    verts.push_back(ix0); verts.push_back(iy0); verts.push_back(1.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);

    // Fringe quad (2 triangles):
    verts.push_back(ix0); verts.push_back(iy0); verts.push_back(1.0f);
    verts.push_back(ox0); verts.push_back(oy0); verts.push_back(0.0f);
    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);

    verts.push_back(ix1); verts.push_back(iy1); verts.push_back(1.0f);
    verts.push_back(ox0); verts.push_back(oy0); verts.push_back(0.0f);
    verts.push_back(ox1); verts.push_back(oy1); verts.push_back(0.0f);
  }
  return verts;
}

// Horizontal line as rect4: {x0, y, x1, y}
static void addHLine(std::vector<float>& v, float x0, float x1, float y) {
  v.push_back(x0); v.push_back(y);
  v.push_back(x1); v.push_back(y);
}

// Vertical line as rect4: {x, y0, x, y1}
static void addVLine(std::vector<float>& v, float x, float y0, float y1) {
  v.push_back(x); v.push_back(y0);
  v.push_back(x); v.push_back(y1);
}

// ---------------------------------------------------------------------------
// Build text overlay JSON
// ---------------------------------------------------------------------------
static std::string buildTextOverlay(
    // Line chart labels
    const float* lineXs, const float* lineYs, int lineN,
    float lineDataXMin, float lineDataXMax, float lineDataYMin, float lineDataYMax,
    float lineClipXMin, float lineClipXMax, float lineClipYMin, float lineClipYMax,
    // Pie chart legend info
    const char* const pieNames[], const float pieValues[], const float pieColors[][3], int pieN,
    float pieCx, float pieCy, // center in clip
    float pieClipYMin,
    // Table data
    const char* const tableHeaders[], int tableCols,
    const char* const tableData[][4], int tableRows,
    float tableClipXMin, float tableClipXMax, float tableClipYMin, float tableClipYMax,
    int W, int H)
{
  std::string json = R"({"fontSize":12,"color":"#b2b5bc","labels":[)";
  bool first = true;

  auto addLabel = [&](float clipX, float clipY, const char* text, const char* align,
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
    } else {
      std::snprintf(buf, sizeof(buf),
        R"({"x":%.1f,"y":%.1f,"t":"%s","a":"%s"})",
        static_cast<double>(px), static_cast<double>(py), text, align);
    }
    json += buf;
  };

  // ---- Line chart title ----
  addLabel((lineClipXMin + lineClipXMax) / 2.0f, lineClipYMax + 0.02f,
           "Revenue (M)", "c", "#e0e0e0", 14);

  // ---- Line chart Y-axis labels ----
  {
    int numYTicks = 5;
    for (int i = 0; i <= numYTicks; i++) {
      float t = static_cast<float>(i) / static_cast<float>(numYTicks);
      float val = lineDataYMin + t * (lineDataYMax - lineDataYMin);
      float clipY = lineClipYMin + t * (lineClipYMax - lineClipYMin);
      char label[32];
      std::snprintf(label, sizeof(label), "%.0f", static_cast<double>(val));
      addLabel(lineClipXMin - 0.02f, clipY, label, "r");
    }
  }

  // ---- Line chart X-axis labels ----
  {
    int numXTicks = 5;
    for (int i = 0; i <= numXTicks; i++) {
      float t = static_cast<float>(i) / static_cast<float>(numXTicks);
      float val = lineDataXMin + t * (lineDataXMax - lineDataXMin);
      float clipX = lineClipXMin + t * (lineClipXMax - lineClipXMin);
      // Month labels
      const char* months[] = {"Jan", "Mar", "May", "Jul", "Sep", "Nov"};
      addLabel(clipX, lineClipYMin - 0.04f, months[i], "c");
    }
  }

  // ---- Pie chart title ----
  addLabel((pieCx - 0.45f + pieCx + 0.45f) / 2.0f, lineClipYMax + 0.02f,
           "Market Share", "c", "#e0e0e0", 14);

  // ---- Pie chart legend ----
  {
    auto toHex = [](float r, float g, float b) {
      char buf[10];
      std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
        static_cast<int>(r * 255.0f),
        static_cast<int>(g * 255.0f),
        static_cast<int>(b * 255.0f));
      return std::string(buf);
    };

    float legendY = pieClipYMin + 0.04f;
    float legendXStart = pieCx - 0.35f;
    for (int i = 0; i < pieN; i++) {
      float x = legendXStart;
      float y = legendY + static_cast<float>(pieN - 1 - i) * 0.065f;
      char label[64];
      std::snprintf(label, sizeof(label), "%s %.0f%%", pieNames[i],
                    static_cast<double>(pieValues[i] * 100.0f));
      std::string col = toHex(pieColors[i][0], pieColors[i][1], pieColors[i][2]);
      addLabel(x, y, label, "l", col.c_str(), 11);
    }
  }

  // ---- Table title ----
  addLabel((tableClipXMin + tableClipXMax) / 2.0f, tableClipYMax + 0.02f,
           "Market Data", "c", "#e0e0e0", 14);

  // ---- Table headers ----
  {
    float colWidth = (tableClipXMax - tableClipXMin) / static_cast<float>(tableCols);
    float headerY = tableClipYMax - 0.06f;
    for (int c = 0; c < tableCols; c++) {
      float cx = tableClipXMin + (static_cast<float>(c) + 0.5f) * colWidth;
      addLabel(cx, headerY, tableHeaders[c], "c", "#ffffff", 12);
    }
  }

  // ---- Table data rows ----
  {
    float colWidth = (tableClipXMax - tableClipXMin) / static_cast<float>(tableCols);
    float rowHeight = 0.11f;
    float startY = tableClipYMax - 0.18f;
    for (int r = 0; r < tableRows; r++) {
      float ry = startY - static_cast<float>(r) * rowHeight;
      for (int c = 0; c < tableCols; c++) {
        float cx = tableClipXMin + (static_cast<float>(c) + 0.5f) * colWidth;
        // Color change column green/red
        const char* color = nullptr;
        if (c == 2) {
          color = (tableData[r][c][0] == '+') ? "#4caf50" : "#ef5350";
        }
        if (color) {
          addLabel(cx, ry, tableData[r][c], "c", color, 11);
        } else {
          addLabel(cx, ry, tableData[r][c], "c");
        }
      }
    }
  }

  json += "]}";
  return json;
}


// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  std::freopen("/dev/null", "w", stderr);

  int W = 900, H = 600;

  // ---- 1. Create OSMesa context ----
  auto ctx = std::make_unique<dc::OsMesaContext>();
  if (!ctx->init(W, H)) return 1;

  // ---- 2. Set up engine ----
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // ---- 3. Create 3 panes with manual regions ----
  // Pane 1 (top-left, line chart)
  createPane(cp, 1);
  setPaneRegion(cp, 1, 0.025f, 0.95f, -0.95f, -0.025f);
  setPaneClearColor(cp, 1, 0.11f, 0.12f, 0.14f, 1.0f);

  // Pane 2 (top-right, pie chart)
  createPane(cp, 2);
  setPaneRegion(cp, 2, 0.025f, 0.95f, 0.025f, 0.95f);
  setPaneClearColor(cp, 2, 0.11f, 0.12f, 0.14f, 1.0f);

  // Pane 3 (bottom, table)
  createPane(cp, 3);
  setPaneRegion(cp, 3, -0.95f, -0.025f, -0.95f, 0.95f);
  setPaneClearColor(cp, 3, 0.13f, 0.14f, 0.16f, 1.0f);

  // ========================================================================
  // PANE 1: Line Chart
  // ========================================================================
  // Clip region for line chart content (with margins for axes)
  const float lc_clipXMin = -0.95f + 0.08f;  // leave room for Y labels
  const float lc_clipXMax = -0.025f - 0.04f;
  const float lc_clipYMin = 0.025f + 0.08f;   // leave room for X labels
  const float lc_clipYMax = 0.95f - 0.06f;

  // Layers for pane 1
  createLayer(cp, 10, 1); // data (lines + points)
  createLayer(cp, 11, 1); // grid + spine (above data)

  // Transform for line chart data→clip mapping
  createTransform(cp, 50);

  // Generate line chart data: 30-point random walk
  constexpr int LINE_N = 30;
  float lineXs[LINE_N], lineYs[LINE_N];
  Rng rng;
  {
    float y = 50.0f;
    for (int i = 0; i < LINE_N; i++) {
      lineXs[i] = static_cast<float>(i);
      y += (rng.next() - 0.45f) * 8.0f; // slight upward bias
      if (y < 10.0f) y = 10.0f;
      lineYs[i] = y;
    }
  }

  // Compute data range
  float lc_dataXMin = 0.0f, lc_dataXMax = static_cast<float>(LINE_N - 1);
  float lc_dataYMin = lineYs[0], lc_dataYMax = lineYs[0];
  for (int i = 1; i < LINE_N; i++) {
    if (lineYs[i] < lc_dataYMin) lc_dataYMin = lineYs[i];
    if (lineYs[i] > lc_dataYMax) lc_dataYMax = lineYs[i];
  }
  // Add 10% margin
  float yMargin = (lc_dataYMax - lc_dataYMin) * 0.1f;
  lc_dataYMin -= yMargin;
  lc_dataYMax += yMargin;

  // Compute transform: data → clip
  {
    float sx = (lc_clipXMax - lc_clipXMin) / (lc_dataXMax - lc_dataXMin);
    float sy = (lc_clipYMax - lc_clipYMin) / (lc_dataYMax - lc_dataYMin);
    float tx = lc_clipXMin - sx * lc_dataXMin;
    float ty = lc_clipYMin - sy * lc_dataYMin;
    setTransform(cp, 50, sx, sy, tx, ty);
  }

  // --- Grid lines (horizontal, in data space, lineAA@1) ---
  constexpr dc::Id LC_GRID_BUF  = 100;
  constexpr dc::Id LC_GRID_GEOM = 101;
  constexpr dc::Id LC_GRID_DI   = 102;
  {
    std::vector<float> gridSegs;
    int numGridLines = 4;
    for (int i = 1; i <= numGridLines; i++) {
      float t = static_cast<float>(i) / static_cast<float>(numGridLines + 1);
      float y = lc_dataYMin + t * (lc_dataYMax - lc_dataYMin);
      addHLine(gridSegs, lc_dataXMin, lc_dataXMax, y);
    }
    createBuffer(cp, LC_GRID_BUF);
    createGeometry(cp, LC_GRID_GEOM, LC_GRID_BUF, "rect4",
                   static_cast<std::uint32_t>(gridSegs.size() / 4));
    createDrawItem(cp, LC_GRID_DI, 11, LC_GRID_GEOM, "lineAA@1",
                   0.3f, 0.3f, 0.35f, 0.5f, 50);
    setDrawItemStyle(cp, LC_GRID_DI, 1.0f, 1.0f);
    uploadBuf(ingest, LC_GRID_BUF, gridSegs);
  }

  // --- Spine (left + bottom edges, lineAA@1, in data space) ---
  constexpr dc::Id LC_SPINE_BUF  = 103;
  constexpr dc::Id LC_SPINE_GEOM = 104;
  constexpr dc::Id LC_SPINE_DI   = 105;
  {
    std::vector<float> spineSegs;
    // Left spine (vertical)
    addVLine(spineSegs, lc_dataXMin, lc_dataYMin, lc_dataYMax);
    // Bottom spine (horizontal)
    addHLine(spineSegs, lc_dataXMin, lc_dataXMax, lc_dataYMin);
    createBuffer(cp, LC_SPINE_BUF);
    createGeometry(cp, LC_SPINE_GEOM, LC_SPINE_BUF, "rect4",
                   static_cast<std::uint32_t>(spineSegs.size() / 4));
    createDrawItem(cp, LC_SPINE_DI, 11, LC_SPINE_GEOM, "lineAA@1",
                   0.5f, 0.5f, 0.55f, 1.0f, 50);
    setDrawItemStyle(cp, LC_SPINE_DI, 2.0f, 1.0f);
    uploadBuf(ingest, LC_SPINE_BUF, spineSegs);
  }

  // --- Line segments (lineAA@1, in data space) ---
  constexpr dc::Id LC_LINE_BUF  = 106;
  constexpr dc::Id LC_LINE_GEOM = 107;
  constexpr dc::Id LC_LINE_DI   = 108;
  {
    auto segs = makeLineSegments(lineXs, lineYs, LINE_N);
    createBuffer(cp, LC_LINE_BUF);
    createGeometry(cp, LC_LINE_GEOM, LC_LINE_BUF, "rect4",
                   static_cast<std::uint32_t>(segs.size() / 4));
    createDrawItem(cp, LC_LINE_DI, 10, LC_LINE_GEOM, "lineAA@1",
                   0.31f, 0.58f, 0.98f, 1.0f, 50); // blue line
    setDrawItemStyle(cp, LC_LINE_DI, 2.5f, 1.0f);
    uploadBuf(ingest, LC_LINE_BUF, segs);
  }

  // --- Data point markers (lineAA@1 crosshairs at each point) ---
  constexpr dc::Id LC_PTS_BUF  = 109;
  constexpr dc::Id LC_PTS_GEOM = 110;
  constexpr dc::Id LC_PTS_DI   = 111;
  {
    // Each data point gets a tiny + marker (2 lineAA segments)
    // The marker size is in data-space units; transform handles the rest.
    float mX = (lc_dataXMax - lc_dataXMin) * 0.008f; // half-width in data X
    float mY = (lc_dataYMax - lc_dataYMin) * 0.015f;  // half-height in data Y
    std::vector<float> markers;
    markers.reserve(static_cast<std::size_t>(LINE_N) * 2 * 4);
    for (int i = 0; i < LINE_N; i++) {
      // Horizontal stroke
      markers.push_back(lineXs[i] - mX); markers.push_back(lineYs[i]);
      markers.push_back(lineXs[i] + mX); markers.push_back(lineYs[i]);
      // Vertical stroke
      markers.push_back(lineXs[i]); markers.push_back(lineYs[i] - mY);
      markers.push_back(lineXs[i]); markers.push_back(lineYs[i] + mY);
    }
    createBuffer(cp, LC_PTS_BUF);
    createGeometry(cp, LC_PTS_GEOM, LC_PTS_BUF, "rect4",
                   static_cast<std::uint32_t>(markers.size() / 4));
    createDrawItem(cp, LC_PTS_DI, 10, LC_PTS_GEOM, "lineAA@1",
                   0.31f, 0.58f, 0.98f, 1.0f, 50);
    setDrawItemStyle(cp, LC_PTS_DI, 3.0f, 1.0f);
    uploadBuf(ingest, LC_PTS_BUF, markers);
  }

  // ========================================================================
  // PANE 2: Pie Chart
  // ========================================================================
  createLayer(cp, 20, 2); // pie slices

  // Pie data: 5 categories
  const char* pieNames[] = {"Tech", "Finance", "Health", "Energy", "Other"};
  float pieValues[] = {0.35f, 0.25f, 0.20f, 0.12f, 0.08f};
  float pieColors[][3] = {
    {0.306f, 0.475f, 0.655f}, // #4e79a7 blue
    {0.949f, 0.557f, 0.169f}, // #f28e2b orange
    {0.882f, 0.341f, 0.349f}, // #e15759 red
    {0.463f, 0.718f, 0.698f}, // #76b7b2 teal
    {0.349f, 0.631f, 0.310f}, // #59a14f green
  };
  constexpr int PIE_N = 5;

  // Pie center in clip space (middle of pane 2)
  float pieCx = (0.025f + 0.95f) / 2.0f;
  float pieCy = (0.025f + 0.95f) / 2.0f + 0.05f; // shift up slightly for legend

  // Aspect-corrected radii: 110 pixel visual radius → circular on screen
  float piePixelRadius = 110.0f;
  float pieRadiusX = piePixelRadius * 2.0f / static_cast<float>(W);
  float pieRadiusY = piePixelRadius * 2.0f / static_cast<float>(H);
  constexpr int SEGS_PER_SLICE = 128;
  constexpr float FRINGE_PIXELS = 2.5f;

  // Create per-slice: buffer + geometry + drawItem
  // IDs: slice i → buffer=200+i*3, geometry=201+i*3, drawItem=202+i*3
  float angle = static_cast<float>(M_PI) / 2.0f; // start at top
  for (int i = 0; i < PIE_N; i++) {
    dc::Id bufId  = 200 + static_cast<dc::Id>(i) * 3;
    dc::Id geomId = 201 + static_cast<dc::Id>(i) * 3;
    dc::Id diId   = 202 + static_cast<dc::Id>(i) * 3;

    float sweep = pieValues[i] * 2.0f * static_cast<float>(M_PI);
    auto verts = tessellatePieSliceAA(pieCx, pieCy, pieRadiusX, pieRadiusY,
                                      FRINGE_PIXELS, angle, sweep,
                                      SEGS_PER_SLICE, W, H);
    // 9 vertices per segment (3 fill + 6 fringe)
    std::uint32_t vertCount = static_cast<std::uint32_t>(SEGS_PER_SLICE) * 9;

    createBuffer(cp, bufId);
    createGeometry(cp, geomId, bufId, "pos2_alpha", vertCount);
    createDrawItem(cp, diId, 20, geomId, "triAA@1",
                   pieColors[i][0], pieColors[i][1], pieColors[i][2], 1.0f);
    uploadBuf(ingest, bufId, verts);

    angle += sweep;
  }

  // ========================================================================
  // PANE 3: Table
  // ========================================================================
  // Table clip region
  const float tbl_clipXMin = -0.95f;
  const float tbl_clipXMax =  0.95f;
  const float tbl_clipYMin = -0.95f;
  const float tbl_clipYMax = -0.025f;

  createLayer(cp, 30, 3); // background rects
  createLayer(cp, 31, 3); // grid lines

  // Table data
  const char* tableHeaders[] = {"Symbol", "Price", "Change", "Volume"};
  constexpr int TABLE_COLS = 4;
  constexpr int TABLE_ROWS = 5;
  const char* tableData[TABLE_ROWS][4] = {
    {"AAPL",  "178.50", "+2.3%",  "45.2M"},
    {"GOOGL", "141.20", "-0.8%",  "28.1M"},
    {"MSFT",  "378.90", "+1.5%",  "22.7M"},
    {"AMZN",  "185.60", "+3.1%",  "38.9M"},
    {"TSLA",  "248.40", "-1.2%",  "52.3M"},
  };

  float colWidth = (tbl_clipXMax - tbl_clipXMin) / static_cast<float>(TABLE_COLS);
  float rowHeight = 0.11f;
  float headerY = tbl_clipYMax - 0.02f;
  float headerBottomY = headerY - 0.10f;

  // --- Header background (instancedRect@1, Rect4) ---
  constexpr dc::Id TBL_HDR_BUF  = 300;
  constexpr dc::Id TBL_HDR_GEOM = 301;
  constexpr dc::Id TBL_HDR_DI   = 302;
  {
    std::vector<float> hdrRect = {
      tbl_clipXMin, headerBottomY, tbl_clipXMax, headerY
    };
    createBuffer(cp, TBL_HDR_BUF);
    createGeometry(cp, TBL_HDR_GEOM, TBL_HDR_BUF, "rect4", 1);
    createDrawItem(cp, TBL_HDR_DI, 30, TBL_HDR_GEOM, "instancedRect@1",
                   0.18f, 0.20f, 0.25f, 1.0f);
    uploadBuf(ingest, TBL_HDR_BUF, hdrRect);
  }

  // --- Alternating row backgrounds (instancedRect@1, Rect4) ---
  constexpr dc::Id TBL_ROW_BUF  = 303;
  constexpr dc::Id TBL_ROW_GEOM = 304;
  constexpr dc::Id TBL_ROW_DI   = 305;
  {
    std::vector<float> rowRects;
    float rowStart = headerBottomY;
    for (int r = 0; r < TABLE_ROWS; r++) {
      float top = rowStart - static_cast<float>(r) * rowHeight;
      float bot = top - rowHeight;
      if (r % 2 == 1) { // alternate rows
        rowRects.push_back(tbl_clipXMin);
        rowRects.push_back(bot);
        rowRects.push_back(tbl_clipXMax);
        rowRects.push_back(top);
      }
    }
    if (!rowRects.empty()) {
      createBuffer(cp, TBL_ROW_BUF);
      createGeometry(cp, TBL_ROW_GEOM, TBL_ROW_BUF, "rect4",
                     static_cast<std::uint32_t>(rowRects.size() / 4));
      createDrawItem(cp, TBL_ROW_DI, 30, TBL_ROW_GEOM, "instancedRect@1",
                     0.15f, 0.16f, 0.19f, 1.0f);
      uploadBuf(ingest, TBL_ROW_BUF, rowRects);
    }
  }

  // --- Table grid lines (lineAA@1) ---
  constexpr dc::Id TBL_GRID_BUF  = 306;
  constexpr dc::Id TBL_GRID_GEOM = 307;
  constexpr dc::Id TBL_GRID_DI   = 308;
  {
    std::vector<float> gridSegs;

    // Horizontal lines (header bottom + row separators)
    addHLine(gridSegs, tbl_clipXMin, tbl_clipXMax, headerBottomY);
    float rowStart = headerBottomY;
    for (int r = 1; r <= TABLE_ROWS; r++) {
      float y = rowStart - static_cast<float>(r) * rowHeight;
      addHLine(gridSegs, tbl_clipXMin, tbl_clipXMax, y);
    }

    // Vertical column separators
    for (int c = 1; c < TABLE_COLS; c++) {
      float x = tbl_clipXMin + static_cast<float>(c) * colWidth;
      float botY = rowStart - static_cast<float>(TABLE_ROWS) * rowHeight;
      addVLine(gridSegs, x, botY, headerY);
    }

    createBuffer(cp, TBL_GRID_BUF);
    createGeometry(cp, TBL_GRID_GEOM, TBL_GRID_BUF, "rect4",
                   static_cast<std::uint32_t>(gridSegs.size() / 4));
    createDrawItem(cp, TBL_GRID_DI, 31, TBL_GRID_GEOM, "lineAA@1",
                   0.25f, 0.27f, 0.30f, 0.7f);
    setDrawItemStyle(cp, TBL_GRID_DI, 1.0f, 1.0f);
    uploadBuf(ingest, TBL_GRID_BUF, gridSegs);
  }

  // ========================================================================
  // Sync all buffers to GPU
  // ========================================================================
  auto gpuBufs = std::make_unique<dc::GpuBufferManager>();
  auto renderer = std::make_unique<dc::Renderer>();
  renderer->init();

  // Sync line chart buffers
  syncGpuBuf(ingest, *gpuBufs, LC_GRID_BUF);
  syncGpuBuf(ingest, *gpuBufs, LC_SPINE_BUF);
  syncGpuBuf(ingest, *gpuBufs, LC_LINE_BUF);
  syncGpuBuf(ingest, *gpuBufs, LC_PTS_BUF);

  // Sync pie chart buffers
  for (int i = 0; i < PIE_N; i++) {
    dc::Id bufId = 200 + static_cast<dc::Id>(i) * 3;
    syncGpuBuf(ingest, *gpuBufs, bufId);
  }

  // Sync table buffers
  syncGpuBuf(ingest, *gpuBufs, TBL_HDR_BUF);
  syncGpuBuf(ingest, *gpuBufs, TBL_ROW_BUF);
  syncGpuBuf(ingest, *gpuBufs, TBL_GRID_BUF);

  gpuBufs->uploadDirty();

  // ========================================================================
  // Build text overlay
  // ========================================================================
  std::string textJson = buildTextOverlay(
    lineXs, lineYs, LINE_N,
    lc_dataXMin, lc_dataXMax, lc_dataYMin, lc_dataYMax,
    lc_clipXMin, lc_clipXMax, lc_clipYMin, lc_clipYMax,
    pieNames, pieValues, pieColors, PIE_N,
    pieCx, pieCy, 0.025f,
    tableHeaders, TABLE_COLS,
    tableData, TABLE_ROWS,
    tbl_clipXMin, tbl_clipXMax, tbl_clipYMin, tbl_clipYMax,
    W, H);

  // ========================================================================
  // Render initial frame
  // ========================================================================
  renderer->render(scene, *gpuBufs, W, H);
  ctx->swapBuffers();
  auto pixels = ctx->readPixels();
  writeTextJson(textJson);
  writeFrame(pixels.data(), W, H);

  // ========================================================================
  // Input loop (minimal — just respond to "render" and "resize")
  // ========================================================================
  while (true) {
    std::string line = readLine();
    if (line.empty()) break;

    // Simple check: any valid command triggers re-render
    if (line.find("cmd") != std::string::npos) {
      // Handle resize
      if (line.find("resize") != std::string::npos) {
        // Parse width/height manually
        auto findInt = [&](const char* key) -> int {
          auto pos = line.find(key);
          if (pos == std::string::npos) return -1;
          pos = line.find(':', pos);
          if (pos == std::string::npos) return -1;
          pos++;
          while (pos < line.size() && line[pos] == ' ') pos++;
          return std::atoi(line.c_str() + pos);
        };
        int newW = findInt("\"w\"");
        int newH = findInt("\"h\"");
        if (newW > 0 && newH > 0 && (newW != W || newH != H)) {
          W = newW;
          H = newH;
          ctx = std::make_unique<dc::OsMesaContext>();
          if (!ctx->init(W, H)) break;
          renderer = std::make_unique<dc::Renderer>();
          renderer->init();
          gpuBufs = std::make_unique<dc::GpuBufferManager>();

          // Re-tessellate pie for new viewport dimensions (fringe + aspect ratio)
          {
            float newRX = piePixelRadius * 2.0f / static_cast<float>(W);
            float newRY = piePixelRadius * 2.0f / static_cast<float>(H);
            float a = static_cast<float>(M_PI) / 2.0f;
            for (int i = 0; i < PIE_N; i++) {
              dc::Id bufId = 200 + static_cast<dc::Id>(i) * 3;
              dc::Id geomId = 201 + static_cast<dc::Id>(i) * 3;
              float sweep = pieValues[i] * 2.0f * static_cast<float>(M_PI);
              auto verts = tessellatePieSliceAA(pieCx, pieCy, newRX, newRY,
                                                FRINGE_PIXELS, a, sweep,
                                                SEGS_PER_SLICE, W, H);
              uploadBuf(ingest, bufId, verts);
              setVertexCount(cp, geomId,
                             static_cast<std::uint32_t>(SEGS_PER_SLICE) * 9);
              a += sweep;
            }
          }

          // Re-sync all buffers
          syncGpuBuf(ingest, *gpuBufs, LC_GRID_BUF);
          syncGpuBuf(ingest, *gpuBufs, LC_SPINE_BUF);
          syncGpuBuf(ingest, *gpuBufs, LC_LINE_BUF);
          syncGpuBuf(ingest, *gpuBufs, LC_PTS_BUF);
          for (int i = 0; i < PIE_N; i++) {
            dc::Id bufId = 200 + static_cast<dc::Id>(i) * 3;
            syncGpuBuf(ingest, *gpuBufs, bufId);
          }
          syncGpuBuf(ingest, *gpuBufs, TBL_HDR_BUF);
          syncGpuBuf(ingest, *gpuBufs, TBL_ROW_BUF);
          syncGpuBuf(ingest, *gpuBufs, TBL_GRID_BUF);

          // Rebuild text overlay with new dimensions
          textJson = buildTextOverlay(
            lineXs, lineYs, LINE_N,
            lc_dataXMin, lc_dataXMax, lc_dataYMin, lc_dataYMax,
            lc_clipXMin, lc_clipXMax, lc_clipYMin, lc_clipYMax,
            pieNames, pieValues, pieColors, PIE_N,
            pieCx, pieCy, 0.025f,
            tableHeaders, TABLE_COLS,
            tableData, TABLE_ROWS,
            tbl_clipXMin, tbl_clipXMax, tbl_clipYMin, tbl_clipYMax,
            W, H);
        }
      }

      gpuBufs->uploadDirty();
      renderer->render(scene, *gpuBufs, W, H);
      ctx->swapBuffers();
      pixels = ctx->readPixels();
      writeTextJson(textJson);
      writeFrame(pixels.data(), W, H);
    }
  }

  return 0;
}
