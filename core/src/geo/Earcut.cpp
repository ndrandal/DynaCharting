// ENC-618c — Earcut polygon triangulation. See Earcut.hpp.
//
// A self-contained C++17 implementation of ear-clipping with hole elimination,
// following the structure of the mapbox/earcut algorithm:
//   1. Build a circular doubly-linked list of vertex nodes for the outer ring.
//   2. Eliminate holes: for each hole, find a bridge to the outer ring (the
//      mutually-visible vertex pair) and splice the hole in, turning the multiply-
//      connected polygon into one simple ring.
//   3. Ear-clipping: repeatedly clip an "ear" (a convex corner whose triangle
//      contains no other vertex), emitting a triangle and removing the ear tip,
//      until 3 vertices remain.
#include "dc/geo/Earcut.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dc {
namespace geo {

namespace {

// A vertex node in the circular doubly-linked list. `i` is the index back into the
// caller's polygon vertex array (so emitted triangles reference original vertices).
struct Node {
  std::uint32_t i;  // original vertex index
  double x, y;
  Node* prev{nullptr};
  Node* next{nullptr};
  bool removed{false};
};

// Arena-owned node pool so we never dangle / leak; nodes live for the call.
struct Arena {
  std::vector<Node> pool;
  Node* make(std::uint32_t i, double x, double y) {
    pool.push_back(Node{i, x, y, nullptr, nullptr, false});
    return &pool.back();
  }
};

// Link a fresh node for vertex (i,x,y) after `last` in the ring; returns the new
// tail. NOTE: the arena may reallocate on push_back, so we resolve all Node* AFTER
// the whole ring is built (see buildRing). During construction we record indices.

// Build a circular doubly-linked ring from polygon vertices [start,end) and return
// the head node. Skips consecutive duplicate points (zero-length edges).
Node* buildRing(Arena& arena, const Polygon& poly, std::size_t start,
                std::size_t end, std::vector<Node*>& outNodes) {
  // First materialize all nodes (indices), then patch prev/next — arena push_back
  // can reallocate, so we must not hold Node* across makes.
  std::vector<std::size_t> idx;
  Vec2 last{std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()};
  for (std::size_t v = start; v < end; ++v) {
    const Vec2 p = poly.vertex(v);
    if (!std::isnan(last.x) && p.x == last.x && p.y == last.y) continue;  // dup
    const std::size_t nodeIndex = arena.pool.size();
    arena.make(static_cast<std::uint32_t>(v), p.x, p.y);
    idx.push_back(nodeIndex);
    last = p;
  }
  if (idx.size() < 3) {
    // Record whatever we made (caller checks for a valid ring).
    for (std::size_t k : idx) outNodes.push_back(&arena.pool[k]);
    return idx.empty() ? nullptr : &arena.pool[idx.front()];
  }
  const std::size_t m = idx.size();
  for (std::size_t k = 0; k < m; ++k) {
    Node* n = &arena.pool[idx[k]];
    n->prev = &arena.pool[idx[(k + m - 1) % m]];
    n->next = &arena.pool[idx[(k + 1) % m]];
    outNodes.push_back(n);
  }
  return &arena.pool[idx[0]];
}

// Signed area*2 of a ring (head..head).
double ringArea2(const Node* head) {
  double sum = 0.0;
  const Node* p = head;
  const Node* q = head->next;
  do {
    sum += (q->x - p->x) * (q->y + p->y);
    p = q;
    q = q->next;
  } while (p != head);
  return sum;  // >0 clockwise, <0 CCW under this convention
}

// Is point (px,py) inside triangle (ax,ay)-(bx,by)-(cx,cy)? Inclusive of edges.
bool pointInTriangle(double ax, double ay, double bx, double by, double cx,
                     double cy, double px, double py) {
  return (cx - px) * (ay - py) - (ax - px) * (cy - py) >= 0 &&
         (ax - px) * (by - py) - (bx - px) * (ay - py) >= 0 &&
         (bx - px) * (cy - py) - (cx - px) * (by - py) >= 0;
}

double area(const Node* p, const Node* q, const Node* r) {
  return (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y);
}

// Is the corner at `ear` a valid ear? Convex AND no other reflex vertex inside its
// triangle.
bool isEar(const Node* ear) {
  const Node* a = ear->prev;
  const Node* b = ear;
  const Node* c = ear->next;
  if (area(a, b, c) >= 0) return false;  // reflex or collinear (CCW ring → <0 convex)

  // No other (live) vertex inside triangle abc.
  const Node* p = ear->next->next;
  while (p != ear->prev) {
    if (!p->removed &&
        pointInTriangle(a->x, a->y, b->x, b->y, c->x, c->y, p->x, p->y) &&
        area(p->prev, p, p->next) >= 0)
      return false;
    p = p->next;
  }
  return true;
}

// Count live nodes in the ring containing `head`.
std::size_t ringCount(const Node* head) {
  std::size_t n = 0;
  const Node* p = head;
  do {
    ++n;
    p = p->next;
  } while (p != head);
  return n;
}

// ----- hole elimination ----------------------------------------------------

// Find a vertex on the OUTER ring that the hole's leftmost vertex can "see" (a
// bridge). Standard ray-cast-to-the-right + visibility refinement is simplified
// here to: pick the outer-ring vertex with the largest x that is still left of the
// hole's bridge point and forms a valid bridge. For the well-formed (clean,
// non-self-intersecting) inputs in scope this finds a usable bridge.
Node* findHoleBridge(Node* hole, Node* outer) {
  // Hole's leftmost point.
  Node* hx = hole;
  {
    Node* p = hole->next;
    while (p != hole) {
      if (p->x < hx->x || (p->x == hx->x && p->y < hx->y)) hx = p;
      p = p->next;
    }
  }
  const double px = hx->x, py = hx->y;

  // Cast a ray to the LEFT (−x) from the hole's leftmost vertex; find the outer
  // edge it hits with the largest x at intersection. The bridge is that edge's
  // endpoint (or a refined vertex). Inclusive, robust enough for clean input.
  Node* m = nullptr;
  double qx = -std::numeric_limits<double>::infinity();
  Node* p = outer;
  do {
    const Node* a = p;
    const Node* b = p->next;
    if ((a->y <= py && b->y >= py) || (b->y <= py && a->y >= py)) {
      const double dy = b->y - a->y;
      const double x = (dy != 0.0) ? a->x + (py - a->y) / dy * (b->x - a->x)
                                   : a->x;
      if (x <= px && x > qx) {
        qx = x;
        m = (a->x >= b->x) ? const_cast<Node*>(a) : const_cast<Node*>(b);
        if (x == px) return m;  // hits a vertex exactly
      }
    }
    p = p->next;
  } while (p != outer);

  return m;  // may be null if no bridge found (degenerate input)
}

// Splice `hole` into the outer ring across the bridge (outer node `b` ↔ hole's
// leftmost-or-bridge node). Inserts duplicate bridge vertices (the standard earcut
// hole-merge), returning the new outer-side node.
void eliminateHole(Arena& arena, Node* outer, Node* hole) {
  Node* bridge = findHoleBridge(hole, outer);
  if (!bridge) return;  // could not bridge — drop this hole (clean-input scope)

  // The hole's bridge point = its leftmost vertex.
  Node* hp = hole;
  {
    Node* p = hole->next;
    while (p != hole) {
      if (p->x < hp->x || (p->x == hp->x && p->y < hp->y)) hp = p;
      p = p->next;
    }
  }

  // Duplicate both bridge endpoints and stitch: bridge → hp ... hole ... hp' →
  // bridge'. Standard earcut: insert hp and a copy of bridge.
  const std::size_t b2i = arena.pool.size();
  arena.make(bridge->i, bridge->x, bridge->y);
  const std::size_t hp2i = arena.pool.size();
  arena.make(hp->i, hp->x, hp->y);
  Node* bridge2 = &arena.pool[b2i];
  Node* hp2 = &arena.pool[hp2i];

  Node* bn = bridge->next;

  // bridge -> hp (walk hole ring) -> hp2 -> bridge2 -> bn
  bridge->next = hp;
  hp->prev = bridge;

  // Re-route the hole ring so traversing from hp returns to hp2.
  Node* hpPrev = hp->prev;  // (now bridge) — we need original hole neighbours
  (void)hpPrev;
  // Walk the hole ring from hp around back to hp, inserting hp2 as the closing copy.
  // Find hp's original previous within the hole (before we overwrote hp->prev).
  // Simpler: relink the hole into the outer chain fully.
  // After bridge->hp, traverse hole via its own next pointers until we come back to
  // the node whose next is hp (its predecessor), then attach hp2.
  Node* holePrev = hp;
  while (holePrev->next != hp) holePrev = holePrev->next;
  holePrev->next = hp2;
  hp2->prev = holePrev;
  hp2->next = bridge2;
  bridge2->prev = hp2;
  bridge2->next = bn;
  bn->prev = bridge2;
}

}  // namespace

std::vector<std::uint32_t> earcut(const Polygon& poly) {
  std::vector<std::uint32_t> out;
  const std::size_t nVerts = poly.vertexCount();
  if (nVerts < 3 || poly.ringStarts.empty()) return out;

  Arena arena;
  // Reserve generously so the pool never reallocs DURING linking of one ring (we
  // patch pointers after each ring build) — but cross-ring growth is fine because
  // we always re-resolve via pool index when bridging.
  arena.pool.reserve(nVerts * 3 + 8 + 2 * poly.ringStarts.size());

  // Outer ring = ring 0.
  const std::size_t ringN = poly.ringStarts.size();
  const std::size_t outerStart = poly.ringStarts[0];
  const std::size_t outerEnd =
      (ringN > 1) ? poly.ringStarts[1] : nVerts;
  std::vector<Node*> outerNodes;
  Node* outer = buildRing(arena, poly, outerStart, outerEnd, outerNodes);
  if (!outer || ringCount(outer) < 3) return out;

  // Normalize the outer ring to CCW (area2 < 0 in this convention = CCW; our isEar
  // assumes CCW so a convex corner has area(a,b,c) < 0).
  if (ringArea2(outer) > 0) {  // clockwise → reverse
    Node* p = outer;
    do {
      std::swap(p->next, p->prev);
      p = p->prev;  // (was next)
    } while (p != outer);
  }

  // Eliminate holes (rings 1..n). Each hole must be wound OPPOSITE the outer ring
  // (clockwise) for correctness; we normalize each to CW.
  for (std::size_t r = 1; r < ringN; ++r) {
    const std::size_t hs = poly.ringStarts[r];
    const std::size_t he = (r + 1 < ringN) ? poly.ringStarts[r + 1] : nVerts;
    std::vector<Node*> holeNodes;
    Node* hole = buildRing(arena, poly, hs, he, holeNodes);
    if (!hole || ringCount(hole) < 3) continue;
    if (ringArea2(hole) < 0) {  // CCW → make CW (opposite of outer)
      Node* p = hole;
      do {
        std::swap(p->next, p->prev);
        p = p->prev;
      } while (p != hole);
    }
    eliminateHole(arena, outer, hole);
  }

  // Ear clipping over the (now single) ring rooted at `outer`.
  Node* ear = outer;
  std::size_t remaining = ringCount(ear);
  std::size_t guard = 0;
  const std::size_t guardMax = remaining * remaining + 16;  // anti-infinite-loop

  while (remaining > 3 && guard++ < guardMax) {
    Node* prev = ear->prev;
    Node* next = ear->next;
    if (isEar(ear)) {
      // Emit triangle (prev, ear, next).
      out.push_back(prev->i);
      out.push_back(ear->i);
      out.push_back(next->i);
      // Clip the ear tip out of the ring.
      prev->next = next;
      next->prev = prev;
      ear->removed = true;
      --remaining;
      ear = next->next;  // skip ahead to keep variety
    } else {
      ear = next;
    }
  }

  // Final triangle.
  if (remaining == 3) {
    Node* a = ear->prev;
    Node* b = ear;
    Node* c = ear->next;
    // Only emit if non-degenerate (guards a collinear residue).
    if (std::fabs(area(a, b, c)) > 0.0) {
      out.push_back(a->i);
      out.push_back(b->i);
      out.push_back(c->i);
    }
  }

  return out;
}

}  // namespace geo
}  // namespace dc
