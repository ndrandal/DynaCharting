// ENC-618a — `treemap` transform (RESEARCH §5.1 "treemap (squarify)", class-3
// resize-stable; §6.3 worked manifest — the engine op the showcase's 188-line
// precompute collapses into).
//
// Squarified treemap: recursively tile each node's rectangle with its children,
// area ∝ value, choosing row breaks that keep tile aspect ratios near 1 (the
// Bruls/Huizing/van Wijk squarify heuristic). Output a rectangle [x0,y0,x1,y1] in
// [0,1] space per node (root = the full padded frame), with optional padding inset
// per level.
//
// RESQUARIFY / STABLE TILING (the headline, RESEARCH line 176 "3 = resize-stable",
// §6.3 `tile:"stable"`). With stability ON, a node REMEMBERS, per parent, the row
// partition (which children share a squarify "row") it produced last time. On the
// next recompute, if the parent's child SET is unchanged (same node keys), it
// REUSES that partition and only RESCALES the slices to the new values — so a tile
// keeps its slot/neighbourhood frame-to-frame as values drift, instead of jumping
// when squarify would re-break the rows. The child set changing (add/remove) forces
// a fresh squarify for that parent. This is a class-3 throttled recompute (the
// scheduler debounces it ~100 ms), and the slot cache is mutable state carried
// across const evaluate() calls.
//
// OUTPUT SCHEMA (data-free): one ROW PER NODE (node index = row, build order):
//   node,parent,depth,leaf (i32) ; value (f32) ; x0,y0,x1,y1 (f32, [0,1]).
//   Internal nodes get their bounding rectangle; leaves get their final tile.
//
// Fail-fast typing (inferSchema): every `levels` column must exist; `size` must be
// numeric.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

class TreemapTransform : public TransformNode {
 public:
  // `levels` = the hierarchy key columns; `size` = the leaf-size column; `padding`
  // = inset (in [0,1] units) applied inside every internal node before tiling its
  // children; `stable` = resquarify (reuse a parent's row partition while its child
  // set is unchanged).
  TreemapTransform(std::vector<std::string> levels, std::string size,
                   double padding = 0.0, bool stable = false)
      : levels_(std::move(levels)),
        size_(std::move(size)),
        padding_(padding),
        stable_(stable) {}

  const char* op() const override { return "treemap"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::vector<std::string> levels_;
  std::string size_;
  double padding_{0.0};
  bool stable_{false};

  // Resquarify cache: per-parent (keyed by the parent node's stable key string) the
  // ordered child key sequence and the row partition (count of children per
  // squarify row). Reused while the child key set is unchanged. Mutable: class-3
  // layout state carried across const evaluate() calls.
  struct ParentPlan {
    std::vector<std::string> childKeys;  // children in their laid order
    std::vector<int> rowSizes;           // # children in each squarify row
  };
  mutable std::unordered_map<std::string, ParentPlan> plans_;
};

}  // namespace dc
