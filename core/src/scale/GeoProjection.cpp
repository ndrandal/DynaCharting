// ENC-618c — GeoProjection: closed-form lng/lat → planar (x, y). See header.
#include "dc/scale/GeoProjection.hpp"

#include <algorithm>
#include <cmath>

namespace dc {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

double clampLat(double latDeg) {
  // Mercator diverges at the poles; clamp to the web-Mercator standard ±85.0511°
  // only matters for normalization, but a hard ±89.9999 keeps tan finite for any
  // closed-form use without distorting mid-latitudes.
  if (latDeg > 89.9999) return 89.9999;
  if (latDeg < -89.9999) return -89.9999;
  return latDeg;
}
}  // namespace

void GeoProjection::setAlbersParameters(double lat0Deg, double lng0Deg,
                                        double parallel1Deg, double parallel2Deg) {
  albersLat0_ = lat0Deg;
  albersLng0_ = lng0Deg;
  albersParallel1_ = parallel1Deg;
  albersParallel2_ = parallel2Deg;
}

PlanarPoint GeoProjection::project(double lngDeg, double latDeg) const {
  switch (type_) {
    case ProjectionType::Equirectangular: {
      // Plate carrée: identity in degrees, y north-up.
      return {lngDeg, latDeg};
    }
    case ProjectionType::Mercator: {
      const double lng = lngDeg * kDegToRad;
      const double lat = clampLat(latDeg) * kDegToRad;
      const double x = lng;
      const double y = std::log(std::tan(kPi / 4.0 + lat / 2.0));
      return {x, y};
    }
    case ProjectionType::Albers: {
      // Albers equal-area conic (unit sphere, R=1). Standard formulae
      // (Snyder, Map Projections — A Working Manual):
      //   n = (sin φ1 + sin φ2) / 2
      //   C = cos²φ1 + 2 n sin φ1
      //   ρ  = sqrt(C - 2 n sin φ) / n
      //   ρ0 = sqrt(C - 2 n sin φ0) / n
      //   θ  = n (λ - λ0)
      //   x  = ρ sin θ ;  y = ρ0 - ρ cos θ
      const double phi1 = albersParallel1_ * kDegToRad;
      const double phi2 = albersParallel2_ * kDegToRad;
      const double phi0 = albersLat0_ * kDegToRad;
      const double lambda0 = albersLng0_ * kDegToRad;
      const double phi = latDeg * kDegToRad;
      const double lambda = lngDeg * kDegToRad;

      const double n = (std::sin(phi1) + std::sin(phi2)) / 2.0;
      if (std::fabs(n) < 1e-12) {
        // Degenerate (parallels symmetric about the equator) → fall back to a
        // simple cylindrical equal-area to avoid a divide-by-zero.
        return {lambda - lambda0, std::sin(phi)};
      }
      const double C = std::cos(phi1) * std::cos(phi1) + 2.0 * n * std::sin(phi1);
      const double rho = std::sqrt(std::max(0.0, C - 2.0 * n * std::sin(phi))) / n;
      const double rho0 =
          std::sqrt(std::max(0.0, C - 2.0 * n * std::sin(phi0))) / n;
      const double theta = n * (lambda - lambda0);
      const double x = rho * std::sin(theta);
      const double y = rho0 - rho * std::cos(theta);
      return {x, y};
    }
  }
  return {lngDeg, latDeg};
}

PlanarPoint GeoProjection::invert(double x, double y) const {
  switch (type_) {
    case ProjectionType::Equirectangular:
      return {x, y};  // lng=x, lat=y (degrees)
    case ProjectionType::Mercator: {
      const double lng = x * kRadToDeg;
      const double lat = (2.0 * std::atan(std::exp(y)) - kPi / 2.0) * kRadToDeg;
      return {lng, lat};
    }
    case ProjectionType::Albers:
    default:
      return {0.0, 0.0};  // no closed-form inverse exposed (see hasInverse()).
  }
}

}  // namespace dc
