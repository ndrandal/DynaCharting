// ENC-1249 — TIER 0 ("Truthful"): prove a chart DEPICTS ITS DATA.
//
// specs/2026-09-19-chart-quality-bar/SPEC.md D1 defines tier 0 as the claim "the
// mark depicts the data", and its falsifiable check as: render a KNOWN-ANSWER
// synthetic series and assert on pixels. This file is that check.
//
// Synthetic data is correct HERE AND ONLY HERE. SPEC D2 bans synthetic feeds for
// the *reference chart* and permits them for tier-0 assertions, because a known
// answer is the entire point: you cannot assert "the last sample is above the
// first" against a series whose true shape you do not know.
//
// WHAT IS ASSERTED
// ----------------
//   A. RAMP (the canonical case). A monotonically increasing series, authored
//      through the real LineRecipe (lineAA@1) with its data->clip mapping done by
//      the real dc::LinearScale, rendered by DawnSceneRenderer. Assertions:
//        A1  something is drawn at all (a blank frame must never pass);
//        A2  every sample's x column carries lit pixels;
//        A3  the LAST sample's lit pixel is ABOVE the first (SPEC D1, verbatim);
//        A4  the per-sample rows are STRICTLY monotonically upward;
//        A5  each sample's row equals the row the scale predicts (+-4px) — the
//            mark is not merely ordered correctly, it is in the right PLACE.
//
//   B. SINGLE CANDLE (the per-mark case) with hand-computed extents, authored
//      through the real CandleRecipe (instancedCandle@1), identity transform, so
//      data y IS clip y and every expected row is arithmetic:
//        B1  the mark's full vertical extent at cx is exactly low..high;
//        B2  the BODY column's lit span is exactly open..close;
//        B3  the wick strictly overhangs the body at BOTH ends, and B3b that the
//            overhang is THIN (a body that is secretly the wick, or vice versa,
//            dies on B2+B3+B3b);
//        B4  an UP candle (close >= open) is the up colour and a DOWN candle the
//            down colour;
//        B5  the gap between two candles is clear (the marks are marks, not a slab).
//
// WHICH RASTER (this is load-bearing — LIMITATIONS.md DC-L05)
// -----------------------------------------------------------
// Every Dawn backend shader negates clip-space y (`vec4(p.x, -p.y, ...)`) while
// DawnDevice's readback is faithfully top-down. Net effect: the RAW readback is
// VERTICALLY MIRRORED relative to what a user sees. The browser compensates at the
// one place frames are painted (EngineHost.blitFramebuffer -> flipRowsRGBA).
//
// "Up is up" is a claim about the PRESENTED raster, so this check renders, then
// applies the same row flip the browser applies, and asserts on THAT. Asserting on
// the raw readback would bake the mirror into the standard and would have "proved"
// that a rising series falls.
//
//   (core/tests/d14_6_dawn_candle.cpp's FILE HEADER claims the opposite — that the
//    high lands near the framebuffer TOP. Its own inline comment 40 lines later
//    says the correct thing, and its assertions are up/down symmetric so they
//    cannot tell the two apart. Fixed in the same commit as this file.)
//
// FALSIFIABILITY (SPEC section 1.4: a claim without a check is an impression)
// ---------------------------------------------------------------------------
// A check never seen to fail is not a check, so this binary ships its own negative
// controls and they are registered as ctest cases with WILL_FAIL:
//
//   --invert-data     feed a deliberately WRONG series: the ramp descends, and each
//                     candle's (open,close) and (high,low) pairs are swapped so the
//                     body carries the wick's extents and the wick the body's.
//   --invert-render   present the RAW readback, i.e. skip the DC-L05 flip — the
//                     exact silent failure DC-L05 warns a third consumer will hit.
//
// Both MUST exit non-zero. scripts/tier0.sh runs all three in one command.
//
// Dawn is required: this check is about pixels, and there is no pixel without a
// renderer. If no adapter comes up it exits 3 (CANNOT RUN) — never 0. On a headless
// box force lavapipe: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnSceneRenderer.hpp"

