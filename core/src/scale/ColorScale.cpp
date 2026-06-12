// ENC-610 / ENC-611 (P2) — COLOR SCALES implementation.
// See dc/scale/ColorScale.hpp for the design rationale (ramps, RGBA8 packing,
// sequential auto-domain, diverging mid-symmetry, and the class-4 baseline policy).
#include "dc/scale/ColorScale.hpp"

#include <algorithm>
#include <cmath>

namespace dc {

namespace {

// Round a 0..1 float to an 8-bit level (clamped, round-half-up).
std::uint8_t to8(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 1.0f) return 255;
  return static_cast<std::uint8_t>(std::lround(v * 255.0f));
}

// Linear per-channel interpolation between two 8-bit colors at fraction f in
// [0,1]. f is clamped by the caller; the alpha channel is interpolated too.
Rgba8 lerp8(const Rgba8& a, const Rgba8& b, double f) {
  auto mix = [f](std::uint8_t x, std::uint8_t y) {
    const double v = static_cast<double>(x) + f * (static_cast<double>(y) - x);
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
  };
  return Rgba8{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

}  // namespace

// ---------------------------------------------------------------------------
// Rgba8
// ---------------------------------------------------------------------------

Rgba8 Rgba8::fromFloats(float r01, float g01, float b01, float a01) {
  return Rgba8{to8(r01), to8(g01), to8(b01), to8(a01)};
}

// ---------------------------------------------------------------------------
// ColorRamp — sampling + named ramps
// ---------------------------------------------------------------------------

Rgba8 ColorRamp::sample(double t) const {
  if (stops_.empty()) return Rgba8{0, 0, 0, 255};
  t = std::clamp(t, 0.0, 1.0);
  // Below the first / above the last stop: clamp to the endpoint color. (Stops
  // are expected ordered with t==0 first and t==1 last, but we don't require it.)
  if (t <= stops_.front().t) return stops_.front().color;
  if (t >= stops_.back().t) return stops_.back().color;
  // Find the bracketing pair [i-1, i] with stops_[i-1].t <= t < stops_[i].t.
  for (std::size_t i = 1; i < stops_.size(); ++i) {
    if (t < stops_[i].t) {
      const ColorStop& lo = stops_[i - 1];
      const ColorStop& hi = stops_[i];
      const double seg = hi.t - lo.t;
      const double f = seg > 0.0 ? (t - lo.t) / seg : 0.0;
      return lerp8(lo.color, hi.color, f);
    }
  }
  return stops_.back().color;
}

ColorRamp ColorRamp::viridis() {
  // 5-stop perceptual approximation of matplotlib viridis (dark blue -> green ->
  // yellow). Endpoints/midpoints are the canonical viridis anchor colors.
  return ColorRamp({
      {0.00, Rgba8{68, 1, 84, 255}},     // #440154
      {0.25, Rgba8{59, 82, 139, 255}},   // #3b528b
      {0.50, Rgba8{33, 145, 140, 255}},  // #21918c
      {0.75, Rgba8{94, 201, 98, 255}},   // #5ec962
      {1.00, Rgba8{253, 231, 37, 255}},  // #fde725
  });
}

ColorRamp ColorRamp::magma() {
  // 5-stop approximation of matplotlib magma (near-black -> purple -> orange ->
  // near-white).
  return ColorRamp({
      {0.00, Rgba8{0, 0, 4, 255}},        // #000004
      {0.25, Rgba8{81, 18, 124, 255}},    // #51127c
      {0.50, Rgba8{183, 55, 121, 255}},   // #b73779
      {0.75, Rgba8{252, 137, 97, 255}},   // #fc8961
      {1.00, Rgba8{252, 253, 191, 255}},  // #fcfdbf
  });
}

ColorRamp ColorRamp::blueRed() {
  // A plain 2-stop magnitude ramp: blue (low) -> red (high).
  return ColorRamp({
      {0.0, Rgba8{0, 0, 255, 255}},
      {1.0, Rgba8{255, 0, 0, 255}},
  });
}

ColorRamp ColorRamp::redNeutralGreen() {
  // Canonical 3-stop diverging ramp: red (low) … white (neutral) … green (high).
  return ColorRamp({
      {0.0, Rgba8{215, 48, 39, 255}},     // #d73027 red
      {0.5, Rgba8{255, 255, 255, 255}},   // white neutral
      {1.0, Rgba8{26, 152, 80, 255}},     // #1a9850 green
  });
}

bool ColorRamp::byName(const std::string& name, ColorRamp& out) {
  if (name == "viridis") {
    out = viridis();
    return true;
  }
  if (name == "magma") {
    out = magma();
    return true;
  }
  if (name == "blueRed" || name == "bluered") {
    out = blueRed();
    return true;
  }
  if (name == "redNeutralGreen" || name == "redgreen" ||
      name == "redWhiteGreen") {
    out = redNeutralGreen();
    return true;
  }
  return false;
}

// ===========================================================================
// ENC-610 — SequentialColorScale
// ===========================================================================

double SequentialColorScale::map(double domainValue) const {
  const double span = domain_.max - domain_.min;
  // Degenerate domain: no unique slope -> park at the ramp midpoint (t=0.5).
  if (domain_.empty || span == 0.0) return 0.5;
  const double t = (domainValue - domain_.min) / span;
  return std::clamp(t, 0.0, 1.0);
}

double SequentialColorScale::invert(double rangeValue) const {
  if (domain_.empty || domain_.max == domain_.min) return domain_.min;
  const double t = std::clamp(rangeValue, 0.0, 1.0);
  return domain_.min + t * (domain_.max - domain_.min);
}

void SequentialColorScale::bindColumn(Id tableId, std::string columnName) {
  boundTable_ = tableId;
  boundColumn_ = std::move(columnName);
  bound_ = true;
  running_.reset();
}

bool SequentialColorScale::updateDomain(const TableStore& tables,
                                        const BufferByteSource& src) {
  if (!bound_) return false;
  if (!running_.reduceColumnF32(tables, boundTable_, boundColumn_, src)) {
    return false;
  }
  const Domain& d = running_.domain();
  if (d.empty) return false;
  domain_ = d;
  return true;
}

// ===========================================================================
// ENC-611 — DivergingColorScale + class-4 baseline policy
// ===========================================================================

bool BaselinePolicy::byName(const std::string& name, BaselinePolicy& out) {
  if (name == "fixedEpoch") {
    out = fixedEpoch();
    return true;
  }
  if (name == "decaying") {
    out = decaying(0.0);
    return true;
  }
  if (name == "referenceWindow") {
    out = referenceWindow(0);
    return true;
  }
  return false;
}

double DivergingColorScale::map(double domainValue) const {
  // Each side is interpolated INDEPENDENTLY so `mid` always lands at t=0.5,
  // regardless of how lopsided [min,mid] vs [mid,max] are — the symmetry contract.
  if (domain_.empty) return 0.5;
  if (domainValue <= mid_) {
    const double loSpan = mid_ - domain_.min;
    if (loSpan <= 0.0) return 0.5;  // degenerate low side -> neutral
    const double f = (domainValue - domain_.min) / loSpan;  // 0..1 across [min,mid]
    return 0.5 * std::clamp(f, 0.0, 1.0);                    // -> [0, 0.5]
  }
  const double hiSpan = domain_.max - mid_;
  if (hiSpan <= 0.0) return 0.5;  // degenerate high side -> neutral
  const double f = (domainValue - mid_) / hiSpan;            // 0..1 across [mid,max]
  return 0.5 + 0.5 * std::clamp(f, 0.0, 1.0);                // -> [0.5, 1]
}

double DivergingColorScale::invert(double rangeValue) const {
  if (domain_.empty) return mid_;
  const double t = std::clamp(rangeValue, 0.0, 1.0);
  if (t <= 0.5) {
    const double f = t / 0.5;  // 0..1 across the low half
    return domain_.min + f * (mid_ - domain_.min);
  }
  const double f = (t - 0.5) / 0.5;  // 0..1 across the high half
  return mid_ + f * (domain_.max - mid_);
}

void DivergingColorScale::bindColumn(Id tableId, std::string columnName) {
  boundTable_ = tableId;
  boundColumn_ = std::move(columnName);
  bound_ = true;
  running_.reset();
}

bool DivergingColorScale::updateDomain(const TableStore& tables,
                                       const BufferByteSource& src) {
  if (!bound_) return false;
  if (!running_.reduceColumnF32(tables, boundTable_, boundColumn_, src)) {
    return false;
  }
  const Domain& d = running_.domain();
  if (d.empty) return false;
  domain_ = d;  // ends drift with the running stats; `mid_` stays fixed
  return true;
}

std::unique_ptr<DivergingColorScale> makeDivergingColorScale(
    double mid, ColorRamp ramp, const BaselinePolicy& policy) {
  // CLASS-4 REJECTION: a diverging scale has no defined streaming semantics
  // without a declared baseline policy (RESEARCH §4.2). Reject at construction.
  if (!policy.valid()) return nullptr;
  std::unique_ptr<DivergingColorScale> s(new DivergingColorScale());
  s->mid_ = mid;
  s->ramp_ = std::move(ramp);
  s->policy_ = policy;
  return s;
}

}  // namespace dc
