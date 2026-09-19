// ENC-1257 — bar sizing rule implementation. See BarSizing.hpp for the why.
#include "dc/render/BarSizing.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace dc {

namespace {

bool finitePos(float v) { return std::isfinite(v) && v > 0.0f; }

float readF32(const std::uint8_t* p) {
  float v = 0.0f;
  std::memcpy(&v, p, sizeof(float));
  return v;
}

// Median by value, destructive on the vector (nth_element). Empty -> 0.
float medianOf(std::vector<float>& v) {
  if (v.empty()) return 0.0f;
  const std::size_t mid = v.size() / 2;
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
  return v[mid];
}

}  // namespace

BarMetrics resolveBarWidth(float pitchPx, float nominalBodyPx,
                           const BarSizingConfig& cfg) {
  BarMetrics m;
  m.pitchPx = pitchPx;

  // No pitch -> no inter-bar relation -> nothing to rule on. The caller keeps
  // whatever it had; this is the single-bar case and it must not change.
  if (!std::isfinite(pitchPx) || pitchPx <= 0.0f) {
    m.bodyPx = std::isfinite(nominalBodyPx) && nominalBodyPx > 0.0f ? nominalBodyPx : 0.0f;
    m.gapPx = 0.0f;
    m.degraded = true;
    return m;
  }

  const float minBody = std::max(0.0f, cfg.minBodyPx);
  const float minGap = std::max(0.0f, cfg.minGapPx);

  // Below minBody+minGap of pitch no arrangement of ink and space is resolvable.
  // Split the pitch in the same proportion and say so, rather than reporting a
  // gap the raster cannot show.
  if (pitchPx < minBody + minGap) {
    const float denom = minBody + minGap;
    const float share = denom > 0.0f ? minBody / denom : 0.5f;
    m.bodyPx = pitchPx * share;
    m.gapPx = pitchPx - m.bodyPx;
    m.degraded = true;
    m.clamped = true;
    return m;
  }

  const float nominal = (std::isfinite(nominalBodyPx) && nominalBodyPx > 0.0f)
                            ? nominalBodyPx
                            : pitchPx * cfg.defaultBodyFraction;

  // The gap floor is the larger of the absolute pixel floor and a share of the
  // pitch — a 1px gap between 120px bodies is legible and still wrong.
  float floorGap = std::max(minGap, pitchPx * std::max(0.0f, cfg.minGapFraction));
  float maxBody = pitchPx - floorGap;
  if (maxBody < minBody) {
    // The fractional floor is the softer of the two constraints; relax it to the
    // absolute one before touching minBodyPx.
    maxBody = pitchPx - minGap;
  }
  const float ceilBody = std::min(maxBody, std::max(minBody, cfg.maxBodyPx));

  float body = std::min(std::max(nominal, minBody), ceilBody);
  // min/max can cross if ceilBody < minBody; the branch above guarantees
  // maxBody >= minBody, and maxBodyPx is floored at minBody, so it cannot.
  m.bodyPx = body;
  m.gapPx = pitchPx - body;
  m.clamped = std::fabs(body - nominal) > 1e-4f;
  return m;
}

BarMetrics barMetricsForCount(int barCount, float plotWidthPx,
                              float nominalBodyFraction,
                              const BarSizingConfig& cfg) {
  if (barCount <= 0 || !std::isfinite(plotWidthPx) || plotWidthPx <= 0.0f) {
    return resolveBarWidth(0.0f, 0.0f, cfg);
  }
  const float pitchPx = plotWidthPx / static_cast<float>(barCount);
  const float nominalPx =
      (std::isfinite(nominalBodyFraction) && nominalBodyFraction > 0.0f)
          ? pitchPx * nominalBodyFraction
          : -1.0f;
  return resolveBarWidth(pitchPx, nominalPx, cfg);
}

int maxLegibleBarCount(float plotWidthPx, const BarSizingConfig& cfg) {
  const float unit = std::max(0.0f, cfg.minBodyPx) + std::max(0.0f, cfg.minGapPx);
  if (!std::isfinite(plotWidthPx) || plotWidthPx <= 0.0f || unit <= 0.0f) return 0;
  return static_cast<int>(std::floor(plotWidthPx / unit));
}

float barPitchFromRecords(const std::uint8_t* bytes, std::size_t byteLen,
                          std::uint32_t strideBytes, std::uint32_t xOffsetBytes,
                          std::size_t maxSamples) {
  if (!bytes || strideBytes < sizeof(float)) return 0.0f;
  if (xOffsetBytes + sizeof(float) > strideBytes) return 0.0f;
  const std::size_t n = byteLen / strideBytes;
  if (n < 2) return 0.0f;
  const std::size_t take = std::min(n, std::max<std::size_t>(2, maxSamples));

  std::vector<float> deltas;
  deltas.reserve(take - 1);
  float prev = readF32(bytes + xOffsetBytes);
  for (std::size_t i = 1; i < take; ++i) {
    const float x = readF32(bytes + i * strideBytes + xOffsetBytes);
    const float d = x - prev;
    prev = x;
    if (finitePos(d)) deltas.push_back(d);
  }
  return medianOf(deltas);
}

float medianRecordField(const std::uint8_t* bytes, std::size_t byteLen,
                        std::uint32_t strideBytes, std::uint32_t fieldOffsetBytes,
                        std::size_t maxSamples) {
  if (!bytes || strideBytes < sizeof(float)) return 0.0f;
  if (fieldOffsetBytes + sizeof(float) > strideBytes) return 0.0f;
  const std::size_t n = byteLen / strideBytes;
  if (n == 0) return 0.0f;
  const std::size_t take = std::min(n, std::max<std::size_t>(1, maxSamples));

  std::vector<float> vals;
  vals.reserve(take);
  for (std::size_t i = 0; i < take; ++i) {
    const float v = readF32(bytes + i * strideBytes + fieldOffsetBytes);
    if (finitePos(v)) vals.push_back(v);
  }
  return medianOf(vals);
}

CandleBodyResolution resolveCandleBodyClip(float pitchData, float nominalHalfData,
                                           float xScale, int viewportW,
                                           const BarSizingConfig& cfg) {
  CandleBodyResolution out;
  if (viewportW <= 0) return out;
  if (!finitePos(pitchData)) return out;
  if (!std::isfinite(xScale)) return out;

  const float pxPerData =
      std::fabs(xScale) * static_cast<float>(viewportW) * 0.5f;
  if (!finitePos(pxPerData)) return out;

  const float pitchPx = pitchData * pxPerData;
  if (!finitePos(pitchPx)) return out;

  const float nominalBodyPx =
      finitePos(nominalHalfData) ? 2.0f * nominalHalfData * pxPerData : -1.0f;

  out.metrics = resolveBarWidth(pitchPx, nominalBodyPx, cfg);
  // bodyPx pixels span 2*bodyPx/W of clip; the half-extent is bodyPx/W.
  out.halfWidthClip = out.metrics.bodyPx / static_cast<float>(viewportW);
  out.apply = finitePos(out.halfWidthClip);
  return out;
}

}  // namespace dc