#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scale/Scale.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Result bookkeeping
// ---------------------------------------------------------------------------
int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what, const std::string& detail) {
  if (ok) {
    ++g_passed;
    std::printf("  PASS  %-58s %s\n", what.c_str(), detail.c_str());
  } else {
    ++g_failed;
    std::printf("  FAIL  %-58s %s\n", what.c_str(), detail.c_str());
  }
}

template <typename... Args>
std::string fmt(const char* f, Args... args) {
  char buf[512];
  std::snprintf(buf, sizeof(buf), f, args...);
  return buf;
}

// ---------------------------------------------------------------------------
// The presented raster
// ---------------------------------------------------------------------------
struct Frame {
  int w{0};
  int h{0};
  std::vector<std::uint8_t> rgba;  // row 0 == TOP of the chart as a user sees it

  const std::uint8_t* at(int x, int y) const {
    static const std::uint8_t kZero[4] = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= w || y >= h) return kZero;
    return &rgba[(static_cast<std::size_t>(y) * w + x) * 4];
  }
  // "Lit" == visibly brighter than the (black) pane clear. Deliberately low so an
  // antialiased fringe still counts; nothing in these scenes is dim on purpose.
  int lum(int x, int y) const {
    const std::uint8_t* p = at(x, y);
    int m = p[0];
    if (p[1] > m) m = p[1];
    if (p[2] > m) m = p[2];
    return m;
  }
  bool lit(int x, int y) const { return lum(x, y) >= 30; }
};

// Render the scene and return the raster to assert on.
//
// `present` applies the row flip EngineHost.blitFramebuffer applies on every
// browser frame (LIMITATIONS.md DC-L05). present=false is the --invert-render
// negative control: the raw, mirrored readback.
Frame renderPresented(dc::DawnSceneRenderer& r, const dc::Scene& scene,
                      dc::CpuBufferStore& store, int W, int H, bool present) {
  r.render(scene, store, W, H);

  std::vector<std::uint8_t> raw(static_cast<std::size_t>(W) * H * 4, 0);
  std::uint32_t gotW = 0, gotH = 0;
  if (!r.device().readFramebufferRGBA(raw.data(), raw.size(), &gotW, &gotH) ||
      gotW != static_cast<std::uint32_t>(W) ||
      gotH != static_cast<std::uint32_t>(H)) {
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        r.device().readPixel(x, y,
                             &raw[(static_cast<std::size_t>(y) * W + x) * 4]);
  }

  Frame f;
  f.w = W;
  f.h = H;
  f.rgba.assign(raw.size(), 0);
  const std::size_t rowBytes = static_cast<std::size_t>(W) * 4;
  for (int y = 0; y < H; ++y) {
    const int src = present ? (H - 1 - y) : y;
    std::memcpy(&f.rgba[static_cast<std::size_t>(y) * rowBytes],
                &raw[static_cast<std::size_t>(src) * rowBytes], rowBytes);
  }
  return f;
}

// clip-space -> presented pixel. The chart the user sees has +y up, so a larger
// clip y is a SMALLER row. This is the whole tier-0 claim, written once.
double rowOfClipY(double clipY, int H) { return (1.0 - clipY) * 0.5 * H; }
double colOfClipX(double clipX, int W) { return (clipX + 1.0) * 0.5 * W; }

// Lit-pixel statistics down one column.
struct ColumnSpan {
  int count{0};
  int top{-1};     // smallest lit row
  int bottom{-1};  // largest lit row
  double centroid{-1.0};
};

ColumnSpan scanColumn(const Frame& f, int x) {
  ColumnSpan s;
  double wsum = 0, wy = 0;
  for (int y = 0; y < f.h; ++y) {
    if (!f.lit(x, y)) continue;
    const double w = f.lum(x, y);
    ++s.count;
    if (s.top < 0) s.top = y;
    s.bottom = y;
    wsum += w;
    wy += w * y;
  }
  if (wsum > 0) s.centroid = wy / wsum;
  return s;
}

