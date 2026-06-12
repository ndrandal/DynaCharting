// ENC-618c — GEO layer (closes Epic ENC-618 layout primitives). Three additions:
//   1. Ragged/list dtype: a column cell holds a variable-length span (offsets +
//      flat values). Store/retrieve variable-length cells; existing dtypes still pass.
//   2. Geo-projection: lng/lat → x/y for Mercator / equirectangular / Albers.
//   3. Polygon triangulation: earcut of a simple polygon + a polygon-with-hole →
//      non-overlapping triangles covering the interior (area-conserving), no NaN.
//   4. A small choropleth chain: ragged polygons → project → triangulate → colored
//      fill, built and run as DAG nodes.
#include "dc/data/TableStore.hpp"
#include "dc/geo/Earcut.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scale/GeoProjection.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/TransformDag.hpp"
#include "dc/transform/transforms/Choropleth.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}
static bool nearly(double x, double y, double tol = 1e-4) {
  return std::fabs(x - y) < tol;
}

using namespace dc;

// Append a raw ingest record (op 1 = APPEND) for `bufferId`.
static void appendRecord(std::vector<std::uint8_t>& out, Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF);
    out.push_back((v >> 16) & 0xFF); out.push_back((v >> 24) & 0xFF);
  };
  out.push_back(1);
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

// ===========================================================================
// 1. Ragged / list dtype
// ===========================================================================
static void testRagged() {
  std::printf("--- ragged/list dtype ---\n");
  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);

  const Id kTable = 1;
  const Id kOff = 10, kVal = 11;   // ragged: offsets + values
  const Id kScalar = 12;           // an existing scalar dtype, must still pass
  tables.defineTable(kTable, "features");
  // A LIST(f32) column "coords" backed by offsets(kOff) + values(kVal).
  check(tables.addListColumn(kTable, "coords", DType::F32, kOff, kVal),
        "addListColumn succeeds");
  // addColumn must REJECT List (needs two buffers).
  check(!tables.addColumn(kTable, "bad", DType::List, 99),
        "addColumn rejects List dtype");
  // An existing scalar column added alongside.
  check(tables.addColumn(kTable, "value", DType::F32, kScalar),
        "scalar column still addable");

  // 3 variable-length cells: lengths {2, 5, 3}. Offsets = [0,2,7,10].
  const std::vector<std::int32_t> offsets = {0, 2, 7, 10};
  const std::vector<float> values = {
      1, 2,             // cell 0 (len 2)
      3, 4, 5, 6, 7,    // cell 1 (len 5)
      8, 9, 10};        // cell 2 (len 3)
  const std::vector<float> scalars = {1.0f, 2.0f, 3.0f};

  std::vector<std::uint8_t> b;
  appendRecord(b, kOff, offsets.data(),
               static_cast<std::uint32_t>(offsets.size() * 4));
  appendRecord(b, kVal, values.data(),
               static_cast<std::uint32_t>(values.size() * 4));
  appendRecord(b, kScalar, scalars.data(),
               static_cast<std::uint32_t>(scalars.size() * 4));
  ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));

  // Ragged retrieval: 3 cells of the expected lengths + contents.
  RaggedColumn<float> rag = tables.viewRaggedF32(kTable, "coords", src);
  check(rag.valid(), "ragged view valid");
  check(rag.cellCount() == 3, "ragged cellCount == 3");
  check(rag.cellLength(0) == 2 && rag.cellLength(1) == 5 && rag.cellLength(2) == 3,
        "ragged cell lengths {2,5,3}");

  bool contentsOk = true;
  const std::vector<std::vector<float>> expect = {
      {1, 2}, {3, 4, 5, 6, 7}, {8, 9, 10}};
  for (std::size_t i = 0; i < 3; ++i) {
    ColumnView<float> cell = rag.cell(i);
    if (cell.size() != expect[i].size()) { contentsOk = false; break; }
    for (std::size_t j = 0; j < cell.size(); ++j)
      if (!nearly(cell[j], expect[i][j])) contentsOk = false;
  }
  check(contentsOk, "ragged cell contents correct");

  // Existing scalar dtype unaffected.
  ColumnView<float> sv = tables.viewF32(kTable, "value", src);
  check(sv.valid() && sv.size() == 3 && nearly(sv[2], 3.0f),
        "scalar f32 column still reads correctly");
  // A scalar view of a List column is empty (dtype guard).
  check(!tables.viewF32(kTable, "coords", src).valid(),
        "scalar view of a List column is empty");
  // parse/toString round-trip for the new dtype.
  check(parseDType("list").value_or(DType::F32) == DType::List,
        "parseDType(\"list\") == List");
}

