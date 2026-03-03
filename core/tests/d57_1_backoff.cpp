// D57.1 — BackoffCalculator: exponential backoff with max cap and reset
#include "dc/data/MessageCodec.hpp"

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

static bool near(double a, double b, double eps = 0.01) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D57.1 BackoffCalculator Tests ===\n");

  // Test 1: Default parameters — initial 1000ms, doubles, caps at 30000ms
  {
    dc::BackoffCalculator calc;
    check(calc.attempt() == 0, "initial attempt is 0");

    double d1 = calc.nextDelay();
    check(near(d1, 1000.0), "first delay = 1000");
    check(calc.attempt() == 1, "attempt = 1 after first call");

    double d2 = calc.nextDelay();
    check(near(d2, 2000.0), "second delay = 2000");
    check(calc.attempt() == 2, "attempt = 2");

    double d3 = calc.nextDelay();
    check(near(d3, 4000.0), "third delay = 4000");

    double d4 = calc.nextDelay();
    check(near(d4, 8000.0), "fourth delay = 8000");

    double d5 = calc.nextDelay();
    check(near(d5, 16000.0), "fifth delay = 16000");

    // Next would be 32000, but capped at 30000
    double d6 = calc.nextDelay();
    check(near(d6, 30000.0), "sixth delay = 30000 (capped)");
    check(calc.attempt() == 6, "attempt = 6");

    // Should stay at cap
    double d7 = calc.nextDelay();
    check(near(d7, 30000.0), "seventh delay = 30000 (stays at cap)");
  }

  // Test 2: Reset
  {
    dc::BackoffCalculator calc;
    calc.nextDelay(); // 1000
    calc.nextDelay(); // 2000
    check(calc.attempt() == 2, "attempt = 2 before reset");

    calc.reset();
    check(calc.attempt() == 0, "attempt = 0 after reset");

    double d1 = calc.nextDelay();
    check(near(d1, 1000.0), "after reset, first delay = 1000 again");

    double d2 = calc.nextDelay();
    check(near(d2, 2000.0), "after reset, second delay = 2000 again");
  }

  // Test 3: Custom parameters
  {
    dc::BackoffCalculator calc(500.0, 5000.0, 3.0);
    double d1 = calc.nextDelay();
    check(near(d1, 500.0), "custom: first delay = 500");

    double d2 = calc.nextDelay();
    check(near(d2, 1500.0), "custom: second delay = 1500 (500*3)");

    double d3 = calc.nextDelay();
    check(near(d3, 4500.0), "custom: third delay = 4500 (1500*3)");

    // Next would be 13500, capped at 5000
    double d4 = calc.nextDelay();
    check(near(d4, 5000.0), "custom: fourth delay = 5000 (capped)");
  }

  // Test 4: Multiplier of 1 — constant backoff
  {
    dc::BackoffCalculator calc(100.0, 100.0, 1.0);
    for (int i = 0; i < 5; ++i) {
      double d = calc.nextDelay();
      check(near(d, 100.0), "constant: every delay = 100");
    }
  }

  std::printf("=== D57.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
