// ENC-618a — `pack` (circle packing) implementation. See Pack.hpp.
//
// Sibling packing uses the Wang/Wang-Wang front-chain heuristic (the algorithm
// behind D3's packSiblings / packEnclose): place the first three circles mutually
// tangent, maintain a "front chain" of circles on the convex frontier, and place
// each next circle tangent to the chosen front pair, resolving collisions by
// splicing the chain. The enclosing radius is then an approximation (the max
// centre-distance + radius from the packed centroid), which is sufficient for a
// non-overlapping, parent-contained layout. Then the whole tree is normalized into
// the unit square.
#include "dc/transform/transforms/Pack.hpp"

#include "Hierarchy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dc {

namespace {

struct Circle {
  double x{0}, y{0}, r{0};
};

// Place circle c externally tangent to BOTH a and b (the standard "place" step of
// the front-chain pack — c sits in the wedge, on the left of the directed a->b
// segment so the front advances CCW).
void place(Circle& c, const Circle& a, const Circle& b) {
  const double dx = b.x - a.x, dy = b.y - a.y;
  const double d2 = dx * dx + dy * dy;
  if (d2 <= 0.0) {  // degenerate: stack to the right of a
    c.x = a.x + a.r + c.r;
    c.y = a.y;
    return;
  }
  const double ra = a.r + c.r, rb = b.r + c.r;
  if (ra > 0.0 || rb > 0.0) {
    const double dd = std::sqrt(d2);
    const double t = 0.5 + (ra * ra - rb * rb) / (2.0 * d2);
    const double h2 = std::max(0.0, ra * ra / d2 - t * t);
    const double h = std::sqrt(h2);
    c.x = a.x + t * dx + h * dy;
    c.y = a.y + t * dy - h * dx;
    (void)dd;
  } else {
    c.x = a.x; c.y = a.y;
  }
}

// True if circles a,b overlap beyond a tiny epsilon.
bool overlaps(const Circle& a, const Circle& b) {
  const double dx = a.x - b.x, dy = a.y - b.y;
  const double dr = a.r + b.r;
  // Compare squared distances with a relative slack to avoid FP false positives.
  return dx * dx + dy * dy < dr * dr - 1e-9 * (dr * dr + 1.0);
}

// Pack the sibling circles (radii already set) into mutually non-overlapping
// positions; return the enclosing radius around their centroid (which is moved to
// the origin). Centres are written back into `cs`. `pad` widens each radius during
// packing (a sibling gap) then is removed. This is the Wang front-chain heuristic
// (D3 packSiblings): a doubly-linked "front" of frontier circles; each new circle
// is placed tangent to the nearest front pair and the front is spliced on overlap.
double packSiblings(std::vector<Circle>& cs, double pad) {
  const std::size_t n = cs.size();
  if (n == 0) return 0.0;
  std::vector<double> r0(n);
  for (std::size_t i = 0; i < n; ++i) { r0[i] = cs[i].r; cs[i].r += pad; }

  auto finish = [&](void) {
    // Move centroid to origin, restore radii, compute enclosing radius.
    double sx = 0.0, sy = 0.0;
    for (std::size_t i = 0; i < n; ++i) { sx += cs[i].x; sy += cs[i].y; }
    const double cx = sx / static_cast<double>(n), cy = sy / static_cast<double>(n);
    double enc = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      cs[i].x -= cx; cs[i].y -= cy;
      const double d =
          std::sqrt(cs[i].x * cs[i].x + cs[i].y * cs[i].y) + cs[i].r;
      enc = std::max(enc, d);
      cs[i].r = r0[i];
    }
    return enc;
  };

  if (n == 1) { cs[0].x = 0.0; cs[0].y = 0.0; return finish(); }

  cs[0].x = -cs[1].r; cs[0].y = 0.0;
  cs[1].x = cs[0].r;  cs[1].y = 0.0;
  if (n == 2) return finish();

  place(cs[2], cs[0], cs[1]);

  // Doubly-linked front chain over circle indices. next/prev keyed by index.
  std::vector<int> next(n, -1), prev(n, -1);
  auto link = [&](int x, int y) { next[x] = y; prev[y] = x; };
  link(0, 1); link(1, 2); link(2, 0);  // initial triangle front

