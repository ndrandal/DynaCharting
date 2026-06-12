// ENC-618a — `stratify` transform implementation. See Stratify.hpp.
#include "dc/transform/transforms/Stratify.hpp"

#include "Hierarchy.hpp"

#include <cstdint>

namespace dc {

SchemaResult StratifyTransform::inferSchema(const ColumnSchema& input) const {
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
  out.columns.push_back({"value", DType::F32});
  out.columns.push_back({"leaf", DType::I32});
  // Representative key per level (dtype preserved). A level name colliding with one
  // of the reserved output names would shadow it — reject for clarity.
  for (const auto& lv : levels_) {
    const SchemaColumn* c = input.find(lv);
    if (out.has(lv)) {
      r.error = "stratify level column '" + lv + "' collides with a reserved output";
      return r;
    }
    out.columns.push_back({lv, c->dtype});
  }
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void StratifyTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& in = *ctx.input;
  const ColumnSchema& schema = *ctx.inputSchema;
  const Id node = ctx.nodeId;

  hier::Tree t = hier::build(schema, in, levels_, size_);
  const std::size_t n = t.nodes.size();

  ctx.out->allocColumn(node, "node", DType::I32, n);
  ctx.out->allocColumn(node, "parent", DType::I32, n);
  ctx.out->allocColumn(node, "depth", DType::I32, n);
  ctx.out->allocColumn(node, "value", DType::F32, n);
  ctx.out->allocColumn(node, "leaf", DType::I32, n);
  for (const auto& lv : levels_) {
    const SchemaColumn* c = schema.find(lv);
    ctx.out->allocColumn(node, lv, c ? c->dtype : DType::F32, n);
  }

  for (std::size_t i = 0; i < n; ++i) {
    const hier::Node& nd = t.nodes[i];
    const bool isLeaf = nd.children.empty();
    ctx.out->setI32(node, "node", i, static_cast<std::int32_t>(i));
    ctx.out->setI32(node, "parent", i, nd.parent);
    ctx.out->setI32(node, "depth", i, nd.depth);
    ctx.out->setF32(node, "value", i, static_cast<float>(nd.value));
    ctx.out->setI32(node, "leaf", i, isLeaf ? 1 : 0);

    // Representative key per level: only meaningful for levels at or above the
    // node's depth (a node at depth d has its first d level keys determined). For
    // deeper levels we emit 0 (the node does not pin them).
    const int rep = hier::firstLeafRow(t, static_cast<int>(i));
    for (std::size_t lv = 0; lv < levels_.size(); ++lv) {
      const SchemaColumn* c = schema.find(levels_[lv]);
      const DType dt = c ? c->dtype : DType::F32;
      const bool pinned = static_cast<int>(lv) < nd.depth && rep >= 0;
      if (!pinned) {
        // Unpinned -> 0 in the level's dtype.
        switch (dt) {
          case DType::F32: ctx.out->setF32(node, levels_[lv], i, 0.0f); break;
          case DType::I32: ctx.out->setI32(node, levels_[lv], i, 0); break;
          case DType::Cat: ctx.out->setCat(node, levels_[lv], i, 0u); break;
          case DType::Timestamp:
            ctx.out->setTimestamp(node, levels_[lv], i, 0); break;
        }
        continue;
      }
      const std::size_t srcRow = static_cast<std::size_t>(rep);
      switch (dt) {
        case DType::F32:
          ctx.out->setF32(node, levels_[lv], i,
                          static_cast<float>(in.readNum(levels_[lv], srcRow)));
          break;
        case DType::I32:
          ctx.out->setI32(node, levels_[lv], i,
                          static_cast<std::int32_t>(in.readNum(levels_[lv], srcRow)));
          break;
        case DType::Cat:
          ctx.out->setCat(node, levels_[lv], i,
                          static_cast<std::uint32_t>(in.readNum(levels_[lv], srcRow)));
          break;
        case DType::Timestamp:
          ctx.out->setTimestamp(node, levels_[lv], i,
                                in.readTimestamp(levels_[lv], srcRow));
          break;
      }
    }
  }
}

}  // namespace dc
