// ENC-610 / ENC-611 (P2) — COLOR SCALES: numeric domain -> packed RGBA8 per row.
//
// WHAT THIS IS
// ------------
// The COLOR scales of RESEARCH §4.2 ("sequential color" + "diverging color"):
// a numeric magnitude is resolved through a color RAMP to a single packed RGBA8
// (u32) that the encode pass can write straight into a per-row color attribute.
// These extend the §4.2 SCALE family (Scale.hpp) — they reuse the SAME streaming
// O(Δ) RunningDomain reducer for auto-domain, and the SAME Domain/Range shapes.
//
//   SequentialColorScale (ENC-610): numeric domain -> a one-sided ramp. The domain
//   is the running [min,max] over an f32 column (heatmap / weather-radar magnitude
//   -> RGBA8/row, RESEARCH §4.2). The auto-domain EXTENDS as data grows, exactly
//   like LinearScale — same RunningDomain reducer, bit-identical to an O(N) rescan.
//
//   DivergingColorScale (ENC-611): numeric domain -> a TWO-sided ramp around a
//   FIXED midpoint (correlation −1..+1 -> red…neutral…green). The two halves
//   [min,mid] and [mid,max] are interpolated independently so the mid color always
//   lands exactly at `mid`, regardless of how lopsided the ends are.
//
// THE CLASS-4 BASELINE POLICY (ENC-611 — RESEARCH §4.2 last paragraph)
// -------------------------------------------------------------------
// Diverging-mid is a CLASS-4 drifting domain: as the stream grows, the [min,max]
// ends drift, so the mapping from a value to a color has NO well-defined streaming
// semantics UNLESS the manifest declares how the baseline (the reference frame the
// mid is anchored in) is held over time. RESEARCH §4.2 names three policies:
//   * fixedEpoch      — anchor the domain to a fixed reference epoch / fixed ends;
//   * decaying        — exponentially-decayed running stats (recent data weighed);
//   * referenceWindow — a sliding window of the last N rows defines the ends.
// A class-4 scale constructed WITHOUT a baseline policy is REJECTED (a diverging
// scale that auto-domains off raw running stats would silently re-anchor its mid
// every tick — there is no defensible streaming meaning). The rejection is at
// construction (the factory returns nullptr) — see makeDivergingColorScale().
#pragma once

#include "dc/scale/Scale.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// Rgba8 — a color packed to 8 bits per channel. toU32() yields the WebGPU
// `RGBA8Unorm` memory layout: bytes in order R,G,B,A, i.e. a little-endian u32
// reading (a<<24)|(b<<16)|(g<<8)|r. This is EXACTLY the 4 bytes the encode pass
// writes into a per-row color attribute, so the GPU samples it without a swizzle.
// ---------------------------------------------------------------------------
struct Rgba8 {
  std::uint8_t r{0}, g{0}, b{0}, a{255};

  std::uint32_t toU32() const {
    return static_cast<std::uint32_t>(r) |
           (static_cast<std::uint32_t>(g) << 8) |
           (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(a) << 24);
  }

  static Rgba8 fromU32(std::uint32_t v) {
    return Rgba8{static_cast<std::uint8_t>(v & 0xFF),
                 static_cast<std::uint8_t>((v >> 8) & 0xFF),
                 static_cast<std::uint8_t>((v >> 16) & 0xFF),
                 static_cast<std::uint8_t>((v >> 24) & 0xFF)};
  }

  // Build from 0..1 floats (the encode-layer Rgba shape). Values are clamped to
  // [0,1] and rounded to the nearest 8-bit level (round-half-up).
  static Rgba8 fromFloats(float r01, float g01, float b01, float a01 = 1.0f);

  bool operator==(const Rgba8& o) const {
    return r == o.r && g == o.g && b == o.b && a == o.a;
  }
};

// ---------------------------------------------------------------------------
// ColorStop — one anchor of a ramp: a position t in [0,1] and its color. A ramp
// is an ordered list of stops; a value's t is interpolated linearly (per channel,
// in 8-bit sRGB-encoded space — the cheap, standard heatmap interpolation) between
// the two bracketing stops. The first stop should be t==0 and the last t==1.
// ---------------------------------------------------------------------------
struct ColorStop {
  double t{0.0};
  Rgba8 color;
};

