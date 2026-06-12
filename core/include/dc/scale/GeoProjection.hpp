// ENC-618c — GEO PROJECTION scale (RESEARCH §7.2/§7.3 geo: "a nonlinear projection
// function"; §7.4 tier-2 "geo-projection" as a name-it-and-bind-it primitive).
//
// WHAT THIS IS
// ------------
// A projection is a SCALE-LIKE mapping, but from a coordinate PAIR (lng, lat in
// degrees) to a planar PAIR (x, y). The linear/time/band scales map one scalar →
// one scalar; a geo projection maps two → two through a closed-form nonlinear
// function. It is the second half of the geo enablement (the first is the ragged
// polygon data model): once a feature's ring vertices live in one ragged cell, the
// projection turns their lng/lat into the planar coordinates the fill mark draws.
//
// SCOPE (ENC-618c): the common CLOSED-FORM projections only —
//   * Equirectangular (plate carrée): the trivial x=lng, y=lat linear baseline.
//   * Mercator: the conformal web-map standard (y = ln(tan(π/4 + φ/2))).
//   * Albers equal-area conic: the standard US-choropleth equal-area projection
//     (two standard parallels) — the "equal-area" leg the task calls for.
// NOT TopoJSON arc decoding, NOT cartograms, NOT GPU — all explicitly out of scope.
//
// CONVENTION
// ----------
// Input is (lng, lat) in DEGREES (lng ∈ [-180,180], lat ∈ [-90,90]). Output is a
// planar (x, y) in projection-native units; for Mercator/equirect y grows NORTH
// (up). The raw projected coordinates are deliberately NOT normalized to a viewport
// here — a downstream LinearScale (or the encode pass) fits the projected extent to
// clip space, exactly as a numeric column is fit. This keeps the projection a pure
// closed-form coordinate transform (testable against known reference values) and
// leaves the data-range fit to the existing scale machinery.
//
// Pure `dc` (C++17, CPU, no GPU).
#pragma once

#include <cstdint>

namespace dc {

// The projection family. Closed-form only (ENC-618c).
enum class ProjectionType : std::uint8_t {
  Equirectangular,  // plate carrée: x=lng, y=lat (degrees → degrees)
  Mercator,         // conformal web standard
  Albers,           // equal-area conic (two standard parallels)
};

// A planar projected point.
struct PlanarPoint {
  double x{0.0};
  double y{0.0};
};

// ---------------------------------------------------------------------------
// GeoProjection — lng/lat (degrees) → planar (x, y). A scale-like coordinate
// mapping. project() is the closed-form forward transform; the inverse is provided
// for the projections that have a clean closed-form one (equirect + Mercator) so a
// pick/hover path can round-trip a pixel back to a coordinate.
//
// Albers parameters default to the canonical CONUS values (φ0=23°, λ0=-96°,
// parallels 29.5° / 45.5°) — the conventional US-states choropleth setup.
// ---------------------------------------------------------------------------
class GeoProjection {
 public:
  explicit GeoProjection(ProjectionType type = ProjectionType::Equirectangular)
      : type_(type) {}

  ProjectionType type() const { return type_; }

  // Forward: (lngDeg, latDeg) → planar (x, y).
  PlanarPoint project(double lngDeg, double latDeg) const;

  // Inverse: planar (x, y) → (lngDeg, latDeg). Implemented for Equirectangular and
  // Mercator (closed-form); for Albers it returns the forward of a best-effort
  // numeric inverse is out of scope — Albers inverse() returns {0,0} (callers that
  // need round-trip use an invertible projection). hasInverse() reports which.
  PlanarPoint invert(double x, double y) const;
  bool hasInverse() const { return type_ != ProjectionType::Albers; }

  // ----- Albers conic configuration (ignored by the other projections) -------
  // Reference origin (lat0/lng0) and the two standard parallels (degrees).
  void setAlbersParameters(double lat0Deg, double lng0Deg, double parallel1Deg,
                           double parallel2Deg);
  double albersLat0() const { return albersLat0_; }
  double albersLng0() const { return albersLng0_; }

 private:
  ProjectionType type_{ProjectionType::Equirectangular};

  // Albers (defaults = CONUS).
  double albersLat0_{23.0};
  double albersLng0_{-96.0};
  double albersParallel1_{29.5};
  double albersParallel2_{45.5};
};

}  // namespace dc
