#include "dc/viewport/DataPicker.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"

#include <cmath>
#include <cstring>

namespace dc {

HitResult DataPicker::pick(double cursorPx, double cursorPy, Id paneId,
                           const Scene& scene, const IngestProcessor& ingest,
                           const Viewport& viewport) const {
  HitResult best;

  // Convert cursor to data-space
  double cursorDx, cursorDy;
  viewport.pixelToData(cursorPx, cursorPy, cursorDx, cursorDy);

  // Get px/data-unit for distance conversion
  double ppduX = viewport.pixelsPerDataUnitX();
  double ppduY = viewport.pixelsPerDataUnitY();

  double bestDistPx = config_.maxDistancePx;

  for (Id layerId : scene.layerIds()) {
    const Layer* layer = scene.getLayer(layerId);
    if (!layer || layer->paneId != paneId) continue;

    for (Id diId : scene.drawItemIds()) {
      const DrawItem* di = scene.getDrawItem(diId);
      if (!di || di->layerId != layerId) continue;
      if (di->transformId == 0) continue; // skip non-data items
      if (di->geometryId == 0) continue;

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

        double dx = 0, dy = 0;
        double distPx = 1e30;

        switch (geo->format) {
          case VertexFormat::Pos2_Clip: {
            dx = rec[0];
            dy = rec[1];
            double ddx = (dx - cursorDx) * ppduX;
            double ddy = (dy - cursorDy) * ppduY;
            distPx = std::sqrt(ddx * ddx + ddy * ddy);
            break;
          }
          case VertexFormat::Candle6: {
            // cx, open, high, low, close, halfWidth
            float cx    = rec[0];
            float open  = rec[1];
            float high  = rec[2];
            float low   = rec[3];
            float close = rec[4];
            float hw    = rec[5];

            dx = cx;
            dy = (open + close) * 0.5;

            // Check if cursor X is within the candle body
            double xDistData = std::fabs(cursorDx - cx);
            double xDistPx = xDistData * ppduX;

            // Y distance to centerline
            double yCenter = (open + close) * 0.5;
            double yDistData = 0.0;
            if (cursorDy >= low && cursorDy <= high) {
              yDistData = 0.0; // cursor within wick range
            } else {
              yDistData = std::min(std::fabs(cursorDy - low), std::fabs(cursorDy - high));
            }
            double yDistPx = yDistData * ppduY;

            // If within body width, horizontal distance is 0
            if (xDistData <= hw) xDistPx = 0.0;

            distPx = std::sqrt(xDistPx * xDistPx + yDistPx * yDistPx);
            (void)yCenter;
            break;
          }
          case VertexFormat::Rect4: {
            float x0 = rec[0], y0 = rec[1], x1 = rec[2], y1 = rec[3];
            dx = (x0 + x1) * 0.5;
            dy = (y0 + y1) * 0.5;

            // Check if inside
            float rMinX = std::min(x0, x1), rMaxX = std::max(x0, x1);
            float rMinY = std::min(y0, y1), rMaxY = std::max(y0, y1);

            if (cursorDx >= rMinX && cursorDx <= rMaxX &&
                cursorDy >= rMinY && cursorDy <= rMaxY) {
              distPx = 0.0;
            } else {
              double clampX = std::max(static_cast<double>(rMinX), std::min(cursorDx, static_cast<double>(rMaxX)));
              double clampY = std::max(static_cast<double>(rMinY), std::min(cursorDy, static_cast<double>(rMaxY)));
              double ddx = (cursorDx - clampX) * ppduX;
              double ddy = (cursorDy - clampY) * ppduY;
              distPx = std::sqrt(ddx * ddx + ddy * ddy);
            }
            break;
          }
          default:
            continue;
        }

        if (distPx < bestDistPx) {
          bestDistPx = distPx;
          best.hit = true;
          best.drawItemId = di->id;
          best.recordIndex = i;
          best.dataX = dx;
          best.dataY = dy;
          best.distancePx = distPx;
        }
      }
    }
  }

  return best;
}

} // namespace dc