// ---------------------------------------------------------------------------
// ColorRamp — an ordered stop list + the interpolator. sample(t) clamps t to
// [0,1], finds the bracketing stops, and lerps per channel. A ramp with a single
// stop is a constant color; an empty ramp samples to opaque black.
// ---------------------------------------------------------------------------
class ColorRamp {
 public:
  ColorRamp() = default;
  explicit ColorRamp(std::vector<ColorStop> stops) : stops_(std::move(stops)) {}

  bool empty() const { return stops_.empty(); }
  std::size_t size() const { return stops_.size(); }
  const std::vector<ColorStop>& stops() const { return stops_; }

  // Sample the ramp at parameter t (clamped to [0,1]).
  Rgba8 sample(double t) const;

  // ----- named ramps (a small built-in set, RESEARCH §4.2) -------------------
  // A manifest may name one of these instead of supplying an explicit stop list.

  // viridis — the perceptually-uniform sequential default (5-stop approximation).
  static ColorRamp viridis();
  // magma — sequential, black->purple->orange->near-white (5-stop approximation).
  static ColorRamp magma();
  // A simple 2-stop blue->red sequential ramp (a plain magnitude ramp).
  static ColorRamp blueRed();
  // redNeutralGreen — the canonical 3-stop DIVERGING ramp (red…white…green).
  static ColorRamp redNeutralGreen();

  // Resolve a named ramp by string (manifest `ramp: "viridis"` etc). Returns true
  // and fills `out` on a known name; false (out untouched) on an unknown name.
  static bool byName(const std::string& name, ColorRamp& out);

 private:
  std::vector<ColorStop> stops_;
};

// ===========================================================================
// ENC-610 — SequentialColorScale: numeric -> ramp -> RGBA8
// ===========================================================================
//
// map() returns the RAMP PARAMETER t in [0,1] for a value (so the Scale numeric
// contract still holds — t is the "range" here, [0,1]); mapColor() returns the
// resolved Rgba8 and mapU32() the packed u32 the encode pass writes. Auto-domain
// reuses RunningDomain over an f32 column, EXACTLY like LinearScale (the running
// [min,max] becomes the domain; the ramp spans that live range).
class SequentialColorScale : public Scale {
 public:
  SequentialColorScale() : ramp_(ColorRamp::viridis()) { range_ = Range{0.0, 1.0}; }
  explicit SequentialColorScale(ColorRamp ramp) : ramp_(std::move(ramp)) {
    range_ = Range{0.0, 1.0};
  }
  SequentialColorScale(const Domain& d, ColorRamp ramp)
      : ramp_(std::move(ramp)) {
    domain_ = d;
    range_ = Range{0.0, 1.0};
  }

  ScaleType type() const override { return ScaleType::Color; }

  const ColorRamp& ramp() const { return ramp_; }
  void setRamp(ColorRamp r) { ramp_ = std::move(r); }

  // Scale::map — the ramp PARAMETER t in [0,1] for a domain value (clamped). A
  // degenerate domain (empty / all-equal) parks every value at t=0.5 (the ramp
  // midpoint), mirroring LinearScale's "park it in the middle".
  double map(double domainValue) const override;

  // Inverse of map(): a ramp parameter t in [0,1] -> the domain value at that t.
  double invert(double rangeValue) const override;

  // The resolved color / packed u32 for a value (the encode-pass output).
  Rgba8 mapColor(double domainValue) const { return ramp_.sample(map(domainValue)); }
  std::uint32_t mapU32(double domainValue) const { return mapColor(domainValue).toU32(); }

  // ----- streaming auto-domain (reused from LinearScale's machinery) ---------
  void bindColumn(Id tableId, std::string columnName);
  bool hasBoundColumn() const { return bound_; }
  bool updateDomain(const TableStore& tables, const BufferByteSource& src);
  const RunningDomain& runningDomain() const { return running_; }

 private:
  ColorRamp ramp_;
  RunningDomain running_;
  Id boundTable_{kInvalidId};
  std::string boundColumn_;
  bool bound_{false};
};

// ===========================================================================
// ENC-611 — Diverging color scale + class-4 baseline policy
// ===========================================================================

// BaselinePolicyKind — how a CLASS-4 drifting domain anchors its baseline over the
// stream (RESEARCH §4.2). `None` is the sentinel for "no policy declared" and is
// what makes a diverging scale REJECT at construction.
enum class BaselinePolicyKind : std::uint8_t {
  None,             // no policy => class-4 scale is REJECTED
  FixedEpoch,       // anchor to fixed reference ends (no drift)
  Decaying,         // exponentially-decayed running stats
  ReferenceWindow,  // sliding window of the last N rows
};

