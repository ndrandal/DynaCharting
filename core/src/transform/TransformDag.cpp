// ENC-616a — Transform DAG: topo eval + ReactiveGraph-gated per-node dirty flag.
// See TransformDag.hpp for the model.
#include "dc/transform/TransformDag.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace dc {

TransformDag::Node* TransformDag::find(NodeId id) {
  auto it = nodes_.find(id);
  return it == nodes_.end() ? nullptr : &it->second;
}
const TransformDag::Node* TransformDag::find(NodeId id) const {
  auto it = nodes_.find(id);
  return it == nodes_.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
bool TransformDag::addSource(Id tableId) {
  if (!tables_.hasTable(tableId)) {
    lastError_ = "addSource: unknown table " + std::to_string(tableId);
    return false;
  }
  if (nodes_.count(tableId)) return true;  // idempotent
  Node n;
  n.id = tableId;
  n.kind = Kind::Source;
  n.tableId = tableId;
  // Source schema = the table's columns + dtypes (control-plane, data-free).
  for (const auto& name : tables_.columnNames(tableId)) {
    const Column* c = tables_.column(tableId, name);
    n.schema.columns.push_back({name, c->dtype});
  }
  nodes_.emplace(tableId, std::move(n));
  built_ = false;
  return true;
}

bool TransformDag::addTransform(NodeId nodeId, NodeId inputNode,
                                std::unique_ptr<TransformNode> transform) {
  if (nodes_.count(nodeId)) {
    lastError_ = "addTransform: node " + std::to_string(nodeId) + " exists";
    return false;
  }
  Node* in = find(inputNode);
  if (!in) {
    lastError_ = "addTransform: unknown input node " + std::to_string(inputNode);
    return false;
  }
  // Fail-fast typing: the output schema is a pure function of the input schema.
  SchemaResult sr = transform->inferSchema(in->schema);
  if (!sr.ok) {
    lastError_ = "addTransform[" + std::string(transform->op()) + "]: " + sr.error;
    return false;
  }
  Node n;
  n.id = nodeId;
  n.kind = Kind::Transform;
  n.input = inputNode;
  n.transform = std::move(transform);
  n.schema = std::move(sr.schema);
  nodes_.emplace(nodeId, std::move(n));

  // Record the edge for downstream dirty-closure, and wire the reactive dependency.
  in->dependents.push_back(nodeId);
  // A transform reacts (transitively) to the SOURCE table(s) it descends from. We
  // register the direct edge here; the downstream closure in evaluate() propagates
  // through intermediate transforms. For a transform reading a SOURCE, the input is
  // a Data-kind table input; for a transform reading a transform, the reactive edge
  // is implicit via the dependents-closure (no extra ReactiveGraph node needed).
  if (in->kind == Kind::Source) {
    reactive_.addDependency(static_cast<DependentId>(nodeId), dataInput(inputNode));
  }
  built_ = false;
  return true;
}

bool TransformDag::addJoin(NodeId nodeId, NodeId leftNode, NodeId rightNode,
                           std::unique_ptr<TransformNode> transform) {
  if (nodes_.count(nodeId)) {
    lastError_ = "addJoin: node " + std::to_string(nodeId) + " exists";
    return false;
  }
  if (!transform || transform->arity() != 2) {
    lastError_ = "addJoin: transform is not a binary (arity-2) node";
    return false;
  }
  Node* left = find(leftNode);
  if (!left) {
    lastError_ = "addJoin: unknown left input node " + std::to_string(leftNode);
    return false;
  }
  Node* right = find(rightNode);
  if (!right) {
    lastError_ = "addJoin: unknown right input node " + std::to_string(rightNode);
    return false;
  }
  // Fail-fast typing: output = f(left schema, right schema). A bad key dtype /
  // missing column is rejected here, before any row runs.
  SchemaResult sr = transform->inferSchemaBinary(left->schema, right->schema);
  if (!sr.ok) {
    lastError_ = "addJoin[" + std::string(transform->op()) + "]: " + sr.error;
    return false;
  }
  Node n;
  n.id = nodeId;
  n.kind = Kind::Transform;
  n.input = leftNode;
  n.rightInput = rightNode;
  n.transform = std::move(transform);
  n.schema = std::move(sr.schema);
  nodes_.emplace(nodeId, std::move(n));

  // Wire BOTH input edges for the downstream dirty-closure + reactive deps so the
  // join recomputes when EITHER the left rows or the right lookup table change.
  left->dependents.push_back(nodeId);
  right->dependents.push_back(nodeId);
  if (left->kind == Kind::Source)
    reactive_.addDependency(static_cast<DependentId>(nodeId), dataInput(leftNode));
  if (right->kind == Kind::Source)
    reactive_.addDependency(static_cast<DependentId>(nodeId), dataInput(rightNode));
  built_ = false;
  return true;
}

bool TransformDag::build() {
  // Topological sort over transform nodes (sources are leaves with no eval work).
  // Kahn's algorithm on the input -> node edges.
  topoOrder_.clear();
  std::unordered_map<NodeId, int> indeg;
  for (auto& [id, n] : nodes_) {
    if (n.kind == Kind::Transform) indeg[id];  // ensure present
  }
  // Count only TRANSFORM->TRANSFORM edges as in-degree: a source predecessor is a
  // DAG leaf with no eval work, so a transform reading only a source has in-degree
  // 0 and is immediately ready (the relaxation loop below mirrors this — it only
  // relaxes through transform predecessors).
  for (auto& [id, n] : nodes_) {
    if (n.kind != Kind::Transform) continue;
    for (NodeId dep : n.dependents) {
      if (find(dep) && find(dep)->kind == Kind::Transform) indeg[dep]++;
    }
  }
  std::vector<NodeId> ready;
  for (auto& [id, d] : indeg)
    if (d == 0) ready.push_back(id);
  std::sort(ready.begin(), ready.end());  // stable order for determinism

  while (!ready.empty()) {
    NodeId cur = ready.front();
    ready.erase(ready.begin());
    topoOrder_.push_back(cur);
    Node* n = find(cur);
    if (!n) continue;
    std::vector<NodeId> newly;
    for (NodeId dep : n->dependents) {
      Node* dn = find(dep);
      if (!dn || dn->kind != Kind::Transform) continue;
      if (--indeg[dep] == 0) newly.push_back(dep);
    }
    std::sort(newly.begin(), newly.end());
    ready.insert(ready.end(), newly.begin(), newly.end());
  }

  std::size_t transformCount = 0;
  for (auto& [id, n] : nodes_)
    if (n.kind == Kind::Transform) ++transformCount;
  if (topoOrder_.size() != transformCount) {
    lastError_ = "build: cycle detected in transform DAG";
    topoOrder_.clear();
    return false;
  }
  built_ = true;
  return true;
}

const ColumnSchema* TransformDag::schemaOf(NodeId node) const {
  const Node* n = find(node);
  return n ? &n->schema : nullptr;
}

// ---------------------------------------------------------------------------
// dirty marking
// ---------------------------------------------------------------------------
std::size_t TransformDag::markTableDirty(Id tableId) {
  return reactive_.markDirty(dataInput(tableId));
}

std::size_t TransformDag::markTouchedBuffers(
    const std::vector<Id>& touchedBuffers) {
  std::unordered_set<Id> touched(touchedBuffers.begin(), touchedBuffers.end());
  std::size_t scheduled = 0;
  for (auto& [id, n] : nodes_) {
    if (n.kind != Kind::Source) continue;
    for (const auto& col : n.schema.columns) {
      const Column* c = tables_.column(n.tableId, col.name);
      if (c && touched.count(c->bufferId)) {
        scheduled += reactive_.markDirty(dataInput(n.tableId));
        break;
      }
    }
  }
  return scheduled;
}

std::size_t TransformDag::syncSourceVersions() {
  std::vector<std::pair<Id, std::uint64_t>> versions;
  for (auto& [id, n] : nodes_) {
    if (n.kind == Kind::Source)
      versions.emplace_back(n.tableId, tables_.version(n.tableId));
  }
  return reactive_.syncTableVersions(versions);
}

// ---------------------------------------------------------------------------
// resolver / reads over either backing store
// ---------------------------------------------------------------------------
std::size_t TransformDag::rowCountOf(const Node& in) const {
  if (in.kind == Kind::Source) return tables_.rowCount(in.tableId, src_);
  // A transform's row count = the row count of its (consistent) output columns.
  // All a node's outputs are equal-length; use the first column.
  if (in.schema.columns.empty()) return 0;
  return store_.rowCount(in.id, in.schema.columns.front().name);
}

double TransformDag::readNumFrom(const Node& in, const std::string& name,
                                 std::size_t i) const {
  if (in.kind == Kind::Source) {
    const Column* c = tables_.column(in.tableId, name);
    if (!c) return std::nan("");
    switch (c->dtype) {
      case DType::F32: {
        auto v = tables_.viewF32(in.tableId, name, src_);
        return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
      }
      case DType::I32: {
        auto v = tables_.viewI32(in.tableId, name, src_);
        return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
      }
      case DType::Cat: {
        auto v = tables_.viewCat(in.tableId, name, src_);
        return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
      }
      case DType::Timestamp: {
        auto v = tables_.viewTimestamp(in.tableId, name, src_);
        return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
      }
    }
    return std::nan("");
  }
  // upstream transform output column
  switch (store_.dtypeOf(in.id, name)) {
    case DType::F32: {
      auto v = store_.viewF32(in.id, name);
      return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
    }
    case DType::I32: {
      auto v = store_.viewI32(in.id, name);
      return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
    }
    case DType::Cat: {
      auto v = store_.viewCat(in.id, name);
      return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
    }
    case DType::Timestamp: {
      auto v = store_.viewTimestamp(in.id, name);
      return i < v.size() ? static_cast<double>(v[i]) : std::nan("");
    }
  }
  return std::nan("");
}

std::int64_t TransformDag::readTsFrom(const Node& in, const std::string& name,
                                      std::size_t i) const {
  if (in.kind == Kind::Source) {
    auto v = tables_.viewTimestamp(in.tableId, name, src_);
    return i < v.size() ? v[i] : 0;
  }
  auto v = store_.viewTimestamp(in.id, name);
  return i < v.size() ? v[i] : 0;
}

ColumnResolver TransformDag::makeResolver(const Node& in) const {
  ColumnResolver r;
  const Node* inPtr = &in;
  r.rowCount = [this, inPtr] { return rowCountOf(*inPtr); };
  r.readNum = [this, inPtr](const std::string& name, std::size_t i) {
    return readNumFrom(*inPtr, name, i);
  };
  r.readTimestamp = [this, inPtr](const std::string& name, std::size_t i) {
    return readTsFrom(*inPtr, name, i);
  };
  r.dtypeOf = [this, inPtr](const std::string& name) -> DType {
    if (inPtr->kind == Kind::Source) {
      const Column* c = tables_.column(inPtr->tableId, name);
      return c ? c->dtype : DType::F32;
    }
    return store_.dtypeOf(inPtr->id, name);
  };
  return r;
}

// ---------------------------------------------------------------------------
// evaluation
// ---------------------------------------------------------------------------
std::vector<NodeId> TransformDag::evaluate() {
  std::vector<NodeId> ran;
  if (!built_) {
    lastError_ = "evaluate: build() not called (or failed)";
    return ran;
  }

  // Drive surface 2: version() reconcile (structural source changes propagate even
  // without an explicit markTouchedBuffers).
  syncSourceVersions();

  // The ReactiveGraph yields the transform nodes directly reacting to a dirtied
  // SOURCE. Seed the dirty set with them PLUS any node HELD (deferred) on a prior
  // pass — a held node's dirtiness persists until the gate lets it run.
  std::unordered_set<NodeId> dirty;
  for (DependentId d : reactive_.drain()) dirty.insert(static_cast<NodeId>(d));
  for (NodeId h : held_) dirty.insert(h);

  // Single topo pass that interleaves the downstream dirty-closure with the
  // scheduling gate (ENC-616e). For each node, in topo order:
  //   * if not dirty -> skip (dirty-gating respected: clean nodes never run).
  //   * if a gate is set and says the node is NOT due -> HOLD it: keep it dirty
  //     for next pass, do NOT run it, and do NOT propagate dirtiness through it
  //     (its dependents must not consume its stale output). A node already
  //     dirtied by some OTHER (running) upstream path stays dirty independently.
  //   * otherwise -> run it, and dirty its dependents (close downstream).
  // With no gate, every dirty node is due and this reduces to the foundation's
  // "full closure, run all dirty" behavior.
  held_.clear();

  for (NodeId id : topoOrder_) {
    if (!dirty.count(id)) continue;
    Node* n = find(id);
    if (!n || n->kind != Kind::Transform) continue;

    if (gate_ && !gate_(id)) {
      // Not due this pass: defer. Held nodes are re-seeded next evaluate(); their
      // downstream is NOT closed through them so dependents keep stale-free inputs.
      held_.insert(id);
      continue;
    }

    // Due: dirty its dependents so the closure flows to downstream transforms.
    for (NodeId dep : n->dependents) {
      if (find(dep) && find(dep)->kind == Kind::Transform) dirty.insert(dep);
    }
    const Node* in = find(n->input);
    if (!in) continue;
    ColumnResolver resolver = makeResolver(*in);
    EvalContext ctx;
    ctx.nodeId = id;
    ctx.inputSchema = &in->schema;
    ctx.input = &resolver;
    ctx.out = &store_;
    // Binary (join) node: also resolve its RIGHT input (the lookup table).
    ColumnResolver rightResolver;
    const Node* rin = nullptr;
    if (n->transform->arity() == 2 && n->rightInput != kInvalidId) {
      rin = find(n->rightInput);
      if (rin) {
        rightResolver = makeResolver(*rin);
        ctx.rightSchema = &rin->schema;
        ctx.right = &rightResolver;
      }
    }
    n->transform->evaluate(ctx);
    ++n->recomputes;
    ran.push_back(id);
  }
  return ran;
}

std::uint64_t TransformDag::recomputeCount(NodeId node) const {
  const Node* n = find(node);
  return n ? n->recomputes : 0;
}

}  // namespace dc
