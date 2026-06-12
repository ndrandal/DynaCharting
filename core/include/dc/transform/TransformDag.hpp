// ENC-616a — The Transform DAG: topological evaluation with per-node dirty gating
// reusing the ReactiveGraph (RESEARCH §5.2).
//
// THE MODEL
// ---------
// A DAG of nodes over named typed columns. Two node kinds:
//   * SOURCE   — a TableStore table. Its output schema is the table's columns; its
//                bytes come from the ingest feed via a BufferByteSource. Sources are
//                the DAG's leaves (no upstream); their dirtiness is driven by the
//                ChartSession touched-buffer set + the TableStore version() counter.
//   * TRANSFORM— a pure node (filter/formula/…) with ONE input (a source or another
//                transform), referenced by id. Its output schema is inferSchema(of
//                its input schema) — validated fail-fast at addTransform (§6.1).
//
// TOPOLOGICAL EVALUATION + DIRTY GATING (the heart of §5.2)
// --------------------------------------------------------
// build() topo-sorts the nodes (acyclicity enforced — a cycle is rejected). Each
// evaluation pass:
//   1. The caller marks what changed: markTableDirty(tableId) when ingest touched a
//      source (sugar over the ReactiveGraph Data-kind input), or the DAG syncs
//      TableStore version() automatically.
//   2. drainDirty() asks the ReactiveGraph which transform nodes react to a dirtied
//      input, then closes that set DOWNSTREAM along DAG edges (a dirty node dirties
//      its dependents). ONLY those nodes recompute — proven by a recompute counter.
//   3. evaluate() runs the dirty nodes in topological order, each reading its input
//      (resolved over TableStore OR an upstream node's ColumnStore outputs) and
//      writing its outputs into the shared ColumnStore.
//
// The dirty gating REUSES ReactiveGraph (it does not reinvent one): each transform
// node is a DependentId; each source table is a Data-kind InputNode keyed by table
// id; addTransform wires the dependency edges. This is exactly the §5.2 "per-node
// dirty flag, driven by the existing ChartSession touched-buffer set + TableStore
// version()".
//
// Pure `dc` (C++17, no GPU). The DAG owns the ColumnStore for intermediates and a
// ReactiveGraph; it borrows the TableStore + BufferByteSource (the ingest seam).
#pragma once

#include "dc/data/ReactiveGraph.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/Transform.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dc {

// A node id in the DAG: same Id space as everything else. Source tables and
// transform nodes share the namespace (the §6.1 "one namespace" rule), so a
// transform input is just "the node id I read from".
using NodeId = Id;

// ---------------------------------------------------------------------------
// TransformDag
// ---------------------------------------------------------------------------
class TransformDag {
 public:
  // Bind the table layer this DAG reads sources from. `tables` provides schemas +
  // version(); `src` provides the bytes (the ingest BufferByteSource). Both are
  // borrowed and must outlive the DAG.
  TransformDag(const TableStore& tables, BufferByteSource src)
      : tables_(tables), src_(std::move(src)) {}

  // ----- DAG construction ----------------------------------------------------

  // Declare a SOURCE node backed by TableStore table `tableId`. The node id is the
  // table id (a table is addressable as a node directly). Idempotent. Returns
  // false if the table is unknown.
  bool addSource(Id tableId);

  // Add a TRANSFORM node `nodeId` running `transform`, reading from input node
  // `inputNode` (a source or another transform). Fails (returns false, sets
  // lastError()) if: the input node is unknown, nodeId already exists, or
  // inferSchema rejects the input schema (fail-fast typing). On success the output
  // schema is recorded and the ReactiveGraph dependency wired.
  bool addTransform(NodeId nodeId, NodeId inputNode,
                    std::unique_ptr<TransformNode> transform);

  // Add a BINARY (relational JOIN) node `nodeId` reading a LEFT input (`leftNode`,
  // the edges/rows being resolved) and a RIGHT input (`rightNode`, the lookup
  // table). `transform->arity()` must be 2 (a JoinTransform). The output schema is
  // inferSchemaBinary(left, right), validated fail-fast here. BOTH inputs are wired
  // for dirty-gating: the join recomputes when EITHER side's key/columns change
  // (RESEARCH §5.1 "recompute on key change"). Fails (false + lastError()) on an
  // unknown input, an existing nodeId, a non-binary transform, or a typing reject.
  bool addJoin(NodeId nodeId, NodeId leftNode, NodeId rightNode,
               std::unique_ptr<TransformNode> transform);

  // Finalize: topo-sort (rejects cycles). Must be called after the last
  // addTransform and before evaluate(). Returns false + lastError() on a cycle.
  bool build();

  // ----- schema queries (data-free) -----------------------------------------

  // The output schema of any node (source table columns, or a transform's
  // inferred schema). nullptr if the node is unknown.
  const ColumnSchema* schemaOf(NodeId node) const;