// ===========================================================================
// 2. Geo-projection
// ===========================================================================
static void testProjection() {
  std::printf("--- geo-projection ---\n");

  // Equirectangular: identity in degrees.
  GeoProjection eq(ProjectionType::Equirectangular);
  PlanarPoint e = eq.project(-96.0, 37.0);
  check(nearly(e.x, -96.0) && nearly(e.y, 37.0), "equirect identity");

  // Mercator: origin maps to origin; known reference for lng/lat 45,45.
  GeoProjection m(ProjectionType::Mercator);
  PlanarPoint o = m.project(0.0, 0.0);
  check(nearly(o.x, 0.0) && nearly(o.y, 0.0), "mercator origin → (0,0)");
  // y(45°) = ln(tan(pi/4 + pi/8)) = 0.881373587 ; x(45°) = 45° in rad = 0.785398.
  PlanarPoint p45 = m.project(45.0, 45.0);
  check(nearly(p45.x, 0.7853981634), "mercator x(45°) = π/4 rad");
  check(nearly(p45.y, 0.8813735870), "mercator y(45°) = 0.8813736");
  // Mercator inverse round-trips.
  PlanarPoint inv = m.invert(p45.x, p45.y);
  check(nearly(inv.x, 45.0) && nearly(inv.y, 45.0), "mercator inverse round-trip");

  // Albers equal-area conic (CONUS defaults). The reference origin (lng0,lat0) =
  // (-96, 23) projects with x = 0 (on the central meridian) and a small y.
  GeoProjection a(ProjectionType::Albers);
  PlanarPoint ao = a.project(-96.0, 23.0);
  check(nearly(ao.x, 0.0), "albers central-meridian x == 0 at origin lat");
  // A point on the central meridian stays at x == 0 (symmetry).
  PlanarPoint acm = a.project(-96.0, 45.0);
  check(nearly(acm.x, 0.0, 1e-9), "albers x == 0 on central meridian");
  // Albers is symmetric about the central meridian: ±dLng → ±x, equal |x|, equal y.
  PlanarPoint aL = a.project(-106.0, 40.0);
  PlanarPoint aR = a.project(-86.0, 40.0);
  check(nearly(aL.x, -aR.x) && nearly(aL.y, aR.y),
        "albers symmetric about central meridian");
  // x increases eastward (monotone in lng on the meridian's east side).
  check(aR.x > 0.0 && aL.x < 0.0, "albers x sign tracks lng offset");
}

// ===========================================================================
// 3. Polygon triangulation (earcut)
// ===========================================================================
// Shoelace area of a closed triangle index fan.
static double triFanArea(const geo::Polygon& poly,
                         const std::vector<std::uint32_t>& tris) {
  double a = 0.0;
  for (std::size_t k = 0; k + 2 < tris.size(); k += 3) {
    geo::Vec2 A = poly.vertex(tris[k]);
    geo::Vec2 B = poly.vertex(tris[k + 1]);
    geo::Vec2 C = poly.vertex(tris[k + 2]);
    a += 0.5 * std::fabs(geo::signedArea2(A, B, C));
  }
  return a;
}
static bool anyNaN(const geo::Polygon& poly) {
  for (double c : poly.coords) if (std::isnan(c)) return true;
  return false;
}

