// D63.1 — ScaleMapping: logarithmic scale toScreen/toData round-trip
#include "dc/viewport/ScaleMapping.hpp"

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
  std::printf("=== D63.1 Log Scale Mapping Tests ===\n");

  dc::ScaleMapping sm;

  // --- Linear mode baseline ---
  check(sm.mode() == dc::ScaleMode::Linear, "default mode is Linear");

  // Linear: midpoint maps to midpoint
  double mid = sm.toScreen(50.0, 0.0, 100.0, 0.0, 100.0);
  check(approx(mid, 50.0), "linear midpoint maps to screen midpoint");

  // Linear round-trip
  double screen = sm.toScreen(75.0, 0.0, 800.0, 0.0, 100.0);
  double data = sm.toData(screen, 0.0, 800.0, 0.0, 100.0);
  check(approx(data, 75.0), "linear round-trip toScreen->toData");

  // --- Logarithmic mode ---
  sm.setMode(dc::ScaleMode::Logarithmic);
  check(sm.mode() == dc::ScaleMode::Logarithmic, "mode set to Logarithmic");

  // Log scale: dataMin maps to screenMin, dataMax maps to screenMax
  double sMin = sm.toScreen(100.0, 0.0, 1000.0, 100.0, 1000.0);
  check(approx(sMin, 0.0), "log: dataMin -> screenMin");

  double sMax = sm.toScreen(1000.0, 0.0, 1000.0, 100.0, 1000.0);
  check(approx(sMax, 1000.0), "log: dataMax -> screenMax");

  // Log scale: verify log(100)->log(1000) maps linearly in screen space.
  // log(100) = 4.605..., log(1000) = 6.908...
  // log(316.228) ~ 5.756... which is about halfway in log-space
  // t = (log(316.228) - log(100)) / (log(1000) - log(100)) = 0.5
  double sqrtVal = std::sqrt(100.0 * 1000.0); // geometric mean = 316.228...
  double sMid = sm.toScreen(sqrtVal, 0.0, 1000.0, 100.0, 1000.0);
  check(approx(sMid, 500.0), "log: geometric mean maps to screen midpoint");

  // Log: 100 -> 1000 range, value 10000 is outside
  // It should extrapolate: t = (log(10000) - log(100))/(log(1000)-log(100))
  // = (9.210 - 4.605)/(6.908 - 4.605) = 4.605/2.303 = 2.0
  double sExtra = sm.toScreen(10000.0, 0.0, 1000.0, 100.0, 1000.0);
  check(approx(sExtra, 2000.0), "log: extrapolation beyond dataMax");

  // Log round-trip
  double logScreen = sm.toScreen(500.0, 0.0, 800.0, 100.0, 10000.0);
  double logData = sm.toData(logScreen, 0.0, 800.0, 100.0, 10000.0);
  check(approx(logData, 500.0), "log round-trip toScreen->toData");

  // Log round-trip at boundaries
  double logScreenMin = sm.toScreen(100.0, 0.0, 800.0, 100.0, 10000.0);
  double logDataMin = sm.toData(logScreenMin, 0.0, 800.0, 100.0, 10000.0);
  check(approx(logDataMin, 100.0), "log round-trip at dataMin");

  double logScreenMax = sm.toScreen(10000.0, 0.0, 800.0, 100.0, 10000.0);
  double logDataMax = sm.toData(logScreenMax, 0.0, 800.0, 100.0, 10000.0);
  check(approx(logDataMax, 10000.0), "log round-trip at dataMax");

  // Edge case: dataMin == dataMax in log mode
  double sEqual = sm.toScreen(100.0, 0.0, 800.0, 100.0, 100.0);
  check(sEqual == 0.0, "log: dataMin == dataMax returns screenMin");

  // Edge case: negative data values fall back to linear
  sm.setMode(dc::ScaleMode::Logarithmic);
  double sNeg = sm.toScreen(0.0, 0.0, 800.0, -100.0, 100.0);
  // Fallback linear: t = (0 - (-100))/(100 - (-100)) = 100/200 = 0.5
  check(approx(sNeg, 400.0), "log: negative dataMin falls back to linear");

  // Edge case: zero dataMin falls back to linear
  double sZero = sm.toScreen(50.0, 0.0, 800.0, 0.0, 100.0);
  check(approx(sZero, 400.0), "log: zero dataMin falls back to linear");

  // Log: value <= 0 is clamped to dataMin
  sm.setMode(dc::ScaleMode::Logarithmic);
  double sClamp = sm.toScreen(0.0, 0.0, 800.0, 10.0, 1000.0);
  double sAtMin = sm.toScreen(10.0, 0.0, 800.0, 10.0, 1000.0);
  check(approx(sClamp, sAtMin), "log: value <= 0 clamped to dataMin");

  double sClampNeg = sm.toScreen(-5.0, 0.0, 800.0, 10.0, 1000.0);
  check(approx(sClampNeg, sAtMin), "log: negative value clamped to dataMin");

  // Inverse round-trip with many values
  sm.setMode(dc::ScaleMode::Logarithmic);
  bool allRoundTrip = true;
  for (double v = 1.0; v <= 1000.0; v *= 1.5) {
    double s = sm.toScreen(v, 0.0, 600.0, 1.0, 1000.0);
    double d = sm.toData(s, 0.0, 600.0, 1.0, 1000.0);
    if (!approx(d, v, 1e-6)) {
      allRoundTrip = false;
      break;
    }
  }
  check(allRoundTrip, "log: round-trip for many values in [1, 1000]");

  std::printf("=== D63.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
