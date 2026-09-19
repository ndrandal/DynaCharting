#pragma once
// ENC-1257 — bar sizing: candle width and gap as a function of bar count.
//
// THE PROBLEM THIS SOLVES. Until now nothing in the engine had an opinion about
// how wide a bar should be. `instancedCandle@1` takes a per-instance `halfWidth`
// in DATA units and scales it by whatever transform the chart happens to carry,
// so the rendered body width is `2*hw * sx * viewportW/2` px and the inter-bar
// gap is whatever is left over. Authors supply a constant ratio (0.4 of a
// 1.0 index step, i.e. 80% body / 20% gap is the usual choice) and that ratio is
// then evaluated at an arbitrary pixel pitch. The same constant therefore
// produces two OPPOSITE failures, both observed and both recorded in
// `specs/2026-09-19-chart-quality-bar/SPEC.md`:
//
//   * §1.1 showcase `candles-aapl.png` — 156 bars, sx=0.011333333, 800px wide.
//     Pitch is 4.53px, the 80% body is 3.63px and the gap is **0.90px**. A
//     sub-pixel gap does not rasterise, so runs of candles fuse into solid slabs
//     and stop reading as bars at all.
//   * §1.0 live `live-nexo-candles-60s.png` — ten candles at a ~55px pitch with
//     ~47px bodies. The same 0.85 ratio now yields wide blocks whose edges very
//     nearly touch; a candle body that wide has stopped reading as a candle.
//
// Both are the missing rule, not the wrong constant: a ratio is scale-free and
// legibility is not. The gap has to have a PIXEL floor and the body a PIXEL
// ceiling, because the reader's eye works in pixels.
//
// THE RULE (`resolveBarWidth`). Given the bar pitch in pixels and the author's
// nominal body width in pixels:
//
//   floorGap = max(minGapPx, pitchPx * minGapFraction)
//   bodyPx   = clamp(nominalBodyPx, minBodyPx, min(pitchPx - floorGap, maxBodyPx))
//   gapPx    = pitchPx - bodyPx
//
// so the author still chooses the *proportion* and the rule enforces the two
// absolute limits. When the pitch is too small to honour `minBodyPx + minGapPx`
// at once — below ~2px of pitch there is no arrangement of ink and space that a
// reader can resolve — the pitch is split in that same proportion and the result
// is flagged `degraded`, rather than silently pretending a gap exists.
//
// This header is deliberately in `dc` (no graphics-API dependency) so the rule
// is unit-testable in the DEFAULT cmake build. That matters here specifically:
// LIMITATIONS.md **DC-L01** — the default `ctest` excludes every Dawn render
// test, so a rule that lived only inside `dc_gpu` would have no check that
// anybody runs. `DawnInstancedCandleBackend` is a thin caller of these
// functions.

#include <cstddef>
#include <cstdint>

