// ENC-596 (P1.5) — SCALE subsystem implementation.
// ENC-597 (TimeScale), ENC-598 (BandScale/PointScale), ENC-599 (NiceTicks).
// See dc/scale/Scale.hpp for the design rationale.
#include "dc/scale/Scale.hpp"

#include "dc/math/NiceTicks.hpp"
#include "dc/math/NiceTimeTicks.hpp"

#include <cmath>
#include <cstdio>
#include <ctime>

namespace dc {

namespace {

// Format a finite numeric tick value compactly: drop a trailing ".000000" so an
// integer-valued tick reads "10" not "10.000000". General enough for axis chrome.
std::string formatNumber(double v) {
  if (v == 0.0) v = 0.0;  // normalize -0
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6g", v);
  return std::string(buf);
}

// Format an epoch-ms instant as a compact UTC label. Drops the time part for
// midnight-aligned ticks (date-only) and seconds when they are zero.
std::string formatEpochMs(double epochMs) {
  auto secs = static_cast<std::time_t>(std::llround(epochMs / 1000.0));
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &secs);
#else
  gmtime_r(&secs, &tm);
#endif
  char buf[64];
  if (tm.tm_hour == 0 && tm.tm_min == 0 && tm.tm_sec == 0) {
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday);
  } else if (tm.tm_sec == 0) {
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  } else {
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                  tm.tm_min, tm.tm_sec);
  }
  return std::string(buf);
}

}  // namespace

// ===========================================================================
// RunningDomain — O(Δ) streaming auto-domain
// ===========================================================================

void RunningDomain::reduceFrom(const double* data, std::size_t count) {
  if (data == nullptr) return;
  // Shrink (or replaced column): the running state is stale. Re-fold from 0.
  if (count < consumed_) reset();
  for (std::size_t i = consumed_; i < count; ++i) domain_.fold(data[i]);
  if (count > consumed_) consumed_ = count;
}

void RunningDomain::reduceFrom(const float* data, std::size_t count) {
  if (data == nullptr) return;
  if (count < consumed_) reset();
  for (std::size_t i = consumed_; i < count; ++i) {
    domain_.fold(static_cast<double>(data[i]));
  }
  if (count > consumed_) consumed_ = count;
}

void RunningDomain::reduceFrom(const std::int64_t* data, std::size_t count) {
  if (data == nullptr) return;
  if (count < consumed_) reset();
  for (std::size_t i = consumed_; i < count; ++i) {
    domain_.fold(static_cast<double>(data[i]));  // lossless for epoch-ms
  }
  if (count > consumed_) consumed_ = count;
}

bool RunningDomain::reduceColumnF32(const TableStore& tables, Id tableId,
                                    const std::string& columnName,
                                    const BufferByteSource& src) {
  ColumnView<float> view = tables.viewF32(tableId, columnName, src);
  if (!view.valid()) return false;
  reduceFrom(view.data, view.count);
  return true;
}

// ===========================================================================
// LinearScale — map / invert / nice / auto-domain
// ===========================================================================

double LinearScale::map(double domainValue) const {
  const double span = domain_.max - domain_.min;
  // Degenerate domain (empty, single value, or all-equal): no unique slope —
  // park every value at the range midpoint.
  if (domain_.empty || span == 0.0) {
    return 0.5 * (range_.r0 + range_.r1);
  }
  const double t = (domainValue - domain_.min) / span;
  return range_.r0 + t * (range_.r1 - range_.r0);
}

double LinearScale::invert(double rangeValue) const {
  const double rspan = range_.r1 - range_.r0;
  // Degenerate domain: invert collapses to the domain min (the only defined
  // pre-image of the parked midpoint).
  if (domain_.empty || domain_.max == domain_.min) {
    return domain_.min;
  }
  // Degenerate range (zero-width target): no information to invert; return the
  // domain min rather than divide by zero.
  if (rspan == 0.0) return domain_.min;
  const double t = (rangeValue - range_.r0) / rspan;
  return domain_.min + t * (domain_.max - domain_.min);
}

LinearScale& LinearScale::nice(int targetTicks) {
  if (domain_.empty || domain_.max == domain_.min || targetTicks < 1) {
    return *this;
  }
  // ENC-599: round the bounds outward via the shared NiceTicks engine (was a
  // self-contained 1/2/2.5/5·10ᵏ stub in ENC-596).
  const TickSet t = computeNiceTicks(static_cast<float>(domain_.min),
                                     static_cast<float>(domain_.max), targetTicks);
  domain_.min = static_cast<double>(t.min);
  domain_.max = static_cast<double>(t.max);
  return *this;
}

