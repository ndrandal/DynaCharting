// ENC-616c — `sample` / LOD (M4) transform implementation. See Sample.hpp.
#include "dc/transform/transforms/Sample.hpp"

#include "TransformUtil.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dc {

SchemaResult SampleTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;

  auto numeric = [&](const std::string& name, const char* role) -> bool {
    const SchemaColumn* c = input.find(name);
    if (!c) {
      r.error = std::string("sample ") + role + " column '" + name + "' not found";
      return false;
    }
    // x may be a timestamp (ordering); y must be f32/i32. Both read as double.
    if (c->dtype != DType::F32 && c->dtype != DType::I32 &&
        !(role[0] == 'x' && c->dtype == DType::Timestamp)) {
      r.error = std::string("sample ") + role + " column '" + name +
                "' must be numeric";
      return false;
    }
    return true;
  };

  if (!numeric(x_, "x")) return r;
  if (!numeric(y_, "y")) return r;
  if (budget_ < 2) {
    r.error = "sample budget must be >= 2";
    return r;
  }

  // Compaction preserves the schema; only the row count shrinks.
  r.schema = input;
  r.ok = true;
  return r;
}

void SampleTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  // Read x as double for bucketing (timestamp widens; ordering only). y drives the
  // min/max selection.
  auto emitAll = [&](const std::vector<std::size_t>& keep) {
    for (const auto& col : in.columns) {
      ctx.out->allocColumn(node, col.name, col.dtype, keep.size());
    }
    for (std::size_t dst = 0; dst < keep.size(); ++dst) {
      for (const auto& col : in.columns) {
        tfutil::copyCell(col, res, keep[dst], *ctx.out, node, dst);
      }
    }
  };

  // Below the budget there is nothing to drop: keep every row in order.
  if (rows <= budget_) {
    std::vector<std::size_t> keep(rows);
    for (std::size_t i = 0; i < rows; ++i) keep[i] = i;
    emitAll(keep);
    return;
  }

  std::vector<double> x(rows);
  std::vector<double> y(rows);
  for (std::size_t i = 0; i < rows; ++i) {
    x[i] = res.readNum(x_, i);
    y[i] = res.readNum(y_, i);
  }

  // M4: budget/4 equal-width buckets over the x domain; each bucket contributes up
  // to 4 rows (first, last, argmin-y, argmax-y). budget>=2 => at least 1 bucket.
  const std::size_t buckets = std::max<std::size_t>(1, budget_ / 4);
  double xmin = x[0], xmax = x[0];
  for (std::size_t i = 1; i < rows; ++i) {
    xmin = std::min(xmin, x[i]);
    xmax = std::max(xmax, x[i]);
  }
  const double span = xmax - xmin;

  auto bucketOf = [&](std::size_t i) -> std::size_t {
    if (span <= 0.0) return 0;
    double f = (x[i] - xmin) / span;  // [0,1]
    std::size_t b = static_cast<std::size_t>(f * static_cast<double>(buckets));
    return std::min(b, buckets - 1);  // xmax lands in the last bucket
  };

  // Per bucket track the 4 pinning row indices.
  struct Pins {
    bool has{false};
    std::size_t first{0}, last{0}, argMin{0}, argMax{0};
  };
  std::vector<Pins> pins(buckets);

  for (std::size_t i = 0; i < rows; ++i) {
    Pins& p = pins[bucketOf(i)];
    if (!p.has) {
      p.has = true;
      p.first = p.last = p.argMin = p.argMax = i;
      continue;
    }
    p.last = i;  // input order => later row is the later x in this bucket
    if (y[i] < y[p.argMin]) p.argMin = i;
    if (y[i] > y[p.argMax]) p.argMax = i;
  }

  // Collect the kept indices, dedup within a bucket, emit in input order.
  std::vector<std::size_t> keep;
  keep.reserve(buckets * 4);
  for (const Pins& p : pins) {
    if (!p.has) continue;
    std::size_t four[4] = {p.first, p.last, p.argMin, p.argMax};
    std::sort(four, four + 4);
    std::size_t prev = static_cast<std::size_t>(-1);
    for (std::size_t k = 0; k < 4; ++k) {
      if (four[k] != prev) keep.push_back(four[k]);
      prev = four[k];
    }
  }
  // Buckets are already in ascending x order, but min/max within a bucket can
  // interleave — a global sort guarantees a monotone, input-order output.
  std::sort(keep.begin(), keep.end());

  emitAll(keep);
}

}  // namespace dc
