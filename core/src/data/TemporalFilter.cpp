#include "dc/data/TemporalFilter.hpp"
#include <algorithm>

namespace dc {

void TemporalFilter::setEnabled(bool enabled) {
  enabled_ = enabled;
}

bool TemporalFilter::enabled() const {
  return enabled_;
}

void TemporalFilter::setCursor(double timestamp) {
  cursor_ = std::clamp(timestamp, rangeStart_, rangeEnd_);
}

double TemporalFilter::cursor() const {
  return cursor_;
}

void TemporalFilter::stepForward(double barInterval) {
  setCursor(cursor_ + barInterval);
}

void TemporalFilter::stepBackward(double barInterval) {
  setCursor(cursor_ - barInterval);
}

void TemporalFilter::jumpTo(double timestamp) {
  setCursor(timestamp);
}

void TemporalFilter::setPlaybackSpeed(double barsPerSecond) {
  playbackSpeed_ = barsPerSecond;
  if (playbackSpeed_ <= 0.0) {
    playing_ = false;
  }
}

double TemporalFilter::playbackSpeed() const {
  return playbackSpeed_;
}

bool TemporalFilter::isPlaying() const {
  return playing_;
}

void TemporalFilter::play() {
  if (playbackSpeed_ > 0.0) {
    playing_ = true;
  }
}

void TemporalFilter::pause() {
  playing_ = false;
}

bool TemporalFilter::tick(double deltaTimeSeconds, double barInterval) {
  if (!playing_ || playbackSpeed_ <= 0.0 || barInterval <= 0.0) {
    return false;
  }

  double oldCursor = cursor_;
  double advance = playbackSpeed_ * barInterval * deltaTimeSeconds;
  setCursor(cursor_ + advance);

  // Auto-pause when reaching the end
  if (cursor_ >= rangeEnd_) {
    playing_ = false;
  }

  return cursor_ != oldCursor;
}

std::size_t TemporalFilter::visibleCount(const double* timestamps, std::size_t count) const {
  if (!enabled_ || count == 0 || timestamps == nullptr) {
    return count;
  }

  // Binary search: find first element > cursor
  // std::upper_bound returns iterator to first element greater than cursor
  const double* end = timestamps + count;
  const double* it = std::upper_bound(timestamps, end, cursor_);
  return static_cast<std::size_t>(it - timestamps);
}

std::size_t TemporalFilter::visibleVertexCount(const double* timestamps,
                                                std::size_t totalVertices,
                                                std::size_t timestampStride) const {
  if (!enabled_ || totalVertices == 0 || timestamps == nullptr || timestampStride == 0) {
    return totalVertices;
  }

  // Number of timestamp entries = totalVertices / timestampStride
  std::size_t tsCount = totalVertices / timestampStride;
  if (tsCount == 0) {
    return 0;
  }

  // Build a count of visible timestamps using binary search on strided data
  // Since timestamps may be strided, we need to manually binary search
  std::size_t lo = 0;
  std::size_t hi = tsCount;
  while (lo < hi) {
    std::size_t mid = lo + (hi - lo) / 2;
    if (timestamps[mid * timestampStride] <= cursor_) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  // lo = number of visible timestamps, convert back to vertex count
  return lo * timestampStride;
}

double TemporalFilter::rangeStart() const {
  return rangeStart_;
}

double TemporalFilter::rangeEnd() const {
  return rangeEnd_;
}

void TemporalFilter::setRange(double start, double end) {
  rangeStart_ = start;
  rangeEnd_ = (end >= start) ? end : start;
  // Re-clamp cursor to new range
  cursor_ = std::clamp(cursor_, rangeStart_, rangeEnd_);
}

double TemporalFilter::progress() const {
  double span = rangeEnd_ - rangeStart_;
  if (span <= 0.0) {
    return 0.0;
  }
  return (cursor_ - rangeStart_) / span;
}

} // namespace dc
