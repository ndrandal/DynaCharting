// ENC-1257 — bar sizing: candle width and gap as a function of bar count.
//
// The acceptance check for SPEC `specs/2026-09-19-chart-quality-bar/SPEC.md`
// D1 tier 2 ("Inter-bar gap scales with bar count"), plus the regression pin for
// §1.1's fused-slab still.
//
// WHY THIS TEST IS IN THE DEFAULT BUILD. LIMITATIONS.md **DC-L01**: the default
// `ctest` registers 190 of 233 tests and excludes EVERY Dawn render test at
// configure time, so a rule implemented only inside `dc_gpu` would ship with no
// check anybody runs. `dc::resolveBarWidth` therefore lives in `dc`, and
// `DawnInstancedCandleBackend` is a thin caller of it. The companion raster
// proof — that the resolved widths really do leave a clear pixel column between
// bodies — is `dc_enc1257_dawn_candle_gap`, which is Dawn-gated.
//
// Each check below FAILS on pre-ENC-1257 code (nothing resolved a width at all;
// the rendered body was `2*hw*sx*W/2` and the gap was whatever remained).
#include "dc/render/BarSizing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool cond, const std::string& name) {
  if (cond) { std::printf("  PASS: %s\n", name.c_str()); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name.c_str()); ++failed; }
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// --- the two measured frames the SPEC records ---------------------------------
// §1.0 live capture `harness/live-nexo-candles-60s.png`: a 1300px-wide plot.
static constexpr float kLivePlotPx = 1300.0f;
// §1.1 showcase still `apps/showcase/stills/candles-aapl.png`: snap-stills runs a
// 1180x760 viewport with a 380px side rail, so the canvas is 800 device px.
static constexpr float kShowcaseCanvasPx = 800.0f;

// candles-aapl's exact production numbers, read out of the committed view:
//   apps/showcase/views/candles-aapl/view.json   -> transform.sx
//   apps/showcase/views/candles-aapl/instruction.json:21
//        {"intraOffset":20,"kind":"const","value":0.4}   -> halfWidth
//   records.json: x is a record index, so the pitch is exactly 1.0 data unit.
static constexpr float kAaplSx = 0.011333333f;
static constexpr float kAaplHalfWidth = 0.4f;
static constexpr float kAaplPitchData = 1.0f;

// Pack a candle6 record array (stride 24: x, open, high, low, close, halfWidth).
static std::vector<std::uint8_t> candles(const std::vector<float>& xs, float hw) {
  std::vector<std::uint8_t> out(xs.size() * 24, 0);
  for (std::size_t i = 0; i < xs.size(); ++i) {
    const float rec[6] = {xs[i], 0.0f, 1.0f, -1.0f, 0.5f, hw};
    std::memcpy(out.data() + i * 24, rec, sizeof(rec));
  }
  return out;
}

