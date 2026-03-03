#include "dc/selection/BoxSelection.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/Geometry.hpp"
#include <cstring>

namespace dc {

void BoxSelection::begin(double dataX, double dataY) {
  active_ = true;
  rect_.x0 = dataX;
  rect_.y0 = dataY;
  rect_.x1 = dataX;
  rect_.y1 = dataY;
}

void BoxSelection::update(double dataX, double dataY) {
  if (!active_) return;
  rect_.x1 = dataX;
  rect_.y1 = dataY;
}

BoxSelectResult BoxSelection::finish(double dataX, double dataY,
                                      const Scene& scene,
                                      const IngestProcessor& ingest) {
  if (!active_) return {};

  rect_.x1 = dataX;
  rect_.y1 = dataY;
  active_ = false;

  BoxSelectResult result;
  result.rect = rect_;

  BoxSelectRect norm;
  norm.x0 = rect_.minX();
  norm.y0 = rect_.minY();
  norm.x1 = rect_.maxX();
  norm.y1 = rect_.maxY();

  if (norm.x0 == norm.x1 && norm.y0 == norm.y1) {
    return result;
  }

  for (Id diId : scene.drawItemIds()) {
    const DrawItem* di = scene.getDrawItem(diId);
    if (!di || !di->visible) continue;
    hitTestDrawItem(*di, scene, ingest, norm, result.hits);
  }

  return result;
}

void BoxSelection::cancel() {
  active_ = false;
}

void BoxSelection::hitTestDrawItem(const DrawItem& di, const Scene& scene,
                                    const IngestProcessor& ingest,
                                    const BoxSelectRect& norm,
                                    std::vector<SelectionKey>& hits) {
  const Geometry* geom = scene.getGeometry(di.geometryId);
  if (!geom) return;

  const std::uint8_t* data = ingest.getBufferData(geom->vertexBufferId);
  std::uint32_t dataSize = ingest.getBufferSize(geom->vertexBufferId);
  if (!data || dataSize == 0) return;

  std::uint32_t stride = strideOf(geom->format);
  if (stride == 0) return;
  std::uint32_t recordCount = dataSize / stride;

  switch (geom->format) {
    case VertexFormat::Pos2_Clip:
    case VertexFormat::Pos2Alpha:
    case VertexFormat::Pos2Color4: {
      for (std::uint32_t i = 0; i < recordCount; i++) {
        float x, y;
        std::memcpy(&x, data + i * stride, sizeof(float));
        std::memcpy(&y, data + i * stride + sizeof(float), sizeof(float));
        if (x >= norm.x0 && x <= norm.x1 && y >= norm.y0 && y <= norm.y1) {
          hits.push_back({di.id, i});
        }
      }
      break;
    }
    case VertexFormat::Rect4: {
      for (std::uint32_t i = 0; i < recordCount; i++) {
        float rx0, ry0, rx1, ry1;
        std::memcpy(&rx0, data + i * stride, sizeof(float));
        std::memcpy(&ry0, data + i * stride + 4, sizeof(float));
        std::memcpy(&rx1, data + i * stride + 8, sizeof(float));
        std::memcpy(&ry1, data + i * stride + 12, sizeof(float));
        float rMinX = rx0 < rx1 ? rx0 : rx1;
        float rMaxX = rx0 > rx1 ? rx0 : rx1;
        float rMinY = ry0 < ry1 ? ry0 : ry1;
        float rMaxY = ry0 > ry1 ? ry0 : ry1;
        if (rMaxX >= norm.x0 && rMinX <= norm.x1 &&
            rMaxY >= norm.y0 && rMinY <= norm.y1) {
          hits.push_back({di.id, i});
        }
      }
      break;
    }
    case VertexFormat::Candle6: {
      for (std::uint32_t i = 0; i < recordCount; i++) {
        float cx, copen, chigh, clow, cclose, chw;
        const std::uint8_t* rec = data + i * stride;
        std::memcpy(&cx, rec, sizeof(float));
        std::memcpy(&copen, rec + 4, sizeof(float));
        std::memcpy(&chigh, rec + 8, sizeof(float));
        std::memcpy(&clow, rec + 12, sizeof(float));
        std::memcpy(&cclose, rec + 16, sizeof(float));
        std::memcpy(&chw, rec + 20, sizeof(float));
        (void)copen; (void)cclose;
        if ((cx + chw) >= norm.x0 && (cx - chw) <= norm.x1 &&
            chigh >= norm.y0 && clow <= norm.y1) {
          hits.push_back({di.id, i});
        }
      }
      break;
    }
    default:
      break;
  }
}

} // namespace dc
