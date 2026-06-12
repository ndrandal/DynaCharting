// ENC-618c — `choropleth` transform: ragged polygons → project → triangulate →
// colored Pos2Color4 fill. See Choropleth.hpp.
#include "dc/transform/transforms/Choropleth.hpp"

#include "dc/geo/Earcut.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace dc {

GeoProjection ChoroplethTransform::makeProjection() const {
  GeoProjection p(projection_);
  if (projection_ == ProjectionType::Albers && hasAlbers_) {
    p.setAlbersParameters(albersLat0_, albersLng0_, albersParallel1_,
                          albersParallel2_);
  }
  return p;
}

SchemaResult ChoroplethTransform::inferSchema(const ColumnSchema& input) const {
  SchemaResult r;
  // Required: a LIST(coords) column + a numeric value column. Typing is data-free:
  // we reason over names + dtypes only.
  const SchemaColumn* coordsCol = input.find(coords_);
  if (!coordsCol) {
    r.error = "choropleth: coords column '" + coords_ + "' not found";
    return r;
  }
  if (coordsCol->dtype != DType::List) {
    r.error = "choropleth: coords column '" + coords_ + "' must be a list dtype";
    return r;
  }
  const SchemaColumn* valueCol = input.find(value_);
  if (!valueCol) {
    r.error = "choropleth: value column '" + value_ + "' not found";
    return r;
  }
  if (valueCol->dtype == DType::List) {
    r.error = "choropleth: value column '" + value_ + "' must be a scalar dtype";
    return r;
  }
  if (!ringSizes_.empty()) {
    const SchemaColumn* rs = input.find(ringSizes_);
    if (!rs || rs->dtype != DType::List) {
      r.error = "choropleth: ringSizes column '" + ringSizes_ +
                "' must be a list dtype";
      return r;
    }
  }

  // Output: a Pos2Color4 vertex stream (one row per triangle vertex).
  ColumnSchema out;
  out.columns.push_back({"x", DType::F32});
  out.columns.push_back({"y", DType::F32});
  out.columns.push_back({"r", DType::F32});
  out.columns.push_back({"g", DType::F32});
  out.columns.push_back({"b", DType::F32});
  out.columns.push_back({"a", DType::F32});
  r.schema = std::move(out);
  r.ok = true;
  return r;
}

void ChoroplethTransform::evaluate(const EvalContext& ctx) const {
  const ColumnResolver& in = *ctx.input;
  const Id node = ctx.nodeId;
  const GeoProjection proj = makeProjection();

  // The ragged coords view (one cell = one feature's flat lng/lat pairs).
  RaggedColumn<float> coords = in.raggedF32 ? in.raggedF32(coords_)
                                            : RaggedColumn<float>{};
  RaggedColumn<std::int32_t> ringSizes =
      (!ringSizes_.empty() && in.raggedI32) ? in.raggedI32(ringSizes_)
                                            : RaggedColumn<std::int32_t>{};

  const std::size_t nFeatures = coords.cellCount();
  const double span = (maxValue_ > minValue_) ? (maxValue_ - minValue_) : 0.0;

  // Accumulate the interleaved Pos2Color4 vertices, then size + fill the output
  // columns from the realized count (the §7.2 counted-output / variable-cardinality
  // pattern — output rows are data-dependent, computed here).
  struct V {
    float x, y, r, g, b, a;
  };
  std::vector<V> verts;

  for (std::size_t f = 0; f < nFeatures; ++f) {
    ColumnView<float> cell = coords.cell(f);
    const std::size_t nCoord = cell.size();
    if (nCoord < 6) continue;  // <3 vertices → no polygon

    // Color: normalized value over [min,max] → a grayscale→blue ramp.
    const double value = in.readNum ? in.readNum(value_, f) : 0.0;
    double t = (span > 0.0) ? (value - minValue_) / span : 0.5;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    const float cr = static_cast<float>(0.9 - 0.6 * t);
    const float cg = static_cast<float>(0.9 - 0.4 * t);
    const float cb = static_cast<float>(0.6 + 0.4 * t);
    const float ca = 1.0f;

    // Build the projected polygon (outer ring + holes). PROJECT each (lng,lat) to
    // planar (x,y) BEFORE triangulation (the projection is nonlinear, so triangle
    // edges must be straight in PROJECTED space, not in lng/lat space).
    geo::Polygon poly;
    const std::size_t totalVerts = nCoord / 2;
    poly.coords.reserve(nCoord);
    for (std::size_t v = 0; v < totalVerts; ++v) {
      const PlanarPoint pp =
          proj.project(cell[2 * v], cell[2 * v + 1]);
      poly.coords.push_back(pp.x);
      poly.coords.push_back(pp.y);
    }

    // Ring partition: from ringSizes (vertex counts per ring) if present, else one
    // outer ring spanning all vertices.
    poly.ringStarts.clear();
    if (ringSizes.valid() && f < ringSizes.cellCount()) {
      ColumnView<std::int32_t> rs = ringSizes.cell(f);
      std::uint32_t acc = 0;
      bool sane = true;
      for (std::size_t k = 0; k < rs.size(); ++k) {
        if (rs[k] <= 0) { sane = false; break; }
        poly.ringStarts.push_back(acc);
        acc += static_cast<std::uint32_t>(rs[k]);
      }
      // If the ring sizes don't sum to the vertex count, fall back to one ring.
      if (!sane || acc != totalVerts || poly.ringStarts.empty()) {
        poly.ringStarts.assign(1, 0u);
      }
    } else {
      poly.ringStarts.assign(1, 0u);
    }

    std::vector<std::uint32_t> tris = geo::earcut(poly);
    for (std::size_t k = 0; k + 2 < tris.size(); k += 3) {
      for (int j = 0; j < 3; ++j) {
        const std::uint32_t vi = tris[k + j];
        const float px = static_cast<float>(poly.coords[2 * vi]);
        const float py = static_cast<float>(poly.coords[2 * vi + 1]);
        verts.push_back(V{px, py, cr, cg, cb, ca});
      }
    }
  }

  // Emit the counted Pos2Color4 stream.
  const std::size_t rows = verts.size();
  ctx.out->allocColumn(node, "x", DType::F32, rows);
  ctx.out->allocColumn(node, "y", DType::F32, rows);
  ctx.out->allocColumn(node, "r", DType::F32, rows);
  ctx.out->allocColumn(node, "g", DType::F32, rows);
  ctx.out->allocColumn(node, "b", DType::F32, rows);
  ctx.out->allocColumn(node, "a", DType::F32, rows);
  for (std::size_t i = 0; i < rows; ++i) {
    ctx.out->setF32(node, "x", i, verts[i].x);
    ctx.out->setF32(node, "y", i, verts[i].y);
    ctx.out->setF32(node, "r", i, verts[i].r);
    ctx.out->setF32(node, "g", i, verts[i].g);
    ctx.out->setF32(node, "b", i, verts[i].b);
    ctx.out->setF32(node, "a", i, verts[i].a);
  }
}

}  // namespace dc
