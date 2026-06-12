// ENC-618a — `partition` / icicle implementation. See Partition.hpp.
#include "dc/transform/transforms/Partition.hpp"

#include "Hierarchy.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace dc {

SchemaResult PartitionTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  const std::string err = hier::validateLevels(input, levels_, size_);
  if (!err.empty()) {
    r.error = err;
    return r;
  }
  ColumnSchema out;
  out.columns.push_back({"node", DType::I32});
  out.columns.push_back({"parent", DType::I32});
  out.columns.push_back({"depth", DType::I32});
  out.columns.push_back({"leaf", DType::I32});
  out.columns.push_back({"value", DType::F32});
  out.columns.push_back({"x0", DType::F32});
  out.columns.push_back({"y0", DType::F32});
  out.columns.push_back({"x1", DType::F32});
  out.columns.push_back({"y1", DType::F32});
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void PartitionTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& in = *ctx.input;
  const ColumnSchema& schema = *ctx.inputSchema;
  const Id node = ctx.nodeId;

  hier::Tree t = hier::build(schema, in, levels_, size_);
  const std::size_t n = t.nodes.size();

  // Cross-axis extent [x0,x1] per node; depth -> y band.
  std::vector<double> x0(n, 0.0), x1(n, 0.0);
  int maxDepth = 0;
  for (const auto& nd : t.nodes) maxDepth = std::max(maxDepth, nd.depth);
  const double bands = static_cast<double>(maxDepth + 1);

  // Root spans the full cross-axis.
  x0[0] = 0.0;
  x1[0] = 1.0;
  // Children partition their parent's [x0,x1] proportional to value, in order.
  for (std::size_t pi = 0; pi < n; ++pi) {
    const hier::Node& parent = t.nodes[pi];
    if (parent.children.empty()) continue;
    double valSum = 0.0;
    for (int c : parent.children)
      valSum += std::max(0.0, t.nodes[static_cast<std::size_t>(c)].value);
    const double span = x1[pi] - x0[pi];
    double cursor = x0[pi];
    for (int c : parent.children) {
      const double v = std::max(0.0, t.nodes[static_cast<std::size_t>(c)].value);
      const double frac = valSum > 0.0 ? v / valSum : 0.0;
      const double w = span * frac;
      x0[static_cast<std::size_t>(c)] = cursor;
      x1[static_cast<std::size_t>(c)] = cursor + w;
      cursor += w;
    }
  }

  ctx.out->allocColumn(node, "node", DType::I32, n);
  ctx.out->allocColumn(node, "parent", DType::I32, n);
  ctx.out->allocColumn(node, "depth", DType::I32, n);
  ctx.out->allocColumn(node, "leaf", DType::I32, n);
  ctx.out->allocColumn(node, "value", DType::F32, n);
  ctx.out->allocColumn(node, "x0", DType::F32, n);
  ctx.out->allocColumn(node, "y0", DType::F32, n);
  ctx.out->allocColumn(node, "x1", DType::F32, n);
  ctx.out->allocColumn(node, "y1", DType::F32, n);

  const double p = padding_;
  for (std::size_t i = 0; i < n; ++i) {
    const hier::Node& nd = t.nodes[i];
    double bx0 = x0[i], bx1 = x1[i];
    double by0 = static_cast<double>(nd.depth) / bands;
    double by1 = static_cast<double>(nd.depth + 1) / bands;
    // Padding gutter inside each cell (clamped so it never inverts a thin cell).
    if (p > 0.0) {
      const double hx = std::min(p, 0.5 * (bx1 - bx0));
      const double hy = std::min(p, 0.5 * (by1 - by0));
      bx0 += hx; bx1 -= hx;
      by0 += hy; by1 -= hy;
    }
    ctx.out->setI32(node, "node", i, static_cast<std::int32_t>(i));
    ctx.out->setI32(node, "parent", i, nd.parent);
    ctx.out->setI32(node, "depth", i, nd.depth);
    ctx.out->setI32(node, "leaf", i, nd.children.empty() ? 1 : 0);
    ctx.out->setF32(node, "value", i, static_cast<float>(nd.value));
    ctx.out->setF32(node, "x0", i, static_cast<float>(bx0));
    ctx.out->setF32(node, "y0", i, static_cast<float>(by0));
    ctx.out->setF32(node, "x1", i, static_cast<float>(bx1));
    ctx.out->setF32(node, "y1", i, static_cast<float>(by1));
  }
}

}  // namespace dc
