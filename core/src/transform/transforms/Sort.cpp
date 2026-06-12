// ENC-616b — `sort` / `rank` transform implementation. See Sort.hpp.
#include "dc/transform/transforms/Sort.hpp"

#include "TransformUtil.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace dc {

SchemaResult SortTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  if (!input.has(key_)) {
    r.error = "sort key '" + key_ + "' not found";
    return r;
  }
  if (mode_ == Mode::Reorder) {
    // A pure permutation: schema is unchanged (same columns, same dtypes).
    r.schema = input;
  } else {
    if (as_.empty()) {
      r.error = "rank needs an 'as' column name";
      return r;
    }
    if (input.has(as_)) {
      r.error = "rank output '" + as_ + "' collides with an input column";
      return r;
    }
    r.schema = input;
    r.schema.columns.push_back({as_, DType::I32});
  }
  r.ok = true;
  return r;
}

void SortTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& in = *ctx.inputSchema;
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  // Read the sort key per row. A timestamp key sorts on its i64 value (no f32
  // trap); every other dtype reads as a double.
  const bool keyIsTs = res.dtypeOf(key_) == DType::Timestamp;
  std::vector<double> keyNum;
  std::vector<std::int64_t> keyTs;
  if (keyIsTs) {
    keyTs.resize(rows);
    for (std::size_t i = 0; i < rows; ++i) keyTs[i] = res.readTimestamp(key_, i);
  } else {
    keyNum.resize(rows);
    for (std::size_t i = 0; i < rows; ++i) keyNum[i] = res.readNum(key_, i);
  }

  // STABLE permutation: equal keys keep their original relative order.
  std::vector<std::size_t> perm(rows);
  std::iota(perm.begin(), perm.end(), std::size_t{0});
  const bool asc = ascending_;
  std::stable_sort(perm.begin(), perm.end(),
                   [&](std::size_t a, std::size_t b) {
                     if (keyIsTs)
                       return asc ? keyTs[a] < keyTs[b] : keyTs[a] > keyTs[b];
                     return asc ? keyNum[a] < keyNum[b] : keyNum[a] > keyNum[b];
                   });

  if (mode_ == Mode::Reorder) {
    // Emit input columns permuted into sorted order.
    for (const auto& col : in.columns) {
      ctx.out->allocColumn(node, col.name, col.dtype, rows);
    }
    for (std::size_t dst = 0; dst < rows; ++dst) {
      const std::size_t srcRow = perm[dst];
      for (const auto& col : in.columns) {
        tfutil::copyCell(col, res, srcRow, *ctx.out, node, dst);
      }
    }
    return;
  }

  // RANK: keep original row order, add an i32 rank column. perm[k] is the row at
  // sorted position k, so rank[perm[k]] = k.
  for (const auto& col : in.columns) {
    ctx.out->allocColumn(node, col.name, col.dtype, rows);
  }
  ctx.out->allocColumn(node, as_, DType::I32, rows);
  for (std::size_t i = 0; i < rows; ++i) {
    for (const auto& col : in.columns) {
      tfutil::copyCell(col, res, i, *ctx.out, node, i);
    }
  }
  for (std::size_t k = 0; k < rows; ++k) {
    ctx.out->setI32(node, as_, perm[k], static_cast<std::int32_t>(k));
  }
}

}  // namespace dc
