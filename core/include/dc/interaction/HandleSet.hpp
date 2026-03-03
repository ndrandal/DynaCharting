#pragma once
#include <cstdint>
#include <vector>

namespace dc {

// D66.1: Control-point handle system for drawing editing.
// After a drawing is placed, draggable handles appear on control points
// for reshaping (like TradingView).

struct Handle {
  std::uint32_t id{0};
  std::uint32_t drawingId{0};  // which drawing this handle belongs to
  std::uint8_t pointIndex{0};  // 0=start, 1=end, 2=midpoint/center, 3+ = corners/edges
  double x{0}, y{0};           // current position in data space
  double hitRadius{8.0};       // pixels for hit testing
  bool dragging{false};
};

class HandleSet {
public:
  // Create handles for a drawing based on its type.
  // drawingType maps to DrawingType enum values:
  //   1 = Trendline:       2 handles (endpoints) + 1 midpoint (move entire) = 3
  //   2 = HorizontalLevel: 1 handle (price level)
  //   3 = VerticalLine:    1 handle (x position)
  //   4 = Rectangle:       4 corners + 4 edge midpoints + 1 center = 9
  //   5 = FibRetracement:  2 handles (endpoints)
  void createForDrawing(std::uint32_t drawingId, std::uint8_t drawingType,
                        double x0, double y0, double x1, double y1);

  // Remove all handles for a drawing.
  void removeForDrawing(std::uint32_t drawingId);

  // Hit test: find handle at data position (returns handle id or 0).
  // pixelPerDataX/pixelPerDataY convert data-space distance to pixel distance
  // for radius comparison.
  std::uint32_t hitTest(double dataX, double dataY,
                        double pixelPerDataX, double pixelPerDataY) const;

  // Begin dragging a handle.
  void beginDrag(std::uint32_t handleId);

  // Update drag position -- returns the drawingId being modified.
  std::uint32_t updateDrag(std::uint32_t handleId, double newDataX, double newDataY);

  // End drag.
  void endDrag(std::uint32_t handleId);

  // Get modified coordinates for a drawing after handle drag.
  // Caller applies these back to DrawingStore.
  struct ModifiedCoords { double x0, y0, x1, y1; };
  ModifiedCoords getModifiedCoords(std::uint32_t drawingId) const;

  const std::vector<Handle>& handles() const { return handles_; }
  const Handle* getHandle(std::uint32_t id) const;
  void clear();

private:
  std::vector<Handle> handles_;
  std::uint32_t nextId_{1};

  // Original coordinates stored when handles are created, used to compute
  // modified coords during drag.
  struct DrawingCoords {
    std::uint32_t drawingId{0};
    std::uint8_t drawingType{0};
    double x0{0}, y0{0}, x1{0}, y1{0};
  };
  std::vector<DrawingCoords> origCoords_;

  Handle* findHandle(std::uint32_t id);
  const Handle* findHandle(std::uint32_t id) const;
  DrawingCoords* findCoords(std::uint32_t drawingId);
  const DrawingCoords* findCoords(std::uint32_t drawingId) const;

  // Recompute all rectangle handles (corners, edge midpoints, center)
  // from the current coords, except the handle being dragged.
  void updateRectHandles_(std::uint32_t drawingId, const DrawingCoords& coords);
};

} // namespace dc
