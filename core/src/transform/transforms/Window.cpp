// ENC-616c — `window` / rolling transform implementation. See Window.hpp.
#include "dc/transform/transforms/Window.hpp"

#include "dc/math/Ema.hpp"  // computeEma — the existing O(1) EMA helper (§5.1)
#include "TransformUtil.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace dc {

SchemaResult WindowTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;

  const SchemaColumn* s = input.find(source_);
  if (!s) {
    r.error = "window source column '" + source_ + "' not found";
    return r;
  }
  if (s->dtype != DType::F32 && s->dtype != DType::I32) {
    r.error = "window source column '" + source_ + "' must be numeric (f32/i32)";
    return r;
  }
  if (window_ < 1) {
    r.error = "window size/period must be >= 1";
    return r;
  }
  if (input.has(as_)) {
    r.error = "window output '" + as_ + "' collides with an input column";
    return r;
  }

  r.schema = input;
  r.schema.columns.push_back({as_, DType::F32});
  r.ok = true;
  return r;
}

void WindowTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  // (Re)allocate passthrough columns + the derived rolling column.
  for (const auto& col : in.columns) {
    ctx.out->allocColumn(node, col.name, col.dtype, rows);
  }
  ctx.out->allocColumn(node, as_, DType::F32, rows);
  for (std::size_t i = 0; i < rows; ++i) {
    for (const auto& col : in.columns) {
      tfutil::copyCell(col, res, i, *ctx.out, node, i);
    }
  }
  if (rows == 0) return;

  // Materialize the source column once as f32 (the form the aggregates + the EMA
  // helper consume).
  std::vector<float> v(rows);
  for (std::size_t i = 0; i < rows; ++i) {
    v[i] = static_cast<float>(res.readNum(source_, i));
  }

  std::vector<float> out(rows, 0.0f);
  const std::size_t W = window_;

  if (agg_ == WindowAgg::Ema) {
    // O(1) EMA via the existing helper: out[0..period-2] passthrough, out[period-1]
    // = SMA seed, then the recurrence. `window_` is the EMA period.
    computeEma(v.data(), out.data(), static_cast<int>(rows),
               static_cast<int>(window_));
  } else {
    // Fixed-window aggregate over the trailing frame [i-W+1 .. i] (clamped at the
    // head). Mean/Sum use a running prefix sum (O(1) per row after the prefix);
    // Min/Max scan the (bounded) frame.
    if (agg_ == WindowAgg::Mean || agg_ == WindowAgg::Sum) {
      double run = 0.0;
      std::vector<double> prefix(rows + 1, 0.0);
      for (std::size_t i = 0; i < rows; ++i) {
        run += v[i];
        prefix[i + 1] = run;
      }
      for (std::size_t i = 0; i < rows; ++i) {
        const std::size_t lo = (i + 1 >= W) ? (i + 1 - W) : 0;
        const double sum = prefix[i + 1] - prefix[lo];
        const std::size_t cnt = i + 1 - lo;
        out[i] = static_cast<float>(agg_ == WindowAgg::Mean
                                        ? sum / static_cast<double>(cnt)
                                        : sum);
      }
    } else {  // Min / Max
      for (std::size_t i = 0; i < rows; ++i) {
        const std::size_t lo = (i + 1 >= W) ? (i + 1 - W) : 0;
        float acc = v[lo];
        for (std::size_t j = lo + 1; j <= i; ++j) {
          acc = (agg_ == WindowAgg::Min) ? std::min(acc, v[j])
                                         : std::max(acc, v[j]);
        }
        out[i] = acc;
      }
    }
  }

  for (std::size_t i = 0; i < rows; ++i) {
    ctx.out->setF32(node, as_, i, out[i]);
  }
}

}  // namespace dc
