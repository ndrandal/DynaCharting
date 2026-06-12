// ENC-618b (Epic ENC-618 — layout primitives) — `sankey` / alluvial transform
// (RESEARCH §5.1 sankey row: "WASM-CPU + CurveTessellator", streaming class-3;
// "node layout + ribbon routing").
//
// WHAT THIS REPLACES (RESEARCH §2.2)
// ----------------------------------
// The showcase ships 208–232 lines of UPSTREAM node-layout + ribbon tessellation,
// and "the explainer concedes [it] uses *straight* quads, not curves". This op
// pulls that layout INTO the engine off the RAW flows table and does better: each
// flow is routed as a SMOOTH cubic-bezier ribbon, not a straight quad.
//
// INPUT — a flows / edges table
// -----------------------------
//   * source — the upstream node key  (Cat / I32 / numeric, read as an id)
//   * target — the downstream node key (same dtype family)
//   * value  — the flow weight         (numeric, > 0 contributes)
// Typically the output of an `aggregate` (groupBy source,target -> sum value), so
// duplicate edges are pre-summed; this op also tolerates duplicate raw edges (it
// sums them while routing). An optional `color` field (Cat) tints each ribbon by
// a categorical palette; absent => tint by source node.
//
// THE LAYOUT (CPU, class-3 throttled global)
// ------------------------------------------
//   1. LAYERS — assign every node to a column by LONGEST-PATH depth from a root
//      (a node with no inbound edge). depth(n) = 0 for a root, else
//      1 + max(depth(src)) over inbound edges. A relaxation pass over a topo order
//      (cycles are broken defensively so it always terminates). Column x is the
//      layer normalized across the max depth.
//   2. ORDER WITHIN A LAYER — reduce crossings with the classic MEDIAN heuristic:
//      a few down/up sweeps reposition each node to the median bary-center of its
//      already-placed neighbors in the adjacent layer.
//   3. SIZE — node height ∝ THROUGHPUT = max(sum of inbound value, sum of outbound
//      value). Heights within a layer are stacked with a small gap; the tallest
//      layer is normalized to the unit band so the diagram fits [0,1]x[0,1].
//   4. POSITION — node rectangles (x0,y0,x1,y1) in normalized layout space (a
//      downstream linear/band scale maps them to clip).
//
// RIBBON ROUTING (CurveTessellator, the §5.1 "+ CurveTessellator")
// ----------------------------------------------------------------
// Each flow leaves a BAND on its source node's right edge and enters a band on its
// target node's left edge, the band height ∝ value (the flow's share of the node's
// throughput). The top and bottom edges of the ribbon are each a CUBIC BEZIER with
// horizontal control handles (an S-curve), tessellated via CurveTessellator. The
// ribbon is the filled strip between the two curves: a triangle list of Pos2Color4
// vertices (pos2 + per-vertex RGBA) — exactly the triGradient@1 / triSolid vertex
// stream the encode pass already consumes. NOT a straight quad.
//
// OUTPUTS (ColumnStore columns under this node — two independently-sized groups)
// -----------------------------------------------------------------------------
// NODE RECTANGLES (one row per distinct node):
//   * node        (I32)  — the node id (its source/target key, widened to i32)
//   * n_layer     (I32)  — the assigned column / layer index
//   * n_x0,n_y0,n_x1,n_y1 (F32) — rectangle corners in normalized [0,1] space
//   * n_value     (F32)  — the node throughput
// RIBBON GEOMETRY (one row per VERTEX — a triangle list, multiple of 3):
//   * v_x,v_y     (F32)  — clip/normalized position
//   * v_r,v_g,v_b,v_a (F32) — per-vertex color (0..1), the flow's tint
//
// Fail-fast typing (inferSchema, data-free): source/target/value must exist;
// value must be numeric; color (if named) must exist. The two output groups have
// different row counts — that is fine, each ColumnStore column is sized
// independently (a downstream encode binds the rect columns to a Rect mark and the
// ribbon columns to a triGradient mark).
#pragma once

#include "dc/transform/Transform.hpp"

#include <cstdint>
#include <string>

namespace dc {

// Tuning knobs for the layout + ribbon tessellation. Defaults give a clean
// unit-square diagram with smooth ribbons.
struct SankeyOptions {
  double nodeWidth{0.04};        // node rect width in normalized x (per column).
  double nodePadding{0.02};      // vertical gap between stacked nodes in a layer.
  int crossingSweeps{4};         // median-heuristic ordering sweeps (>=0).
  int ribbonSegments{24};        // cubic-bezier segments per ribbon edge curve.
  double curvature{0.5};         // S-curve handle reach as a fraction of the span.
};

class SankeyTransform : public TransformNode {
 public:
  SankeyTransform(std::string source, std::string target, std::string value,
                  std::string color = std::string(),
                  SankeyOptions opts = SankeyOptions{})
      : source_(std::move(source)),
        target_(std::move(target)),
        value_(std::move(value)),
        color_(std::move(color)),
        opts_(opts) {}

  const char* op() const override { return "sankey"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::string source_;
  std::string target_;
  std::string value_;
  std::string color_;  // optional; empty => tint by source node
  SankeyOptions opts_;
};

}  // namespace dc
