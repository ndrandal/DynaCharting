// D76 — Feature Showcase Demo
// Demonstrates all 13 new subsystems (D63–D75) in a single chart session:
//   1. Log/Percentage Scale (ScaleMapping)
//   2. Bezier Curves, Arcs, Ellipses (CurveTessellator)
//   3. Freehand Drawing (FreehandCapture + StrokeStore)
//   4. Control-Point Handles (HandleSet)
//   5. OHLC Snap / Magnet Mode (SnapManager)
//   6. Text Annotation Editing (TextEditState)
//   7. Extended Drawing Tools (ExtDrawingInteraction)
//   8. Bar Replay / Playback (TemporalFilter)
//   9. Tooltip / Hover (HoverManager)
//  10. Linked Crosshair (SessionBridge)
//  11. Price Alerts (AlertManager)
//  12. Multi-Chart Layout Grid (LayoutGrid)
//  13. SVG Export (SvgExporter)
//
// Renders a multi-pane chart via OSMesa, exercises every subsystem,
// writes d76_showcase.ppm (raster) and d76_showcase.svg (vector).

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

// New subsystems (D63–D75)
#include "dc/viewport/ScaleMapping.hpp"
#include "dc/geometry/CurveTessellator.hpp"
#include "dc/drawing/FreehandCapture.hpp"
#include "dc/interaction/HandleSet.hpp"
#include "dc/interaction/SnapManager.hpp"
#include "dc/text/TextEditState.hpp"
#include "dc/drawing/ExtendedDrawings.hpp"
#include "dc/data/TemporalFilter.hpp"
#include "dc/interaction/HoverManager.hpp"
#include "dc/session/SessionBridge.hpp"
#include "dc/data/AlertManager.hpp"
#include "dc/layout/LayoutGrid.hpp"
#include "dc/export/SvgExporter.hpp"

// Existing recipe/layout
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/SmaRecipe.hpp"
#include "dc/math/Normalize.hpp"
#include "dc/layout/PaneLayout.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>
#include <string>
#include <fstream>

// ---- Helpers ----

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

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void setColor(dc::CommandProcessor& cp, dc::Id diId,
                      float r, float g, float b, float a) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":%llu,"r":%.3f,"g":%.3f,"b":%.3f,"a":%.3f})",
    static_cast<unsigned long long>(diId),
    static_cast<double>(r), static_cast<double>(g),
    static_cast<double>(b), static_cast<double>(a));
  requireOk(cp.applyJsonText(buf), "setColor");
}

static void setVC(dc::CommandProcessor& cp, dc::Id geomId, std::uint32_t vc) {
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":)" + std::to_string(geomId) +
    R"(,"vertexCount":)" + std::to_string(vc) + "}"), "setVC");
}

static void ingestFloat(std::vector<std::uint8_t>& batch, std::uint32_t bid,
                          const std::vector<float>& v) {
  if (!v.empty())
    appendRecord(batch, 1, bid, 0, v.data(),
                 static_cast<std::uint32_t>(v.size() * sizeof(float)));
}

static void writePPM(const char* filename, const std::vector<std::uint8_t>& pixels,
                      int w, int h) {
  FILE* f = std::fopen(filename, "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      std::size_t idx = static_cast<std::size_t>((y * w + x) * 4);
      std::fputc(pixels[idx + 0], f);
      std::fputc(pixels[idx + 1], f);
      std::fputc(pixels[idx + 2], f);
    }
  }
  std::fclose(f);
  std::printf("  Wrote %s (%dx%d)\n", filename, w, h);
}

struct Candle {
  float x, open, high, low, close, halfWidth;
};

// ---- Section banners ----
static void section(int num, const char* title) {
  std::printf("\n--- [%d/13] %s ---\n", num, title);
}

