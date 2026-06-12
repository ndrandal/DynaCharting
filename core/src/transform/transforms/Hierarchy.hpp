// ENC-618a — shared hierarchy builder for the layout-tier transforms (RESEARCH
// §5.1 layout row + §6.3 worked treemap). Internal (src-only): turns a table whose
// rows are LEAVES into a node tree, given a list of LEVEL key columns (the
// `hierarchy:[...]` spec) and a numeric SIZE column.
//
// THE MODEL
// ---------
// Each input row is a leaf. The hierarchy spec is an ordered list of key columns
// (e.g. ["symbol","bucket"]): a leaf's ancestry is the running prefix of its key
// values. So {AAPL, b0} and {AAPL, b1} share an "AAPL" parent under a synthetic
// ROOT. Internal-node VALUE is the sum of its descendant leaves' sizes; the root's
// value is the grand total. Node index 0 is always the synthetic root (depth 0).
//
// Children appear in FIRST-APPEARANCE order of their key within their parent — the
// same group-discovery determinism the aggregate transform uses, so a given input
// always yields the same tree (and the SAME node indices for already-seen nodes,
// which is what makes resquarify's slot cache stable across recomputes).
#pragma once

#include "dc/transform/Transform.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {
namespace hier {

// One node in the built hierarchy. `parent` is the node index of the parent (-1 for
// the root). `depth` is 0 at the root. `value` is the size (leaf: its row's size
// column; internal/root: sum of descendant leaves). `firstLeafRow` is the input row
// index of the FIRST leaf under this node (-1 for internal nodes that are not also a
// leaf) — used to copy a representative key/measure through for labelling. `leafRow`
// is the input row this node IS (a leaf), else -1.
struct Node {
  int parent{-1};
  int depth{0};
  double value{0.0};
  int leafRow{-1};                 // input row if this node is a leaf, else -1
  std::vector<int> children;       // child node indices, first-appearance order
  // A stable identity string (the joined key path) so a layout cache can match a
  // node across recomputes even as sibling counts change.
  std::string key;
};

// The built hierarchy: nodes[0] is the synthetic root. Leaves are the nodes with
// leafRow >= 0 (and no children). `leafNodeOfRow[r]` maps input row r -> its node
// index (so a layout can write per-leaf geometry back keyed by node order).
struct Tree {
  std::vector<Node> nodes;
  std::vector<int> leafNodeOfRow;  // size = input rows
  bool ok{false};
  std::string error;
};

// Build the tree from the resolver's rows. `levels` are the key columns (>=1);
// `sizeField` is the numeric leaf-size column. Sizes are read as doubles; a
// negative or NaN size is clamped to 0 (a leaf with no area). The resolver/schema
// come from the EvalContext.
inline Tree build(const ColumnSchema& schema, const ColumnResolver& in,
                  const std::vector<std::string>& levels,
                  const std::string& sizeField) {
  Tree t;
  if (levels.empty()) {
    t.error = "hierarchy needs at least one level column";
    return t;
  }
  const std::size_t rows = in.rowCount();
  t.leafNodeOfRow.assign(rows, -1);

  // Synthetic root.
  t.nodes.push_back(Node{});
  t.nodes[0].parent = -1;
  t.nodes[0].depth = 0;
  t.nodes[0].key = "";

  // childIndex[parentNode] : key-segment -> child node index (first-appearance).
  std::vector<std::unordered_map<std::string, int>> childIndex;
  childIndex.resize(1);  // for root

  auto bits = [](double v) {
    std::uint64_t b;
    static_assert(sizeof(b) == sizeof(double), "double 64-bit");
    std::memcpy(&b, &v, sizeof(b));
    return std::string(reinterpret_cast<const char*>(&b), sizeof(b));
  };

  for (std::size_t r = 0; r < rows; ++r) {
    int cur = 0;  // start at root
    std::string path;
    for (std::size_t lv = 0; lv < levels.size(); ++lv) {
      const double kv = in.readNum(levels[lv], r);
      const std::string seg = bits(kv);
      path += seg;
      auto it = childIndex[static_cast<std::size_t>(cur)].find(seg);
      int childNode;
      if (it == childIndex[static_cast<std::size_t>(cur)].end()) {
        childNode = static_cast<int>(t.nodes.size());
        Node n;
        n.parent = cur;
        n.depth = t.nodes[static_cast<std::size_t>(cur)].depth + 1;
        n.key = path;
        t.nodes.push_back(n);
        childIndex.emplace_back();  // index map for the new node (may realloc)
        t.nodes[static_cast<std::size_t>(cur)].children.push_back(childNode);
        // Re-index AFTER both vectors may have reallocated — never hold a stale ref.
        childIndex[static_cast<std::size_t>(cur)].emplace(seg, childNode);
      } else {
        childNode = it->second;
      }
      cur = childNode;
    }
    // `cur` is the leaf node for this row. (If two rows share the full key path
    // they map to the SAME leaf node and their sizes accumulate — the aggregate
    // tier normally makes leaves unique, but we tolerate duplicates.)
    double sz = in.readNum(sizeField, r);
    if (!(sz > 0.0)) sz = 0.0;  // clamp NaN / negatives / zero to a 0-area leaf
    Node& leaf = t.nodes[static_cast<std::size_t>(cur)];
    if (leaf.leafRow < 0) leaf.leafRow = static_cast<int>(r);
    leaf.value += sz;
    t.leafNodeOfRow[r] = cur;
  }

  // Roll leaf values up to ancestors (process deepest-first by iterating nodes in
  // reverse: a child always has a higher index than its parent by construction).
  for (std::size_t i = t.nodes.size(); i-- > 1;) {
    Node& n = t.nodes[i];
    if (n.parent >= 0) t.nodes[static_cast<std::size_t>(n.parent)].value += n.value;
  }

  t.ok = true;
  return t;
}

// First descendant leaf's input row of node `i` (i itself if it is a leaf). -1 if
// the subtree has no leaf (cannot happen for a built tree, but defensive). Used to
// carry a representative key per node through to the output for labelling.
inline int firstLeafRow(const Tree& t, int i) {
  int cur = i;
  while (true) {
    const Node& n = t.nodes[static_cast<std::size_t>(cur)];
    if (n.leafRow >= 0) return n.leafRow;
    if (n.children.empty()) return -1;
    cur = n.children.front();
  }
}

// Shared inferSchema validation for a layout transform: every level column + the
// size column must exist; size must be numeric (f32/i32/cat). Returns an error
// string (empty if valid). `outCols` is appended with the level dtypes preserved
// (so a representative key can be carried through per node for labelling).
inline std::string validateLevels(const ColumnSchema& input,
                                  const std::vector<std::string>& levels,
                                  const std::string& sizeField) {
  if (levels.empty()) return "hierarchy needs at least one level column";
  for (const auto& lv : levels) {
    if (!input.find(lv)) return "hierarchy level column '" + lv + "' not found";
  }
  const SchemaColumn* s = input.find(sizeField);
  if (!s) return "hierarchy size column '" + sizeField + "' not found";
  if (s->dtype == DType::Timestamp)
    return "hierarchy size column '" + sizeField + "' must be numeric";
  return "";
}

}  // namespace hier
}  // namespace dc
