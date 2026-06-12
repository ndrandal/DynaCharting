// ENC-618b — `sankey` / alluvial layout + cubic-bezier ribbon routing. See
// Sankey.hpp. Pure `dc` (C++17). Reuses dc::CurveTessellator for the ribbon edges.
#include "dc/transform/transforms/Sankey.hpp"

#include "dc/geometry/CurveTessellator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace dc {

namespace {

bool isNumeric(DType dt) {
  return dt == DType::F32 || dt == DType::I32 || dt == DType::Cat;
}

// A flow read off the input table: endpoints interned to dense node indices.
struct Flow {
  std::size_t src{0};   // dense node index of the source
  std::size_t dst{0};   // dense node index of the target
  double value{0.0};    // flow weight (already > 0)
  std::uint32_t color{0};  // categorical tint key (color field, or source id)
};

// Per-node layout state.
struct Node {
  std::int64_t id{0};    // the node's raw key (source/target value), as i64
  int layer{0};          // assigned column / depth
  double inflow{0.0};     // sum of inbound flow value
  double outflow{0.0};    // sum of outbound flow value
  double throughput{0.0}; // max(inflow, outflow) — the node band height driver
  double order{0.0};      // bary-center used by the median ordering sweeps
  int slot{0};           // final stacked position within the layer
  // Filled rectangle (normalized [0,1] layout space).
  double x0{0.0}, y0{0.0}, x1{0.0}, y1{0.0};
};

// A simple 32-bit categorical -> RGBA palette (stable, distinct, opaque). Mirrors
// the kind of ordinal color scale a sankey ribbon is tinted by.
void paletteColor(std::uint32_t key, float& r, float& g, float& b, float& a) {
  static const std::uint32_t kPalette[] = {
      0x4E79A7u, 0xF28E2Bu, 0xE15759u, 0x76B7B2u, 0x59A14Fu,
      0xEDC948u, 0xB07AA1u, 0xFF9DA7u, 0x9C755Fu, 0xBAB0ACu,
  };
  const std::uint32_t c = kPalette[key % (sizeof(kPalette) / sizeof(kPalette[0]))];
  r = static_cast<float>((c >> 16) & 0xFFu) / 255.0f;
  g = static_cast<float>((c >> 8) & 0xFFu) / 255.0f;
  b = static_cast<float>(c & 0xFFu) / 255.0f;
  a = 1.0f;
}

}  // namespace

