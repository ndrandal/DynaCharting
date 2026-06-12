// ENC-616b — `bin` transform implementation. See Bin.hpp.
#include "dc/transform/transforms/Bin.hpp"

#include "TransformUtil.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dc {

namespace {

// A "nice" bin step (d3/Vega tickStep flavor): given a span and a target bin
// count, round the raw step up to 1/2/5 * 10^k so bins land on human-friendly
// boundaries and the span is covered by AT MOST `maxbins` bins.
double niceStep(double span, int maxbins) {
  if (span <= 0.0 || maxbins <= 0) return 1.0;
  const double raw = span / maxbins;
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double norm = raw / mag;  // in [1,10)
  double nice;
  if (norm <= 1.0) nice = 1.0;
  else if (norm <= 2.0) nice = 2.0;
  else if (norm <= 5.0) nice = 5.0;
  else nice = 10.0;
  return nice * mag;
}

// Is dtype a column `bin` accepts (numeric, NOT timestamp)?
bool isBinnable(DType dt) { return dt == DType::F32 || dt == DType::I32; }

}  // namespace

BinTransform::BinSpec BinTransform::resolveSpec(double dataMin,
                                                double dataMax) const {
  // Pinned extent overrides the data extent (stable edges across appends).
  double lo = dataMin, hi = dataMax;
  if (extentLo_ < extentHi_) {
    lo = extentLo_;
    hi = extentHi_;
  }
  if (!(hi > lo)) hi = lo + 1.0;  // degenerate domain -> one unit-wide bin

  BinSpec s;
  s.step = (mode_ == Mode::Step) ? step_ : niceStep(hi - lo, maxbins_);
  if (!(s.step > 0.0)) s.step = 1.0;
  // Anchor the first edge on a step multiple at or below lo (so edges are stable
  // regardless of where the data min lands within a bin).
  s.firstEdge = std::floor(lo / s.step) * s.step;
  // Cover [firstEdge, hi]; the last value hi lands in bin floor((hi-firstEdge)/step).
  const double rawCount = std::floor((hi - s.firstEdge) / s.step) + 1.0;
  s.count = std::max(1, static_cast<int>(rawCount));
  return s;
}

SchemaResult BinTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  const SchemaColumn* col = input.find(field_);
  if (!col) {
    r.error = "bin field '" + field_ + "' not found";
    return r;
  }
  if (!isBinnable(col->dtype)) {
    r.error = "bin field '" + field_ + "' must be numeric (f32/i32)";
    return r;
  }
  if (mode_ == Mode::Step && !(step_ > 0.0)) {
    r.error = "bin step must be > 0";
    return r;
  }
  if (mode_ == Mode::MaxBins && maxbins_ <= 0) {
    r.error = "bin maxbins must be > 0";
    return r;
  }
  const std::string idx = idxField(), lo = loField(), hi = hiField();
  for (const auto& name : {idx, lo, hi}) {
    if (input.has(name)) {
      r.error = "bin output '" + name + "' collides with an input column";
      return r;
    }
  }
  // Output = passthrough inputs + bin index (i32) + lo/hi edges (f32).
  r.schema = input;
  r.schema.columns.push_back({idx, DType::I32});
  r.schema.columns.push_back({lo, DType::F32});
  r.schema.columns.push_back({hi, DType::F32});
  r.ok = true;
  return r;
}

void BinTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;
  const std::string idx = idxField(), loF = loField(), hiF = hiField();

  // Pass 1: data extent of the field (a single O(N) min/max — class-1 reducer).
  double dmin = std::numeric_limits<double>::infinity();
  double dmax = -std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < rows; ++i) {
    const double v = res.readNum(field_, i);
    if (std::isnan(v)) continue;
    dmin = std::min(dmin, v);
    dmax = std::max(dmax, v);
  }
  if (!std::isfinite(dmin)) {  // no finite rows
    dmin = 0.0;
    dmax = 0.0;
  }
  const BinSpec spec = resolveSpec(dmin, dmax);

  // Allocate passthrough columns + the three derived columns.
  for (const auto& col : in.columns) {
    ctx.out->allocColumn(node, col.name, col.dtype, rows);
  }
  ctx.out->allocColumn(node, idx, DType::I32, rows);
  ctx.out->allocColumn(node, loF, DType::F32, rows);
  ctx.out->allocColumn(node, hiF, DType::F32, rows);

  // Pass 2: per-row passthrough + bin assignment (the §5.1 "increment target bin"
  // grain — here we LABEL each row; a downstream aggregate counts).
  for (std::size_t i = 0; i < rows; ++i) {
    for (const auto& col : in.columns) {
      tfutil::copyCell(col, res, i, *ctx.out, node, i);
    }
    const double v = res.readNum(field_, i);
    int b = 0;
    if (std::isfinite(v)) {
      b = static_cast<int>(std::floor((v - spec.firstEdge) / spec.step));
      b = std::max(0, std::min(b, spec.count - 1));
    }
    const double lo = spec.firstEdge + b * spec.step;
    ctx.out->setI32(node, idx, i, b);
    ctx.out->setF32(node, loF, i, static_cast<float>(lo));
    ctx.out->setF32(node, hiF, i, static_cast<float>(lo + spec.step));
  }
}

}  // namespace dc