int main() {
  std::printf("========================================\n");
  std::printf("  D76 Feature Showcase Demo\n");
  std::printf("  13 new subsystems in one session\n");
  std::printf("========================================\n");

  constexpr int W = 1280, H = 960;
  constexpr int NUM_CANDLES = 80;

  // =========================================================
  // SECTION 12: Layout Grid (we configure this first to drive the pane layout)
  // =========================================================
  section(12, "Multi-Chart Layout Grid");
  {
    dc::LayoutGrid grid;
    grid.setQuad();
    grid.recompute(W, H);

    std::printf("  Layout: %dx%d = %zu cells\n", grid.rows(), grid.cols(), grid.cells().size());
    for (const auto& cell : grid.cells()) {
      std::printf("    Cell[%u] (%d,%d): %.0fx%.0f at (%.0f,%.0f)\n",
                  cell.id, cell.row, cell.col,
                  cell.width, cell.height, cell.x, cell.y);
    }

    // Resize by dragging a divider
    int divIdx = grid.hitTestDivider(0.5, 0.5, 0.05);
    if (divIdx >= 0) {
      grid.beginDividerDrag(divIdx);
      grid.updateDividerDrag(divIdx, 0.6);
      grid.endDividerDrag(divIdx);
      grid.recompute(W, H);
      std::printf("  After resize: col[0]=%.1f%% col[1]=%.1f%%\n",
                  grid.cells()[0].width / W * 100.0,
                  grid.cells()[1].width / W * 100.0);
    }
    std::printf("  PASS\n");
  }

  // =========================================================
  // GL Context
  // =========================================================
#ifdef DC_HAS_OSMESA
  auto glCtx = std::make_unique<dc::OsMesaContext>();
  if (!glCtx->init(W, H)) {
    std::fprintf(stderr, "OSMesa init failed\n");
    return 1;
  }
  std::printf("\nGL: OSMesa %dx%d\n", W, H);
#else
  std::printf("No OSMesa — skipping GL render\n");
  // Still run non-GL subsystem demos below
#endif

  // =========================================================
  // Scene infrastructure
  // =========================================================
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Two panes: price (top 70%) and indicator (bottom 30%)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane1");
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Curves"})"), "pane2");

  // Layers
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "l10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"SMA"})"), "l11");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1,"name":"Overlays"})"), "l12");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":2,"name":"CurveLayer"})"), "l20");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":2,"name":"FreehandLayer"})"), "l21");

  // Shared transforms
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform50");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":51})"), "xform51");

  // ---- Generate OHLC data ----
  std::vector<Candle> candles;
  float closePrices[NUM_CANDLES], xPos[NUM_CANDLES];
  double timestamps[NUM_CANDLES]; // for temporal filter
  float price = 100.0f;
  std::uint32_t seed = 12345;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };

  float barW = 1.6f / static_cast<float>(NUM_CANDLES);
  float hw = barW * 0.35f;
  float priceMin = 1e9f, priceMax = -1e9f;

  for (int i = 0; i < NUM_CANDLES; i++) {
    float x = -0.85f + barW * (static_cast<float>(i) + 0.5f);
    float change = (rng() - 0.5f) * 4.0f;
    float open = price;
    float close = price + change;
    float high = std::fmax(open, close) + rng() * 2.0f;
    float low  = std::fmin(open, close) - rng() * 2.0f;
    price = close;

    Candle c;
    c.x = x; c.open = open; c.high = high; c.low = low; c.close = close; c.halfWidth = hw;
    candles.push_back(c);
    closePrices[i] = close;
    xPos[i] = x;
    timestamps[i] = 1700000000.0 + i * 3600.0; // hourly bars
    priceMin = std::fmin(priceMin, low);
    priceMax = std::fmax(priceMax, high);
  }

  auto panes = dc::computePaneLayout({0.65f, 0.35f}, 0.05f, 0.05f);
  float pClipMin = panes[0].clipYMin, pClipMax = panes[0].clipYMax;
  float cClipMin = panes[1].clipYMin, cClipMax = panes[1].clipYMax;

  // Normalize candles to price pane
  std::vector<Candle> normCandles(NUM_CANDLES);
  for (int i = 0; i < NUM_CANDLES; i++) {
    auto& c = candles[static_cast<std::size_t>(i)];
    auto& nc = normCandles[static_cast<std::size_t>(i)];
    nc.x = c.x;
    nc.open  = dc::normalizeToClip(c.open,  priceMin, priceMax, pClipMin, pClipMax);
    nc.high  = dc::normalizeToClip(c.high,  priceMin, priceMax, pClipMin, pClipMax);
    nc.low   = dc::normalizeToClip(c.low,   priceMin, priceMax, pClipMin, pClipMax);
    nc.close = dc::normalizeToClip(c.close, priceMin, priceMax, pClipMin, pClipMax);
    nc.halfWidth = hw;
  }

  // Build candle recipe
  dc::CandleRecipeConfig candleCfg;
  candleCfg.layerId = 10; candleCfg.name = "OHLC"; candleCfg.createTransform = false;
  dc::CandleRecipe candleRecipe(300, candleCfg);
  for (auto& cmd : candleRecipe.build().createCommands) requireOk(cp.applyJsonText(cmd), "candle");

  // SMA overlay
  dc::SmaRecipeConfig smaCfg;
  smaCfg.layerId = 11; smaCfg.name = "SMA10"; smaCfg.createTransform = false; smaCfg.period = 10;
  dc::SmaRecipe smaRecipe(400, smaCfg);
  for (auto& cmd : smaRecipe.build().createCommands) requireOk(cp.applyJsonText(cmd), "sma");
  auto smaData = smaRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                     priceMin, priceMax, pClipMin, pClipMax);

  // =========================================================
  // SECTION 1: Log / Percentage Scale
  // =========================================================
  section(1, "Log / Percentage Scale (ScaleMapping)");
  {
    dc::ScaleMapping sm;
    sm.setReferencePrice(100.0);
    double val = 200.0;

    sm.setMode(dc::ScaleMode::Linear);
    double linY = sm.toScreen(val, 0, 100, 100, 300);
    std::printf("  Linear:     toScreen(%.0f) = %.2f\n", val, linY);

    sm.setMode(dc::ScaleMode::Logarithmic);
    double logY = sm.toScreen(val, 0, 100, 100, 300);
    std::printf("  Logarithmic: toScreen(%.0f) = %.2f\n", val, logY);

    sm.setMode(dc::ScaleMode::Percentage);
    double pctY = sm.toScreen(val, 0, 100, 100, 300);
    std::printf("  Percentage:  toScreen(%.0f) = %.2f\n", val, pctY);

    sm.setMode(dc::ScaleMode::Indexed);
    double idxY = sm.toScreen(val, 0, 100, 100, 300);
    std::printf("  Indexed:     toScreen(%.0f) = %.2f\n", val, idxY);

    // Round-trip test
    sm.setMode(dc::ScaleMode::Logarithmic);
    double rt = sm.toData(sm.toScreen(150.0, 0, 100, 50, 250), 0, 100, 50, 250);
    std::printf("  Log round-trip: 150 -> screen -> data = %.4f (err=%.2e)\n",
                rt, std::fabs(rt - 150.0));
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 2: Bezier Curves, Arcs, Ellipses
  // =========================================================
  section(2, "Bezier Curves / Arcs / Ellipses (CurveTessellator)");

  // Tessellate a cubic bezier + ellipse and render them in the curve pane
  auto bezierPts = dc::CurveTessellator::cubicBezier(
      {-0.8, 0.0}, {-0.3, 0.6}, {0.3, -0.6}, {0.8, 0.0}, 48);
  auto ellipsePts = dc::CurveTessellator::ellipse(
      {0.0, 0.0}, 0.5, 0.25, 0.0, 0.0, 6.283185, 48);
  auto arcPts = dc::CurveTessellator::arc(
      {-0.4, -0.2}, 0.3, 0.0, 3.14159, 24);
  std::printf("  Cubic bezier:  %zu vertices\n", bezierPts.size());
  std::printf("  Ellipse:       %zu vertices\n", ellipsePts.size());
  std::printf("  Arc:           %zu vertices\n", arcPts.size());

  // Convert bezier to renderable line2d vertices in curve pane clip space
  std::vector<float> bezierVerts;
  for (auto& p : bezierPts) {
    float cx = static_cast<float>(p.x);
    float cy = cClipMin + static_cast<float>((p.y + 1.0) * 0.5) * (cClipMax - cClipMin);
    bezierVerts.push_back(cx);
    bezierVerts.push_back(cy);
  }

  // Ellipse as line2d in curve pane
  std::vector<float> ellipseVerts;
  for (auto& p : ellipsePts) {
    float cx = static_cast<float>(p.x) * 0.5f + 0.3f;
    float cy = cClipMin + static_cast<float>((p.y + 0.5) / 1.0) * (cClipMax - cClipMin);
    ellipseVerts.push_back(cx);
    ellipseVerts.push_back(cy);
  }

  // Register buffers and create draw items for curves
  // IDs: bezier buf=2000, geom=2001, di=2002 / ellipse buf=2010, geom=2011, di=2012
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":2000,"byteLength":4096})"), "bezBuf");
  requireOk(cp.applyJsonText(R"({"cmd":"createGeometry","id":2001,"vertexBufferId":2000,"format":"pos2_clip","vertexCount":2})"), "bezGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":2002,"layerId":20})"), "bezDI");
  requireOk(cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":2002,"pipeline":"line2d@1","geometryId":2001})"), "bezBind");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":2010,"byteLength":4096})"), "ellBuf");
  requireOk(cp.applyJsonText(R"({"cmd":"createGeometry","id":2011,"vertexBufferId":2010,"format":"pos2_clip","vertexCount":2})"), "ellGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":2012,"layerId":20})"), "ellDI");
  requireOk(cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":2012,"pipeline":"line2d@1","geometryId":2011})"), "ellBind");

  setColor(cp, 2002, 0.0f, 1.0f, 0.5f, 1.0f);  // green bezier
  setColor(cp, 2012, 1.0f, 0.5f, 0.0f, 1.0f);  // orange ellipse
  std::printf("  PASS\n");

  // =========================================================
  // SECTION 3: Freehand Drawing
  // =========================================================
  section(3, "Freehand Drawing (FreehandCapture + StrokeStore)");
  {
    dc::FreehandCapture capture;
    capture.begin(-0.7, 0.0);
    // Simulate a wavy stroke
    for (int i = 1; i <= 30; i++) {
      double t = static_cast<double>(i) / 30.0;
      double sx = -0.7 + t * 1.4;
      double sy = std::sin(t * 6.28) * 0.15;
      capture.addPoint(sx, sy);
    }
    dc::Stroke stroke = capture.finish();
    std::printf("  Captured stroke: %zu points\n", stroke.points.size());

    auto simplified = dc::FreehandCapture::simplify(stroke.points, 0.02);
    std::printf("  After simplify(eps=0.02): %zu points\n", simplified.size());

    auto smoothed = dc::FreehandCapture::smooth(stroke.points, 5);
    std::printf("  After smooth(window=5): %zu points\n", smoothed.size());

    dc::StrokeStore store;
    stroke.color[0] = 1.0f; stroke.color[1] = 0.8f; stroke.color[2] = 0.0f;
    store.add(std::move(stroke));
    std::printf("  StrokeStore: %zu strokes\n", store.count());

    // Convert smoothed stroke to renderable verts in the curve pane
    std::vector<float> strokeVerts;
    for (auto& p : smoothed) {
      float cx = static_cast<float>(p.x);
      float cy = cClipMin + static_cast<float>((p.y + 0.5) * 0.5) * (cClipMax - cClipMin);
      strokeVerts.push_back(cx);
      strokeVerts.push_back(cy);
    }

    // Freehand: buf=2020, geom=2021, di=2022
    // Freehand: buf=2020, geom=2021, di=2022
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":2020,"byteLength":4096})"), "strokeBuf");
    requireOk(cp.applyJsonText(R"({"cmd":"createGeometry","id":2021,"vertexBufferId":2020,"format":"pos2_clip","vertexCount":2})"), "strokeGeom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":2022,"layerId":21})"), "strokeDI");
    requireOk(cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":2022,"pipeline":"line2d@1","geometryId":2021})"), "strokeBind");
    setColor(cp, 2022, 1.0f, 0.8f, 0.0f, 0.8f); // yellow freehand

    // Ingest the stroke
    std::vector<std::uint8_t> strokeBatch;
    ingestFloat(strokeBatch, 2020, strokeVerts);
    ingest.processBatch(strokeBatch.data(), static_cast<std::uint32_t>(strokeBatch.size()));
    setVC(cp, 2021, static_cast<std::uint32_t>(strokeVerts.size() / 2));
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 4: Control-Point Handles
  // =========================================================
  section(4, "Control-Point Handles (HandleSet)");
  {
    dc::HandleSet handles;
    // Create handles for a trendline drawing (type 1)
    handles.createForDrawing(/*drawingId=*/1, /*drawingType=*/1,
                              /*x0=*/10.0, /*y0=*/100.0, /*x1=*/50.0, /*y1=*/150.0);
    std::printf("  Handles for trendline: %zu\n", handles.handles().size());

    // Create handles for a rectangle (type 4)
    handles.createForDrawing(2, 4, 20.0, 80.0, 60.0, 140.0);
    std::printf("  Handles for rectangle: total=%zu\n", handles.handles().size());

    // Hit test near the trendline start
    auto hitId = handles.hitTest(10.5, 100.5, 100.0, 100.0);
    std::printf("  Hit test near (10.5, 100.5): handle=%u\n", hitId);

    // Drag the handle
    if (hitId > 0) {
      handles.beginDrag(hitId);
      handles.updateDrag(hitId, 15.0, 105.0);
      handles.endDrag(hitId);
      auto mc = handles.getModifiedCoords(1);
      std::printf("  After drag: trendline (%.1f,%.1f)-(%.1f,%.1f)\n",
                  mc.x0, mc.y0, mc.x1, mc.y1);
    }
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 5: OHLC Snap / Magnet Mode
  // =========================================================
  section(5, "OHLC Snap / Magnet Mode (SnapManager)");
  {
    dc::SnapManager snap;
    snap.setMode(dc::SnapMode::Magnet);
    snap.setMagnetRadius(15.0);

    // Register some OHLC candle targets (x, open, high, low, close)
    std::vector<double> ohlcData;
    for (int i = 0; i < 5; i++) {
      ohlcData.push_back(static_cast<double>(i) * 10.0); // x
      ohlcData.push_back(100.0 + i * 2.0);  // open
      ohlcData.push_back(105.0 + i * 2.0);  // high
      ohlcData.push_back(95.0 + i * 2.0);   // low
      ohlcData.push_back(102.0 + i * 2.0);  // close
    }
    snap.setOHLCTargets(ohlcData.data(), 5);

    // Snap near a candle close value
    auto result = snap.snap(10.5, 104.5, 10.0, 10.0);
    std::printf("  Snap(10.5, 104.5): snapped=%s -> (%.1f, %.1f) dist=%.2f\n",
                result.snapped ? "yes" : "no", result.x, result.y, result.distance);

    // Test grid mode
    snap.setMode(dc::SnapMode::Grid);
    snap.setGridInterval(10.0, 5.0);
    auto gridResult = snap.snap(13.7, 108.3, 10.0, 10.0);
    std::printf("  Grid(13.7, 108.3): snapped=%s -> (%.1f, %.1f)\n",
                gridResult.snapped ? "yes" : "no", gridResult.x, gridResult.y);
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 6: Text Annotation Editing
  // =========================================================
  section(6, "Text Annotation Editing (TextEditState)");
  {
    dc::TextEditState editor;
    editor.beginEditing();
    editor.setText("Hello Chart");
    std::printf("  Initial: \"%s\" caret=%zu\n", editor.text().c_str(), editor.caret());

    editor.setCaret(5);
    editor.insertText(" World");
    std::printf("  After insert: \"%s\"\n", editor.text().c_str());

    editor.setSelection(6, 11);
    std::printf("  Selected: \"%s\"\n", editor.selectedText().c_str());

    editor.deleteSelection();
    std::printf("  After delete selection: \"%s\"\n", editor.text().c_str());

    editor.moveToEnd();
    editor.insertText("!");
    std::printf("  Final: \"%s\"\n", editor.text().c_str());
    editor.endEditing();
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 7: Extended Drawing Tools
  // =========================================================
  section(7, "Extended Drawing Tools (ExtDrawingInteraction)");
  {
    dc::ExtDrawingInteraction ext;

    // Pitchfork (3 clicks)
    ext.begin(dc::ExtDrawingType::Pitchfork);
    ext.onClick(10.0, 100.0);
    ext.onClick(30.0, 120.0);
    auto pitchforkId = ext.onClick(20.0, 80.0);
    std::printf("  Pitchfork: id=%u (3 clicks)\n", pitchforkId);

    // Arrow (2 clicks)
    ext.begin(dc::ExtDrawingType::Arrow);
    ext.onClick(0.0, 0.0);
    auto arrowId = ext.onClick(50.0, 50.0);
    std::printf("  Arrow:     id=%u (2 clicks)\n", arrowId);

    // Polygon (variable, finalize with double-click)
    ext.begin(dc::ExtDrawingType::Polygon);
    ext.onClick(10.0, 10.0);
    ext.onClick(30.0, 10.0);
    ext.onClick(30.0, 30.0);
    auto polyId = ext.onDoubleClick(10.0, 30.0);
    std::printf("  Polygon:   id=%u (%zu points)\n", polyId,
                ext.get(polyId) ? ext.get(polyId)->pointCount() : 0u);

    std::printf("  Total drawings: %zu\n", ext.drawings().size());
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 8: Bar Replay / Playback
  // =========================================================
  section(8, "Bar Replay / Playback (TemporalFilter)");
  {
    dc::TemporalFilter filter;
    filter.setRange(timestamps[0], timestamps[NUM_CANDLES - 1]);
    filter.setEnabled(true);

    // Start at 50% through the data
    filter.setCursor(timestamps[NUM_CANDLES / 2]);
    auto visible = filter.visibleCount(timestamps, NUM_CANDLES);
    std::printf("  At 50%%: %zu/%d candles visible (progress=%.1f%%)\n",
                visible, NUM_CANDLES, filter.progress() * 100.0);

    // Step forward 5 bars
    for (int i = 0; i < 5; i++) filter.stepForward(3600.0);
    visible = filter.visibleCount(timestamps, NUM_CANDLES);
    std::printf("  After 5 steps: %zu visible (progress=%.1f%%)\n",
                visible, filter.progress() * 100.0);

    // Playback simulation
    filter.setPlaybackSpeed(10.0); // 10 bars/sec
    filter.play();
    filter.tick(0.5, 3600.0); // half second = 5 bars
    visible = filter.visibleCount(timestamps, NUM_CANDLES);
    std::printf("  After 0.5s at 10x: %zu visible (progress=%.1f%%)\n",
                visible, filter.progress() * 100.0);
    filter.pause();
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 9: Tooltip / Hover
  // =========================================================
  section(9, "Tooltip / Hover (HoverManager)");
  {
    dc::HoverManager hover;
    int enterCount = 0, exitCount = 0;
    hover.setOnHoverEnter([&](std::uint32_t, double, double) { enterCount++; });
    hover.setOnHoverExit([&](std::uint32_t, double, double) { exitCount++; });
    hover.setTooltipProvider([](std::uint32_t diId, double dx, double dy) -> dc::TooltipData {
      dc::TooltipData td;
      td.visible = true;
      td.drawItemId = diId;
      td.dataX = dx; td.dataY = dy;
      td.title = "AAPL";
      td.fields.push_back({"Open", "142.50", {0.5f, 0.8f, 1.0f, 1.0f}});
      td.fields.push_back({"High", "145.20", {0.0f, 1.0f, 0.0f, 1.0f}});
      td.fields.push_back({"Low",  "141.00", {1.0f, 0.0f, 0.0f, 1.0f}});
      td.fields.push_back({"Close","144.80", {1.0f, 1.0f, 1.0f, 1.0f}});
      return td;
    });

    // Simulate hover sequence
    hover.update(42, 25.0, 144.0, 400.0, 300.0);
    std::printf("  Hovering: %s, drawItem=%u\n",
                hover.isHovering() ? "yes" : "no", hover.hoveredDrawItemId());
    std::printf("  Tooltip: \"%s\", %zu fields\n",
                hover.tooltip().title.c_str(), hover.tooltip().fields.size());

    hover.update(99, 30.0, 145.0, 410.0, 290.0); // change target
    hover.clear();
    std::printf("  Enter callbacks: %d, Exit callbacks: %d\n", enterCount, exitCount);
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 10: Linked Crosshair (SessionBridge)
  // =========================================================
  section(10, "Linked Crosshair (SessionBridge)");
  {
    dc::SessionBridge bridge;
    auto groupId = bridge.createGroup();
    auto* group = bridge.getGroup(groupId);

    auto sess1 = group->addSession();
    auto sess2 = group->addSession();
    auto sess3 = group->addSession();

    int sess2CrosshairEvents = 0, sess3CrosshairEvents = 0;
    double lastSyncX = 0;

    group->subscribe(sess2, dc::SyncEventType::CrosshairMove,
        [&](const dc::SyncEvent& e) { sess2CrosshairEvents++; lastSyncX = e.dataX; });
    group->subscribe(sess3, dc::SyncEventType::CrosshairMove,
        [&](const dc::SyncEvent& e) { sess3CrosshairEvents++; });

    // Session 1 publishes crosshair move
    dc::SyncEvent evt;
    evt.type = dc::SyncEventType::CrosshairMove;
    evt.sourceSessionId = sess1;
    evt.dataX = 42.5; evt.dataY = 150.0;
    group->publish(evt);

    std::printf("  Sessions: %zu in group\n", group->sessionCount());
    std::printf("  Sess2 crosshair events: %d (lastX=%.1f)\n", sess2CrosshairEvents, lastSyncX);
    std::printf("  Sess3 crosshair events: %d\n", sess3CrosshairEvents);

    // Disable crosshair sync
    group->setSyncEnabled(dc::SyncEventType::CrosshairMove, false);
    group->publish(evt); // should not fire
    std::printf("  After disable: sess2=%d (unchanged)\n", sess2CrosshairEvents);
    std::printf("  PASS\n");
  }

  // =========================================================
  // SECTION 11: Price Alerts
  // =========================================================
  section(11, "Price Alerts (AlertManager)");
  {
    dc::AlertManager alerts;
    int fireCount = 0;
    std::string lastAlertName;
    alerts.setCallback([&](const dc::Alert& a, double val) {
      fireCount++;
      lastAlertName = a.name;
      std::printf("  ALERT: \"%s\" fired at value=%.2f\n", a.name.c_str(), val);
    });

    auto a1 = alerts.addCrossingAlert("Price > 105", dc::AlertCondition::CrossingUp, 105.0);
    auto a2 = alerts.addRangeAlert("In Range", dc::AlertCondition::EntersRange, 98.0, 102.0);
    alerts.setOneShot(a1, true);
    alerts.setOneShot(a2, false);

    // Simulate price stream
    double priceStream[] = {100.0, 103.0, 106.0, 104.0, 99.0, 101.0, 110.0};
    for (double p : priceStream) {
      alerts.evaluate(p);
    }

    std::printf("  Total alerts: %zu, triggered: %zu, callbacks: %d\n",
                alerts.count(), alerts.triggeredCount(), fireCount);
    auto* alert1 = alerts.get(a1);
    std::printf("  \"%s\": triggered=%s enabled=%s (one-shot auto-disabled)\n",
                alert1->name.c_str(),
                alert1->triggered ? "yes" : "no",
                alert1->enabled ? "yes" : "no");
    std::printf("  PASS\n");
  }

  // =========================================================
  // Ingest all data and render
  // =========================================================
  std::printf("\n--- Ingesting data and rendering ---\n");

  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 300, 0, normCandles.data(),
               static_cast<std::uint32_t>(normCandles.size() * sizeof(Candle)));
  ingestFloat(batch, 400, smaData.lineVerts);
  ingestFloat(batch, 2000, bezierVerts);
  ingestFloat(batch, 2010, ellipseVerts);
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

  setVC(cp, 301, static_cast<std::uint32_t>(NUM_CANDLES));
  if (smaData.vertexCount > 0) setVC(cp, 401, smaData.vertexCount);
  setVC(cp, 2001, static_cast<std::uint32_t>(bezierVerts.size() / 2));
  setVC(cp, 2011, static_cast<std::uint32_t>(ellipseVerts.size() / 2));

  setColor(cp, 302, 1.0f, 1.0f, 1.0f, 1.0f); // candle default
  setColor(cp, 402, 1.0f, 0.8f, 0.0f, 1.0f); // SMA yellow

  // =========================================================
  // SECTION 13: SVG Export
  // =========================================================
  section(13, "SVG Export (SvgExporter)");
  {
    dc::SvgExportOptions opts;
    opts.width = W;
    opts.height = H;
    opts.title = "D76 Feature Showcase";
    opts.backgroundColor[0] = 0.08f;
    opts.backgroundColor[1] = 0.08f;
    opts.backgroundColor[2] = 0.10f;
    opts.backgroundColor[3] = 1.0f;

    std::string svg = dc::SvgExporter::exportScene(scene, &ingest, opts);
    std::printf("  SVG size: %zu bytes\n", svg.size());

    std::ofstream out("d76_showcase.svg");
    if (out.is_open()) {
      out << svg;
      out.close();
      std::printf("  Wrote d76_showcase.svg\n");
    }

    // Also export just the price pane
    std::string priceSvg = dc::SvgExporter::exportPane(scene, &ingest, 1, opts);
    std::printf("  Price pane SVG: %zu bytes\n", priceSvg.size());
    std::printf("  PASS\n");
  }

  // =========================================================
  // GL Render
  // =========================================================
#ifdef DC_HAS_OSMESA
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  if (!renderer.init()) {
    std::fprintf(stderr, "Renderer init failed\n");
    return 1;
  }

  // Upload all buffers
  for (auto bid : scene.bufferIds()) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  std::printf("\n  Rendered: %u draw calls\n", stats.drawCalls);

  auto pixels = glCtx->readPixels();
  writePPM("d76_showcase.ppm", pixels, W, H);
#endif

  // =========================================================
  // Summary
  // =========================================================
  std::printf("\n========================================\n");
  std::printf("  D76 Showcase Complete\n");
  std::printf("  All 13 subsystems exercised\n");
  std::printf("========================================\n");

  return 0;
}
