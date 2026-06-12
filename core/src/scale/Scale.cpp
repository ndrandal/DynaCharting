// ENC-596 (P1.5) — SCALE subsystem implementation.
// See dc/scale/Scale.hpp for the design rationale.
#include "dc/scale/Scale.hpp"

#include <cmath>

namespace dc {

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
  const double span = domain_.max - domain_.min;
  // Raw step that would give ~targetTicks intervals, then round it to a "nice"
  // 1 / 2 / 2.5 / 5 · 10ᵏ value (the standard d3 nice-step rounding).
  const double rawStep = span / static_cast<double>(targetTicks);
  const double mag = std::pow(10.0, std::floor(std::log10(rawStep)));
  const double norm = rawStep / mag;  // in [1, 10)
  double niceNorm;
  if (norm < 1.5) {
    niceNorm = 1.0;
  } else if (norm < 3.0) {
    niceNorm = 2.0;
  } else if (norm < 7.0) {
    niceNorm = 5.0;
  } else {
    niceNorm = 10.0;
  }
  const double step = niceNorm * mag;
  // Round the bounds OUTWARD to multiples of the nice step.
  domain_.min = std::floor(domain_.min / step) * step;
  domain_.max = std::ceil(domain_.max / step) * step;
  return *this;
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

}  // namespace dc
