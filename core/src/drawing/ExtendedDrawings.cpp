#include "dc/drawing/ExtendedDrawings.hpp"
#include <algorithm>

namespace dc {

void ExtDrawingInteraction::begin(ExtDrawingType type) {
  active_ = true;
  activeType_ = type;
  tempX_.clear();
  tempY_.clear();
  previewX_ = 0;
  previewY_ = 0;
}

void ExtDrawingInteraction::cancel() {
  active_ = false;
  tempX_.clear();
  tempY_.clear();
}

std::uint32_t ExtDrawingInteraction::onClick(double dataX, double dataY) {
  if (!active_) return 0;

  tempX_.push_back(dataX);
  tempY_.push_back(dataY);

  int needed = requiredClicks(activeType_);

  // Fixed-point types: complete when requiredClicks points are collected
  if (needed > 0 && static_cast<int>(tempX_.size()) >= needed) {
    return finalize();
  }

  // Variable-point types (needed == 0): keep accumulating, finalize on double-click
  return 0;
}

std::uint32_t ExtDrawingInteraction::onDoubleClick(double dataX, double dataY) {
  if (!active_) return 0;

  int needed = requiredClicks(activeType_);

  // Double-click only finalizes variable-point types (Polygon, Polyline)
  if (needed != 0) return 0;

  // Add the final point
  tempX_.push_back(dataX);
  tempY_.push_back(dataY);

  // Need at least 2 points for a variable-point drawing
  if (tempX_.size() < 2) return 0;

  return finalize();
}

bool ExtDrawingInteraction::isActive() const {
  return active_;
}

ExtDrawingType ExtDrawingInteraction::activeType() const {
  return activeType_;
}

std::size_t ExtDrawingInteraction::currentPointCount() const {
  return tempX_.size();
}

void ExtDrawingInteraction::updatePreview(double dataX, double dataY) {
  previewX_ = dataX;
  previewY_ = dataY;
}

double ExtDrawingInteraction::previewX() const {
  return previewX_;
}

double ExtDrawingInteraction::previewY() const {
  return previewY_;
}

const std::vector<ExtDrawing>& ExtDrawingInteraction::drawings() const {
  return drawings_;
}

const ExtDrawing* ExtDrawingInteraction::get(std::uint32_t id) const {
  for (const auto& d : drawings_) {
    if (d.id == id) return &d;
  }
  return nullptr;
}

void ExtDrawingInteraction::remove(std::uint32_t id) {
  drawings_.erase(
    std::remove_if(drawings_.begin(), drawings_.end(),
      [id](const ExtDrawing& d) { return d.id == id; }),
    drawings_.end());
}

void ExtDrawingInteraction::clear() {
  drawings_.clear();
}

std::uint32_t ExtDrawingInteraction::finalize() {
  ExtDrawing d;
  d.id = nextId_++;
  d.type = activeType_;
  d.pointsX = std::move(tempX_);
  d.pointsY = std::move(tempY_);

  std::uint32_t id = d.id;
  drawings_.push_back(std::move(d));

  active_ = false;
  tempX_.clear();
  tempY_.clear();

  return id;
}

} // namespace dc
