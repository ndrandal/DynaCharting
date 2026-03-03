#include "dc/debug/FrameProfiler.hpp"

namespace dc {

FrameProfiler::FrameProfiler(std::size_t historySize)
  : historySize_(historySize) {
  history_.reserve(historySize_);
}

void FrameProfiler::beginFrame() {
  current_ = ProfileStats{};
  frameStart_ = std::chrono::high_resolution_clock::now();
}

void FrameProfiler::beginSection(ProfileSection section) {
  sectionStart_[static_cast<int>(section)] = std::chrono::high_resolution_clock::now();
}

void FrameProfiler::endSection(ProfileSection section) {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
    now - sectionStart_[static_cast<int>(section)]).count();
  current_.durationUs[static_cast<int>(section)] = static_cast<double>(elapsed) / 1000.0;
}

void FrameProfiler::endFrame() {
  // Compute total frame time
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
    now - frameStart_).count();
  current_.durationUs[static_cast<int>(ProfileSection::Total)] = static_cast<double>(elapsed) / 1000.0;

  // Push to ring buffer
  if (history_.size() < historySize_) {
    history_.push_back(current_);
  } else {
    // Replace oldest entry (ring buffer)
    std::size_t idx = (history_.size() == 0) ? 0 : (frameCount() % historySize_);
    // We need a write index, but since we may overflow historySize_ many times,
    // we track via current size. Once full, we rotate.
    // Simple approach: shift left and append.
    history_.erase(history_.begin());
    history_.push_back(current_);
  }
}

const ProfileStats& FrameProfiler::lastFrame() const {
  static const ProfileStats empty{};
  if (history_.empty()) return empty;
  return history_.back();
}

ProfileStats FrameProfiler::averageStats() const {
  ProfileStats avg{};
  if (history_.empty()) return avg;

  for (const auto& s : history_) {
    for (int i = 0; i < 6; ++i) {
      avg.durationUs[i] += s.durationUs[i];
    }
    avg.drawCallCount += s.drawCallCount;
    avg.vertexCount += s.vertexCount;
    avg.bufferUploadBytes += s.bufferUploadBytes;
  }

  double n = static_cast<double>(history_.size());
  for (int i = 0; i < 6; ++i) {
    avg.durationUs[i] /= n;
  }
  avg.drawCallCount = static_cast<std::uint32_t>(avg.drawCallCount / n);
  avg.vertexCount = static_cast<std::uint32_t>(avg.vertexCount / n);
  avg.bufferUploadBytes = static_cast<std::uint64_t>(avg.bufferUploadBytes / n);

  return avg;
}

std::size_t FrameProfiler::frameCount() const {
  return history_.size();
}

void FrameProfiler::setDrawCallCount(std::uint32_t count) {
  current_.drawCallCount = count;
}

void FrameProfiler::setVertexCount(std::uint32_t count) {
  current_.vertexCount = count;
}

void FrameProfiler::setBufferUploadBytes(std::uint64_t bytes) {
  current_.bufferUploadBytes = bytes;
}

// ScopedProfile

ScopedProfile::ScopedProfile(FrameProfiler& profiler, ProfileSection section)
  : profiler_(profiler), section_(section) {
  profiler_.beginSection(section_);
}

ScopedProfile::~ScopedProfile() {
  profiler_.endSection(section_);
}

} // namespace dc