static void testTriangulation() {
  std::printf("--- polygon triangulation (earcut) ---\n");

  // Simple unit square (CCW). Area 1 → 2 triangles.
  {
    geo::Polygon sq;
    sq.coords = {0, 0, 1, 0, 1, 1, 0, 1};
    sq.ringStarts = {0};
    std::vector<std::uint32_t> t = geo::earcut(sq);
    check(t.size() == 6, "square → 2 triangles (6 indices)");
    check(nearly(triFanArea(sq, t), 1.0), "square triangle area == 1");
    check(t.size() == geo::maxTriangles(4) * 3, "square hits max-triangle bound");
    // No degenerate triangle.
    bool degen = false;
    for (std::size_t k = 0; k + 2 < t.size(); k += 3)
      if (nearly(std::fabs(geo::signedArea2(sq.vertex(t[k]), sq.vertex(t[k + 1]),
                                            sq.vertex(t[k + 2]))), 0.0))
        degen = true;
    check(!degen, "square: no degenerate triangle");
  }

  // A concave (L-shaped) polygon — ear-clipping must handle the reflex vertex.
  {
    geo::Polygon L;
    L.coords = {0, 0, 2, 0, 2, 1, 1, 1, 1, 2, 0, 2};  // L shape, area 3
    L.ringStarts = {0};
    std::vector<std::uint32_t> t = geo::earcut(L);
    check(!t.empty() && t.size() % 3 == 0, "L-shape triangulates");
    check(nearly(triFanArea(L, t), 3.0, 1e-3), "L-shape area conserved (==3)");
  }

  // Polygon WITH A HOLE: a 4x4 outer square with a 1x1 hole centered. The hole ring
  // is wound CW (opposite the CCW outer). Filled area = 16 - 1 = 15.
  {
    geo::Polygon h;
    // outer (CCW): (0,0)(4,0)(4,4)(0,4)
    // hole   (CW): (1.5,1.5)(1.5,2.5)(2.5,2.5)(2.5,1.5)  -> 1x1 square
    h.coords = {0, 0, 4, 0, 4, 4, 0, 4,
                1.5, 1.5, 1.5, 2.5, 2.5, 2.5, 2.5, 1.5};
    h.ringStarts = {0, 4};  // ring0 = outer (4 verts), ring1 = hole (4 verts)
    std::vector<std::uint32_t> t = geo::earcut(h);
    check(!t.empty() && t.size() % 3 == 0, "polygon-with-hole triangulates");
    double area = triFanArea(h, t);
    check(nearly(area, 15.0, 1e-2), "polygon-with-hole area == outer - hole (15)");
    check(!anyNaN(h), "no NaN coords");
    // No triangle's centroid falls inside the hole (the hole is empty).
    bool insideHole = false;
    for (std::size_t k = 0; k + 2 < t.size(); k += 3) {
      geo::Vec2 A = h.vertex(t[k]), B = h.vertex(t[k + 1]), C = h.vertex(t[k + 2]);
      double cx = (A.x + B.x + C.x) / 3.0, cy = (A.y + B.y + C.y) / 3.0;
      if (cx > 1.5 && cx < 2.5 && cy > 1.5 && cy < 2.5) insideHole = true;
    }
    check(!insideHole, "no triangle centroid inside the hole");
  }
}

