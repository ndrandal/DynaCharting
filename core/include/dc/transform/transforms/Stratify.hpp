// ENC-618a — `stratify` transform (RESEARCH §5.1 layout row; the shared prerequisite
// for every hierarchy layout).
//
// Builds a HIERARCHY from flat rows. Each input row is a LEAF; the `levels` key
// columns (e.g. ["symbol","bucket"]) give each leaf its ancestry as the running key
// prefix, so leaves that share a prefix share an ancestor under a synthetic ROOT.
// This is the §5.1 "stratify" op: flat rows -> a parent/depth/value tree, the input
// the treemap/partition/pack/dendrogram layouts all consume.
//
// OUTPUT SCHEMA (data-free): one ROW PER NODE (root + internal + leaves), node index
// = row index, in BUILD order (root=0, then nodes in first-appearance order):
//   * node     (i32) — this node's index (== its row).
//   * parent   (i32) — parent node index, or -1 for the root.
//   * depth    (i32) — 0 at the root, +1 per level.
//   * value    (f32) — leaf size, or the sum of descendant leaf sizes (root = total).
//   * leaf     (i32) — 1 if the node is a leaf (no children), else 0.
//   * <level>  (dtype-preserved) — a REPRESENTATIVE key value per level column,
//                taken from the node's first descendant leaf (0 above the node's
//                depth) so a downstream encode can label/colour a node by its keys.
//
// Fail-fast typing (inferSchema): every `levels` column must exist; `size` must
// exist and be numeric. The level value columns keep their input dtype.
#pragma once

#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {

class StratifyTransform : public TransformNode {
 public:
  StratifyTransform(std::vector<std::string> levels, std::string size)
      : levels_(std::move(levels)), size_(std::move(size)) {}

  const char* op() const override { return "stratify"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

 private:
  std::vector<std::string> levels_;
  std::string size_;
};

}  // namespace dc
