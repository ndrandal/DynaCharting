// D27.1 — Easing functions test
#include "dc/anim/Easing.hpp"

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

static bool near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D27.1 Easing Functions ===\n");

  // All easing types
  dc::EasingType types[] = {
    dc::EasingType::Linear,
    dc::EasingType::EaseInQuad,
    dc::EasingType::EaseOutQuad,
    dc::EasingType::EaseInOutQuad,
    dc::EasingType::EaseInCubic,
    dc::EasingType::EaseOutCubic,
    dc::EasingType::EaseInOutCubic,
    dc::EasingType::EaseOutBack,
    dc::EasingType::EaseOutElastic,
  };
  const char* names[] = {
    "Linear", "EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
    "EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
    "EaseOutBack", "EaseOutElastic",
  };

  // Test 1: All easings return 0 at t=0 and 1 at t=1
  for (int i = 0; i < 9; i++) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s: f(0)=0", names[i]);
    check(near(dc::ease(types[i], 0.0f), 0.0f), buf);
    std::snprintf(buf, sizeof(buf), "%s: f(1)=1", names[i]);
    check(near(dc::ease(types[i], 1.0f), 1.0f), buf);
  }

  // Test 2: Clamping at boundaries
  check(near(dc::ease(dc::EasingType::Linear, -0.5f), 0.0f), "clamp below 0");
  check(near(dc::ease(dc::EasingType::Linear, 1.5f), 1.0f), "clamp above 1");

  // Test 3: Linear is identity
  check(near(dc::ease(dc::EasingType::Linear, 0.25f), 0.25f), "Linear(0.25)=0.25");
  check(near(dc::ease(dc::EasingType::Linear, 0.5f), 0.5f), "Linear(0.5)=0.5");
  check(near(dc::ease(dc::EasingType::Linear, 0.75f), 0.75f), "Linear(0.75)=0.75");

  // Test 4: EaseIn starts slow (f(0.25) < 0.25 for quad/cubic)
  check(dc::ease(dc::EasingType::EaseInQuad, 0.25f) < 0.25f, "EaseInQuad slow start");
  check(dc::ease(dc::EasingType::EaseInCubic, 0.25f) < 0.25f, "EaseInCubic slow start");

  // Test 5: EaseOut starts fast (f(0.25) > 0.25 for quad/cubic)
  check(dc::ease(dc::EasingType::EaseOutQuad, 0.25f) > 0.25f, "EaseOutQuad fast start");
  check(dc::ease(dc::EasingType::EaseOutCubic, 0.25f) > 0.25f, "EaseOutCubic fast start");

  // Test 6: InOut: symmetric — f(0.5) ≈ 0.5
  check(near(dc::ease(dc::EasingType::EaseInOutQuad, 0.5f), 0.5f), "EaseInOutQuad midpoint");
  check(near(dc::ease(dc::EasingType::EaseInOutCubic, 0.5f), 0.5f), "EaseInOutCubic midpoint");

  // Test 7: EaseOutBack overshoots (value > 1 at some point)
  {
    bool overshoots = false;
    for (float t = 0.5f; t < 1.0f; t += 0.01f) {
      if (dc::ease(dc::EasingType::EaseOutBack, t) > 1.0f) {
        overshoots = true;
        break;
      }
    }
    check(overshoots, "EaseOutBack overshoots past 1.0");
  }

  // Test 8: EaseOutElastic oscillates (has values < 1 near end)
  {
    float v = dc::ease(dc::EasingType::EaseOutElastic, 0.5f);
    check(v > 0.0f, "EaseOutElastic(0.5) > 0");
  }

  // Test 9: Monotonicity for standard easings (Linear, Quad, Cubic — no overshoot)
  {
    dc::EasingType monotonic[] = {
      dc::EasingType::Linear,
      dc::EasingType::EaseInQuad, dc::EasingType::EaseOutQuad,
      dc::EasingType::EaseInCubic, dc::EasingType::EaseOutCubic,
    };
    const char* mNames[] = {"Linear", "EaseInQuad", "EaseOutQuad", "EaseInCubic", "EaseOutCubic"};
    for (int i = 0; i < 5; i++) {
      bool mono = true;
      float prev = 0.0f;
      for (float t = 0.01f; t <= 1.0f; t += 0.01f) {
        float v = dc::ease(monotonic[i], t);
        if (v < prev - 1e-6f) { mono = false; break; }
        prev = v;
      }
      char buf[128];
      std::snprintf(buf, sizeof(buf), "%s is monotonic", mNames[i]);
      check(mono, buf);
    }
  }

  std::printf("=== D27.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