// ===========================================================================
// 4. Choropleth chain as DAG nodes
// ===========================================================================
static void testChoroplethChain() {
  std::printf("--- choropleth DAG chain ---\n");

  IngestProcessor ingest;
  TableStore tables;
  auto src = makeBufferByteSource(ingest);

  const Id kTable = 1;
  const Id kOff = 20, kVal = 21, kValue = 22;
  tables.defineTable(kTable, "regions");
  tables.addListColumn(kTable, "coords", DType::F32, kOff, kVal);
  tables.addColumn(kTable, "measure", DType::F32, kValue);

  // Two features, each a square (CCW), in lng/lat degrees:
  //   feature 0: square around (0,0) ; measure 0.0
  //   feature 1: square around (10,10) ; measure 1.0
  const std::vector<float> coords = {
      -1, -1, 1, -1, 1, 1, -1, 1,        // feature 0 (4 verts = 8 floats)
       9,  9, 11, 9, 11, 11, 9, 11};     // feature 1 (4 verts = 8 floats)
  const std::vector<std::int32_t> offsets = {0, 8, 16};  // 8 floats per feature
  const std::vector<float> measure = {0.0f, 1.0f};

  std::vector<std::uint8_t> b;
  appendRecord(b, kOff, offsets.data(),
               static_cast<std::uint32_t>(offsets.size() * 4));
  appendRecord(b, kVal, coords.data(),
               static_cast<std::uint32_t>(coords.size() * 4));
  appendRecord(b, kValue, measure.data(),
               static_cast<std::uint32_t>(measure.size() * 4));
  ingest.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));

  TransformDag dag(tables, src);
  check(dag.addSource(kTable), "addSource(regions)");
  const NodeId kChoro = 100;
  auto choro = std::make_unique<ChoroplethTransform>(
      "coords", "measure", ProjectionType::Equirectangular, 0.0, 1.0);
  check(dag.addTransform(kChoro, kTable, std::move(choro)),
        "addTransform(choropleth) types OK");
  check(dag.build(), "dag.build() OK");

  // Schema is the Pos2Color4 fill stream.
  const ColumnSchema* outSchema = dag.schemaOf(kChoro);
  check(outSchema && outSchema->columns.size() == 6 &&
            outSchema->columns[0].name == "x" &&
            outSchema->columns[5].name == "a",
        "choropleth output schema = x,y,r,g,b,a");

  dag.markTableDirty(kTable);
  std::vector<NodeId> ran = dag.evaluate();
  bool choroRan = false;
  for (NodeId n : ran) if (n == kChoro) choroRan = true;
  check(choroRan, "choropleth node ran");

  // Each square → 2 triangles → 6 verts ; two features → 12 verts.
  const ColumnStore& cols = dag.columns();
  check(cols.rowCount(kChoro, "x") == 12, "12 output triangle-vertices (2 sq × 6)");

  ColumnView<float> vx = cols.viewF32(kChoro, "x");
  ColumnView<float> vy = cols.viewF32(kChoro, "y");
  ColumnView<float> vr = cols.viewF32(kChoro, "r");
  ColumnView<float> vb = cols.viewF32(kChoro, "b");
  bool finite = vx.valid() && vy.valid();
  for (std::size_t i = 0; i < vx.size(); ++i)
    if (std::isnan(vx[i]) || std::isnan(vy[i])) finite = false;
  check(finite, "all output coords finite (no NaN)");

  // Equirect identity: feature-0 verts cluster near (0,0), feature-1 near (10,10).
  // The first 6 verts belong to feature 0 (within [-1,1]); the last 6 to feature 1
  // (within [9,11]).
  bool f0ok = true, f1ok = true;
  for (std::size_t i = 0; i < 6; ++i)
    if (vx[i] < -1.001f || vx[i] > 1.001f) f0ok = false;
  for (std::size_t i = 6; i < 12; ++i)
    if (vx[i] < 8.999f || vx[i] > 11.001f) f1ok = false;
  check(f0ok, "feature-0 verts in [-1,1] (projected)");
  check(f1ok, "feature-1 verts in [9,11] (projected)");

  // Color tracks the value: feature 0 (value 0 → t 0) vs feature 1 (value 1 → t 1)
  // differ in the blue channel (ramp endpoint b: 0.6 vs 1.0).
  check(vr.valid() && vb.valid() && !nearly(vb[0], vb[6]),
        "feature color tracks value (b channel differs)");

  // Re-evaluate without a new dirty mark → choropleth does NOT recompute
  // (dirty-gating respected).
  std::vector<NodeId> ran2 = dag.evaluate();
  bool reran = false;
  for (NodeId n : ran2) if (n == kChoro) reran = true;
  check(!reran, "choropleth not recomputed when clean (dirty-gating)");
}

int main() {
  std::printf("=== ENC-618c GEO layer (ragged + projection + triangulation) ===\n");
  testRagged();
  testProjection();
  testTriangulation();
  testChoroplethChain();
  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
