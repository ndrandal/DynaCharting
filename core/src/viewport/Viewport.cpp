#include "dc/viewport/Viewport.hpp"

namespace dc {

void Viewport::setDataRange(double xMin, double xMax, double yMin, double yMax) {
  data_.xMin = xMin;
  data_.xMax = xMax;
  data_.yMin = yMin;
  data_.yMax = yMax;
}

void Viewport::setClipRegion(const PaneRegion& region) {
  clip_ = region;
}

void Viewport::setPixelViewport(int fbWidth, int fbHeight) {
  fbW_ = fbWidth;
  fbH_ = fbHeight;
}

void Viewport::pixelToClip(double px, double py, double& cx, double& cy) const {
  cx = px / static_cast<double>(fbW_) * 2.0 - 1.0;
  cy = 1.0 - py / static_cast<double>(fbH_) * 2.0; // Y flipped
}

void Viewport::clipToData(double cx, double cy, double& dx, double& dy) const {
  double clipW = static_cast<double>(clip_.clipXMax) - static_cast<double>(clip_.clipXMin);
  double clipH = static_cast<double>(clip_.clipYMax) - static_cast<double>(clip_.clipYMin);

  double tx = (cx - static_cast<double>(clip_.clipXMin)) / clipW;
  double ty = (cy - static_cast<double>(clip_.clipYMin)) / clipH;

  dx = data_.xMin + tx * (data_.xMax - data_.xMin);
  dy = data_.yMin + ty * (data_.yMax - data_.yMin);
}

void Viewport::dataToClip(double dx, double dy, double& cx, double& cy) const {
  double tx = (dx - data_.xMin) / (data_.xMax - data_.xMin);
  double ty = (dy - data_.yMin) / (data_.yMax - data_.yMin);

  cx = static_cast<double>(clip_.clipXMin) + tx * (static_cast<double>(clip_.clipXMax) - static_cast<double>(clip_.clipXMin));
  cy = static_cast<double>(clip_.clipYMin) + ty * (static_cast<double>(clip_.clipYMax) - static_cast<double>(clip_.clipYMin));
}

void Viewport::pixelToData(double px, double py, double& dx, double& dy) const {
  double cx, cy;
  pixelToClip(px, py, cx, cy);
  clipToData(cx, cy, dx, dy);
}

void Viewport::pan(double dxPixels, double dyPixels) {
  // Convert pixel delta to clip delta
  double clipDx = dxPixels / static_cast<double>(fbW_) * 2.0;
  double clipDy = -dyPixels / static_cast<double>(fbH_) * 2.0; // Y flipped

  // Convert clip delta to data delta
  double clipW = static_cast<double>(clip_.clipXMax) - static_cast<double>(clip_.clipXMin);
  double clipH = static_cast<double>(clip_.clipYMax) - static_cast<double>(clip_.clipYMin);

  double dataDx = clipDx / clipW * (data_.xMax - data_.xMin);
  double dataDy = clipDy / clipH * (data_.yMax - data_.yMin);

  // Pan shifts data range in opposite direction (dragging right shows earlier data)
  data_.xMin -= dataDx;
  data_.xMax -= dataDx;
  data_.yMin -= dataDy;
  data_.yMax -= dataDy;
}

void Viewport::zoom(double factor, double pivotPx, double pivotPy) {
  // Convert pivot to data coords
  double pivotDx, pivotDy;
  pixelToData(pivotPx, pivotPy, pivotDx, pivotDy);

  // Scale range around pivot
  double scale = 1.0 / (1.0 + factor); // factor > 0 = zoom in = smaller range

  data_.xMin = pivotDx + (data_.xMin - pivotDx) * scale;
  data_.xMax = pivotDx + (data_.xMax - pivotDx) * scale;
  data_.yMin = pivotDy + (data_.yMin - pivotDy) * scale;
  data_.yMax = pivotDy + (data_.yMax - pivotDy) * scale;
}

TransformParams Viewport::computeTransformParams() const {
  double clipW = static_cast<double>(clip_.clipXMax) - static_cast<double>(clip_.clipXMin);
  double clipH = static_cast<double>(clip_.clipYMax) - static_cast<double>(clip_.clipYMin);
  double dataW = data_.xMax - data_.xMin;
  double dataH = data_.yMax - data_.yMin;

  float sx = static_cast<float>(clipW / dataW);
  float sy = static_cast<float>(clipH / dataH);
  float tx = static_cast<float>(static_cast<double>(clip_.clipXMin) - data_.xMin * (clipW / dataW));
  float ty = static_cast<float>(static_cast<double>(clip_.clipYMin) - data_.yMin * (clipH / dataH));

  return TransformParams{tx, ty, sx, sy};
}

// ---- Zoom metrics (D8.1) ----

double Viewport::visibleDataWidth() const {
  return data_.xMax - data_.xMin;
}

double Viewport::pixelsPerDataUnitX() const {
  double dataW = data_.xMax - data_.xMin;
  if (dataW <= 0.0) return 0.0;
  double clipW = static_cast<double>(clip_.clipXMax) - static_cast<double>(clip_.clipXMin);
  double pixelW = clipW / 2.0 * static_cast<double>(fbW_);
  return pixelW / dataW;
}

double Viewport::pixelsPerDataUnitY() const {
  double dataH = data_.yMax - data_.yMin;
  if (dataH <= 0.0) return 0.0;
  double clipH = static_cast<double>(clip_.clipYMax) - static_cast<double>(clip_.clipYMin);
  double pixelH = clipH / 2.0 * static_cast<double>(fbH_);
  return pixelH / dataH;
}

bool Viewport::containsPixel(double px, double py) const {
  double cx, cy;
  pixelToClip(px, py, cx, cy);
  return cx >= static_cast<double>(clip_.clipXMin) &&
         cx <= static_cast<double>(clip_.clipXMax) &&
         cy >= static_cast<double>(clip_.clipYMin) &&
         cy <= static_cast<double>(clip_.clipYMax);
}

} // namespace dc
