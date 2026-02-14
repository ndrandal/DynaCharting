// D17.1+D17.3 — RSI + Stochastic indicator math

#include "dc/math/Indicators.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

static void requireClose(float a, float b, float tol, const char* msg) {
  if (std::fabs(a - b) > tol) {
    std::fprintf(stderr, "ASSERT FAIL: %s (%.4f vs %.4f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // ---- Test 1: RSI basic ----
  {
    // 20 prices trending up
    float closes[20];
    for (int i = 0; i < 20; i++) {
      closes[i] = 100.0f + static_cast<float>(i) * 2.0f;
    }

    auto rsi = dc::computeRSI(closes, 20, 14);
    requireTrue(rsi.size() == 20, "20 RSI values");

    // First 14 should be NaN
    requireTrue(std::isnan(rsi[0]), "rsi[0] is NaN");
    requireTrue(std::isnan(rsi[13]), "rsi[13] is NaN");

    // RSI at index 14 should be 100 (all gains, no losses)
    requireClose(rsi[14], 100.0f, 0.01f, "rsi[14] = 100 (all up)");

    std::printf("  Test 1 (RSI trending up): PASS\n");
  }

  // ---- Test 2: RSI mixed ----
  {
    float closes[] = {44,44.34f,44.09f,43.61f,44.33f,44.83f,45.10f,45.42f,45.84f,
                      46.08f,45.89f,46.03f,45.61f,46.28f,46.28f,46.00f,46.03f,46.41f,
                      46.22f,45.64f};
    int n = 20;

    auto rsi = dc::computeRSI(closes, n, 14);
    requireTrue(rsi.size() == 20, "20 values");
    requireTrue(!std::isnan(rsi[14]), "rsi[14] has value");
    requireTrue(rsi[14] > 0 && rsi[14] < 100, "RSI in range");

    std::printf("  Test 2 (RSI mixed): PASS (rsi[14]=%.2f)\n", rsi[14]);
  }

  // ---- Test 3: RSI edge cases ----
  {
    // Too few data points
    float closes[] = {100, 101, 102};
    auto rsi = dc::computeRSI(closes, 3, 14);
    requireTrue(rsi.size() == 3, "3 values");
    requireTrue(std::isnan(rsi[0]), "all NaN for short data");
    requireTrue(std::isnan(rsi[2]), "all NaN");

    std::printf("  Test 3 (RSI edge): PASS\n");
  }

  // ---- Test 4: Stochastic basic ----
  {
    float highs[20], lows[20], closes[20];
    for (int i = 0; i < 20; i++) {
      float base = 100.0f + static_cast<float>(i) * 1.0f;
      highs[i] = base + 2.0f;
      lows[i] = base - 2.0f;
      closes[i] = base + 1.0f; // closing near highs
    }

    auto result = dc::computeStochastic(highs, lows, closes, 20, 14, 3);
    requireTrue(result.percentK.size() == 20, "20 %K values");
    requireTrue(result.percentD.size() == 20, "20 %D values");

    // First 13 %K should be NaN
    requireTrue(std::isnan(result.percentK[12]), "%K[12] NaN");
    requireTrue(!std::isnan(result.percentK[13]), "%K[13] has value");

    // %K should be high (closing near top of range)
    requireTrue(result.percentK[13] > 50.0f, "%K > 50 (near high)");

    // %D should exist from index 15 (14-1 + 3-1)
    requireTrue(!std::isnan(result.percentD[15]), "%D[15] has value");

    std::printf("  Test 4 (Stochastic): PASS (%%K[13]=%.2f, %%D[15]=%.2f)\n",
                result.percentK[13], result.percentD[15]);
  }

  // ---- Test 5: Stochastic edge ----
  {
    float h[] = {100, 101};
    float l[] = {99, 100};
    float c[] = {99.5f, 100.5f};

    auto result = dc::computeStochastic(h, l, c, 2, 14, 3);
    requireTrue(std::isnan(result.percentK[0]), "too few data");
    requireTrue(std::isnan(result.percentK[1]), "too few data");

    std::printf("  Test 5 (Stochastic edge): PASS\n");
  }

  std::printf("D17.1+D17.3 indicators: ALL PASS\n");
  return 0;
}
