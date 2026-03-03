// D63.2 — ScaleMapping: percentage and indexed scale modes
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
  std::printf("=== D63.2 Percentage & Indexed Scale Tests ===\n");

  dc::ScaleMapping sm;

  // ========================================
  // Percentage mode
  // ========================================
  sm.setMode(dc::ScaleMode::Percentage);
  sm.setReferencePrice(100.0);
  check(sm.mode() == dc::ScaleMode::Percentage, "mode set to Percentage");
  check(approx(sm.referencePrice(), 100.0), "referencePrice is 100");

  // Reference price maps to 0% change.
  // With dataRange [80, 120], referencePrice=100:
  //   pctMin = (80 - 100)/100 * 100 = -20%
  //   pctMax = (120 - 100)/100 * 100 = +20%
  //   pctRef = (100 - 100)/100 * 100 = 0%
  //   t = (0 - (-20)) / (20 - (-20)) = 20/40 = 0.5
  double sRef = sm.toScreen(100.0, 0.0, 800.0, 80.0, 120.0);
  check(approx(sRef, 400.0), "pct: referencePrice maps to screen midpoint");

  // dataMin maps to screenMin
  double sPctMin = sm.toScreen(80.0, 0.0, 800.0, 80.0, 120.0);
  check(approx(sPctMin, 0.0), "pct: dataMin -> screenMin");

  // dataMax maps to screenMax
  double sPctMax = sm.toScreen(120.0, 0.0, 800.0, 80.0, 120.0);
  check(approx(sPctMax, 800.0), "pct: dataMax -> screenMax");

  // 110 is +10% from ref=100, which maps to t = (10 - (-20))/(20 - (-20)) = 30/40 = 0.75
  double s110 = sm.toScreen(110.0, 0.0, 800.0, 80.0, 120.0);
  check(approx(s110, 600.0), "pct: 110 (+10%) maps to 3/4 screen");

  // Round-trip
  double pctData = sm.toData(s110, 0.0, 800.0, 80.0, 120.0);
  check(approx(pctData, 110.0), "pct: round-trip toScreen->toData for 110");

  // Round-trip at reference price
  double pctRefData = sm.toData(sRef, 0.0, 800.0, 80.0, 120.0);
  check(approx(pctRefData, 100.0), "pct: round-trip at referencePrice");

  // Round-trip for many values
  bool allPctRT = true;
  for (double v = 80.0; v <= 120.0; v += 2.5) {
    double s = sm.toScreen(v, 0.0, 600.0, 80.0, 120.0);
    double d = sm.toData(s, 0.0, 600.0, 80.0, 120.0);
    if (!approx(d, v, 1e-6)) {
      allPctRT = false;
      break;
    }
  }
  check(allPctRT, "pct: round-trip for many values in [80, 120]");

  // Percentage mode: symmetric data range around reference
  // dataRange [50, 150], ref=100: -50% to +50%
  // Value 100 (0%) -> t = (0 - (-50))/(50 - (-50)) = 50/100 = 0.5
  sm.setReferencePrice(100.0);
  double sSym = sm.toScreen(100.0, 0.0, 1000.0, 50.0, 150.0);
  check(approx(sSym, 500.0), "pct: symmetric range, ref at screen midpoint");

  // Percentage with non-symmetric range: dataRange [90, 130], ref=100
  // pctMin = -10%, pctMax = +30%, pct(100) = 0%
  // t = (0 - (-10)) / (30 - (-10)) = 10/40 = 0.25
  double sAsym = sm.toScreen(100.0, 0.0, 800.0, 90.0, 130.0);
  check(approx(sAsym, 200.0), "pct: asymmetric range, ref at 1/4 screen");

  // ========================================
  // Indexed mode
  // ========================================
  sm.setMode(dc::ScaleMode::Indexed);
  sm.setReferencePrice(50.0);
  check(sm.mode() == dc::ScaleMode::Indexed, "mode set to Indexed");

  // Indexed: referencePrice=50, so 50 -> index 100.
  // dataRange [25, 75]:
  //   idxMin = 25/50 * 100 = 50
  //   idxMax = 75/50 * 100 = 150
  //   idxRef = 50/50 * 100 = 100
  //   t = (100 - 50) / (150 - 50) = 50/100 = 0.5
  double sIdxRef = sm.toScreen(50.0, 0.0, 800.0, 25.0, 75.0);
  check(approx(sIdxRef, 400.0), "idx: referencePrice maps to screen midpoint");

  // dataMin maps to screenMin
  double sIdxMin = sm.toScreen(25.0, 0.0, 800.0, 25.0, 75.0);
  check(approx(sIdxMin, 0.0), "idx: dataMin -> screenMin");

  // dataMax maps to screenMax
  double sIdxMax = sm.toScreen(75.0, 0.0, 800.0, 25.0, 75.0);
  check(approx(sIdxMax, 800.0), "idx: dataMax -> screenMax");

  // Round-trip
  double idxData = sm.toData(sIdxRef, 0.0, 800.0, 25.0, 75.0);
  check(approx(idxData, 50.0), "idx: round-trip at referencePrice");

  // Round-trip many values
  bool allIdxRT = true;
  for (double v = 25.0; v <= 75.0; v += 3.0) {
    double s = sm.toScreen(v, 0.0, 600.0, 25.0, 75.0);
    double d = sm.toData(s, 0.0, 600.0, 25.0, 75.0);
    if (!approx(d, v, 1e-6)) {
      allIdxRT = false;
      break;
    }
  }
  check(allIdxRT, "idx: round-trip for many values in [25, 75]");

  // Indexed: value at 2x reference -> index 200
  // dataRange [50, 150], ref=50:
  //   idxMin = 100, idxMax = 300, idx(100) = 200
  //   t = (200 - 100)/(300 - 100) = 100/200 = 0.5
  sm.setReferencePrice(50.0);
  double s2x = sm.toScreen(100.0, 0.0, 800.0, 50.0, 150.0);
  check(approx(s2x, 400.0), "idx: 2x reference at screen midpoint");

  // ========================================
  // Edge cases
  // ========================================

  // Percentage with zero reference falls back to linear
  sm.setMode(dc::ScaleMode::Percentage);
  sm.setReferencePrice(0.0);
  double sZeroRef = sm.toScreen(50.0, 0.0, 800.0, 0.0, 100.0);
  check(approx(sZeroRef, 400.0), "pct: zero referencePrice falls back to linear");

  // Indexed with zero reference falls back to linear
  sm.setMode(dc::ScaleMode::Indexed);
  sm.setReferencePrice(0.0);
  double sIdxZero = sm.toScreen(50.0, 0.0, 800.0, 0.0, 100.0);
  check(approx(sIdxZero, 400.0), "idx: zero referencePrice falls back to linear");

  // Linear mode: dataMin == dataMax
  sm.setMode(dc::ScaleMode::Linear);
  double sLinEqual = sm.toScreen(100.0, 0.0, 800.0, 100.0, 100.0);
  check(sLinEqual == 0.0, "linear: dataMin == dataMax returns screenMin");

  // Percentage mode equivalence: since percentage is a linear transformation of
  // data values, the screen mapping should be equivalent to linear.
  // That is, for any data range and reference price, the ordering and proportional
  // spacing should match linear (because both pctValue and pctMin/pctMax scale
  // the same way).
  sm.setMode(dc::ScaleMode::Linear);
  double sLin = sm.toScreen(150.0, 0.0, 800.0, 100.0, 200.0);
  sm.setMode(dc::ScaleMode::Percentage);
  sm.setReferencePrice(100.0);
  double sPct = sm.toScreen(150.0, 0.0, 800.0, 100.0, 200.0);
  check(approx(sLin, sPct), "pct and linear produce same screen position");

  std::printf("=== D63.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
