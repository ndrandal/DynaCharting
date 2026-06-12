// ENC-618a — `dendrogram` (tree) implementation. See Dendrogram.hpp.
#include "dc/transform/transforms/Dendrogram.hpp"

#include "Hierarchy.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace dc {

SchemaResult DendrogramTransform::inferSchema(const ColumnSchema& input) const {
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
  out.columns.push_back({"x", DType::F32});
  out.columns.push_back({"y", DType::F32});
  out.columns.push_back({"px", DType::F32});
  out.columns.push_back({"py", DType::F32});
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void DendrogramTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& in = *ctx.input;
  const ColumnSchema& schema = *ctx.inputSchema;
  const Id node = ctx.nodeId;

  hier::Tree t = hier::build(schema, in, levels_, size_);
  const std::size_t n = t.nodes.size();

  int maxDepth = 0;
  for (const auto& nd : t.nodes) maxDepth = std::max(maxDepth, nd.depth);
  const double yDen = maxDepth > 0 ? static_cast<double>(maxDepth) : 1.0;

  // X: assign leaves evenly in left-to-right (build-discovery) order, then set each
  // internal node to the mean of its children's x. Count leaves first.
  std::vector<int> leafOrder;
  for (std::size_t i = 0; i < n; ++i)
    if (t.nodes[i].children.empty()) leafOrder.push_back(static_cast<int>(i));
  const std::size_t L = leafOrder.size();

  std::vector<double> x(n, 0.0), y(n, 0.0);
  for (std::size_t k = 0; k < L; ++k) {
    const double xv =
        L > 1 ? static_cast<double>(k) / static_cast<double>(L - 1) : 0.5;
    x[static_cast<std::size_t>(leafOrder[k])] = xv;
  }
  // Internal x = mean of children x (reverse pass: children done before parents).
  for (std::size_t i = n; i-- > 0;) {
    const hier::Node& nd = t.nodes[i];
    if (nd.children.empty()) continue;
    double sum = 0.0;
    for (int c : nd.children) sum += x[static_cast<std::size_t>(c)];
    x[i] = sum / static_cast<double>(nd.children.size());
  }
  // Y = depth proportional (monotonic non-decreasing by depth, strictly increasing
  // for a child vs its parent when maxDepth>0).
  for (std::size_t i = 0; i < n; ++i)
    y[i] = static_cast<double>(t.nodes[i].depth) / yDen;

  ctx.out->allocColumn(node, "node", DType::I32, n);
  ctx.out->allocColumn(node, "parent", DType::I32, n);
  ctx.out->allocColumn(node, "depth", DType::I32, n);
  ctx.out->allocColumn(node, "leaf", DType::I32, n);
  ctx.out->allocColumn(node, "value", DType::F32, n);
  ctx.out->allocColumn(node, "x", DType::F32, n);
  ctx.out->allocColumn(node, "y", DType::F32, n);
  ctx.out->allocColumn(node, "px", DType::F32, n);
  ctx.out->allocColumn(node, "py", DType::F32, n);
  for (std::size_t i = 0; i < n; ++i) {
    const hier::Node& nd = t.nodes[i];
    const std::size_t par = nd.parent >= 0 ? static_cast<std::size_t>(nd.parent) : i;
    ctx.out->setI32(node, "node", i, static_cast<std::int32_t>(i));
    ctx.out->setI32(node, "parent", i, nd.parent);
    ctx.out->setI32(node, "depth", i, nd.depth);
    ctx.out->setI32(node, "leaf", i, nd.children.empty() ? 1 : 0);
    ctx.out->setF32(node, "value", i, static_cast<float>(nd.value));
    ctx.out->setF32(node, "x", i, static_cast<float>(x[i]));
    ctx.out->setF32(node, "y", i, static_cast<float>(y[i]));
    ctx.out->setF32(node, "px", i, static_cast<float>(x[par]));
    ctx.out->setF32(node, "py", i, static_cast<float>(y[par]));
  }
}

}  // namespace dc
