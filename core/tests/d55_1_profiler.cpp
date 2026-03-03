// D55.1 — FrameProfiler: section timing, stats storage
#include "dc/debug/FrameProfiler.hpp"

#include <cstdio>
#include <thread>
#include <chrono>

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
  std::printf("=== D55.1 FrameProfiler Section Timing ===\n");

  // Test 1: Fresh profiler has zero frame count
  {
    dc::FrameProfiler profiler;
    check(profiler.frameCount() == 0, "initial frameCount is 0");
  }

  // Test 2: Profile a section, verify durationUs > 0
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();
    profiler.beginSection(dc::ProfileSection::SceneWalk);

    // Burn some time
    volatile int sink = 0;
    for (int i = 0; i < 100000; ++i) { sink += i; }
    (void)sink;

    profiler.endSection(dc::ProfileSection::SceneWalk);
    profiler.endFrame();

    check(profiler.frameCount() == 1, "frameCount after one frame is 1");

    const auto& stats = profiler.lastFrame();
    check(stats.durationUs[static_cast<int>(dc::ProfileSection::SceneWalk)] > 0.0,
          "SceneWalk durationUs > 0");
    check(stats.durationUs[static_cast<int>(dc::ProfileSection::Total)] > 0.0,
          "Total durationUs > 0");
  }

  // Test 3: Total time >= section time
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();
    profiler.beginSection(dc::ProfileSection::DrawCalls);

    volatile int sink = 0;
    for (int i = 0; i < 50000; ++i) { sink += i; }
    (void)sink;

    profiler.endSection(dc::ProfileSection::DrawCalls);
    profiler.endFrame();

    const auto& s = profiler.lastFrame();
    double drawUs = s.durationUs[static_cast<int>(dc::ProfileSection::DrawCalls)];
    double totalUs = s.durationUs[static_cast<int>(dc::ProfileSection::Total)];
    check(totalUs >= drawUs, "total >= DrawCalls section");
  }

  // Test 4: Multiple sections in one frame
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();

    profiler.beginSection(dc::ProfileSection::DataDrain);
    volatile int sink = 0;
    for (int i = 0; i < 10000; ++i) { sink += i; }
    (void)sink;
    profiler.endSection(dc::ProfileSection::DataDrain);

    profiler.beginSection(dc::ProfileSection::BufferUpload);
    for (int i = 0; i < 10000; ++i) { sink += i; }
    (void)sink;
    profiler.endSection(dc::ProfileSection::BufferUpload);

    profiler.endFrame();

    const auto& s = profiler.lastFrame();
    check(s.durationUs[static_cast<int>(dc::ProfileSection::DataDrain)] > 0.0,
          "DataDrain > 0");
    check(s.durationUs[static_cast<int>(dc::ProfileSection::BufferUpload)] > 0.0,
          "BufferUpload > 0");
  }

  // Test 5: setDrawCallCount / setVertexCount / setBufferUploadBytes
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();
    profiler.setDrawCallCount(42);
    profiler.setVertexCount(12345);
    profiler.setBufferUploadBytes(1024 * 1024);
    profiler.endFrame();

    const auto& s = profiler.lastFrame();
    check(s.drawCallCount == 42, "drawCallCount = 42");
    check(s.vertexCount == 12345, "vertexCount = 12345");
    check(s.bufferUploadBytes == 1024 * 1024, "bufferUploadBytes = 1MB");
  }

  // Test 6: beginFrame resets current stats
  {
    dc::FrameProfiler profiler;

    profiler.beginFrame();
    profiler.setDrawCallCount(99);
    profiler.endFrame();
    check(profiler.lastFrame().drawCallCount == 99, "first frame drawCallCount=99");

    profiler.beginFrame();
    // Don't set draw call count this frame
    profiler.endFrame();
    check(profiler.lastFrame().drawCallCount == 0, "second frame drawCallCount=0 (reset)");
  }

  // Test 7: Default history size
  {
    dc::FrameProfiler profiler;  // default historySize = 120
    // No crash, just verify construction
    check(profiler.frameCount() == 0, "default constructed profiler has 0 frames");
  }

  // Test 8: Unused sections stay zero
  {
    dc::FrameProfiler profiler;
    profiler.beginFrame();
    profiler.beginSection(dc::ProfileSection::DrawCalls);
    volatile int sink = 0;
    for (int i = 0; i < 10000; ++i) { sink += i; }
    (void)sink;
    profiler.endSection(dc::ProfileSection::DrawCalls);
    profiler.endFrame();

    const auto& s = profiler.lastFrame();
    check(s.durationUs[static_cast<int>(dc::ProfileSection::TransformSync)] == 0.0,
          "unused TransformSync section is 0");
  }

  std::printf("=== D55.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
