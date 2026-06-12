// ENC-618c — POLYGON TRIANGULATION (earcut / ear-clipping) for the geo layer
// (RESEARCH §7.2/§7.3: "conformable via a list dtype + projection + earcut
// triangulation"; §7.2 topology row "output count is data-dependent → max-bounded
// buffer + count").
//
// WHAT THIS IS
// ------------
// Ear-clipping triangulation of a SIMPLE polygon, optionally WITH HOLES, into a
// fan of fill triangles (the third leg of the geo enablement: a projected polygon
// feature's ring vertices → the triangles the existing fill mark renders). Holes
// are bridged into the outer ring (the standard "eliminate holes" pass) so the whole
// feature triangulates as one ring. This is a self-contained C++17 port of the
// well-known ear-clipping algorithm (the mapbox/earcut approach: a doubly-linked
// vertex ring, hole bridging by a visible mutual vertex, then greedy ear removal).
//
// VARIABLE OUTPUT CARDINALITY (the §7.2 "topology extraction" wall): a polygon with
// V total vertices triangulates to at most (V - 2) triangles (exactly V-2 for a
// simple ring; fewer collapse out for degenerate input). The caller sizes a
// max-bounded buffer at (V - 2) triangles and reads back the actual emitted COUNT
// (triangles.size() / 3) — exactly the counted-output pattern the research names.
//
// SCOPE: planar (already-PROJECTED) coordinates in. Self-intersecting input is not
// repaired (out of scope); the result for a clean simple polygon-with-holes is a
// non-overlapping cover of the interior (area-conserving to numeric tolerance),
// with no degenerate (zero-area) triangles emitted.
//
// Pure `dc` (C++17, CPU, no GPU).
#pragma once

#include <cstdint>
#include <vector>

namespace dc {
namespace geo {

// One planar 2D point (a projected polygon vertex).
struct Vec2 {
  double x{0.0};
  double y{0.0};
};

// A polygon as a flat coordinate list (x0,y0,x1,y1,…) per ring. The FIRST ring is
// the outer boundary; any further rings are HOLES. Rings need NOT repeat their first
// vertex at the end (an implicit closing edge is assumed). Winding is not required
// to be CCW/CW — the triangulator normalizes internally.
struct Polygon {
  // Flat interleaved coordinates for ALL rings, concatenated.
  std::vector<double> coords;
  // Start index (in VERTICES, not floats) of each ring; ring r spans vertices
  // [ringStarts[r], ringStarts[r+1]) (last ring runs to coords.size()/2). The outer
  // ring is ring 0.
  std::vector<std::uint32_t> ringStarts;

  std::size_t vertexCount() const { return coords.size() / 2; }
  Vec2 vertex(std::size_t i) const { return {coords[2 * i], coords[2 * i + 1]}; }
};

// Triangulate `poly` (outer ring + holes) by ear-clipping. Returns a flat list of
// vertex INDICES (into poly's vertex array), 3 per triangle. An empty result means
// the polygon was degenerate (fewer than 3 distinct vertices / zero area).
//
// The maximum triangle count is poly.vertexCount() - 2; callers sizing a fixed
// output buffer use that bound and the returned size for the actual count.
std::vector<std::uint32_t> earcut(const Polygon& poly);

// Convenience: the upper bound on emitted triangles for a vertex count (the
// max-bounded-buffer size, §7.2). 0 for fewer than 3 vertices.
inline std::size_t maxTriangles(std::size_t vertexCount) {
  return vertexCount >= 3 ? vertexCount - 2 : 0;
}

// Signed area * 2 of the triangle (a,b,c). Positive = CCW. Exposed for tests
// (non-overlap / area-conservation checks) and reused internally.
inline double signedArea2(const Vec2& a, const Vec2& b, const Vec2& c) {
  return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
}

}  // namespace geo
}  // namespace dc