namespace dc {

// Limits for `resolveBarWidth`. Every field is in PIXELS except the fractions.
struct BarSizingConfig {
  // The minimum inter-bar gap, and 1.0 is not a round number picked by taste.
  // The engine rasterises with pixel-CENTRE sampling and no MSAA (the Dawn
  // offscreen target has none, and the browser path blits with putImageData, so
  // nothing downstream softens it either). A gap spanning [a, a+w) contains a
  // sample point k+0.5 for EVERY a exactly when w >= 1: at w = 1 there is always
  // exactly one clear pixel column between neighbouring bodies, and at w = 0.9
  // there is one only for some alignments — which is precisely why the showcase
  // candles fuse in runs rather than uniformly. One pixel is therefore the
  // smallest gap that is guaranteed to appear at all, not merely likely to.
  float minGapPx{1.0f};
  // A bar must keep at least this much ink or it stops being a mark.
  float minBodyPx{1.0f};
  // Above this a candle body reads as a block, not a bar. Chosen at 24px: the
  // widest existing in-repo candle configuration is 19.2px (the d14_6 Dawn
  // render test, 3 bars at a 32px pitch), so the ceiling does not disturb any
  // scene that was already reasonable, and it does bind on the live capture's
  // ~47px bodies.
  float maxBodyPx{24.0f};
  // At a generous pitch a 1px gap is legible but mean. The gap floor is
  // therefore also a fraction of the pitch, whichever is larger.
  float minGapFraction{0.15f};
  // Used only when the caller has no nominal width to offer.
  float defaultBodyFraction{0.8f};
};

struct BarMetrics {
  float pitchPx{0.0f};  // centre-to-centre spacing
  float bodyPx{0.0f};   // resolved body width
  float gapPx{0.0f};    // pitchPx - bodyPx
  // The pitch could not carry minBodyPx + minGapPx; both minima were scaled
  // down proportionally. The chart is over-dense — the fix is fewer bars, not a
  // different width.
  bool degraded{false};
  // The rule moved the body away from the caller's nominal width.
  bool clamped{false};
  // The widest HALF-extent any part of the mark may occupy and still leave the
  // gap. The body is already inside it; the WICK is not automatically, because
  // `instancedCandle@1` draws the wick at a FIXED pixel width (1px half / 2px
  // total) that ignores the transform entirely. At a 2.6px pitch — 500 bars on
  // the live capture's plot — a 2px wick alone consumes everything a 1px gap
  // needs, so sizing the body correctly and leaving the wick alone still
  // produces a fused slab. The wick must be capped by this too.
  float maxMarkHalfPx{0.0f};
};

// The rule. `nominalBodyPx <= 0` means "no author preference" and falls back to
// `cfg.defaultBodyFraction` of the pitch. A non-positive `pitchPx` is returned
// unresolved (`degraded`, body = nominal) because there is no inter-bar
// relation to reason about.
BarMetrics resolveBarWidth(float pitchPx, float nominalBodyPx,
                           const BarSizingConfig& cfg = BarSizingConfig{});

// Convenience for the "N bars evenly across a plot" framing: pitch = W/N.
BarMetrics barMetricsForCount(int barCount, float plotWidthPx,
                              float nominalBodyFraction = -1.0f,
                              const BarSizingConfig& cfg = BarSizingConfig{});

// The consequence nobody had written down: a minimum-gap guarantee is a CEILING
// ON BAR COUNT. Below `minBodyPx + minGapPx` of pitch no width assignment is
// legible, so `floor(plotWidthPx / (minBodyPx + minGapPx))` is the largest bar
// count this plot can carry — 650 at the live capture's 1300px plot, 400 at the
// showcase's 800px. Past it `resolveBarWidth` reports `degraded` and the honest
// fix is aggregation (see dc::CandleAggregator) or a wider plot, not a thinner
// bar.
int maxLegibleBarCount(float plotWidthPx,
                       const BarSizingConfig& cfg = BarSizingConfig{});

// Robust bar pitch, in DATA units, read out of a packed record array (candle6
// is stride 24, x at offset 0). Returns the MEDIAN strictly-positive delta
// between consecutive x values, which is what makes it survive the live path:
// bars arrive on a time axis where a missing bucket leaves a 2x delta and a
// duplicate leaves a 0. Returns 0 when fewer than two distinct x are present —
// the caller must treat that as "no rule applies".
float barPitchFromRecords(const std::uint8_t* bytes, std::size_t byteLen,
                          std::uint32_t strideBytes, std::uint32_t xOffsetBytes,
                          std::size_t maxSamples = 1024);

// Median of a positive float field across the same records (used for the
// nominal half-width at offset 20 of a candle6 record). 0 when none is finite
// and positive.
float medianRecordField(const std::uint8_t* bytes, std::size_t byteLen,
                        std::uint32_t strideBytes, std::uint32_t fieldOffsetBytes,
                        std::size_t maxSamples = 1024);

// What `instancedCandle@1` needs: the effective body HALF-width in CLIP units.
//
//   pxPerData = |xScale| * viewportW / 2        (mat3 column 0, x row)
//   halfClip  = bodyPx / viewportW              (bodyPx px -> 2*bodyPx/W clip)
//
// `apply == false` means the rule does not apply (no pitch, no viewport, or a
// degenerate transform) and the caller must fall back to the per-instance
// halfWidth exactly as before — which is what keeps a single-bar scene
// byte-identical to its pre-ENC-1257 render.
struct CandleBodyResolution {
  bool apply{false};
  float halfWidthClip{0.0f};
  // The widest the fixed-pixel wick may be drawn, in clip units, before it eats
  // the inter-bar gap. The caller takes min(its own wick half-width, this).
  float maxMarkHalfClip{0.0f};
  BarMetrics metrics{};
};

CandleBodyResolution resolveCandleBodyClip(
    float pitchData, float nominalHalfData, float xScale, int viewportW,
    const BarSizingConfig& cfg = BarSizingConfig{});

}  // namespace dc
