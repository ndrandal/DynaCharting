// ENC-618c — `choropleth` LAYOUT transform (RESEARCH §7.2/§7.3 geo enablement;
// §7.4 tier-2 "geo-projection" name-it-and-bind-it). The DAG node that closes the
// geo data-model wall by chaining the three new primitives:
//
//   ragged polygon rings  →  project (lng/lat → x/y)  →  earcut triangulate
//                         →  per-feature-colored Pos2Color4 fill triangles
//
// INPUT SCHEMA
// ------------
// One ROW PER FEATURE. Two required columns:
//   * a LIST column of the feature's RING coordinates: a ragged f32 cell holding
//     the feature's polygon as flat (lng,lat) pairs. The cell is one outer ring;
//     hole support rides the optional `ringSizes` column (below). The list dtype is
//     the §7.2 ragged data model — a whole feature's vertices in one cell.
//   * a numeric VALUE column (f32) — the choropleth measure that colors the fill.
//
// Optional:
//   * `ringSizes` — a LIST(i32) column, one cell per feature, giving the vertex
//     count of each ring of that feature (ring 0 = outer, rest = holes). Absent →
//     the feature is a single outer ring (no holes).
//
// OUTPUT SCHEMA (data-free): the Pos2Color4 vertex stream the existing fill mark
// renders — one ROW PER TRIANGLE VERTEX (3 rows per emitted triangle):
//   x, y (f32, PROJECTED planar coords) ; r, g, b, a (f32, the feature's color).
// Variable output cardinality (RESEARCH §7.2 topology wall) is handled by a COUNTED
// output: the row count is the total emitted triangle-vertices across all features,
// computed at evaluate() time (the ColumnStore is sized to it).
//
// COLOR
// -----
// Color is a simple normalized ramp of the feature value over the [minValue,
// maxValue] passed in (a grayscale-to-blue default), so the chain is self-contained
// and testable without pulling in the full ColorScale (ENC-610). A richer color
// scale can replace this mapping without changing the geo plumbing.
//
// Pure `dc` (C++17, CPU, no GPU).
#pragma once

#include "dc/scale/GeoProjection.hpp"
#include "dc/transform/Transform.hpp"

#include <string>

namespace dc {

class ChoroplethTransform : public TransformNode {
 public:
  // `coords` = the LIST(f32) column of per-feature flat (lng,lat) ring vertices;
  // `value` = the per-feature numeric measure; `projection` = the geo projection to
  // apply; [minValue,maxValue] = the color-ramp domain (value → fill color).
  // `ringSizes` (optional) = a LIST(i32) column of per-feature ring vertex counts
  // for polygons-with-holes; empty = single outer ring per feature.
  ChoroplethTransform(std::string coords, std::string value,
                      ProjectionType projection, double minValue, double maxValue,
                      std::string ringSizes = {})
      : coords_(std::move(coords)),
        value_(std::move(value)),
        ringSizes_(std::move(ringSizes)),
        projection_(projection),
        minValue_(minValue),
        maxValue_(maxValue) {}

  const char* op() const override { return "choropleth"; }
  SchemaResult inferSchema(const ColumnSchema& input) const override;
  void evaluate(const EvalContext& ctx) const override;

  // Configure the Albers conic parameters used when projection==Albers.
  void setAlbersParameters(double lat0, double lng0, double p1, double p2) {
    albersLat0_ = lat0;
    albersLng0_ = lng0;
    albersParallel1_ = p1;
    albersParallel2_ = p2;
    hasAlbers_ = true;
  }

 private:
  GeoProjection makeProjection() const;

  std::string coords_;
  std::string value_;
  std::string ringSizes_;
  ProjectionType projection_{ProjectionType::Equirectangular};
  double minValue_{0.0};
  double maxValue_{1.0};

  bool hasAlbers_{false};
  double albersLat0_{23.0}, albersLng0_{-96.0};
  double albersParallel1_{29.5}, albersParallel2_{45.5};
};

}  // namespace dc
