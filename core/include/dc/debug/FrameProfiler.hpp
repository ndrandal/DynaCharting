#pragma once
#include <chrono>
#include <cstdint>
#include <vector>

namespace dc {

enum class ProfileSection : std::uint8_t {
  DataDrain, TransformSync, BufferUpload, SceneWalk, DrawCalls, Total
};

struct ProfileStats {
  double durationUs[6] = {}; // indexed by ProfileSection
  std::uint32_t drawCallCount{0};
  std::uint32_t vertexCount{0};
  std::uint64_t bufferUploadBytes{0};
};

class FrameProfiler {
public:
  explicit FrameProfiler(std::size_t historySize = 120);

  void beginFrame();
  void beginSection(ProfileSection section);
  void endSection(ProfileSection section);
  void endFrame();

  const ProfileStats& lastFrame() const;
  ProfileStats averageStats() const;
  std::size_t frameCount() const;
  void setDrawCallCount(std::uint32_t count);
  void setVertexCount(std::uint32_t count);
  void setBufferUploadBytes(std::uint64_t bytes);

private:
  std::size_t historySize_;
  std::vector<ProfileStats> history_;
  ProfileStats current_;
  std::chrono::high_resolution_clock::time_point sectionStart_[6];
  std::chrono::high_resolution_clock::time_point frameStart_;
};

// RAII helper
class ScopedProfile {
public:
  ScopedProfile(FrameProfiler& profiler, ProfileSection section);
  ~ScopedProfile();
private:
  FrameProfiler& profiler_;
  ProfileSection section_;
};

} // namespace dc