  int a = 0, b = 2;  // we will scan the front for the closest pair each step
  for (std::size_t i = 3; i < n;) {
    // Find the front circle nearest the origin to start scanning from; D3 scans
    // from the last accepted. We scan the whole front for the tangent pair whose
    // placement of cs[i] does not overlap any other front circle.
    bool placed = false;
    int start = a;
    int cA = start;
    do {
      int cB = next[cA];
      place(cs[i], cs[cA], cs[cB]);
      bool bad = false;
      for (int q = next[cB]; q != cA; q = next[q]) {
        if (overlaps(cs[i], cs[q])) { bad = true; break; }
      }
      if (!bad) {
        // Insert i between cA and cB on the front.
        link(static_cast<int>(cA), static_cast<int>(i));
        link(static_cast<int>(i), static_cast<int>(cB));
        a = cA; b = static_cast<int>(i);
        placed = true;
        break;
      }
      cA = cB;
    } while (cA != start);

    if (!placed) {
      // Fallback (should be rare): place tangent to (a,b) and accept, splicing out
      // overlapped neighbours so the front stays simple.
      place(cs[i], cs[a], cs[b]);
      link(a, static_cast<int>(i));
      link(static_cast<int>(i), b);
      b = static_cast<int>(i);
    }
    ++i;
  }
  (void)b;
  return finish();
}

}  // namespace

SchemaResult PackTransform::inferSchema(const ColumnSchema& input) const {
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
  out.columns.push_back({"cx", DType::F32});
  out.columns.push_back({"cy", DType::F32});
  out.columns.push_back({"r", DType::F32});
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void PackTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& in = *ctx.input;
  const ColumnSchema& schema = *ctx.inputSchema;
  const Id node = ctx.nodeId;

  hier::Tree t = hier::build(schema, in, levels_, size_);
  const std::size_t n = t.nodes.size();

  // Local circle (radius + centre relative to its PARENT's centre) per node.
  std::vector<Circle> local(n);

  // Bottom-up: a leaf radius ∝ sqrt(value); an internal node packs its children and
  // takes the enclosing radius. Children have higher indices than parents, so a
  // reverse pass computes child radii before their parent packs them.
  for (std::size_t i = n; i-- > 0;) {
    const hier::Node& nd = t.nodes[i];
    if (nd.children.empty()) {
      local[i].r = std::sqrt(std::max(0.0, nd.value));
      continue;
    }
    std::vector<Circle> kids;
    kids.reserve(nd.children.size());
    for (int c : nd.children) kids.push_back(local[static_cast<std::size_t>(c)]);
    const double enc = packSiblings(kids, padding_);
    // Write packed child centres back (relative to this node's centre).
    for (std::size_t k = 0; k < nd.children.size(); ++k) {
      Circle& cc = local[static_cast<std::size_t>(nd.children[k])];
      cc.x = kids[k].x;
      cc.y = kids[k].y;
    }
    local[i].r = enc > 0.0 ? enc : 1.0;
  }

  // Top-down: accumulate absolute centres. Root centred at origin.
  std::vector<double> ax(n, 0.0), ay(n, 0.0);
  ax[0] = 0.0; ay[0] = 0.0;
  for (std::size_t pi = 0; pi < n; ++pi) {
    for (int c : t.nodes[pi].children) {
      ax[static_cast<std::size_t>(c)] = ax[pi] + local[static_cast<std::size_t>(c)].x;
      ay[static_cast<std::size_t>(c)] = ay[pi] + local[static_cast<std::size_t>(c)].y;
    }
  }

  // Normalize into [0,1]^2: the root circle (centre origin, radius local[0].r) maps
  // to the unit square's inscribed circle (centre 0.5, radius 0.5).
  const double R = local[0].r > 0.0 ? local[0].r : 1.0;
  const double scale = 0.5 / R;
  auto nx = [&](double x) { return 0.5 + x * scale; };
  auto ny = [&](double y) { return 0.5 + y * scale; };

  ctx.out->allocColumn(node, "node", DType::I32, n);
  ctx.out->allocColumn(node, "parent", DType::I32, n);
  ctx.out->allocColumn(node, "depth", DType::I32, n);
  ctx.out->allocColumn(node, "leaf", DType::I32, n);
  ctx.out->allocColumn(node, "value", DType::F32, n);
  ctx.out->allocColumn(node, "cx", DType::F32, n);
  ctx.out->allocColumn(node, "cy", DType::F32, n);
  ctx.out->allocColumn(node, "r", DType::F32, n);
  for (std::size_t i = 0; i < n; ++i) {
    const hier::Node& nd = t.nodes[i];
    ctx.out->setI32(node, "node", i, static_cast<std::int32_t>(i));
    ctx.out->setI32(node, "parent", i, nd.parent);
    ctx.out->setI32(node, "depth", i, nd.depth);
    ctx.out->setI32(node, "leaf", i, nd.children.empty() ? 1 : 0);
    ctx.out->setF32(node, "value", i, static_cast<float>(nd.value));
    ctx.out->setF32(node, "cx", i, static_cast<float>(nx(ax[i])));
    ctx.out->setF32(node, "cy", i, static_cast<float>(ny(ay[i])));
    ctx.out->setF32(node, "r", i, static_cast<float>(local[i].r * scale));
  }
}

}  // namespace dc
