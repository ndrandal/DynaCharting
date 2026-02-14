#include "dc/viewport/AutoScale.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dc {

bool AutoScale::computeYRange(const std::vector<Id>& drawItemIds,
                               const Scene& scene, const IngestProcessor& ingest,
                               const Viewport& viewport,
                               double& yMin, double& yMax) const {
  auto range = viewport.dataRange();
  double xLo = range.xMin;
  double xHi = range.xMax;

  double lo = std::numeric_limits<double>::max();
  double hi = std::numeric_limits<double>::lowest();
  bool found = false;

  for (Id diId : drawItemIds) {
    const DrawItem* di = scene.getDrawItem(diId);
    if (!di || di->geometryId == 0) continue;

    const Geometry* geo = scene.getGeometry(di->geometryId);
    if (!geo) continue;

    const std::uint8_t* bufData = ingest.getBufferData(geo->vertexBufferId);
    std::uint32_t bufSize = ingest.getBufferSize(geo->vertexBufferId);
    if (!bufData || bufSize == 0) continue;

    std::uint32_t stride = strideOf(geo->format);
    if (stride == 0) continue;
    std::uint32_t recordCount = bufSize / stride;

    for (std::uint32_t i = 0; i < recordCount; i++) {
      const float* rec = reinterpret_cast<const float*>(bufData + i * stride);

      double x = 0;
      double recLo = 0, recHi = 0;

      switch (geo->format) {
        case VertexFormat::Pos2_Clip: {
          x = rec[0];
          recLo = rec[1];
          recHi = rec[1];
          break;
        }
        case VertexFormat::Candle6: {
          x = rec[0];
          // high = rec[2], low = rec[3]
          recLo = rec[3]; // low
          recHi = rec[2]; // high
          break;
        }
        case VertexFormat::Rect4: {
          x = (rec[0] + rec[2]) * 0.5;
          recLo = std::min(static_cast<double>(rec[1]), static_cast<double>(rec[3]));
          recHi = std::max(static_cast<double>(rec[1]), static_cast<double>(rec[3]));
          break;
        }
        default:
          continue;
      }

      // Filter by visible X range
      if (x < xLo || x > xHi) continue;

      lo = std::min(lo, recLo);
      hi = std::max(hi, recHi);
      found = true;
    }
  }

  if (!found) return false;

  // Apply margin
  double span = hi - lo;
  if (span < 1e-12) span = 1.0; // avoid zero span
  double margin = span * config_.marginFraction;
  lo -= margin;
  hi += margin;

  // Include zero if configured
  if (config_.includeZero) {
    if (lo > 0.0) lo = 0.0;
    if (hi < 0.0) hi = 0.0;
  }

  yMin = lo;
  yMax = hi;
  return true;
}

} // namespace dc
