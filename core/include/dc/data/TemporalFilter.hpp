#pragma once
#include <cstddef>

namespace dc {

class TemporalFilter {
public:
  // Enable/disable replay mode
  void setEnabled(bool enabled);
  bool enabled() const;

  // Set the replay cursor (timestamp in seconds, e.g. unix epoch)
  void setCursor(double timestamp);
  double cursor() const;

  // Step controls
  void stepForward(double barInterval);   // advance by one bar
  void stepBackward(double barInterval);  // go back one bar
  void jumpTo(double timestamp);

  // Set playback speed (bars per second, 0 = paused)
  void setPlaybackSpeed(double barsPerSecond);
  double playbackSpeed() const;
  bool isPlaying() const;
  void play();
  void pause();

  // Advance playback by deltaTime seconds (call from frame loop)
  // Returns true if cursor changed
  bool tick(double deltaTimeSeconds, double barInterval);

  // Filter query: given an array of timestamps, return how many are visible
  // (i.e., <= cursor). Assumes timestamps are sorted ascending.
  std::size_t visibleCount(const double* timestamps, std::size_t count) const;

  // Convenience: get the visible vertex count for a buffer
  // given that each vertex corresponds to a timestamp at stride
  std::size_t visibleVertexCount(const double* timestamps, std::size_t totalVertices,
                                  std::size_t timestampStride = 1) const;

  // Range query
  double rangeStart() const;  // earliest possible timestamp
  double rangeEnd() const;    // latest possible timestamp (full data extent)
  void setRange(double start, double end);

  // Progress (0.0 to 1.0)
  double progress() const;

private:
  bool enabled_{false};
  double cursor_{0};
  double rangeStart_{0}, rangeEnd_{0};
  double playbackSpeed_{1.0};  // bars per second
  bool playing_{false};
};

} // namespace dc