int litPixels(const Frame& f) {
  int n = 0;
  for (int y = 0; y < f.h; ++y)
    for (int x = 0; x < f.w; ++x)
      if (f.lit(x, y)) ++n;
  return n;
}

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------
struct Mode {
  bool invertData{false};    // feed a deliberately wrong series
  bool invertRender{false};  // present the raw (mirrored) readback
};

// ===========================================================================
// CASE A — the ramp
// ===========================================================================
//
// Nine samples of a strictly increasing series. The data->clip mapping is done by
// the real dc::LinearScale over the real auto-folded Domain, so this covers
// data -> clip -> geometry -> pixels, not just the renderer.
constexpr int kRampN = 9;
constexpr double kRampClipLo = -0.85;
constexpr double kRampClipHi = 0.85;

void caseRamp(dc::DawnSceneRenderer& renderer, const Mode& mode) {
  constexpr int W = 512;
  constexpr int H = 256;

  std::printf("\n-- A. RAMP: a monotonically increasing series (%d samples) --\n",
              kRampN);

  double values[kRampN];
  for (int i = 0; i < kRampN; ++i) {
    // Honest: 100, 110, ... 180 — strictly increasing.
    // --invert-data: the same values in reverse — strictly DECREASING. The
    // assertions below are unchanged; they are the assertions for a RISING ramp,
    // and a falling ramp must break them.
    const int k = mode.invertData ? (kRampN - 1 - i) : i;
    values[i] = 100.0 + 10.0 * k;
  }

  dc::Domain yDomain;
  for (double v : values) yDomain.fold(v);
  dc::LinearScale yScale;
  yScale.setDomain(yDomain);
  yScale.setRange(kRampClipLo, kRampClipHi);

  dc::LinearScale xScale;
  xScale.setDomain(0.0, static_cast<double>(kRampN - 1));
  xScale.setRange(kRampClipLo, kRampClipHi);

  double clipX[kRampN], clipY[kRampN];
  for (int i = 0; i < kRampN; ++i) {
    clipX[i] = xScale.map(i);
    clipY[i] = yScale.map(values[i]);
  }

  // LineRecipe takes rect4 segment records (x0,y0,x1,y1), two endpoints each.
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>(kRampN - 1) * 4);
  for (int i = 0; i + 1 < kRampN; ++i) {
    segs.push_back(static_cast<float>(clipX[i]));
    segs.push_back(static_cast<float>(clipY[i]));
    segs.push_back(static_cast<float>(clipX[i + 1]));
    segs.push_back(static_cast<float>(clipY[i + 1]));
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneClearColor","id":1,"r":0,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  dc::LineRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.layerId = 2;
  cfg.name = "Ramp";
  cfg.createTransform = false;  // identity: clip space is the scale's range
  dc::LineRecipe rec(100, cfg);
  for (const auto& c : rec.build().createCommands) cp.applyJsonText(c);
  cp.applyJsonText(
      R"({"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":)" +
      std::to_string((kRampN - 1) * 2) + "}");
  cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":102,"r":1,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":102,"lineWidth":3})");
  store.setCpuData(100, segs.data(),
                   static_cast<std::uint32_t>(segs.size() * sizeof(float)));

  const Frame f =
      renderPresented(renderer, scene, store, W, H, !mode.invertRender);

  // ---- A1: something was drawn.
  const int ink = litPixels(f);
  check(ink > 200, "A1 the frame is not blank",
        fmt("lit=%d px (need > 200)", ink));

  // ---- A2/A5: every sample's column, measured against the scale's prediction.
  double measured[kRampN];
  bool allColumnsLit = true;
  bool allInPlace = true;
  const double kPlaceTolPx = 4.0;
  for (int i = 0; i < kRampN; ++i) {
    const int x = static_cast<int>(std::lround(colOfClipX(clipX[i], W)));
    const ColumnSpan s = scanColumn(f, x);
    measured[i] = s.centroid;
    const double predicted = rowOfClipY(clipY[i], H);
    if (s.count == 0) {
      allColumnsLit = false;
      allInPlace = false;
      std::printf("        sample %d  x=%3d  NOTHING LIT IN COLUMN\n", i, x);
      continue;
    }
    const double err = std::fabs(s.centroid - predicted);
    if (err > kPlaceTolPx) allInPlace = false;
    std::printf("        sample %d  value=%6.1f  x=%3d  row=%6.2f  predicted=%6.2f  err=%5.2f\n",
                i, values[i], x, s.centroid, predicted, err);
  }
  check(allColumnsLit, "A2 every sample's x column carries lit pixels", "");
  check(allInPlace, "A5 every sample sits where the scale predicts",
        fmt("tolerance +-%.0f px", kPlaceTolPx));

  // ---- A3: SPEC D1, verbatim. The last sample's lit pixel is ABOVE the first.
  if (measured[0] >= 0 && measured[kRampN - 1] >= 0) {
    const double rise = measured[0] - measured[kRampN - 1];  // rows: up == smaller
    check(rise > H * 0.5,
          "A3 last sample's lit pixel is ABOVE the first",
          fmt("first row=%.2f, last row=%.2f, rise=%.2f px (need > %.0f)",
              measured[0], measured[kRampN - 1], rise, H * 0.5));
  } else {
    check(false, "A3 last sample's lit pixel is ABOVE the first",
          "a sample column was empty");
  }

  // ---- A4: strictly upward the whole way, not merely at the ends.
  bool monotone = true;
  for (int i = 0; i + 1 < kRampN; ++i) {
    if (measured[i] < 0 || measured[i + 1] < 0 ||
        measured[i + 1] >= measured[i] - 1.0) {
      monotone = false;
      std::printf("        not upward at %d->%d: row %.2f -> %.2f\n", i, i + 1,
                  measured[i], measured[i + 1]);
    }
  }
  check(monotone, "A4 rows are strictly monotonically upward", "");
}

