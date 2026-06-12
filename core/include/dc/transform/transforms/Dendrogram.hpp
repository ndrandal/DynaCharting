// ENC-618a — `dendrogram` (tree) transform (RESEARCH §5.1 layout row; "dendrogram"
// node-link tree layout).
//
// Node-link tree positioning: LEAVES are spaced evenly along the cross-axis (X), an
// internal node sits at the MEAN cross-position of its children, and DEPTH maps to
// the main axis (Y = depth / maxDepth). The output is one (x,y) per node plus the
// `parent` link, so a line/rule mark can draw each node->parent edge and a point
// mark the nodes — the classic dendrogram / tidy-tree node-link diagram. (A
// dendrogram proper aligns all leaves at the deepest band; we emit depth-proportional
// y, which is the tidy-tree form and equals a dendrogram for a balanced tree.)
//
// OUTPUT SCHEMA (data-free): one ROW PER NODE (node index = row, build order):
//   node,parent,depth,leaf (i32) ; value (f32) ; x,y (f32, [0,1]) ;
//   px,py (f32, [0,1]) = the PARENT's position (the root's parent point = its own
//   position) so an edge mark can read both endpoints from one row.
//
// Fail-fast typing (inferSchema): every `levels` column must exist; `size` numeric
// (carried as the node value; not used for positioning — positions are structural).
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {

class DendrogramTransform : public TransformNode {
 public:
  DendrogramTransform(std::vector<std::string> levels, std::string size)
      : levels_(std::move(levels)), size_(std::move(size)) {}

  const char* op() const override { return "dendrogram"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::vector<std::string> levels_;
  std::string size_;
};

}  // namespace dc