SchemaResult SankeyTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  const SchemaColumn* s = input.find(source_);
  const SchemaColumn* t = input.find(target_);
  const SchemaColumn* v = input.find(value_);
  if (!s) { r.error = "sankey source column '" + source_ + "' not found"; return r; }
  if (!t) { r.error = "sankey target column '" + target_ + "' not found"; return r; }
  if (!v) { r.error = "sankey value column '" + value_ + "' not found"; return r; }
  if (!isNumeric(v->dtype)) {
    r.error = "sankey value column '" + value_ + "' must be numeric";
    return r;
  }
  if (!color_.empty() && !input.has(color_)) {
    r.error = "sankey color column '" + color_ + "' not found";
    return r;
  }

  ColumnSchema out;
  // Node-rectangle group (one row per node).
  out.columns.push_back({"node", DType::I32});
  out.columns.push_back({"n_layer", DType::I32});
  out.columns.push_back({"n_x0", DType::F32});
  out.columns.push_back({"n_y0", DType::F32});
  out.columns.push_back({"n_x1", DType::F32});
  out.columns.push_back({"n_y1", DType::F32});
  out.columns.push_back({"n_value", DType::F32});
  // Ribbon-geometry group (one row per vertex; triangle list, Pos2Color4).
  out.columns.push_back({"v_x", DType::F32});
  out.columns.push_back({"v_y", DType::F32});
  out.columns.push_back({"v_r", DType::F32});
  out.columns.push_back({"v_g", DType::F32});
  out.columns.push_back({"v_b", DType::F32});
  out.columns.push_back({"v_a", DType::F32});

  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void SankeyTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& res = *ctx.input;
  const std::size_t rows = res.rowCount();
  const Id node = ctx.nodeId;

  // ----- 1. intern node ids (union of source + target) ----------------------
  std::vector<Node> nodes;
  std::unordered_map<std::int64_t, std::size_t> nodeIndex;
  auto intern = [&](std::int64_t id) -> std::size_t {
    auto it = nodeIndex.find(id);
    if (it != nodeIndex.end()) return it->second;
    const std::size_t i = nodes.size();
    nodeIndex.emplace(id, i);
    Node n;
    n.id = id;
    nodes.push_back(n);
    return i;
  };

  std::vector<Flow> flows;
  flows.reserve(rows);
  const bool haveColor = !color_.empty();
  for (std::size_t i = 0; i < rows; ++i) {
    const double vv = res.readNum(value_, i);
    if (!(vv > 0.0) || std::isnan(vv)) continue;  // skip zero/negative/NaN flows
    const auto sId = static_cast<std::int64_t>(
        std::llround(res.readNum(source_, i)));
    const auto tId = static_cast<std::int64_t>(
        std::llround(res.readNum(target_, i)));
    Flow f;
    f.src = intern(sId);
    f.dst = intern(tId);
    f.value = vv;
    if (haveColor) {
      f.color = static_cast<std::uint32_t>(std::llround(res.readNum(color_, i)));
    } else {
      f.color = static_cast<std::uint32_t>(sId);
    }
    nodes[f.src].outflow += vv;
    nodes[f.dst].inflow += vv;
    flows.push_back(f);
  }

  const std::size_t nNodes = nodes.size();
  for (auto& n : nodes) n.throughput = std::max(n.inflow, n.outflow);

  // ----- 2. layering by longest-path depth ----------------------------------
  // Build adjacency (per source: its targets) for the relaxation + ordering.
  std::vector<std::vector<std::size_t>> outEdges(nNodes), inEdges(nNodes);
  for (const auto& f : flows) {
    if (f.src == f.dst) continue;  // ignore self-loops for layering
    outEdges[f.src].push_back(f.dst);
    inEdges[f.dst].push_back(f.src);
  }
  // Longest-path relaxation: depth(dst) = max(depth(dst), depth(src)+1). Repeat
  // until stable or nNodes passes (a cycle can never extend a finite diagram past
  // nNodes layers, so this bound also breaks cycles defensively).
  for (std::size_t pass = 0; pass < nNodes; ++pass) {
    bool changed = false;
    for (std::size_t s = 0; s < nNodes; ++s) {
      for (std::size_t d : outEdges[s]) {
        if (nodes[d].layer < nodes[s].layer + 1) {
          nodes[d].layer = nodes[s].layer + 1;
          changed = true;
        }
      }
    }
    if (!changed) break;
  }
  int maxLayer = 0;
  for (const auto& n : nodes) maxLayer = std::max(maxLayer, n.layer);

  // Group node indices by layer (initial order = discovery order).
  std::vector<std::vector<std::size_t>> layers(static_cast<std::size_t>(maxLayer) + 1);
  for (std::size_t i = 0; i < nNodes; ++i)
    layers[static_cast<std::size_t>(nodes[i].layer)].push_back(i);

  // ----- 3. order within a layer: median / bary-center crossing reduction ----
  // Each sweep recomputes a node's bary-center as the mean slot of its neighbors
  // in the adjacent (already-ordered) layer, then re-sorts the layer by it.
  auto slotOf = [&](std::size_t n) { return nodes[n].order; };
  auto reorder = [&](std::vector<std::size_t>& layer,
                     const std::vector<std::vector<std::size_t>>& adj) {
    for (std::size_t n : layer) {
      const auto& nb = adj[n];
      if (nb.empty()) continue;  // keep current order for sourceless/sinkless
      double sum = 0.0;
      for (std::size_t m : nb) sum += slotOf(m);
      nodes[n].order = sum / static_cast<double>(nb.size());
    }
    std::stable_sort(layer.begin(), layer.end(),
                     [&](std::size_t a, std::size_t b) {
                       return nodes[a].order < nodes[b].order;
                     });
  };
  // seed order = position within layer
  for (auto& layer : layers)
    for (std::size_t k = 0; k < layer.size(); ++k)
      nodes[layer[k]].order = static_cast<double>(k);
  for (int sweep = 0; sweep < std::max(0, opts_.crossingSweeps); ++sweep) {
    // down sweep (order by upstream neighbors), then up sweep (downstream).
    for (std::size_t L = 1; L < layers.size(); ++L) reorder(layers[L], inEdges);
    for (std::size_t k = 0; k < layers.size(); ++k) {
      const std::size_t L = layers.size() - 1 - k;
      reorder(layers[L], outEdges);
    }
    // refresh slot indices so the next sweep reads the new order
    for (auto& layer : layers)
      for (std::size_t pos = 0; pos < layer.size(); ++pos)
        nodes[layer[pos]].order = static_cast<double>(pos);
  }

  // ----- 4. size + position node rectangles ---------------------------------
  // Height ∝ throughput. Find the tallest layer (sum of throughputs + paddings)
  // and normalize so it fills the unit band; every layer uses that same scale so
  // ribbon widths are comparable across columns.
  double maxLayerExtent = 0.0;
  for (const auto& layer : layers) {
    double sum = 0.0;
    for (std::size_t n : layer) sum += nodes[n].throughput;
    const double pad = layer.empty() ? 0.0
                                     : opts_.nodePadding *
                                           static_cast<double>(layer.size() - 1);
    maxLayerExtent = std::max(maxLayerExtent, sum + pad);
  }
  if (!(maxLayerExtent > 0.0)) maxLayerExtent = 1.0;
  const double vscale = 1.0 / maxLayerExtent;  // throughput -> normalized height

  const double xspan = (maxLayer > 0)
                           ? (1.0 - opts_.nodeWidth) / static_cast<double>(maxLayer)
                           : 0.0;
  for (std::size_t L = 0; L < layers.size(); ++L) {
    const double x0 = static_cast<double>(L) * xspan;
    const double x1 = x0 + opts_.nodeWidth;
    double y = 0.0;
    for (std::size_t pos = 0; pos < layers[L].size(); ++pos) {
      Node& n = nodes[layers[L][pos]];
      n.slot = static_cast<int>(pos);
      const double h = n.throughput * vscale;
      n.x0 = x0;
      n.x1 = x1;
      n.y0 = y;
      n.y1 = y + h;
      y += h + opts_.nodePadding;
    }
  }

  // ----- 5. route ribbons as cubic-bezier filled strips ---------------------
  // Each node hands out vertical BANDS on its right edge (outbound) and left edge
  // (inbound), each band height = flow.value * vscale, stacked in the order the
  // node's flows are consumed. We walk flows; per node we track the running outbound
  // offset (from its top) and inbound offset.
  std::vector<double> outCursor(nNodes, 0.0);
  std::vector<double> inCursor(nNodes, 0.0);

  // Vertex sinks: one f32 per attribute lane, appended per triangle vertex.
  std::vector<float> vx, vy, vr, vg, vb, va;
  const int segs = std::max(1, opts_.ribbonSegments);
  vx.reserve(flows.size() * static_cast<std::size_t>(segs) * 6);

  auto emitVertex = [&](double x, double y, float cr, float cg, float cb,
                        float ca) {
    vx.push_back(static_cast<float>(x));
    vy.push_back(static_cast<float>(y));
    vr.push_back(cr);
    vg.push_back(cg);
    vb.push_back(cb);
    va.push_back(ca);
  };

  for (const auto& f : flows) {
    const Node& sn = nodes[f.src];
    const Node& tn = nodes[f.dst];
    const double w = f.value * vscale;  // ribbon width (band height)

    // Source band: on sn's right edge; target band: on tn's left edge.
    const double sx = sn.x1;
    const double tx = tn.x0;
    const double sTop = sn.y0 + outCursor[f.src];
    const double tTop = tn.y0 + inCursor[f.dst];
    outCursor[f.src] += w;
    inCursor[f.dst] += w;
    const double sBot = sTop + w;
    const double tBot = tTop + w;

    float cr, cg, cb, ca;
    paletteColor(f.color, cr, cg, cb, ca);

    // Horizontal-handle S-curve: handles reach `curvature` of the x-span toward
    // the midpoint, giving a smooth sigmoid with vertical tangents at the bands.
    const double dx = (tx - sx);
    const double h0x = sx + dx * opts_.curvature;
    const double h1x = tx - dx * opts_.curvature;

    // Top edge: source-top -> target-top. Bottom edge: source-bot -> target-bot.
    const std::vector<Vec2> topEdge = CurveTessellator::cubicBezier(
        {sx, sTop}, {h0x, sTop}, {h1x, tTop}, {tx, tTop}, segs);
    const std::vector<Vec2> botEdge = CurveTessellator::cubicBezier(
        {sx, sBot}, {h0x, sBot}, {h1x, tBot}, {tx, tBot}, segs);

    // Fill the strip between the two polylines as a triangle list. Both curves
    // have the same sample count; each adjacent pair forms a quad (2 triangles).
    const std::size_t n = std::min(topEdge.size(), botEdge.size());
    for (std::size_t k = 0; k + 1 < n; ++k) {
      const Vec2& ta = topEdge[k];
      const Vec2& tb = topEdge[k + 1];
      const Vec2& ba = botEdge[k];
      const Vec2& bb = botEdge[k + 1];
      // quad (ta, ba, bb) + (ta, bb, tb)
      emitVertex(ta.x, ta.y, cr, cg, cb, ca);
      emitVertex(ba.x, ba.y, cr, cg, cb, ca);
      emitVertex(bb.x, bb.y, cr, cg, cb, ca);
      emitVertex(ta.x, ta.y, cr, cg, cb, ca);
      emitVertex(bb.x, bb.y, cr, cg, cb, ca);
      emitVertex(tb.x, tb.y, cr, cg, cb, ca);
    }
  }

  // ----- 6. write outputs ---------------------------------------------------
  // Node-rectangle group (one row per node).
  ctx.out->allocColumn(node, "node", DType::I32, nNodes);
  ctx.out->allocColumn(node, "n_layer", DType::I32, nNodes);
  ctx.out->allocColumn(node, "n_x0", DType::F32, nNodes);
  ctx.out->allocColumn(node, "n_y0", DType::F32, nNodes);
  ctx.out->allocColumn(node, "n_x1", DType::F32, nNodes);
  ctx.out->allocColumn(node, "n_y1", DType::F32, nNodes);
  ctx.out->allocColumn(node, "n_value", DType::F32, nNodes);
  for (std::size_t i = 0; i < nNodes; ++i) {
    const Node& n = nodes[i];
    ctx.out->setI32(node, "node", i, static_cast<std::int32_t>(n.id));
    ctx.out->setI32(node, "n_layer", i, static_cast<std::int32_t>(n.layer));
    ctx.out->setF32(node, "n_x0", i, static_cast<float>(n.x0));
    ctx.out->setF32(node, "n_y0", i, static_cast<float>(n.y0));
    ctx.out->setF32(node, "n_x1", i, static_cast<float>(n.x1));
    ctx.out->setF32(node, "n_y1", i, static_cast<float>(n.y1));
    ctx.out->setF32(node, "n_value", i, static_cast<float>(n.throughput));
  }

  // Ribbon-geometry group (one row per vertex).
  const std::size_t nVerts = vx.size();
  ctx.out->allocColumn(node, "v_x", DType::F32, nVerts);
  ctx.out->allocColumn(node, "v_y", DType::F32, nVerts);
  ctx.out->allocColumn(node, "v_r", DType::F32, nVerts);
  ctx.out->allocColumn(node, "v_g", DType::F32, nVerts);
  ctx.out->allocColumn(node, "v_b", DType::F32, nVerts);
  ctx.out->allocColumn(node, "v_a", DType::F32, nVerts);
  for (std::size_t i = 0; i < nVerts; ++i) {
    ctx.out->setF32(node, "v_x", i, vx[i]);
    ctx.out->setF32(node, "v_y", i, vy[i]);
    ctx.out->setF32(node, "v_r", i, vr[i]);
    ctx.out->setF32(node, "v_g", i, vg[i]);
    ctx.out->setF32(node, "v_b", i, vb[i]);
    ctx.out->setF32(node, "v_a", i, va[i]);
  }
}

}  // namespace dc