// ===========================================================================
// CASE B — one candle, hand-computed extents
// ===========================================================================
//
// Identity transform, so data y IS clip y and every expected row is arithmetic:
//   row(y) = (1 - y)/2 * H.
//
//   high  =  0.80 -> row  25.6      body spans open..close  -> rows  76.8 ..153.6
//   close =  0.40 -> row  76.8      wick spans low ..high   -> rows  25.6 ..204.8
//   open  = -0.20 -> row 153.6
//   low   = -0.60 -> row 204.8
//
// The four values are deliberately ASYMMETRIC about y=0, so a vertical mirror of
// the frame cannot satisfy the expected spans. That is what makes --invert-render
// detectable here at all; a symmetric candle would pass upside down, which is
// precisely how DC-L05 survived undetected ("silent on any vertically symmetric
// scene").
constexpr float kOpen = -0.20f;
constexpr float kHigh = 0.80f;
constexpr float kLow = -0.60f;
constexpr float kClose = 0.40f;
constexpr float kHalfW = 0.20f;
constexpr float kCxUp = -0.45f;
constexpr float kCxDown = 0.45f;

void caseCandle(dc::DawnSceneRenderer& renderer, const Mode& mode) {
  constexpr int W = 256;
  constexpr int H = 256;

  std::printf("\n-- B. SINGLE CANDLE: hand-computed extents --\n");

  // Honest records. candle6 == (cx, open, high, low, close, halfWidth).
  float up[6] = {kCxUp, kOpen, kHigh, kLow, kClose, kHalfW};
  float down[6] = {kCxDown, kClose, kHigh, kLow, kOpen, kHalfW};  // open/close swapped

  if (mode.invertData) {
    // Deliberately wrong input: swap the BODY pair with the WICK pair, so the
    // body carries low..high and the wick carries open..close. Every span
    // assertion below must break. (Swapping high<->low alone would NOT be
    // detectable: the shader mixes low..high, so the wick draws the same span
    // either way — a real hole this control had to be designed around.)
    up[1] = kHigh;  up[2] = kOpen;  up[3] = kClose; up[4] = kLow;
    down[1] = kHigh; down[2] = kClose; down[3] = kOpen; down[4] = kLow;
  }

  float candles[12];
  std::memcpy(&candles[0], up, sizeof(up));
  std::memcpy(&candles[6], down, sizeof(down));

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneClearColor","id":1,"r":0,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  dc::CandleRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.layerId = 2;
  cfg.name = "OHLC";
  cfg.createTransform = false;  // identity
  cfg.colorUp[0] = 0.0f; cfg.colorUp[1] = 1.0f; cfg.colorUp[2] = 0.0f; cfg.colorUp[3] = 1.0f;
  cfg.colorDown[0] = 1.0f; cfg.colorDown[1] = 0.0f; cfg.colorDown[2] = 0.0f; cfg.colorDown[3] = 1.0f;
  dc::CandleRecipe rec(200, cfg);
  for (const auto& c : rec.build().createCommands) cp.applyJsonText(c);
  cp.applyJsonText(
      R"({"cmd":"setGeometryVertexCount","geometryId":201,"vertexCount":2})");
  store.setCpuData(200, candles, sizeof(candles));

  const Frame f =
      renderPresented(renderer, scene, store, W, H, !mode.invertRender);

  const double rowHigh = rowOfClipY(kHigh, H);
  const double rowClose = rowOfClipY(kClose, H);
  const double rowOpen = rowOfClipY(kOpen, H);
  const double rowLow = rowOfClipY(kLow, H);
  const double kSpanTolPx = 2.0;

  struct CandleProbe {
    const char* label;
    float cx;
    bool expectUp;
  };
  const CandleProbe probes[2] = {{"up", kCxUp, true}, {"down", kCxDown, false}};

  std::printf("        expected rows: high=%.1f close=%.1f open=%.1f low=%.1f\n",
              rowHigh, rowClose, rowOpen, rowLow);

  for (const CandleProbe& p : probes) {
    const int cxPx = static_cast<int>(std::lround(colOfClipX(p.cx, W)));
    // Body half-width in pixels: kHalfW * W/2 == 25.6 px. Probe 10 px off centre:
    // inside the body, clear of the ~1 px wick at cx.
    const int bodyX = cxPx + 10;

    const ColumnSpan wick = scanColumn(f, cxPx);
    const ColumnSpan body = scanColumn(f, bodyX);

    std::printf("        candle '%s'  cx=%d wick[%d..%d]  bodyX=%d body[%d..%d]\n",
                p.label, cxPx, wick.top, wick.bottom, bodyX, body.top,
                body.bottom);

    // B1 — the mark's FULL vertical extent at cx is low..high.
    //
    // Measured at cx this is the union of the wick and the body, because the body
    // straddles cx — so B1 alone cannot prove the wick is the thing reaching
    // low..high (a body drawn with the wick's extents satisfies it, which the
    // --invert-data control demonstrates). B1 + B2 + B3 + B3b together do pin it:
    // B1 fixes the total extent, B2 fixes the body, B3 fixes that something
    // overhangs the body at both ends, and B3b fixes that the overhang is a thin
    // wick rather than more body.
    const bool wickOk = wick.count > 0 &&
                        std::fabs(wick.top - rowHigh) <= kSpanTolPx &&
                        std::fabs(wick.bottom - rowLow) <= kSpanTolPx;
    check(wickOk,
          std::string("B1 [") + p.label + "] mark's full extent at cx is low..high",
          fmt("got [%d..%d] want [%.1f..%.1f]", wick.top, wick.bottom, rowHigh,
              rowLow));

    // B2 — the body column spans open..close.
    const bool bodyOk = body.count > 0 &&
                        std::fabs(body.top - rowClose) <= kSpanTolPx &&
                        std::fabs(body.bottom - rowOpen) <= kSpanTolPx;
    check(bodyOk, std::string("B2 [") + p.label + "] body spans open..close",
          fmt("got [%d..%d] want [%.1f..%.1f]", body.top, body.bottom, rowClose,
              rowOpen));

    // B3 — the wick overhangs the body at BOTH ends. A body drawn with the
    // wick's extents (or the reverse) fails here even if a tolerance slipped.
    const bool overhang = wick.count > 0 && body.count > 0 &&
                          (body.top - wick.top) > 30 &&
                          (wick.bottom - body.bottom) > 30;
    check(overhang,
          std::string("B3 [") + p.label + "] wick overhangs the body both ends",
          fmt("above=%d below=%d (need > 30 each)", body.top - wick.top,
              wick.bottom - body.bottom));

    // B3b — the part that overhangs the body is THIN: a wick, not more body.
    // Probed off-centre (inside the body's x span, clear of the ~1 px wick) at a
    // row just beyond each end of the body.
    const int aboveRow = static_cast<int>(std::lround(rowClose)) - 8;
    const int belowRow = static_cast<int>(std::lround(rowOpen)) + 8;
    const bool thin = !f.lit(bodyX, aboveRow) && !f.lit(bodyX, belowRow);
    check(thin, std::string("B3b [") + p.label + "] the overhang is a thin wick",
          fmt("x=%d rows %d/%d lum %d/%d (want dark)", bodyX, aboveRow, belowRow,
              f.lum(bodyX, aboveRow), f.lum(bodyX, belowRow)));

    // B4 — up/down colour follows close >= open.
    const int midRow = static_cast<int>(std::lround((rowClose + rowOpen) * 0.5));
    const std::uint8_t* c = f.at(bodyX, midRow);
    const bool colourOk = p.expectUp ? (c[1] > 180 && c[0] < 70)
                                     : (c[0] > 180 && c[1] < 70);
    check(colourOk,
          std::string("B4 [") + p.label + "] body carries the " +
              (p.expectUp ? "UP" : "DOWN") + " colour",
          fmt("rgb=(%d,%d,%d)", int(c[0]), int(c[1]), int(c[2])));
  }

  // B5 — the gap between the two candles is clear: these are marks, not a slab.
  const int gapX = W / 2;
  const ColumnSpan gap = scanColumn(f, gapX);
  check(gap.count == 0, "B5 the gap between candles is clear",
        fmt("x=%d lit=%d px", gapX, gap.count));
}

}  // namespace

