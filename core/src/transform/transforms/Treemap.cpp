// ENC-618a — `treemap` (squarify + resquarify) implementation. See Treemap.hpp.
#include "dc/transform/transforms/Treemap.hpp"

#include "Hierarchy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace dc {

namespace {

struct Rect {
  double x0{0}, y0{0}, x1{0}, y1{0};
  double w() const { return x1 - x0; }
  double h() const { return y1 - y0; }
};

// Worst aspect ratio of a row of areas laid along the shorter side `length`, given
// the row's total area `rowArea` and side `side` = (rowArea / length) is the row's
// thickness. Bruls et al. "worst" ratio. `areas` are the row members' areas.
double worstRatio(const std::vector<double>& areas, double rowSum, double length) {
  if (length <= 0.0 || rowSum <= 0.0) return std::numeric_limits<double>::infinity();
  double rmax = -std::numeric_limits<double>::infinity();
  double rmin = std::numeric_limits<double>::infinity();
  for (double a : areas) {
    rmax = std::max(rmax, a);
    rmin = std::min(rmin, a);
  }
  const double s2 = rowSum * rowSum;
  const double l2 = length * length;
  // max( l^2*rmax / s^2 , s^2 / (l^2*rmin) )
  const double hi = (l2 * rmax) / s2;
  const double lo = s2 / (l2 * rmin);
  return std::max(hi, lo);
}

// Lay a single row of children (their normalized areas) across the current free
// rect, stacking along the shorter side. Returns the leftover rect after the row.
// Writes each member's rect into `out` (indexed parallel to `members`).
Rect placeRow(const Rect& free, const std::vector<double>& areas, double rowSum,
              std::vector<Rect>& out) {
  Rect rem = free;
  const double freeArea = free.w() * free.h();
  if (freeArea <= 0.0 || rowSum <= 0.0) {
    // Degenerate: collapse all members to zero-area rects at the corner.
    for (std::size_t i = 0; i < areas.size(); ++i)
      out.push_back(Rect{free.x0, free.y0, free.x0, free.y0});
    return rem;
  }
  if (free.w() >= free.h()) {
    // Row stacks vertically in a left-hand column of width = rowSum/height.
    const double colW = rowSum / free.h();
    double y = free.y0;
    for (double a : areas) {
      const double frac = rowSum > 0 ? a / rowSum : 0.0;
      const double cellH = free.h() * frac;
      out.push_back(Rect{free.x0, y, free.x0 + colW, y + cellH});
      y += cellH;
    }
    rem.x0 = free.x0 + colW;
  } else {
    // Row stacks horizontally in a bottom row of height = rowSum/width.
    const double rowH = rowSum / free.w();
    double x = free.x0;
    for (double a : areas) {
      const double frac = rowSum > 0 ? a / rowSum : 0.0;
      const double cellW = free.w() * frac;
      out.push_back(Rect{x, free.y0, x + cellW, free.y0 + rowH});
      x += cellW;
    }
    rem.y0 = free.y0 + rowH;
  }
  return rem;
}

}  // namespace

