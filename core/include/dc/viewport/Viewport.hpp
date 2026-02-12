#pragma once
#include "dc/layout/PaneLayout.hpp"
#include "dc/scene/Types.hpp"

namespace dc {

struct DataRange {
  double xMin{0}, xMax{1}, yMin{0}, yMax{1};
};

class Viewport {
public:
  void setDataRange(double xMin, double xMax, double yMin, double yMax);
  void setClipRegion(const PaneRegion& region);
  void setPixelViewport(int fbWidth, int fbHeight);

  // Coordinate mapping
  void pixelToClip(double px, double py, double& cx, double& cy) const;
  void clipToData(double cx, double cy, double& dx, double& dy) const;
  void dataToClip(double dx, double dy, double& cx, double& cy) const;
  void pixelToData(double px, double py, double& dx, double& dy) const;

  // Pan/zoom
  void pan(double dxPixels, double dyPixels);
  void zoom(double factor, double pivotPx, double pivotPy);

  // Engine integration
  TransformParams computeTransformParams() const;
  bool containsPixel(double px, double py) const;

  // Zoom metrics (D8.1)
  double visibleDataWidth() const;
  double pixelsPerDataUnitX() const;
  double pixelsPerDataUnitY() const;

  const DataRange& dataRange() const { return data_; }
  const PaneRegion& clipRegion() const { return clip_; }
  int fbWidth() const { return fbW_; }
  int fbHeight() const { return fbH_; }

private:
  DataRange data_{0, 1, 0, 1};
  PaneRegion clip_{-1.0f, 1.0f, -1.0f, 1.0f};
  int fbW_{800};
  int fbH_{600};
};

} // namespace dc
