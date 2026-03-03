#include "dc/interaction/HandleSet.hpp"

#include <algorithm>
#include <cmath>

namespace dc {

// --- Private helpers ---

Handle* HandleSet::findHandle(std::uint32_t id) {
  for (auto& h : handles_) {
    if (h.id == id) return &h;
  }
  return nullptr;
}

const Handle* HandleSet::findHandle(std::uint32_t id) const {
  for (auto& h : handles_) {
    if (h.id == id) return &h;
  }
  return nullptr;
}

HandleSet::DrawingCoords* HandleSet::findCoords(std::uint32_t drawingId) {
  for (auto& dc : origCoords_) {
    if (dc.drawingId == drawingId) return &dc;
  }
  return nullptr;
}

const HandleSet::DrawingCoords* HandleSet::findCoords(std::uint32_t drawingId) const {
  for (auto& dc : origCoords_) {
    if (dc.drawingId == drawingId) return &dc;
  }
  return nullptr;
}

void HandleSet::updateRectHandles_(std::uint32_t drawingId, const DrawingCoords& coords) {
  double mx = (coords.x0 + coords.x1) * 0.5;
  double my = (coords.y0 + coords.y1) * 0.5;

  for (auto& h : handles_) {
    if (h.drawingId != drawingId) continue;
    switch (h.pointIndex) {
      case 0: h.x = coords.x0; h.y = coords.y0; break;
      case 1: h.x = coords.x1; h.y = coords.y0; break;
      case 2: h.x = coords.x1; h.y = coords.y1; break;
      case 3: h.x = coords.x0; h.y = coords.y1; break;
      case 4: h.x = mx;        h.y = coords.y0; break;
      case 5: h.x = coords.x1; h.y = my;        break;
      case 6: h.x = mx;        h.y = coords.y1; break;
      case 7: h.x = coords.x0; h.y = my;        break;
      case 8: h.x = mx;        h.y = my;        break;
      default: break;
    }
  }
}

// --- Public API ---

void HandleSet::createForDrawing(std::uint32_t drawingId, std::uint8_t drawingType,
                                  double x0, double y0, double x1, double y1) {
  // Remove any existing handles for this drawing first.
  removeForDrawing(drawingId);

  // Store original coordinates.
  DrawingCoords coords;
  coords.drawingId = drawingId;
  coords.drawingType = drawingType;
  coords.x0 = x0;
  coords.y0 = y0;
  coords.x1 = x1;
  coords.y1 = y1;
  origCoords_.push_back(coords);

  auto addHandle = [&](std::uint8_t pointIndex, double hx, double hy) {
    Handle h;
    h.id = nextId_++;
    h.drawingId = drawingId;
    h.pointIndex = pointIndex;
    h.x = hx;
    h.y = hy;
    handles_.push_back(h);
  };

  switch (drawingType) {
    case 1: // Trendline: start(0), end(1), midpoint(2)
      addHandle(0, x0, y0);
      addHandle(1, x1, y1);
      addHandle(2, (x0 + x1) * 0.5, (y0 + y1) * 0.5);
      break;

    case 2: // HorizontalLevel: single price handle(0)
      addHandle(0, x0, y0);
      break;

    case 3: // VerticalLine: single x-position handle(0)
      addHandle(0, x0, y0);
      break;

    case 4: { // Rectangle: 4 corners + 4 edge midpoints + 1 center = 9
      // Corners: pointIndex 0-3
      //   0 = (x0, y0) top-left
      //   1 = (x1, y0) top-right
      //   2 = (x1, y1) bottom-right
      //   3 = (x0, y1) bottom-left
      addHandle(0, x0, y0);
      addHandle(1, x1, y0);
      addHandle(2, x1, y1);
      addHandle(3, x0, y1);

      // Edge midpoints: pointIndex 4-7
      double mx = (x0 + x1) * 0.5;
      double my = (y0 + y1) * 0.5;
      addHandle(4, mx, y0);  // top edge midpoint
      addHandle(5, x1, my);  // right edge midpoint
      addHandle(6, mx, y1);  // bottom edge midpoint
      addHandle(7, x0, my);  // left edge midpoint

      // Center: pointIndex 8
      addHandle(8, mx, my);
      break;
    }

    case 5: // FibRetracement: start(0), end(1)
      addHandle(0, x0, y0);
      addHandle(1, x1, y1);
      break;

    default:
      break;
  }
}

void HandleSet::removeForDrawing(std::uint32_t drawingId) {
  handles_.erase(
    std::remove_if(handles_.begin(), handles_.end(),
                   [drawingId](const Handle& h) { return h.drawingId == drawingId; }),
    handles_.end());

  origCoords_.erase(
    std::remove_if(origCoords_.begin(), origCoords_.end(),
                   [drawingId](const DrawingCoords& c) { return c.drawingId == drawingId; }),
    origCoords_.end());
}

std::uint32_t HandleSet::hitTest(double dataX, double dataY,
                                  double pixelPerDataX, double pixelPerDataY) const {
  std::uint32_t bestId = 0;
  double bestDistSq = 1e30;

  for (auto& h : handles_) {
    // Convert data-space delta to pixel-space for radius comparison.
    double dxData = dataX - h.x;
    double dyData = dataY - h.y;
    double dxPx = dxData * pixelPerDataX;
    double dyPx = dyData * pixelPerDataY;
    double distSq = dxPx * dxPx + dyPx * dyPx;
    double radiusSq = h.hitRadius * h.hitRadius;

    if (distSq <= radiusSq && distSq < bestDistSq) {
      bestDistSq = distSq;
      bestId = h.id;
    }
  }
  return bestId;
}

void HandleSet::beginDrag(std::uint32_t handleId) {
  Handle* h = findHandle(handleId);
  if (h) {
    h->dragging = true;
  }
}

std::uint32_t HandleSet::updateDrag(std::uint32_t handleId, double newDataX, double newDataY) {
  Handle* h = findHandle(handleId);
  if (!h) return 0;

  std::uint32_t drawingId = h->drawingId;
  DrawingCoords* coords = findCoords(drawingId);
  if (!coords) return drawingId;

  double dx = newDataX - h->x;
  double dy = newDataY - h->y;

  std::uint8_t type = coords->drawingType;
  std::uint8_t pi = h->pointIndex;

  if (type == 1) { // Trendline
    if (pi == 0) {
      // Move start point; update midpoint.
      coords->x0 = newDataX;
      coords->y0 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
      // Update midpoint handle.
      for (auto& oh : handles_) {
        if (oh.drawingId == drawingId && oh.pointIndex == 2) {
          oh.x = (coords->x0 + coords->x1) * 0.5;
          oh.y = (coords->y0 + coords->y1) * 0.5;
        }
      }
    } else if (pi == 1) {
      // Move end point; update midpoint.
      coords->x1 = newDataX;
      coords->y1 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
      for (auto& oh : handles_) {
        if (oh.drawingId == drawingId && oh.pointIndex == 2) {
          oh.x = (coords->x0 + coords->x1) * 0.5;
          oh.y = (coords->y0 + coords->y1) * 0.5;
        }
      }
    } else if (pi == 2) {
      // Move entire drawing (midpoint drag).
      coords->x0 += dx;
      coords->y0 += dy;
      coords->x1 += dx;
      coords->y1 += dy;
      // Update all handles.
      for (auto& oh : handles_) {
        if (oh.drawingId == drawingId) {
          oh.x += dx;
          oh.y += dy;
        }
      }
    }
  } else if (type == 2) { // HorizontalLevel
    // Only y changes.
    coords->y0 = newDataY;
    h->x = newDataX;
    h->y = newDataY;
  } else if (type == 3) { // VerticalLine
    // Only x changes.
    coords->x0 = newDataX;
    h->x = newDataX;
    h->y = newDataY;
  } else if (type == 4) { // Rectangle
    if (pi == 8) {
      // Center handle: move entire rectangle.
      coords->x0 += dx;
      coords->y0 += dy;
      coords->x1 += dx;
      coords->y1 += dy;
      for (auto& oh : handles_) {
        if (oh.drawingId == drawingId) {
          oh.x += dx;
          oh.y += dy;
        }
      }
    } else if (pi == 0) {
      // Top-left corner.
      coords->x0 = newDataX;
      coords->y0 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 1) {
      // Top-right corner.
      coords->x1 = newDataX;
      coords->y0 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 2) {
      // Bottom-right corner.
      coords->x1 = newDataX;
      coords->y1 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 3) {
      // Bottom-left corner.
      coords->x0 = newDataX;
      coords->y1 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 4) {
      // Top edge midpoint: constrain to Y axis only.
      coords->y0 = newDataY;
      h->y = newDataY;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 5) {
      // Right edge midpoint: constrain to X axis only.
      coords->x1 = newDataX;
      h->x = newDataX;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 6) {
      // Bottom edge midpoint: constrain to Y axis only.
      coords->y1 = newDataY;
      h->y = newDataY;
      updateRectHandles_(drawingId, *coords);
    } else if (pi == 7) {
      // Left edge midpoint: constrain to X axis only.
      coords->x0 = newDataX;
      h->x = newDataX;
      updateRectHandles_(drawingId, *coords);
    }
  } else if (type == 5) { // FibRetracement
    if (pi == 0) {
      coords->x0 = newDataX;
      coords->y0 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
    } else if (pi == 1) {
      coords->x1 = newDataX;
      coords->y1 = newDataY;
      h->x = newDataX;
      h->y = newDataY;
    }
  }

  return drawingId;
}

void HandleSet::endDrag(std::uint32_t handleId) {
  Handle* h = findHandle(handleId);
  if (h) {
    h->dragging = false;
  }
}

HandleSet::ModifiedCoords HandleSet::getModifiedCoords(std::uint32_t drawingId) const {
  const DrawingCoords* coords = findCoords(drawingId);
  if (!coords) return {0, 0, 0, 0};
  return {coords->x0, coords->y0, coords->x1, coords->y1};
}

const Handle* HandleSet::getHandle(std::uint32_t id) const {
  return findHandle(id);
}

void HandleSet::clear() {
  handles_.clear();
  origCoords_.clear();
  nextId_ = 1;
}

} // namespace dc
