#pragma once
#include "dc/ids/Id.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/scene/Scene.hpp"
#include <vector>

namespace dc {

class IngestProcessor;

struct BoxSelectRect {
  double x0{0}, y0{0}, x1{0}, y1{0};

  double minX() const { return x0 < x1 ? x0 : x1; }
  double maxX() const { return x0 > x1 ? x0 : x1; }
  double minY() const { return y0 < y1 ? y0 : y1; }
  double maxY() const { return y0 > y1 ? y0 : y1; }
};

struct BoxSelectResult {
  std::vector<SelectionKey> hits;
  BoxSelectRect rect;
};

class BoxSelection {
public:
  void begin(double dataX, double dataY);
  void update(double dataX, double dataY);
  BoxSelectResult finish(double dataX, double dataY,
                         const Scene& scene,
                         const IngestProcessor& ingest);
  void cancel();

  bool isActive() const { return active_; }
  BoxSelectRect currentRect() const { return rect_; }

private:
  bool active_{false};
  BoxSelectRect rect_;

  void hitTestDrawItem(const DrawItem& di, const Scene& scene,
                       const IngestProcessor& ingest,
                       const BoxSelectRect& normalized,
                       std::vector<SelectionKey>& hits);
};

} // namespace dc
