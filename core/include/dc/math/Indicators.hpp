#pragma once
#include <cstdint>
#include <vector>

namespace dc {

// D17.1: RSI (Relative Strength Index) computation.
// Input: array of close prices. Output: RSI values (0-100).
// First `period` values are NaN (not enough data).
std::vector<float> computeRSI(const float* closes, int count, int period = 14);

// D17.3: Stochastic oscillator (%K and %D).
// Input: high, low, close arrays. Output: %K and %D values (0-100).
struct StochasticResult {
  std::vector<float> percentK;
  std::vector<float> percentD;
};

StochasticResult computeStochastic(const float* highs, const float* lows,
                                    const float* closes, int count,
                                    int kPeriod = 14, int dPeriod = 3);

} // namespace dc
