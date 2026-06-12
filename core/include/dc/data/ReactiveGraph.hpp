// ENC-595 (P1.4) — Generic reactive dirty/recompute mechanism.
//
// WHAT THIS IS
// ------------
// A small dirty/reactive trigger that fires recompute of dependent nodes when
// ANY input node changes. Per RESEARCH §3 (the typed acyclic streaming DAG) and
// §5.2 ("a per-node dirty flag, driven by the existing ChartSession touched-
// buffer set, gates recompute"). Today the only thing that changes is appended
// DATA; LATER (the §5/§6 interaction hooks) an interaction-SIGNAL node (a
// selection, a hovered row, a brush range) must flow through the SAME path.
//
// THE GENERICITY CONTRACT (non-negotiable, the whole point of the ticket)
// -----------------------------------------------------------------------
// The mechanism must NOT hardcode "only appended data changes". An input node is
// identified by an opaque (kind, key) pair, where `kind` distinguishes the class
// of source — Data (a TableStore version / an ingest buffer id) vs Signal (a
// future interaction node) vs anything added later — and `key` is the per-kind
// identity (a buffer id, a table id, a signal id). A dependent registers on a
// SET of input nodes; marking ANY of those inputs dirty schedules the dependent.
//
// HOW IT IS DRIVEN
// ----------------
// Two drive surfaces map the existing world onto this generic graph WITHOUT the
// graph knowing about either:
//   * markDataBuffersDirty(touched)  — the ChartSession touched-buffer set.
//   * syncTableVersions(...)         — the TableStore monotonic version() counter:
//     a Data-kind input keyed by table id is dirty iff its version advanced.
// A future interaction layer calls markSignalDirty(signalKey) on the SAME graph;
// its dependents recompute through the identical drain() path — no new plumbing.
//
// This node is a pure-`dc` control-plane primitive (C++17, no GPU, no Scene dep);
// it owns no buffers and recomputes nothing itself — it only decides WHO must
// recompute and hands the caller that set (the encode pass, a compute callback).
#pragma once

#include "dc/ids/Id.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// InputKind — the CLASS of a reactive input. Deliberately open-ended: `Data` is
// all this layer needs today, `Signal` is the forward-compat interaction slot
// (selection / hover / brush). New kinds can be appended without touching the
// graph machinery — the genericity contract above.
// ---------------------------------------------------------------------------
enum class InputKind : std::uint8_t {
  Data = 0,    // a TableStore version or an ingest buffer (changes on append)
  Signal = 1,  // a (future) interaction-signal node (selection/hover/brush)
};

// A reactive input node's identity: its kind + a per-kind key (buffer id, table
// id, signal id). Two inputs are the same node iff both halves match — so the
// SAME numeric key under different kinds is two distinct nodes (a buffer id 7
// and a signal id 7 never collide).
struct InputNode {
  InputKind kind{InputKind::Data};
  Id key{kInvalidId};

  bool operator==(const InputNode& o) const {
    return kind == o.kind && key == o.key;
  }
};

// Convenience constructors (call sites read better than brace-init).
inline InputNode dataInput(Id key) { return {InputKind::Data, key}; }
inline InputNode signalInput(Id key) { return {InputKind::Signal, key}; }

struct InputNodeHash {
  std::size_t operator()(const InputNode& n) const {
    // Mix kind into the high bits so Data/Signal of the same key differ.
    return std::hash<std::uint64_t>{}(
        (static_cast<std::uint64_t>(n.kind) << 56) ^ n.key);
  }
};

// A dependent of one or more input nodes. Identified by an opaque handle the
// caller chooses (e.g. a RecipeHandle, a transform-node id). When any of its
// inputs is marked dirty, the dependent is scheduled and surfaced by drain().
using DependentId = std::uint64_t;

// ---------------------------------------------------------------------------
// ReactiveGraph — the generic dirty/recompute trigger.
//
// Lifecycle per frame:
//   1. The driver marks inputs dirty (markDataBuffersDirty / syncTableVersions /
//      markSignalDirty / markDirty).
//   2. drain() returns the dependents whose inputs changed since the last drain,
//      deduplicated, and clears the pending set. The caller recomputes them.
// ---------------------------------------------------------------------------
class ReactiveGraph {
 public:
  // Register that `dependent` reacts to `input`. Idempotent: registering the
  // same edge twice is a no-op. A dependent may depend on many inputs and many
  // dependents may share an input.
  void addDependency(DependentId dependent, const InputNode& input);

  // Drop every edge of `dependent` (e.g. on unmount). Also clears it from the
  // pending set.
  void removeDependent(DependentId dependent);

  // ----- generic dirty marking ----------------------------------------------

  // Mark ANY input node dirty. The generic entry point — every other marker is
  // sugar over this. Schedules all dependents registered on `input`. Returns the
  // number of dependents newly scheduled (already-pending ones are not counted).
  std::size_t markDirty(const InputNode& input);

  // Mark a SIGNAL input dirty (the forward-compat interaction path). Pure sugar
  // for markDirty(signalInput(signalKey)).
  std::size_t markSignalDirty(Id signalKey) {
    return markDirty(signalInput(signalKey));
  }

  // Mark a set of DATA buffers dirty — the ChartSession touched-buffer set.
  // Each id becomes a Data-kind input. Returns total dependents scheduled.
  std::size_t markDataBuffersDirty(const std::vector<Id>& bufferIds);

  // Reconcile Data-kind inputs keyed by TABLE id against a version snapshot: a
  // table whose `current` version differs from the last seen version is marked
  // dirty (and the new version recorded). Pass the (tableId -> version()) pairs
  // for the tables this graph tracks. This is the TableStore version() drive
  // surface — structural table changes (e.g. a pivot adding a column) propagate
  // even when no buffer byte was appended. Returns dependents scheduled.
  std::size_t syncTableVersions(
      const std::vector<std::pair<Id, std::uint64_t>>& tableVersions);

  // ----- draining ------------------------------------------------------------

  // The dependents scheduled since the last drain, deduplicated and in a stable
  // (sorted) order, then clears the pending set. Empty when nothing was dirtied.
  std::vector<DependentId> drain();

  // Peek: is `dependent` currently scheduled (pending a drain)?
  bool isPending(DependentId dependent) const {
    return pending_.count(dependent) > 0;
  }

  // ----- introspection (tests / debugging) ----------------------------------

  // True iff `dependent` is registered on `input`.
  bool dependsOn(DependentId dependent, const InputNode& input) const;

  std::size_t inputCount() const { return inputs_.size(); }
  std::size_t pendingCount() const { return pending_.size(); }

 private:
  // input node -> dependents that react to it.
  std::unordered_map<InputNode, std::unordered_set<DependentId>, InputNodeHash>
      inputs_;
  // Last-seen version per Data-kind table input, for syncTableVersions().
  std::unordered_map<Id, std::uint64_t> tableVersionSeen_;
  // Dependents scheduled to recompute on the next drain.
  std::unordered_set<DependentId> pending_;
};

}  // namespace dc