int main(int argc, char** argv) {
  Mode mode;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--invert-data") == 0) mode.invertData = true;
    else if (std::strcmp(argv[i], "--invert-render") == 0) mode.invertRender = true;
    else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      std::fprintf(stderr,
                   "usage: dc_enc1249_tier0_truthful [--invert-data] "
                   "[--invert-render]\n");
      return 2;
    }
  }

  std::printf("=== ENC-1249 tier 0 (Truthful): does the chart depict its data? ===\n");
  std::printf("SPEC: specs/2026-09-19-chart-quality-bar/SPEC.md D1\n");
  if (mode.invertData || mode.invertRender) {
    std::printf("MODE: NEGATIVE CONTROL — %s%s%s. This run MUST FAIL.\n",
                mode.invertData ? "deliberately wrong DATA" : "",
                (mode.invertData && mode.invertRender) ? " + " : "",
                mode.invertRender ? "raw (unflipped) readback" : "");
  } else {
    std::printf("MODE: honest data, presented raster (DC-L05 flip applied).\n");
  }

  dc::DawnSceneRenderer renderer;
  if (!renderer.init()) {
    // Deliberately NOT a graceful skip. DC-L01's whole lesson is that a skip that
    // looks like a pass is how "190/190 passed" came to mean nothing about the
    // renderer. A tier-0 check that could not run is a tier-0 check that failed.
    std::fprintf(stderr,
                 "CANNOT RUN: no Dawn adapter (%s)\n"
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json\n",
                 renderer.errorMessage().c_str());
    return 3;
  }
  // SPEC D8: a render observation that does not name its adapter is inadmissible.
  std::printf("adapter: backend=%s name=\"%s\"\n",
              renderer.device().backendName().c_str(),
              renderer.device().adapterName().c_str());

  caseRamp(renderer, mode);
  caseCandle(renderer, mode);

  std::printf("\n=== tier 0: %d passed, %d failed ===\n", g_passed, g_failed);
  if (mode.invertData || mode.invertRender) {
    std::printf(
        "(negative control: a non-zero exit below is the EXPECTED outcome)\n");
  }
  return g_failed > 0 ? 1 : 0;
}