  bool hasNode(NodeId node) const { return nodes_.count(node) > 0; }

  // ----- dirty marking + evaluation -----------------------------------------

  // Mark a SOURCE table dirty (sugar over the ReactiveGraph Data-kind input). Call
  // when the ingest feed touched that table's buffers. Returns dependents scheduled.
  std::size_t markTableDirty(Id tableId);

  // Mark a set of source tables dirty by the ChartSession touched-buffer set: each
  // table whose any column buffer id is in `touchedBuffers` is dirtied. (A source's
  // buffers are its columns' bufferIds.)
  std::size_t markTouchedBuffers(const std::vector<Id>& touchedBuffers);

  // Reconcile against TableStore version(): any source whose version advanced since
  // the last sync is dirtied. Returns dependents scheduled. Called automatically at
  // the top of evaluate(); exposed for tests.
  std::size_t syncSourceVersions();

  // Evaluate the DAG: compute the dirty-closure (dirtied sources -> reacting
  // transforms -> their downstream), then run ONLY those transform nodes in topo
  // order. Returns the list of node ids that recomputed this pass (for tests /
  // incremental proofs). A node not in the closure is NOT touched.
  std::vector<NodeId> evaluate();

  // ----- streaming-scheduler seam (ENC-616e) --------------------------------
  //
  // A SCHEDULING GATE lets a streaming scheduler decide, per evaluation pass,
  // which dirty nodes are DUE to recompute by their streaming class (class-1 every
  // frame, class-2 on a window/hop boundary, class-3 on a throttled cadence). The
  // gate is consulted for every node in the dirty closure; if it returns false the
  // node is NOT due this pass: it is HELD (its dirtiness persists, re-seeded next
  // evaluate) and its downstream is NOT closed THROUGH it (an expensive global
  // does not force its dependents to re-run before it has itself run).
  //
  // The gate is purely additive: with no gate set (the default) evaluate() behaves
  // exactly as the foundation — every dirty node runs every pass. A node the gate
  // is never asked about (not dirty) is untouched: dirty-gating is still respected.
  using NodeGate = std::function<bool(NodeId)>;
  void setNodeGate(NodeGate gate) { gate_ = std::move(gate); }
  void clearNodeGate() { gate_ = nullptr; }

  // Is `node` currently HELD (dirty but deferred by the gate on the last pass)?
  // A scheduler reads this to know a class-3 global still owes a recompute.
  bool isHeld(NodeId node) const { return held_.count(node) > 0; }
  std::size_t heldCount() const { return held_.size(); }

  // ----- introspection -------------------------------------------------------

  const ColumnStore& columns() const { return store_; }
  ColumnStore& columns() { return store_; }

  // Total recomputes of `node` over the DAG's life (the dirty-gating proof
  // counter). 0 for sources / unknown nodes.
  std::uint64_t recomputeCount(NodeId node) const;

  const std::string& lastError() const { return lastError_; }
  std::size_t nodeCount() const { return nodes_.size(); }

 private:
  enum class Kind : std::uint8_t { Source, Transform };

  struct Node {
    NodeId id{kInvalidId};
    Kind kind{Kind::Source};
    Id tableId{kInvalidId};                 // Source only
    NodeId input{kInvalidId};               // Transform: LEFT input
    NodeId rightInput{kInvalidId};          // Join only: RIGHT input
    std::unique_ptr<TransformNode> transform;  // Transform only
    ColumnSchema schema;                    // output schema of this node
    std::vector<NodeId> dependents;         // nodes that read THIS node
    std::uint64_t recomputes{0};
  };

  Node* find(NodeId id);
  const Node* find(NodeId id) const;

  // Build the input ColumnResolver for a transform node (reads TableStore if its
  // input is a source, else the ColumnStore for an upstream transform).
  ColumnResolver makeResolver(const Node& inputNode) const;

  // Read one input column value as a double from either backing store.
  double readNumFrom(const Node& inputNode, const std::string& name,
                     std::size_t i) const;
  std::int64_t readTsFrom(const Node& inputNode, const std::string& name,
                          std::size_t i) const;
  std::size_t rowCountOf(const Node& inputNode) const;

  const TableStore& tables_;
  BufferByteSource src_;
  ColumnStore store_;
  ReactiveGraph reactive_;

  std::unordered_map<NodeId, Node> nodes_;
  std::vector<NodeId> topoOrder_;  // transform nodes only, in eval order
  bool built_{false};
  std::string lastError_;

  // Streaming-scheduler gate + the set of nodes it deferred last pass (held
  // dirty). held_ is re-seeded into the dirty set at the top of each evaluate().
  NodeGate gate_;
  std::unordered_set<NodeId> held_;
};

}  // namespace dc