std::vector<ScaleTick> LinearScale::ticks(int targetCount) const {
  std::vector<ScaleTick> out;
  if (domain_.empty || domain_.max == domain_.min || targetCount < 1) return out;
  const TickSet t = computeNiceTicks(static_cast<float>(domain_.min),
                                     static_cast<float>(domain_.max), targetCount);
  out.reserve(t.values.size());
  for (float v : t.values) {
    out.push_back(ScaleTick{static_cast<double>(v), formatNumber(v)});
  }
  return out;
}

void LinearScale::bindColumn(Id tableId, std::string columnName) {
  boundTable_ = tableId;
  boundColumn_ = std::move(columnName);
  bound_ = true;
  running_.reset();
}

bool LinearScale::updateDomain(const TableStore& tables,
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
// ENC-597 — TimeScale (epoch-ms on CPU; relative f32 offset only at the GPU edge)
// ===========================================================================

double TimeScale::map(double epochMs) const {
  // Pure f64 epoch-ms math — identical algebra to LinearScale, no f32 anywhere.
  const double span = domain_.max - domain_.min;
  if (domain_.empty || span == 0.0) return 0.5 * (range_.r0 + range_.r1);
  const double t = (epochMs - domain_.min) / span;
  return range_.r0 + t * (range_.r1 - range_.r0);
}

double TimeScale::invert(double rangeValue) const {
  const double rspan = range_.r1 - range_.r0;
  if (domain_.empty || domain_.max == domain_.min || rspan == 0.0) {
    return domain_.min;
  }
  const double t = (rangeValue - range_.r0) / rspan;
  return domain_.min + t * (domain_.max - domain_.min);
}

std::vector<ScaleTick> TimeScale::ticks(int targetCount) const {
  std::vector<ScaleTick> out;
  if (domain_.empty || domain_.max <= domain_.min || targetCount < 1) return out;

  // PRECISION: do NOT hand absolute epoch to the f32 NiceTimeTicks engine
  // (epoch-seconds ~1.7e9 overflows the f32 mantissa, exactly the trap). Instead
  // pass a RELATIVE offset [0, span] in SECONDS — a view's span fits f32 cleanly
  // — to select the human-meaningful step, then generate the absolute tick values
  // back in f64 epoch-ms on the CPU.
  const double minSec = domain_.min / 1000.0;
  const double maxSec = domain_.max / 1000.0;
  const double spanSec = maxSec - minSec;
  const TimeTickSet ts =
      computeNiceTimeTicks(0.0f, static_cast<float>(spanSec), targetCount);
  const double step = static_cast<double>(ts.stepSeconds);
  if (step <= 0.0) return out;

  // Align the first tick to a step boundary in ABSOLUTE epoch-seconds (f64), then
  // walk by step. For sub-day steps this is modular alignment; for day+ steps the
  // boundary is "midnight" (step is a whole number of days/weeks here), so modular
  // alignment against the unix epoch (itself a midnight) lands on a day boundary.
  double firstSec = std::ceil(minSec / step) * step;
  for (int i = 0; i < targetCount * 4 + 4; ++i) {
    const double sec = firstSec + static_cast<double>(i) * step;
    if (sec > maxSec + step * 0.001) break;
    const double epochMs = sec * 1000.0;
    out.push_back(ScaleTick{epochMs, formatEpochMs(epochMs)});
  }
  return out;
}

void TimeScale::bindColumn(Id tableId, std::string columnName) {
  boundTable_ = tableId;
  boundColumn_ = std::move(columnName);
  bound_ = true;
  running_.reset();
}

bool TimeScale::updateDomain(const TableStore& tables,
                             const BufferByteSource& src) {
  if (!bound_) return false;
  ColumnView<std::int64_t> view =
      tables.viewTimestamp(boundTable_, boundColumn_, src);
  if (!view.valid()) return false;  // missing or not a Timestamp column
  running_.reduceFrom(view.data, view.count);
  const Domain& d = running_.domain();
  if (d.empty) return false;
  domain_ = d;
  baseEpochMs_ = d.min;  // re-pin the CPU base epoch to the live domain min
  return true;
}

// ===========================================================================
// ENC-598 — OrdinalDomain + Band/Point scales over a GROWING category set
// ===========================================================================

bool OrdinalDomain::add(std::uint32_t code) {
  for (std::uint32_t c : codes) {
    if (c == code) return false;
  }
  codes.push_back(code);
  return true;
}

std::ptrdiff_t OrdinalDomain::ordinalOf(std::uint32_t code) const {
  for (std::size_t i = 0; i < codes.size(); ++i) {
    if (codes[i] == code) return static_cast<std::ptrdiff_t>(i);
  }
  return -1;
}

void OrdinalScale::setCategories(const std::vector<std::uint32_t>& codes) {
  ordinal_.clear();
  for (std::uint32_t c : codes) ordinal_.add(c);
  consumedCats_ = 0;  // a manual seed is independent of the dict high-water
}

// Resolve the band layout. Following D3's band-scale construction over the range
// [r0,r1] (which may be inverted): with n categories, paddingInner pi and
// paddingOuter po (both fractions of a step),
//   step = extent / (n - pi + 2*po),   bandwidth = step * (1 - pi),
//   start0 = r0 + step*po  (in the direction of the range).
void OrdinalScale::layout(double& start, double& span, double& stepOut,
                          double& bandOut) const {
  const std::size_t n = ordinal_.size();
  const double extent = range_.r1 - range_.r0;  // signed (inverted-range aware)
  span = extent;
  if (n == 0) {
    start = range_.r0;
    stepOut = 0.0;
    bandOut = 0.0;
    return;
  }
  const double pi = effectiveInnerPadding();
  // For n == 1 the band/point denom degenerates; place the single band to fill
  // the padded extent (D3: a 1-element band spans the inner range).
  double denom = static_cast<double>(n) - pi + 2.0 * paddingOuter_;
  if (denom <= 0.0) denom = 1.0;  // n==1 point (pi==1, po==0): single position
  const double step = extent / denom;
  stepOut = step;
  bandOut = step * (1.0 - pi);
  start = range_.r0 + step * paddingOuter_;
}

double OrdinalScale::startAt(std::size_t i) const {
  double start, span, step, band;
  layout(start, span, step, band);
  return start + step * static_cast<double>(i);
}

double OrdinalScale::step() const {
  double start, span, step, band;
  layout(start, span, step, band);
  return step;
}

double OrdinalScale::map(double code) const {
  const std::ptrdiff_t i =
      ordinal_.ordinalOf(static_cast<std::uint32_t>(std::llround(code)));
  if (i < 0) return 0.5 * (range_.r0 + range_.r1);  // unknown → parked midpoint
  return startAt(static_cast<std::size_t>(i));
}

double OrdinalScale::center(std::uint32_t code) const {
  const std::ptrdiff_t i = ordinal_.ordinalOf(code);
  if (i < 0) return 0.5 * (range_.r0 + range_.r1);
  return startAt(static_cast<std::size_t>(i)) + 0.5 * bandwidth();
}

double OrdinalScale::invert(double rangeValue) const {
  const std::size_t n = ordinal_.size();
  if (n == 0) return -1.0;
  double start, span, step, band;
  layout(start, span, step, band);
  if (step == 0.0) return static_cast<double>(ordinal_.codes[0]);
  // Index of the slot containing rangeValue, measured from the first band start
  // in the direction of the (possibly inverted) range.
  const double rel = (rangeValue - start) / step;
  std::ptrdiff_t idx = static_cast<std::ptrdiff_t>(std::floor(rel));
  if (idx < 0 || idx >= static_cast<std::ptrdiff_t>(n)) return -1.0;
  return static_cast<double>(ordinal_.codes[static_cast<std::size_t>(idx)]);
}

std::vector<ScaleTick> OrdinalScale::ticksWithDict(
    const CatDictionary* dict) const {
  std::vector<ScaleTick> out;
  out.reserve(ordinal_.size());
  for (std::uint32_t code : ordinal_.codes) {
    std::string label;
    if (dict != nullptr) {
      label = dict->labelOf(code);
    }
    if (label.empty()) label = formatNumber(static_cast<double>(code));
    out.push_back(ScaleTick{center(code), std::move(label)});
  }
  return out;
}

void OrdinalScale::bindColumn(Id tableId, std::string columnName) {
  boundTable_ = tableId;
  boundColumn_ = std::move(columnName);
  bound_ = true;
  consumedCats_ = 0;
}

bool OrdinalScale::updateDomain(const TableStore& tables) {
  if (!bound_) return false;
  const CatDictionary* dict = tables.catDict(boundTable_, boundColumn_);
  if (dict == nullptr) return false;  // missing or not a Cat column
  const std::uint32_t n = dict->size();
  // Codes are dense [0, n) assigned in arrival order. Fold ONLY the new tail
  // codes [consumedCats_, n) — genuinely O(Δ) over the growing dictionary. A
  // shrink (dictionary replaced) resets the high-water and re-folds from 0.
  if (n < consumedCats_) {
    ordinal_.clear();
    consumedCats_ = 0;
  }
  for (std::uint32_t code = consumedCats_; code < n; ++code) ordinal_.add(code);
  consumedCats_ = n;
  return true;
}

double BandScale::bandwidth() const {
  double start, span, step, band;
  layout(start, span, step, band);
  return band;
}

}  // namespace dc
