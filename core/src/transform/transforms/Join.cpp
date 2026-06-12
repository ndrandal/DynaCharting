// ENC-616d — `join`/`lookup` transform implementation. See Join.hpp.
#include "dc/transform/transforms/Join.hpp"

#include "TransformUtil.hpp"

#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// Key normalization. Keys are read as doubles through the resolver (f32/i32/cat
// widen exactly; timestamp epoch-ms fits in a double's 53-bit mantissa for any
// realistic range). We index the right table by the BIT pattern of that double so
// integer-valued cat/i32 keys compare exactly (no float fuzz) and NaN never keys.
// ---------------------------------------------------------------------------
namespace {

std::uint64_t keyBits(double v) {
  std::uint64_t b;
  static_assert(sizeof(b) == sizeof(v), "double is 8 bytes");
  std::memcpy(&b, &v, sizeof(b));
  return b;
}

// Sentinel written for a missed lookup under JoinMiss::Null, per dtype.
void writeSentinel(ColumnStore& out, Id node, const SchemaColumn& col,
                   std::size_t row) {
  switch (col.dtype) {
    case DType::F32:
      out.setF32(node, col.name, row, std::nanf(""));
      break;
    case DType::I32:
      out.setI32(node, col.name, row, 0);
      break;
    case DType::Cat:
      out.setCat(node, col.name, row, 0);
      break;
    case DType::Timestamp:
      out.setTimestamp(node, col.name, row, 0);
      break;
  }
}

// Copy right column `field` at right row `srcRow` into output (node, outName) at
// `dstRow`, preserving dtype (timestamp as i64 — no f32 trap).
void copyRightCell(const std::string& field, DType dtype,
                   const std::string& outName, const ColumnResolver& right,
                   std::size_t srcRow, ColumnStore& out, Id node,
                   std::size_t dstRow) {
  switch (dtype) {
    case DType::F32:
      out.setF32(node, outName, dstRow,
                 static_cast<float>(right.readNum(field, srcRow)));
      break;
    case DType::I32:
      out.setI32(node, outName, dstRow,
                 static_cast<std::int32_t>(right.readNum(field, srcRow)));
      break;
    case DType::Cat:
      out.setCat(node, outName, dstRow,
                 static_cast<std::uint32_t>(right.readNum(field, srcRow)));
      break;
    case DType::Timestamp:
      out.setTimestamp(node, outName, dstRow, right.readTimestamp(field, srcRow));
      break;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// inferSchema (unary) — never reached for an arity-2 node; a clear diagnostic in
// case a join is mistakenly added via the unary addTransform path.
// ---------------------------------------------------------------------------
SchemaResult JoinTransform::inferSchema(const ColumnSchema& /*left*/) const {
  SchemaResult r;
  r.error = "join is a binary node; add it with addJoin(left, right)";
  return r;
}

// ---------------------------------------------------------------------------
// inferSchemaBinary — output = left columns (passthrough) + the prefixed pulled
// right columns. Fail-fast on a missing column, a key dtype mismatch, or an output
// name collision.
// ---------------------------------------------------------------------------
SchemaResult JoinTransform::inferSchemaBinary(const ColumnSchema& left,
                                              const ColumnSchema& right) const {
  SchemaResult r;
  if (lookups_.empty()) {
    r.error = "join: no lookups declared";
    return r;
  }
  // The right key must exist (it is what every lookup resolves against).
  const SchemaColumn* rk = right.find(rightKey_);
  if (!rk) {
    r.error = "join: right key column '" + rightKey_ + "' not found in right input";
    return r;
  }

  // Output starts as the left passthrough; appended names must stay unique.
  r.schema = left;
  for (const JoinLookup& lk : lookups_) {
    // The left key must exist and its dtype must MATCH the right key's (you cannot
    // resolve a cat key against an f32 key).
    const SchemaColumn* lkCol = left.find(lk.leftKey);
    if (!lkCol) {
      r.error = "join: left key column '" + lk.leftKey + "' not found in left input";
      return r;
    }
    if (lkCol->dtype != rk->dtype) {
      r.error = "join: key dtype mismatch for '" + lk.leftKey + "' (" +
                toString(lkCol->dtype) + ") vs right key '" + rightKey_ + "' (" +
                toString(rk->dtype) + ")";
      return r;
    }
    if (lk.fields.empty()) {
      r.error = "join: lookup on '" + lk.leftKey + "' pulls no fields";
      return r;
    }
    for (const std::string& f : lk.fields) {
      const SchemaColumn* rf = right.find(f);
      if (!rf) {
        r.error = "join: right field '" + f + "' not found in right input";
        return r;
      }
      const std::string outName = qualified(lk.prefix, f);
      if (r.schema.has(outName)) {
        r.error = "join: output column '" + outName + "' collides with an existing column";
        return r;
      }
      r.schema.columns.push_back({outName, rf->dtype});
    }
  }
  r.ok = true;
  return r;
}

// ---------------------------------------------------------------------------
// evaluate — hash the right table by its key, then per left row resolve every
// lookup and append the pulled columns. With JoinMiss::Drop a row survives only if
// EVERY lookup hits; with JoinMiss::Null a missed lookup writes sentinels.
// ---------------------------------------------------------------------------
void JoinTransform::evaluate(const EvalContext& ctx) const {
  const ColumnSchema& left = *ctx.inputSchema;
  const ColumnResolver& lres = *ctx.input;
  if (!ctx.rightSchema || !ctx.right) return;  // not wired as a binary node
  const ColumnSchema& right = *ctx.rightSchema;
  const ColumnResolver& rres = *ctx.right;
  const Id node = ctx.nodeId;

  // 1) Index the RIGHT side: key value -> right row. Last write wins on a dup key
  //    (a stable choice; the right key is expected unique for an id table).
  const std::size_t rrows = rres.rowCount();
  std::unordered_map<std::uint64_t, std::size_t> index;
  index.reserve(rrows * 2);
  for (std::size_t i = 0; i < rrows; ++i) {
    double k = rres.readNum(rightKey_, i);
    if (std::isnan(k)) continue;  // NaN never indexes
    index[keyBits(k)] = i;
  }

  // Precompute the (field, dtype, outName) plan for each lookup once.
  struct PulledCol {
    std::string field;
    DType dtype{DType::F32};
    std::string outName;
  };
  struct LookupPlan {
    std::string leftKey;
    std::vector<PulledCol> cols;
  };
  std::vector<LookupPlan> plans;
  plans.reserve(lookups_.size());
  for (const JoinLookup& lk : lookups_) {
    LookupPlan p;
    p.leftKey = lk.leftKey;
    for (const std::string& f : lk.fields) {
      const SchemaColumn* rf = right.find(f);
      DType dt = rf ? rf->dtype : DType::F32;
      p.cols.push_back({f, dt, qualified(lk.prefix, f)});
    }
    plans.push_back(std::move(p));
  }

  const std::size_t lrows = lres.rowCount();

  // 2) Resolve. For Drop we first select survivors (rows where every lookup hits)
  //    so output columns can be sized exactly, mirroring the filter compaction
  //    pattern. For Null every left row survives.
  std::vector<std::size_t> survivors;
  if (miss_ == JoinMiss::Drop) {
    survivors.reserve(lrows);
    for (std::size_t i = 0; i < lrows; ++i) {
      bool allHit = true;
      for (const LookupPlan& p : plans) {
        double k = lres.readNum(p.leftKey, i);
        if (std::isnan(k) || index.find(keyBits(k)) == index.end()) {
          allHit = false;
          break;
        }
      }
      if (allHit) survivors.push_back(i);
    }
  } else {
    survivors.resize(lrows);
    for (std::size_t i = 0; i < lrows; ++i) survivors[i] = i;
  }
  const std::size_t outRows = survivors.size();

  // 3) Allocate every output column (left passthrough + the appended pulled ones).
  for (const auto& col : left.columns)
    ctx.out->allocColumn(node, col.name, col.dtype, outRows);
  for (const LookupPlan& p : plans)
    for (const PulledCol& pc : p.cols)
      ctx.out->allocColumn(node, pc.outName, pc.dtype, outRows);

  // 4) Fill.
  for (std::size_t dst = 0; dst < outRows; ++dst) {
    const std::size_t srcRow = survivors[dst];
    // left passthrough (dtype-preserving, timestamp as i64)
    for (const auto& col : left.columns)
      tfutil::copyCell(col, lres, srcRow, *ctx.out, node, dst);
    // appended right columns per lookup
    for (const LookupPlan& p : plans) {
      double k = lres.readNum(p.leftKey, srcRow);
      auto it = std::isnan(k) ? index.end() : index.find(keyBits(k));
      const bool hit = it != index.end();
      const std::size_t rightRow = hit ? it->second : 0;
      for (const PulledCol& pc : p.cols) {
        if (hit) {
          copyRightCell(pc.field, pc.dtype, pc.outName, rres, rightRow, *ctx.out,
                        node, dst);
        } else {
          // miss_ == Null here (Drop already filtered misses out).
          writeSentinel(*ctx.out, node, {pc.outName, pc.dtype}, dst);
        }
      }
    }
  }
}

}  // namespace dc