// BaselinePolicy — the declared baseline policy for a class-4 scale. `valid()` is
// false for None; the factory rejects an invalid policy.
struct BaselinePolicy {
  BaselinePolicyKind kind{BaselinePolicyKind::None};
  double decay{0.0};            // FixedEpoch/decay factor (Decaying); 0 otherwise
  std::size_t windowRows{0};    // sliding-window size (ReferenceWindow)

  bool valid() const { return kind != BaselinePolicyKind::None; }

  static BaselinePolicy fixedEpoch() {
    return BaselinePolicy{BaselinePolicyKind::FixedEpoch, 0.0, 0};
  }
  static BaselinePolicy decaying(double d) {
    return BaselinePolicy{BaselinePolicyKind::Decaying, d, 0};
  }
  static BaselinePolicy referenceWindow(std::size_t n) {
    return BaselinePolicy{BaselinePolicyKind::ReferenceWindow, 0.0, n};
  }

  // Resolve a policy by manifest name. Returns true (fills `out`) on a known name
  // ("fixedEpoch" | "decaying" | "referenceWindow"); false on unknown/missing.
  static bool byName(const std::string& name, BaselinePolicy& out);
};

// DivergingColorScale — a two-sided ramp around a FIXED midpoint. The domain is
// [min,max] with a fixed `mid` (default 0). map() interpolates [min,mid] -> [0,0.5]
// and [mid,max] -> [0.5,1] INDEPENDENTLY, so a value exactly at `mid` always lands
// on t=0.5 (the neutral ramp color) regardless of how lopsided the ends are —
// the symmetry the acceptance test asserts.
//
// CLASS-4: a diverging scale REQUIRES a baseline policy. Construct ONLY via
// makeDivergingColorScale() — it returns nullptr (rejection) when the policy is
// absent/invalid. The raw constructor is intentionally non-public so a policy-less
// diverging scale cannot exist.
class DivergingColorScale : public Scale {
 public:
  ScaleType type() const override { return ScaleType::Color; }

  double mid() const { return mid_; }
  void setMid(double m) { mid_ = m; }

  const ColorRamp& ramp() const { return ramp_; }
  void setRamp(ColorRamp r) { ramp_ = std::move(r); }

  const BaselinePolicy& baselinePolicy() const { return policy_; }

  // Scale::map — ramp parameter t in [0,1], with the two halves interpolated
  // independently around `mid` (mid -> 0.5). A degenerate side (min==mid or
  // max==mid) collapses that side to its endpoint t without div-by-zero.
  double map(double domainValue) const override;

  // Inverse of map(): t in [0,1] -> the domain value (piecewise around mid).
  double invert(double rangeValue) const override;

  Rgba8 mapColor(double domainValue) const { return ramp_.sample(map(domainValue)); }
  std::uint32_t mapU32(double domainValue) const { return mapColor(domainValue).toU32(); }

  // ----- streaming auto-domain -----------------------------------------------
  // Same RunningDomain reducer; the running [min,max] become the ends while `mid`
  // stays fixed (the baseline policy governs how the ends are *meant* to be held —
  // ENC-611 records the policy; the running-stats binding is the FixedEpoch-style
  // accumulation, the only one with a streaming O(Δ) implementation in this PR).
  void bindColumn(Id tableId, std::string columnName);
  bool hasBoundColumn() const { return bound_; }
  bool updateDomain(const TableStore& tables, const BufferByteSource& src);
  const RunningDomain& runningDomain() const { return running_; }

 private:
  // Private: a diverging scale may ONLY be made through the policy-checked factory.
  friend std::unique_ptr<DivergingColorScale> makeDivergingColorScale(
      double mid, ColorRamp ramp, const BaselinePolicy& policy);
  DivergingColorScale() : ramp_(ColorRamp::redNeutralGreen()) {
    range_ = Range{0.0, 1.0};
  }

  double mid_{0.0};
  ColorRamp ramp_;
  BaselinePolicy policy_;
  RunningDomain running_;
  Id boundTable_{kInvalidId};
  std::string boundColumn_;
  bool bound_{false};
};

// Factory for a class-4 diverging scale. Returns nullptr (REJECTION) if `policy`
// is invalid (BaselinePolicyKind::None) — a diverging scale without a declared
// baseline policy has no well-defined streaming semantics (RESEARCH §4.2) and is
// rejected at construction/load. On success the scale carries the policy.
std::unique_ptr<DivergingColorScale> makeDivergingColorScale(
    double mid, ColorRamp ramp, const BaselinePolicy& policy);

}  // namespace dc
