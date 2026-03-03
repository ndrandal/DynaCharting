// D70.2 — TemporalFilter: stepForward/stepBackward, play/pause, tick, progress, range clamping
#include "dc/data/TemporalFilter.hpp"

#include <cmath>
#include <cstdio>

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

static bool approx(double a, double b, double tol = 1e-9) {
  return std::fabs(a - b) < tol;
}

int main() {
  std::printf("=== D70.2 TemporalFilter Step/Playback Tests ===\n");

  // Test 1: stepForward advances cursor by barInterval
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(10.0);

    tf.stepForward(5.0);
    check(tf.cursor() == 15.0, "stepForward(5): 10 -> 15");

    tf.stepForward(5.0);
    check(tf.cursor() == 20.0, "stepForward(5): 15 -> 20");
  }

  // Test 2: stepBackward decreases cursor by barInterval
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(20.0);

    tf.stepBackward(5.0);
    check(tf.cursor() == 15.0, "stepBackward(5): 20 -> 15");

    tf.stepBackward(5.0);
    check(tf.cursor() == 10.0, "stepBackward(5): 15 -> 10");
  }

  // Test 3: stepForward clamps at rangeEnd
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(98.0);

    tf.stepForward(5.0);
    check(tf.cursor() == 100.0, "stepForward clamps at rangeEnd");
  }

  // Test 4: stepBackward clamps at rangeStart
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(3.0);

    tf.stepBackward(5.0);
    check(tf.cursor() == 0.0, "stepBackward clamps at rangeStart");
  }

  // Test 5: jumpTo sets cursor directly (with clamping)
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);

    tf.jumpTo(50.0);
    check(tf.cursor() == 50.0, "jumpTo(50)");

    tf.jumpTo(-10.0);
    check(tf.cursor() == 0.0, "jumpTo(-10) clamped to 0");

    tf.jumpTo(200.0);
    check(tf.cursor() == 100.0, "jumpTo(200) clamped to 100");
  }

  // Test 6: play/pause state
  {
    dc::TemporalFilter tf;
    check(!tf.isPlaying(), "default: not playing");

    tf.play();
    check(tf.isPlaying(), "play() -> playing");

    tf.pause();
    check(!tf.isPlaying(), "pause() -> not playing");
  }

  // Test 7: play does nothing if playbackSpeed <= 0
  {
    dc::TemporalFilter tf;
    tf.setPlaybackSpeed(0.0);
    tf.play();
    check(!tf.isPlaying(), "play with speed=0 -> not playing");

    tf.setPlaybackSpeed(-1.0);
    tf.play();
    check(!tf.isPlaying(), "play with speed<0 -> not playing");
  }

  // Test 8: setPlaybackSpeed(0) auto-pauses
  {
    dc::TemporalFilter tf;
    tf.setPlaybackSpeed(2.0);
    tf.play();
    check(tf.isPlaying(), "playing at speed 2");

    tf.setPlaybackSpeed(0.0);
    check(!tf.isPlaying(), "setPlaybackSpeed(0) auto-pauses");
  }

  // Test 9: tick advances cursor when playing
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 1000.0);
    tf.setCursor(100.0);
    tf.setPlaybackSpeed(2.0); // 2 bars per second
    tf.play();

    double barInterval = 10.0;
    double dt = 0.5; // 0.5 seconds elapsed
    // Expected advance: 2.0 * 10.0 * 0.5 = 10.0
    bool changed = tf.tick(dt, barInterval);
    check(changed, "tick returns true when cursor changes");
    check(approx(tf.cursor(), 110.0), "tick advances: 100 + 2*10*0.5 = 110");
  }

  // Test 10: tick does not advance when paused
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 1000.0);
    tf.setCursor(100.0);
    tf.setPlaybackSpeed(2.0);
    // Not playing

    bool changed = tf.tick(0.5, 10.0);
    check(!changed, "tick returns false when paused");
    check(tf.cursor() == 100.0, "cursor unchanged when paused");
  }

  // Test 11: tick auto-pauses at rangeEnd
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(95.0);
    tf.setPlaybackSpeed(1.0);
    tf.play();

    // Advance by 10.0 (1.0 * 10.0 * 1.0 = 10.0)
    bool changed = tf.tick(1.0, 10.0);
    check(changed, "tick returns true when reaching end");
    check(tf.cursor() == 100.0, "cursor clamped to rangeEnd");
    check(!tf.isPlaying(), "auto-paused at rangeEnd");
  }

  // Test 12: tick with zero deltaTime
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(50.0);
    tf.setPlaybackSpeed(1.0);
    tf.play();

    bool changed = tf.tick(0.0, 10.0);
    check(!changed, "tick with dt=0 returns false");
    check(tf.cursor() == 50.0, "cursor unchanged with dt=0");
  }

  // Test 13: tick with zero barInterval
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);
    tf.setCursor(50.0);
    tf.setPlaybackSpeed(1.0);
    tf.play();

    bool changed = tf.tick(0.5, 0.0);
    check(!changed, "tick with barInterval=0 returns false");
  }

  // Test 14: progress calculation
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 100.0);

    tf.setCursor(0.0);
    check(approx(tf.progress(), 0.0), "progress at rangeStart = 0.0");

    tf.setCursor(50.0);
    check(approx(tf.progress(), 0.5), "progress at midpoint = 0.5");

    tf.setCursor(100.0);
    check(approx(tf.progress(), 1.0), "progress at rangeEnd = 1.0");

    tf.setCursor(25.0);
    check(approx(tf.progress(), 0.25), "progress at quarter = 0.25");
  }

  // Test 15: progress with zero-length range
  {
    dc::TemporalFilter tf;
    tf.setRange(50.0, 50.0);
    tf.setCursor(50.0);
    check(tf.progress() == 0.0, "zero-length range: progress = 0.0");
  }

  // Test 16: progress with non-zero rangeStart
  {
    dc::TemporalFilter tf;
    tf.setRange(1000.0, 2000.0);

    tf.setCursor(1500.0);
    check(approx(tf.progress(), 0.5), "offset range: progress at midpoint = 0.5");

    tf.setCursor(1000.0);
    check(approx(tf.progress(), 0.0), "offset range: progress at start = 0.0");

    tf.setCursor(2000.0);
    check(approx(tf.progress(), 1.0), "offset range: progress at end = 1.0");
  }

  // Test 17: Multiple ticks accumulate
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 1000.0);
    tf.setCursor(0.0);
    tf.setPlaybackSpeed(1.0); // 1 bar per second
    tf.play();

    double barInterval = 60.0; // 60-second bars
    // 10 ticks of 0.1s each = 1 second total
    for (int i = 0; i < 10; ++i) {
      tf.tick(0.1, barInterval);
    }
    // Expected: 1.0 * 60.0 * (10 * 0.1) = 60.0
    check(approx(tf.cursor(), 60.0, 1e-6), "10 ticks of 0.1s = 60.0 advance");
  }

  // Test 18: stepForward + visibleCount integration
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    double ts[] = {10.0, 20.0, 30.0, 40.0, 50.0};
    tf.setRange(0.0, 100.0);
    tf.setCursor(0.0);

    check(tf.visibleCount(ts, 5) == 0, "cursor=0: no bars visible");

    tf.stepForward(10.0);
    check(tf.visibleCount(ts, 5) == 1, "step to 10: 1 bar visible");

    tf.stepForward(10.0);
    check(tf.visibleCount(ts, 5) == 2, "step to 20: 2 bars visible");

    tf.stepForward(10.0);
    check(tf.visibleCount(ts, 5) == 3, "step to 30: 3 bars visible");
  }

  // Test 19: playbackSpeed getter/setter
  {
    dc::TemporalFilter tf;
    check(tf.playbackSpeed() == 1.0, "default playbackSpeed = 1.0");

    tf.setPlaybackSpeed(4.0);
    check(tf.playbackSpeed() == 4.0, "playbackSpeed set to 4.0");

    tf.setPlaybackSpeed(0.5);
    check(tf.playbackSpeed() == 0.5, "playbackSpeed set to 0.5");
  }

  // Test 20: High playback speed
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 10000.0);
    tf.setCursor(0.0);
    tf.setPlaybackSpeed(100.0); // 100 bars/sec
    tf.play();

    double barInterval = 1.0;
    tf.tick(1.0, barInterval); // 100 * 1.0 * 1.0 = 100
    check(approx(tf.cursor(), 100.0), "high speed: 100 bars in 1 second");
  }

  std::printf("=== D70.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
