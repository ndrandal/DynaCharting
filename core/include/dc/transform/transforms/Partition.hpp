// ENC-618a — `partition` / icicle transform (RESEARCH §5.1 layout row; the
// rectangular-hierarchy layout: sunburst-unrolled / icicle).
//
// One axis = DEPTH (each level is a band of equal thickness), the other axis =
// VALUE-PROPORTIONAL extent (a node spans a slice proportional to its value within
// its parent's slice). The result tiles [0,1]x[0,1]: the root spans the full
// cross-axis at depth band 0; its children partition the next band by value; and so
// on. This is the icicle / partition layout (a Cartesian sunburst).
//
// ORIENTATION: depth runs along Y by default (band y in [depth/D, (depth+1)/D], the
// classic top-down icicle) and value along X. The number of bands D = maxDepth+1.
//
// OUTPUT SCHEMA (data-free): one ROW PER NODE (node index = row, build order):
//   node,parent,depth,leaf (i32) ; value (f32) ; x0,y0,x1,y1 (f32, [0,1]).
//
// Fail-fast typing (inferSchema): every `levels` column must exist; `size` numeric.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {

class PartitionTransform : public TransformNode {
 public:
  PartitionTransform(std::vector<std::string> levels, std::string size,
                     double padding = 0.0)
      : levels_(std::move(levels)), size_(std::move(size)), padding_(padding) {}

  const char* op() const override { return "partition"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::vector<std::string> levels_;
  std::string size_;
  double padding_{0.0};
};

}  // namespace dc
