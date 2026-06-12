// ENC-618a — `pack` (circle packing) transform (RESEARCH §5.1 layout row).
//
// Enclosure circle-packing: each leaf becomes a circle with radius ∝ sqrt(value)
// (area ∝ value); siblings are packed tightly without overlap (the Wang et al.
// front-chain heuristic, as in D3's packSiblings), then wrapped in their parent's
// enclosing circle; the whole tree is fit into the unit square [0,1]x[0,1]. Output
// a circle (cx,cy,r) per node — the point/circle marks render it.
//
// OUTPUT SCHEMA (data-free): one ROW PER NODE (node index = row, build order):
//   node,parent,depth,leaf (i32) ; value (f32) ; cx,cy,r (f32, [0,1]).
//
// Fail-fast typing (inferSchema): every `levels` column must exist; `size` numeric.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {

class PackTransform : public TransformNode {
 public:
  PackTransform(std::vector<std::string> levels, std::string size,
                double padding = 0.0)
      : levels_(std::move(levels)), size_(std::move(size)), padding_(padding) {}

  const char* op() const override { return "pack"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::vector<std::string> levels_;
  std::string size_;
  double padding_{0.0};  // gap between sibling circles (in pre-fit units)
};

}  // namespace dc
