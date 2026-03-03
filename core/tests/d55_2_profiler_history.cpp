// D55.2 — FrameProfiler: ring buffer averaging, ScopedProfile RAII
#include "dc/debug/FrameProfiler.hpp"

#include <cstdio>
#include <cmath>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D55.2 FrameProfiler History & ScopedProfile ===\n");

  // Test 1: Fill 5 frames, verify averageStats
  {
    dc::FrameProfiler profiler(10);

    for (int i = 1; i <= 5; ++i) {
      profiler.beginFrame();
      profiler.setDrawCallCount(static_cast<std::uint32_t>(i * 10));
      profiler.setVertexCount(static_cast<std::uint32_t>(i * 100));
      profiler.setBufferUploadBytes(static_cast<std::uint64_t>(i * 1000));
      profiler.endFrame();
    }

    check(profiler.frameCount() == 5, "frameCount == 5");

    auto avg = profiler.averageStats();
    // drawCallCount: (10+20+30+40+50)/5 = 30
    check(avg.drawCallCount == 30, "average drawCallCount = 30");
    // vertexCount: (100+200+300+400+500)/5 = 300
    check(avg.vertexCount == 300, "average vertexCount = 300");
    // bufferUploadBytes: (1000+2000+3000+4000+5000)/5 = 3000
    check(avg.bufferUploadBytes == 3000, "average bufferUploadBytes = 3000");
  }

  // Test 2: Ring buffer eviction — history capped at historySize
  {
    dc::FrameProfiler profiler(3);  // capacity = 3

    for (int i = 1; i <= 5; ++i) {
      profiler.beginFrame();
      profiler.setDrawCallCount(static_cast<std::uint32_t>(i));
      profiler.endFrame();
    }

    // After 5 frames with capacity 3, should have frames 3,4,5
    check(profiler.frameCount() == 3, "ring buffer capped at 3");

    auto avg = profiler.averageStats();
    // (3+4+5)/3 = 4
    check(avg.drawCallCount == 4, "average of last 3 frames drawCallCount = 4");

    // Last frame should be the 5th
    check(profiler.lastFrame().drawCallCount == 5, "lastFrame is most recent (5)");
  }

  // Test 3: ScopedProfile RAII auto begin/end
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();

    {
      dc::ScopedProfile sp(profiler, dc::ProfileSection::SceneWalk);
      volatile int sink = 0;
      for (int i = 0; i < 100000; ++i) { sink += i; }
      (void)sink;
    } // ScopedProfile destructor calls endSection

    profiler.endFrame();

    const auto& s = profiler.lastFrame();
    check(s.durationUs[static_cast<int>(dc::ProfileSection::SceneWalk)] > 0.0,
          "ScopedProfile recorded SceneWalk time > 0");
  }

  // Test 4: Multiple scoped profiles in sequence
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();

    {
      dc::ScopedProfile sp(profiler, dc::ProfileSection::DataDrain);
      volatile int sink = 0;
      for (int i = 0; i < 50000; ++i) { sink += i; }
      (void)sink;
    }
    {
      dc::ScopedProfile sp(profiler, dc::ProfileSection::DrawCalls);
      volatile int sink = 0;
      for (int i = 0; i < 50000; ++i) { sink += i; }
      (void)sink;
    }

    profiler.endFrame();

    const auto& s = profiler.lastFrame();
    check(s.durationUs[static_cast<int>(dc::ProfileSection::DataDrain)] > 0.0,
          "scoped DataDrain > 0");
    check(s.durationUs[static_cast<int>(dc::ProfileSection::DrawCalls)] > 0.0,
          "scoped DrawCalls > 0");
  }

  // Test 5: averageStats with empty profiler returns zeroes
  {
    dc::FrameProfiler profiler;
    auto avg = profiler.averageStats();
    check(avg.drawCallCount == 0, "empty profiler average drawCallCount = 0");
    check(avg.vertexCount == 0, "empty profiler average vertexCount = 0");
    for (int i = 0; i < 6; ++i) {
      check(avg.durationUs[i] == 0.0, "empty profiler average durationUs = 0");
    }
  }

  // Test 6: Average timing is positive after timed frames
  {
    dc::FrameProfiler profiler(5);
    for (int i = 0; i < 5; ++i) {
      profiler.beginFrame();
      profiler.beginSection(dc::ProfileSection::BufferUpload);
      volatile int sink = 0;
      for (int j = 0; j < 10000; ++j) { sink += j; }
      (void)sink;
      profiler.endSection(dc::ProfileSection::BufferUpload);
      profiler.endFrame();
    }

    auto avg = profiler.averageStats();
    check(avg.durationUs[static_cast<int>(dc::ProfileSection::BufferUpload)] > 0.0,
          "average BufferUpload timing > 0");
    check(avg.durationUs[static_cast<int>(dc::ProfileSection::Total)] > 0.0,
          "average Total timing > 0");
  }

  // Test 7: History size 1 — only latest frame kept
  {
    dc::FrameProfiler profiler(1);

    profiler.beginFrame();
    profiler.setDrawCallCount(10);
    profiler.endFrame();
    check(profiler.lastFrame().drawCallCount == 10, "history(1): first frame dc=10");

    profiler.beginFrame();
    profiler.setDrawCallCount(20);
    profiler.endFrame();
    check(profiler.lastFrame().drawCallCount == 20, "history(1): second frame dc=20");
    check(profiler.frameCount() == 1, "history(1): only 1 frame retained");
  }

  std::printf("=== D55.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