SchemaResult TreemapTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  const std::string err = hier::validateLevels(input, levels_, size_);
  if (!err.empty()) {
    r.error = err;
    return r;
  }
  ColumnSchema out;
  out.columns.push_back({"node", DType::I32});
  out.columns.push_back({"parent", DType::I32});
  out.columns.push_back({"depth", DType::I32});
  out.columns.push_back({"leaf", DType::I32});
  out.columns.push_back({"value", DType::F32});
  out.columns.push_back({"x0", DType::F32});
  out.columns.push_back({"y0", DType::F32});
  out.columns.push_back({"x1", DType::F32});
  out.columns.push_back({"y1", DType::F32});
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void TreemapTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& in = *ctx.input;
  const ColumnSchema& schema = *ctx.inputSchema;
  const Id node = ctx.nodeId;

  hier::Tree t = hier::build(schema, in, levels_, size_);
  const std::size_t n = t.nodes.size();

  std::vector<Rect> rects(n);
  // Root frame: the full [0,1] square, padding-inset.
  const double p = padding_;
  rects[0] = Rect{p, p, 1.0 - p, 1.0 - p};

  // Track which parent keys we touched this pass so stale plans can be pruned (keeps
  // the resquarify cache from growing unbounded over a long stream).
  std::vector<std::string> touchedParents;

  // Lay out a parent's children into the parent's rect (BFS from root: a parent is
  // always processed before its children since children have higher node indices).
  for (std::size_t pi = 0; pi < n; ++pi) {
    const hier::Node& parent = t.nodes[pi];
    if (parent.children.empty()) continue;
    // Tile the parent's INTERIOR: its stored tile inset by `padding` (the root's
    // stored rect is already the padded frame, so a second inset gives a gutter
    // around every nesting level — the §6.3 `padding` knob). Leaves are NOT inset
    // (they have no children to tile).
    Rect frame = rects[pi];
    if (padding_ > 0.0 && pi != 0) {
      const double ix0 = std::min(frame.x0 + padding_, frame.x1);
      const double iy0 = std::min(frame.y0 + padding_, frame.y1);
      const double ix1 = std::max(frame.x1 - padding_, ix0);
      const double iy1 = std::max(frame.y1 - padding_, iy0);
      frame = Rect{ix0, iy0, ix1, iy1};
    }

    // Children's areas ∝ value. Total area = the frame area; normalize.
    const std::vector<int>& kids = parent.children;
    double valSum = 0.0;
    for (int c : kids) valSum += std::max(0.0, t.nodes[static_cast<std::size_t>(c)].value);
    const double frameArea = std::max(0.0, frame.w()) * std::max(0.0, frame.h());

    std::vector<double> areas(kids.size());
    for (std::size_t i = 0; i < kids.size(); ++i) {
      const double v = std::max(0.0, t.nodes[static_cast<std::size_t>(kids[i])].value);
      areas[i] = valSum > 0.0 ? frameArea * (v / valSum) : 0.0;
    }

    // Determine the row partition: reuse a stable plan if the child set matches,
    // else run squarify to choose row breaks.
    std::vector<int> rowSizes;
    bool reused = false;
    const std::string& pkey = parent.key;
    if (stable_) {
      auto it = plans_.find(pkey);
      if (it != plans_.end()) {
        // Match child key set (order-insensitive set equality on the laid keys).
        const ParentPlan& plan = it->second;
        std::size_t planCount = 0;
        for (int rs : plan.rowSizes) planCount += static_cast<std::size_t>(rs);
        if (planCount == kids.size()) {
          std::vector<std::string> cur;
          cur.reserve(kids.size());
          for (int c : kids) cur.push_back(t.nodes[static_cast<std::size_t>(c)].key);
          std::vector<std::string> a = cur, b = plan.childKeys;
          std::sort(a.begin(), a.end());
          std::sort(b.begin(), b.end());
          if (a == b) {
            rowSizes = plan.rowSizes;
            reused = true;
          }
        }
      }
    }

    if (!reused) {
      // Squarify: greedily grow the current row while the worst aspect ratio
      // improves, then commit the row and continue with the remainder.
      Rect free = frame;
      std::size_t i = 0;
      while (i < areas.size()) {
        std::vector<double> rowAreas;
        double rowSum = 0.0;
        const double side = std::min(free.w(), free.h());
        rowAreas.push_back(areas[i]);
        rowSum += areas[i];
        double best = worstRatio(rowAreas, rowSum, side);
        std::size_t j = i + 1;
        for (; j < areas.size(); ++j) {
          std::vector<double> trial = rowAreas;
          trial.push_back(areas[j]);
          const double trialSum = rowSum + areas[j];
          const double w = worstRatio(trial, trialSum, side);
          if (w > best) break;  // adding worsens -> commit current row
          rowAreas = std::move(trial);
          rowSum = trialSum;
          best = w;
        }
        rowSizes.push_back(static_cast<int>(j - i));
        // Advance free rect past this committed row (placement done below uniformly).
        Rect tmp;
        std::vector<Rect> tmpOut;
        // recompute free purely (placeRow returns the leftover).
        free = placeRow(free, rowAreas, rowSum, tmpOut);
        i = j;
      }
    }

    // Now PLACE children using rowSizes (the partition), filling row by row. This
    // is shared by both the fresh-squarify and the reused-plan paths, so a reused
    // plan re-flows the SAME grouping against the new values (resquarify rescale).
    Rect free = frame;
    std::size_t idx = 0;
    for (int rs : rowSizes) {
      std::vector<double> rowAreas;
      double rowSum = 0.0;
      for (int k = 0; k < rs && idx + static_cast<std::size_t>(k) < areas.size(); ++k) {
        rowAreas.push_back(areas[idx + static_cast<std::size_t>(k)]);
        rowSum += areas[idx + static_cast<std::size_t>(k)];
      }
      std::vector<Rect> placed;
      free = placeRow(free, rowAreas, rowSum, placed);
      for (std::size_t k = 0; k < placed.size(); ++k) {
        const int child = kids[idx + k];
        rects[static_cast<std::size_t>(child)] = placed[k];
      }
      idx += rowAreas.size();
    }

    // Record the plan for resquarify stability.
    if (stable_) {
      ParentPlan plan;
      plan.rowSizes = rowSizes;
      plan.childKeys.reserve(kids.size());
      for (int c : kids) plan.childKeys.push_back(t.nodes[static_cast<std::size_t>(c)].key);
      plans_[pkey] = std::move(plan);
      touchedParents.push_back(pkey);
    }
  }

  // Prune stale plans (parents that disappeared from the tree this pass).
  if (stable_ && !plans_.empty()) {
    std::vector<std::string> keys;
    keys.reserve(plans_.size());
    for (const auto& kv : plans_) keys.push_back(kv.first);
    std::sort(touchedParents.begin(), touchedParents.end());
    for (const auto& k : keys) {
      if (!std::binary_search(touchedParents.begin(), touchedParents.end(), k))
        plans_.erase(k);
    }
  }

  // Emit.
  ctx.out->allocColumn(node, "node", DType::I32, n);
  ctx.out->allocColumn(node, "parent", DType::I32, n);
  ctx.out->allocColumn(node, "depth", DType::I32, n);
  ctx.out->allocColumn(node, "leaf", DType::I32, n);
  ctx.out->allocColumn(node, "value", DType::F32, n);
  ctx.out->allocColumn(node, "x0", DType::F32, n);
  ctx.out->allocColumn(node, "y0", DType::F32, n);
  ctx.out->allocColumn(node, "x1", DType::F32, n);
  ctx.out->allocColumn(node, "y1", DType::F32, n);
  for (std::size_t i = 0; i < n; ++i) {
    const hier::Node& nd = t.nodes[i];
    const Rect& rc = rects[i];
    ctx.out->setI32(node, "node", i, static_cast<std::int32_t>(i));
    ctx.out->setI32(node, "parent", i, nd.parent);
    ctx.out->setI32(node, "depth", i, nd.depth);
    ctx.out->setI32(node, "leaf", i, nd.children.empty() ? 1 : 0);
    ctx.out->setF32(node, "value", i, static_cast<float>(nd.value));
    ctx.out->setF32(node, "x0", i, static_cast<float>(rc.x0));
    ctx.out->setF32(node, "y0", i, static_cast<float>(rc.y0));
    ctx.out->setF32(node, "x1", i, static_cast<float>(rc.x1));
    ctx.out->setF32(node, "y1", i, static_cast<float>(rc.y1));
  }
}

}  // namespace dc
