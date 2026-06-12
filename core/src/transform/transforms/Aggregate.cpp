// ENC-616b — `aggregate` transform implementation. See Aggregate.hpp.
#include "dc/transform/transforms/Aggregate.hpp"

#include "TransformUtil.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

namespace dc {

namespace {

bool isNumeric(DType dt) {
  return dt == DType::F32 || dt == DType::I32 || dt == DType::Cat;
}

// Per-group running state for every measure. min/max/sum/count are streaming
// (class-1); Median keeps the value list (class-2, exact via nth_element).
struct Accum {
  double sum{0.0};
  double mn{std::numeric_limits<double>::infinity()};
  double mx{-std::numeric_limits<double>::infinity()};
  std::int64_t count{0};
  std::vector<double> vals;  // Median only
};

}  // namespace

SchemaResult AggregateTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  if (groupBy_.empty()) {
    r.error = "aggregate requires at least one groupBy column";
    return r;
  }
  // Validate keys (must exist) and remember their dtypes for the output.
  ColumnSchema out;
  for (const auto& key : groupBy_) {
    const SchemaColumn* col = input.find(key);
    if (!col) {
      r.error = "aggregate groupBy column '" + key + "' not found";
      return r;
    }
    out.columns.push_back({col->name, col->dtype});
  }
  // Validate measures (field exists + numeric for non-count; `as` unique + no key
  // collision) and append a measure column per reducer.
  for (const auto& m : measures_) {
    if (m.as.empty()) {
      r.error = "aggregate measure needs an 'as' name";
      return r;
    }
    if (out.has(m.as)) {
      r.error = "aggregate measure '" + m.as + "' collides with another column";
      return r;
    }
    if (m.op != AggOp::Count) {
      const SchemaColumn* col = input.find(m.field);
      if (!col) {
        r.error = "aggregate field '" + m.field + "' not found";
        return r;
      }
      if (!isNumeric(col->dtype)) {
        r.error = "aggregate field '" + m.field + "' must be numeric";
        return r;
      }
    }
    // Count -> i32; every other reducer -> f32 (GPU-native numeric).
    out.columns.push_back({m.as, m.op == AggOp::Count ? DType::I32 : DType::F32});
  }
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void AggregateTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;
  const std::size_t nKeys = groupBy_.size();
  const std::size_t nMeas = measures_.size();

  // Build the key tuple per row (keys read as doubles) and intern into groups in
  // first-appearance order. The map key is the joined numeric tuple.
  struct Group {
    std::vector<double> key;       // raw numeric key values (for the output)
    std::vector<Accum> acc;        // one per measure
  };
  std::vector<Group> groups;
  std::unordered_map<std::string, std::size_t> index;
  std::string keyStr;

  for (std::size_t i = 0; i < rows; ++i) {
    keyStr.clear();
    std::vector<double> keyVals(nKeys);
    for (std::size_t k = 0; k < nKeys; ++k) {
      const double kv = res.readNum(groupBy_[k], i);
      keyVals[k] = kv;
      // Bit-pattern join so distinct doubles never alias (handles negatives/NaN).
      std::uint64_t bits;
      static_assert(sizeof(bits) == sizeof(double), "double is 64-bit");
      std::memcpy(&bits, &kv, sizeof(bits));
      keyStr.append(reinterpret_cast<const char*>(&bits), sizeof(bits));
    }
    auto it = index.find(keyStr);
    std::size_t gi;
    if (it == index.end()) {
      gi = groups.size();
      index.emplace(keyStr, gi);
      Group g;
      g.key = std::move(keyVals);
      g.acc.resize(nMeas);
      groups.push_back(std::move(g));
    } else {
      gi = it->second;
    }
    Group& g = groups[gi];
    for (std::size_t m = 0; m < nMeas; ++m) {
      Accum& a = g.acc[m];
      ++a.count;
      if (measures_[m].op == AggOp::Count) continue;
      const double v = res.readNum(measures_[m].field, i);
      a.sum += v;
      a.mn = std::min(a.mn, v);
      a.mx = std::max(a.mx, v);
      if (measures_[m].op == AggOp::Median) a.vals.push_back(v);
    }
  }

  const std::size_t nGroups = groups.size();

  // Allocate the output: key columns (dtype preserved) + measure columns.
  const ColumnSchema& in = *ctx.inputSchema;
  for (std::size_t k = 0; k < nKeys; ++k) {
    const SchemaColumn* col = in.find(groupBy_[k]);
    const DType dt = col ? col->dtype : DType::F32;
    ctx.out->allocColumn(node, groupBy_[k], dt, nGroups);
  }
  for (const auto& m : measures_) {
    ctx.out->allocColumn(node, m.as,
                         m.op == AggOp::Count ? DType::I32 : DType::F32, nGroups);
  }

  // Write one row per group: key passthrough (dtype-aware) + each reducer's value.
  for (std::size_t gi = 0; gi < nGroups; ++gi) {
    Group& g = groups[gi];
    for (std::size_t k = 0; k < nKeys; ++k) {
      const SchemaColumn* col = in.find(groupBy_[k]);
      const DType dt = col ? col->dtype : DType::F32;
      const double kv = g.key[k];
      switch (dt) {
        case DType::F32:
          ctx.out->setF32(node, groupBy_[k], gi, static_cast<float>(kv));
          break;
        case DType::I32:
          ctx.out->setI32(node, groupBy_[k], gi, static_cast<std::int32_t>(kv));
          break;
        case DType::Cat:
          ctx.out->setCat(node, groupBy_[k], gi, static_cast<std::uint32_t>(kv));
          break;
        case DType::Timestamp:
          ctx.out->setTimestamp(node, groupBy_[k], gi,
                                static_cast<std::int64_t>(kv));
          break;
      }
    }
    for (std::size_t m = 0; m < nMeas; ++m) {
      Accum& a = g.acc[m];
      const AggMeasure& meas = measures_[m];
      if (meas.op == AggOp::Count) {
        ctx.out->setI32(node, meas.as, gi, static_cast<std::int32_t>(a.count));
        continue;
      }
      double val = 0.0;
      switch (meas.op) {
        case AggOp::Sum:
          val = a.sum;
          break;
        case AggOp::Mean:
          val = a.count > 0 ? a.sum / static_cast<double>(a.count) : 0.0;
          break;
        case AggOp::Min:
          val = a.count > 0 ? a.mn : 0.0;
          break;
        case AggOp::Max:
          val = a.count > 0 ? a.mx : 0.0;
          break;
        case AggOp::Median: {
          // Exact p50 via nth_element. Even count averages the two central values.
          auto& v = a.vals;
          if (v.empty()) {
            val = 0.0;
          } else {
            const std::size_t n = v.size();
            const std::size_t mid = n / 2;
            std::nth_element(v.begin(), v.begin() + mid, v.end());
            const double hi = v[mid];
            if (n % 2 == 1) {
              val = hi;
            } else {
              // lower-middle = max of the left partition.
              const double lo =
                  *std::max_element(v.begin(), v.begin() + mid);
              val = 0.5 * (lo + hi);
            }
          }
          break;
        }
        case AggOp::Count:  // handled above
          break;
      }
      ctx.out->setF32(node, meas.as, gi, static_cast<float>(val));
    }
  }
}

}  // namespace dc
