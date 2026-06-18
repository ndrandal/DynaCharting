// ENC-655 (B5b) — InteractionRuntime: the reactive render loop that turns a
// signal change into updated geometry.
//
// THE LOOP IT CLOSES
// ------------------
//   SignalStore.set(sig, ...)            (user brushes / hovers / clicks)
//     -> markSignalDirty (ENC-624)       (schedules the dependent transform nodes)
//   InteractionRuntime::refresh():
//     -> TransformDag::evaluate()        (recompute the dirty closure — incl.
//                                         selectionFilter / conditional-color)
//     -> materializeNodeToTable (B5a)    (each mark's output node -> a table)
//     -> EncodePass::compile             (re-encode the mark from that table)
//     => compiledMarks()                 (the rendered scene reflecting the
//                                         CURRENT selection state)
//
// This is the Manifest runtime's sibling for the interaction path: like Manifest
// it produces CompiledMark-shaped results (geometry + draw item + bytes) the
// caller wires into a Scene; unlike Manifest it sources each mark from a transform
// DAG node, so it re-runs correctly on a signal OR data change. Pure `dc` (no GPU);
// the bytes/geometry are testable headlessly.
#pragma once

#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/interaction/RenderBridge.hpp"
#include "dc/transform/TransformDag.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dc {

// One compiled interaction mark: its id + the encode result (geometry + draw item
// + packed bytes) reflecting the latest refresh().
struct RuntimeMark {
  std::string id;
  EncodeResult result;
};

class InteractionRuntime {
 public:
  // `dag` is borrowed and must outlive the runtime. Bind a SignalStore to
  // dag.reactive() so signal mutations dirty the nodes this runtime re-encodes.
  explicit InteractionRuntime(TransformDag& dag) : dag_(dag) {}

  // Register a mark sourced from transform node `node` (a source or transform —
  // typically a selectionFilter / conditional output). `enc` binds the mark's
  // channels to the node's output columns. The ids are the geometry / draw-item /
  // vertex-buffer ids the caller allocated for this mark; `tableId`/`firstBufferId`
  // namespace the per-mark materialization buffers. Returns the mark index.
  std::size_t addMark(std::string id, NodeId node, Mark mark, Encoding enc,
                      Id geometryId, Id drawItemId, Id vertexBufferId,
                      Id tableId = 1, Id firstBufferId = 1000,
                      LineStyle lineStyle = LineStyle::Line2d);

  // Re-evaluate the DAG, then materialize + re-encode every registered mark from
  // its CURRENT output. Returns the compiled marks (the rendered scene). Call
  // after a SignalStore mutation (or any data change) to refresh the view.
  const std::vector<RuntimeMark>& refresh();

  // The marks compiled by the last refresh().
  const std::vector<RuntimeMark>& compiledMarks() const { return compiled_; }
  const RuntimeMark* compiledMark(const std::string& id) const;

  std::size_t markCount() const { return marks_.size(); }

 private:
  struct MarkSpec {
    std::string id;
    NodeId node{kInvalidId};
    Mark mark{Mark::Point};
    Encoding enc;
    Id geometryId{kInvalidId}, drawItemId{kInvalidId}, vertexBufferId{kInvalidId};
    Id tableId{1}, firstBufferId{1000};
    LineStyle lineStyle{LineStyle::Line2d};
    // Per-mark materialization backing (overwritten in place each refresh).
    IngestProcessor ingest;
    TableStore table;
  };

  TransformDag& dag_;
  EncodePass encode_;
  std::vector<std::unique_ptr<MarkSpec>> marks_;
  std::vector<RuntimeMark> compiled_;
};

}  // namespace dc
