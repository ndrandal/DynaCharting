// D70.1 — TemporalFilter: enable/disable, setCursor, visibleCount with sorted timestamps
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

int main() {
  std::printf("=== D70.1 TemporalFilter Replay Tests ===\n");

  // Test 1: Default state — disabled, cursor at 0
  {
    dc::TemporalFilter tf;
    check(!tf.enabled(), "default: not enabled");
    check(tf.cursor() == 0.0, "default: cursor = 0");
    check(!tf.isPlaying(), "default: not playing");
    check(tf.playbackSpeed() == 1.0, "default: playbackSpeed = 1.0");
  }

  // Test 2: Enable/disable toggle
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    check(tf.enabled(), "setEnabled(true)");
    tf.setEnabled(false);
    check(!tf.enabled(), "setEnabled(false)");
  }

  // Test 3: setCursor clamps to range
  {
    dc::TemporalFilter tf;
    tf.setRange(100.0, 200.0);
    tf.setCursor(150.0);
    check(tf.cursor() == 150.0, "setCursor within range");

    tf.setCursor(50.0);
    check(tf.cursor() == 100.0, "setCursor below range clamped to rangeStart");

    tf.setCursor(300.0);
    check(tf.cursor() == 200.0, "setCursor above range clamped to rangeEnd");
  }

  // Test 4: visibleCount when disabled returns full count
  {
    dc::TemporalFilter tf;
    double ts[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    tf.setRange(0.0, 10.0);
    tf.setCursor(3.0);
    // Disabled: should return all
    check(tf.visibleCount(ts, 5) == 5, "disabled: visibleCount returns all");
  }

  // Test 5: visibleCount when enabled filters by cursor
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    tf.setRange(0.0, 10.0);
    double ts[] = {1.0, 2.0, 3.0, 4.0, 5.0};

    tf.setCursor(3.0);
    check(tf.visibleCount(ts, 5) == 3, "cursor=3.0: 3 visible (1,2,3)");

    tf.setCursor(0.5);
    check(tf.visibleCount(ts, 5) == 0, "cursor=0.5: 0 visible (all > 0.5)");

    tf.setCursor(5.0);
    check(tf.visibleCount(ts, 5) == 5, "cursor=5.0: all 5 visible");

    tf.setCursor(5.5);
    check(tf.visibleCount(ts, 5) == 5, "cursor=5.5: all 5 visible (beyond last)");

    tf.setCursor(3.5);
    check(tf.visibleCount(ts, 5) == 3, "cursor=3.5: 3 visible (1,2,3 <= 3.5)");
  }

  // Test 6: visibleCount with exact boundary values
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    tf.setRange(0.0, 100.0);
    double ts[] = {10.0, 20.0, 30.0, 40.0, 50.0};

    tf.setCursor(10.0);
    check(tf.visibleCount(ts, 5) == 1, "cursor=10.0: exactly 1 visible");

    tf.setCursor(20.0);
    check(tf.visibleCount(ts, 5) == 2, "cursor=20.0: exactly 2 visible");

    tf.setCursor(9.999);
    check(tf.visibleCount(ts, 5) == 0, "cursor=9.999: 0 visible (just below first)");
  }

  // Test 7: visibleCount with empty array
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    tf.setRange(0.0, 10.0);
    tf.setCursor(5.0);
    check(tf.visibleCount(nullptr, 0) == 0, "null timestamps: 0 visible");
  }

  // Test 8: visibleCount with single element
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    tf.setRange(0.0, 10.0);
    double ts[] = {5.0};

    tf.setCursor(5.0);
    check(tf.visibleCount(ts, 1) == 1, "single element at cursor: 1 visible");

    tf.setCursor(4.9);
    check(tf.visibleCount(ts, 1) == 0, "single element above cursor: 0 visible");
  }

  // Test 9: visibleVertexCount with stride
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    tf.setRange(0.0, 100.0);
    // Simulate interleaved data: [ts, y, ts, y, ts, y, ...]
    // 3 data points, stride=2 means timestamps at indices 0, 2, 4
    double data[] = {10.0, 0.5, 20.0, 0.6, 30.0, 0.7};

    tf.setCursor(25.0);
    std::size_t visible = tf.visibleVertexCount(data, 6, 2);
    // 2 timestamps visible (10.0, 20.0), * stride 2 = 4 vertices
    check(visible == 4, "stride=2, cursor=25: 4 vertices visible");

    tf.setCursor(30.0);
    visible = tf.visibleVertexCount(data, 6, 2);
    check(visible == 6, "stride=2, cursor=30: all 6 vertices visible");

    tf.setCursor(5.0);
    visible = tf.visibleVertexCount(data, 6, 2);
    check(visible == 0, "stride=2, cursor=5: 0 vertices visible");
  }

  // Test 10: visibleVertexCount with stride=1 (default)
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    tf.setRange(0.0, 100.0);
    double ts[] = {10.0, 20.0, 30.0, 40.0};

    tf.setCursor(25.0);
    std::size_t visible = tf.visibleVertexCount(ts, 4, 1);
    check(visible == 2, "stride=1, cursor=25: 2 visible");
  }

  // Test 11: visibleVertexCount disabled returns totalVertices
  {
    dc::TemporalFilter tf;
    // Not enabled
    double ts[] = {10.0, 20.0, 30.0};
    check(tf.visibleVertexCount(ts, 3, 1) == 3, "disabled: all vertices returned");
  }

  // Test 12: Range query
  {
    dc::TemporalFilter tf;
    check(tf.rangeStart() == 0.0, "default rangeStart = 0");
    check(tf.rangeEnd() == 0.0, "default rangeEnd = 0");

    tf.setRange(1000.0, 2000.0);
    check(tf.rangeStart() == 1000.0, "rangeStart set to 1000");
    check(tf.rangeEnd() == 2000.0, "rangeEnd set to 2000");
  }

  // Test 13: setRange with inverted values
  {
    dc::TemporalFilter tf;
    tf.setRange(200.0, 100.0);
    check(tf.rangeStart() == 200.0, "inverted range: rangeStart = 200");
    check(tf.rangeEnd() == 200.0, "inverted range: rangeEnd clamped to start");
  }

  // Test 14: setRange re-clamps cursor
  {
    dc::TemporalFilter tf;
    tf.setRange(0.0, 1000.0);
    tf.setCursor(500.0);
    check(tf.cursor() == 500.0, "cursor at 500 within [0,1000]");

    // Shrink range — cursor should be re-clamped
    tf.setRange(600.0, 1000.0);
    check(tf.cursor() == 600.0, "cursor re-clamped to new rangeStart 600");
  }

  // Test 15: Large timestamp dataset (unix epoch style)
  {
    dc::TemporalFilter tf;
    tf.setEnabled(true);
    constexpr int N = 1000;
    double ts[N];
    double base = 1700000000.0; // ~Nov 2023 epoch
    for (int i = 0; i < N; ++i) {
      ts[i] = base + i * 60.0; // 1-minute bars
    }
    tf.setRange(base, base + (N - 1) * 60.0);

    // Cursor at bar 500
    tf.setCursor(base + 500 * 60.0);
    check(tf.visibleCount(ts, N) == 501, "epoch timestamps: 501 visible at bar 500");

    // Cursor at bar 0
    tf.setCursor(base);
    check(tf.visibleCount(ts, N) == 1, "epoch timestamps: 1 visible at bar 0");

    // Cursor just before bar 0
    tf.setCursor(base - 1.0);
    // Cursor is clamped to rangeStart = base, so visibleCount should be 1
    check(tf.visibleCount(ts, N) == 1, "epoch timestamps: cursor clamped to rangeStart");
  }

  std::printf("=== D70.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