int main() {
  std::printf("=== ENC-1257 bar sizing ===\n");
  const dc::BarSizingConfig cfg{};  // the shipped defaults

  // ---------------------------------------------------------------------------
  // [1] THE ACCEPTANCE CHECK: the tier-2 minimum inter-bar gap holds at 10 bars
  //     AND at 500 bars, on the plot width the live capture actually has.
  // ---------------------------------------------------------------------------
  {
    const dc::BarMetrics m10 = dc::barMetricsForCount(10, kLivePlotPx);
    const dc::BarMetrics m500 = dc::barMetricsForCount(500, kLivePlotPx);
    std::printf("  10 bars/1300px : pitch %.3f body %.3f gap %.3f\n",
                m10.pitchPx, m10.bodyPx, m10.gapPx);
    std::printf("  500 bars/1300px: pitch %.3f body %.3f gap %.3f\n",
                m500.pitchPx, m500.bodyPx, m500.gapPx);
    check(m10.gapPx >= cfg.minGapPx, "tier2: 10 bars -> gap >= 1px");
    check(m500.gapPx >= cfg.minGapPx, "tier2: 500 bars -> gap >= 1px");
    check(!m10.degraded && !m500.degraded,
          "tier2: neither 10 nor 500 bars is degraded at 1300px");
    check(m10.bodyPx >= cfg.minBodyPx && m500.bodyPx >= cfg.minBodyPx,
          "tier2: the body stays at least 1px at both counts");
    check(near(m10.bodyPx + m10.gapPx, m10.pitchPx, 1e-3f) &&
              near(m500.bodyPx + m500.gapPx, m500.pitchPx, 1e-3f),
          "body + gap == pitch at both counts");
    // The whole point of the rule: the gap is NOT a fixed fraction. 500 bars
    // cannot keep 10 bars' proportion and 10 bars must not keep 500 bars'.
    check(m10.gapPx > m500.gapPx * 5.0f,
          "the gap is a function of bar count, not a constant ratio");
  }

  // ---------------------------------------------------------------------------
  // [2] THE FUSED-SLAB REGRESSION (SPEC §1.1, `candles-aapl.png`).
  //     Pre-ENC-1257: pitch 4.533px, body 3.627px, gap 0.907px — a sub-pixel gap
  //     that does not always contain a sample point, so runs of candles fuse
  //     into solid slabs. This pins the arithmetic of that exact still.
  // ---------------------------------------------------------------------------
  {
    const float pxPerData = kAaplSx * kShowcaseCanvasPx * 0.5f;
    const float pitchPx = kAaplPitchData * pxPerData;
    const float oldBodyPx = 2.0f * kAaplHalfWidth * pxPerData;
    const float oldGapPx = pitchPx - oldBodyPx;
    std::printf("  candles-aapl pre-fix: pitch %.4f body %.4f gap %.4f\n",
                pitchPx, oldBodyPx, oldGapPx);
    check(near(pitchPx, 4.5333f, 1e-3f), "pin: candles-aapl pitch is 4.533px");
    check(oldGapPx < 1.0f,
          "pin: the SHIPPED still's inter-bar gap was sub-pixel (0.907px)");

    const dc::CandleBodyResolution r = dc::resolveCandleBodyClip(
        kAaplPitchData, kAaplHalfWidth, kAaplSx,
        static_cast<int>(kShowcaseCanvasPx));
    check(r.apply, "candles-aapl: the rule applies");
    std::printf("  candles-aapl post-fix: body %.4f gap %.4f halfClip %.6f\n",
                r.metrics.bodyPx, r.metrics.gapPx, r.halfWidthClip);
    check(r.metrics.gapPx >= cfg.minGapPx,
          "FUSED SLAB FIXED: candles-aapl now has a >= 1px inter-bar gap");
    check(r.metrics.bodyPx < oldBodyPx,
          "the body gave up the pixels the gap needed");
    check(r.metrics.clamped, "candles-aapl is reported as clamped");
    // The clip half-width the shader receives, cross-checked independently:
    // bodyPx px on an 800px viewport is bodyPx/800 of clip half-extent.
    check(near(r.halfWidthClip, r.metrics.bodyPx / kShowcaseCanvasPx, 1e-7f),
          "halfWidthClip == bodyPx / viewportW");
  }

  // ---------------------------------------------------------------------------
  // [3] THE OPPOSITE FAILURE, SAME ABSENT RULE (SPEC §1.0, live capture).
  //     Ten candles at a ~55px pitch with ~47px bodies: the bodies very nearly
  //     abut and have stopped reading as candles. The ceiling binds here and the
  //     floor does not.
  // ---------------------------------------------------------------------------
  {
    const dc::BarMetrics m = dc::resolveBarWidth(55.0f, 47.0f);
    std::printf("  live 55px pitch: body %.3f gap %.3f\n", m.bodyPx, m.gapPx);
    check(near(m.bodyPx, cfg.maxBodyPx, 1e-3f),
          "live: a 47px body is capped at the 24px ceiling");
    check(m.gapPx > 8.0f, "live: the gap widens well past its pre-fix ~8px");
    check(m.clamped && !m.degraded, "live: clamped, not degraded");
  }

  // ---------------------------------------------------------------------------
  // [4] NO EXISTING IN-REPO SCENE MOVES. Both Dawn candle render tests and the
  //     golden parity scene already sit inside the rule's bounds, so the rule
  //     must return their authored width UNCHANGED. If this ever fails, a Dawn
  //     render test is about to fail too — and DC-L01 means nobody would see it.
  // ---------------------------------------------------------------------------
  {
    // core/tests/d14_6_dawn_candle.cpp and d_enc558_instanced_grow.cpp:
    // 3 candles at cx -0.5/0.0/0.5, hw 0.15, identity transform, 128px wide.
    const dc::CandleBodyResolution a =
        dc::resolveCandleBodyClip(0.5f, 0.15f, 1.0f, 128);
    check(a.apply && near(a.halfWidthClip, 0.15f, 1e-6f),
          "unchanged: d14_6 / enc558 (3 bars, hw 0.15, 128px) keeps hw");
    // core/tests/parity_conformance.cpp sceneCandles: hw 0.12, 96px wide.
    const dc::CandleBodyResolution b =
        dc::resolveCandleBodyClip(0.5f, 0.12f, 1.0f, 96);
    check(b.apply && near(b.halfWidthClip, 0.12f, 1e-6f),
          "unchanged: golden parity volume/candles (hw 0.12, 96px) keeps hw");
    // core/demos/gallery.cpp: 10 candles, x pitch 0.18 clip, hw 0.06, 900px.
    const dc::CandleBodyResolution c =
        dc::resolveCandleBodyClip(0.18f, 0.06f, 1.0f, 900);
    // 0.06*900 = 54px body at an 81px pitch -> the ceiling binds. That is a
    // real defect in the demo, not a regression: 54px bodies are the §1.0
    // failure exactly.
    check(c.apply && c.metrics.clamped && near(c.metrics.bodyPx, 24.0f, 1e-3f),
          "gallery demo's 54px bodies are capped to 24px");
  }

  // ---------------------------------------------------------------------------
  // [5] A SINGLE BAR HAS NO INTER-BAR RELATION -> the rule must not apply, and
  //     the render must stay byte-identical to its pre-ENC-1257 self.
  // ---------------------------------------------------------------------------
  {
    check(!dc::resolveCandleBodyClip(0.0f, 0.15f, 1.0f, 128).apply,
          "no pitch -> rule does not apply (single-bar render unchanged)");
    check(!dc::resolveCandleBodyClip(0.5f, 0.15f, 1.0f, 0).apply,
          "no viewport -> rule does not apply");
    check(!dc::resolveCandleBodyClip(0.5f, 0.15f, 0.0f, 128).apply,
          "degenerate transform -> rule does not apply");
  }

  // ---------------------------------------------------------------------------
  // [6] THE PITCH IS MEASURED, NOT ASSUMED. The live path puts bars on a time
  //     axis where a missing bucket leaves a 2x delta and a duplicated tick
  //     leaves 0 — SPEC §1.0's "irregular gaps". A mean would be dragged by
  //     both; the median is not.
  // ---------------------------------------------------------------------------
  {
    auto pitch = [](const std::vector<float>& xs) {
      const std::vector<std::uint8_t> b = candles(xs, 0.4f);
      return dc::barPitchFromRecords(b.data(), b.size(), 24, 0);
    };
    check(near(pitch({0, 1, 2, 3, 4, 5}), 1.0f, 1e-6f), "pitch: uniform -> 1.0");
    check(near(pitch({0, 1, 2, 4, 5, 6}), 1.0f, 1e-6f),
          "pitch: a missing bucket (2x delta) does not move the median");
    check(near(pitch({0, 1, 1, 2, 3, 4}), 1.0f, 1e-6f),
          "pitch: a duplicated x (0 delta) is discarded, not averaged");
    check(near(pitch({100, 160, 220, 280}), 60.0f, 1e-4f),
          "pitch: a 60s time axis reads 60, not 1");
    check(pitch({7}) == 0.0f, "pitch: a single record has none");
    check(dc::barPitchFromRecords(nullptr, 0, 24, 0) == 0.0f,
          "pitch: null input is 0, not a crash");
    const std::vector<std::uint8_t> hwRecs = candles({0, 1, 2, 3}, 0.4f);
    check(near(dc::medianRecordField(hwRecs.data(), hwRecs.size(), 24, 20), 0.4f,
               1e-6f),
          "nominal half-width is read from record offset 20");
  }

  // ---------------------------------------------------------------------------
  // [7] THE DENSITY CEILING, STATED HONESTLY. Below minBody+minGap of pitch no
  //     assignment is legible; the rule says so instead of pretending.
  // ---------------------------------------------------------------------------
  {
    check(dc::maxLegibleBarCount(kLivePlotPx) == 650,
          "1300px carries at most 650 legible bars");
    check(dc::maxLegibleBarCount(kShowcaseCanvasPx) == 400,
          "800px carries at most 400 legible bars");
    const dc::BarMetrics over = dc::barMetricsForCount(2000, kLivePlotPx);
    check(over.degraded, "2000 bars on 1300px is reported degraded");
    check(over.gapPx > 0.0f && over.bodyPx > 0.0f,
          "degraded still splits the pitch rather than collapsing a side");
    check(near(over.bodyPx + over.gapPx, over.pitchPx, 1e-4f),
          "degraded still conserves the pitch");
    // 500 bars fits at 1300px and does NOT fit at 800px — the acceptance
    // criterion is a statement about a plot width, and this pins which.
    check(!dc::barMetricsForCount(500, kLivePlotPx).degraded &&
              dc::barMetricsForCount(500, kShowcaseCanvasPx).degraded,
          "500 bars fits 1300px and does not fit 800px");
  }

  // ---------------------------------------------------------------------------
  // [8] THE RULE IS IN PIXELS, SO IT IS INVARIANT TO THE DATA'S UNITS. The same
  //     ten bars expressed as indices, as epoch seconds and as epoch millis must
  //     resolve to the same body width in pixels — which is the property the old
  //     scale-free ratio did not have.
  // ---------------------------------------------------------------------------
  {
    // pitch 1 index unit at sx 0.02, pitch 60 s at sx 0.02/60, pitch 60000 ms.
    const dc::CandleBodyResolution byIndex =
        dc::resolveCandleBodyClip(1.0f, 0.4f, 0.02f, 900);
    const dc::CandleBodyResolution bySecond =
        dc::resolveCandleBodyClip(60.0f, 24.0f, 0.02f / 60.0f, 900);
    const dc::CandleBodyResolution byMilli =
        dc::resolveCandleBodyClip(60000.0f, 24000.0f, 0.02f / 60000.0f, 900);
    check(byIndex.apply && bySecond.apply && byMilli.apply, "units: all apply");
    check(near(byIndex.metrics.bodyPx, bySecond.metrics.bodyPx, 1e-2f) &&
              near(byIndex.metrics.bodyPx, byMilli.metrics.bodyPx, 1e-2f),
          "units: index / seconds / millis resolve the same pixel body");
  }

  // ---------------------------------------------------------------------------
  // [9] INVARIANTS ACROSS THE WHOLE RANGE. Sweep every bar count from 1 to 4000
  //     on both measured plot widths; nothing may go negative, overflow the
  //     pitch, or exceed the ceiling, and the gap must hold its floor on every
  //     non-degraded count.
  // ---------------------------------------------------------------------------
  {
    int violations = 0, degradedFrom = 0;
    for (const float W : {kLivePlotPx, kShowcaseCanvasPx}) {
      int firstDegraded = 0;
      for (int n = 1; n <= 4000; ++n) {
        const dc::BarMetrics m = dc::barMetricsForCount(n, W);
        const bool ok =
            m.bodyPx > 0.0f && m.gapPx >= 0.0f &&
            m.bodyPx <= cfg.maxBodyPx + 1e-3f &&
            near(m.bodyPx + m.gapPx, m.pitchPx, 1e-2f) &&
            (m.degraded || (m.gapPx >= cfg.minGapPx - 1e-4f &&
                            m.bodyPx >= cfg.minBodyPx - 1e-4f));
        if (!ok) { ++violations; if (violations < 4) std::fprintf(stderr,
            "    n=%d W=%.0f pitch=%.4f body=%.4f gap=%.4f degraded=%d\n",
            n, static_cast<double>(W), m.pitchPx, m.bodyPx, m.gapPx,
            m.degraded ? 1 : 0); }
        if (m.degraded && firstDegraded == 0) firstDegraded = n;
      }
      degradedFrom = firstDegraded;
      std::printf("  sweep W=%.0f: first degraded at n=%d (ceiling %d)\n",
                  static_cast<double>(W), firstDegraded,
                  dc::maxLegibleBarCount(W));
    }
    check(violations == 0, "sweep 1..4000 bars x 2 widths: no invariant broken");
    check(degradedFrom == dc::maxLegibleBarCount(kShowcaseCanvasPx) + 1,
          "degradation begins exactly one bar past maxLegibleBarCount");
  }

  // ---------------------------------------------------------------------------
  // [10] THE WICK IS PART OF THE MARK. `instancedCandle@1` draws the wick at a
  //      FIXED pixel width (1px half / 2px total, `wickHalfClip = 2/viewW`),
  //      which the transform does not touch. Sizing the body correctly and
  //      leaving the wick alone still fuses a dense chart, because at a 2.6px
  //      pitch the 2px wick has already eaten the gap. `maxMarkHalfPx` is the
  //      cap the backend applies to the wick as well.
  // ---------------------------------------------------------------------------
  {
    const float kWickHalfPx = 1.0f;  // the engine's fixed wick half-width
    auto wickAfterCap = [&](int bars, float W) {
      const dc::BarMetrics m = dc::barMetricsForCount(bars, W);
      return std::min(kWickHalfPx, m.maxMarkHalfPx);
    };
    const dc::BarMetrics m500 = dc::barMetricsForCount(500, kLivePlotPx);
    check(m500.maxMarkHalfPx < kWickHalfPx,
          "500 bars/1300px: the 2px wick alone would have eaten the gap");
    check(near(m500.pitchPx - 2.0f * wickAfterCap(500, kLivePlotPx),
               cfg.minGapPx, 1e-3f),
          "500 bars: capping the wick restores exactly the 1px gap");
    // Above a ~5px pitch the cap never binds — every existing scene is untouched.
    check(wickAfterCap(10, kLivePlotPx) == kWickHalfPx,
          "10 bars: the wick cap does not bind");
    const dc::CandleBodyResolution d146 =
        dc::resolveCandleBodyClip(0.5f, 0.15f, 1.0f, 128);
    check(d146.maxMarkHalfClip > 2.0f / 128.0f,
          "unchanged: d14_6's wick is wider than the cap, so the cap is inert");
    const dc::CandleBodyResolution aapl = dc::resolveCandleBodyClip(
        kAaplPitchData, kAaplHalfWidth, kAaplSx,
        static_cast<int>(kShowcaseCanvasPx));
    check(aapl.maxMarkHalfClip > 2.0f / kShowcaseCanvasPx,
          "candles-aapl: 4.53px pitch still affords the full 2px wick");
    // Whatever the counts, body and wick together never exceed the pitch minus
    // the gap floor — the property the whole rule exists to guarantee.
    int bad = 0;
    for (int n = 1; n <= 650; ++n) {
      const dc::BarMetrics m = dc::barMetricsForCount(n, kLivePlotPx);
      const float markHalf = std::max(m.bodyPx * 0.5f,
                                      std::min(kWickHalfPx, m.maxMarkHalfPx));
      if (m.pitchPx - 2.0f * markHalf < cfg.minGapPx - 1e-3f) ++bad;
    }
    check(bad == 0,
          "1..650 bars: body AND wick together always leave the 1px gap");
  }

  std::printf("=== ENC-1257 bar sizing: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
