#include "dc/math/Indicators.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace dc {

std::vector<float> computeRSI(const float* closes, int count, int period) {
  std::vector<float> rsi(static_cast<std::size_t>(count), std::numeric_limits<float>::quiet_NaN());
  if (count < period + 1 || period < 1) return rsi;

  // First pass: compute initial average gain/loss
  float avgGain = 0.0f, avgLoss = 0.0f;
  for (int i = 1; i <= period; i++) {
    float change = closes[i] - closes[i - 1];
    if (change > 0) avgGain += change;
    else avgLoss -= change;
  }
  avgGain /= static_cast<float>(period);
  avgLoss /= static_cast<float>(period);

  if (avgLoss == 0.0f) {
    rsi[static_cast<std::size_t>(period)] = 100.0f;
  } else {
    float rs = avgGain / avgLoss;
    rsi[static_cast<std::size_t>(period)] = 100.0f - 100.0f / (1.0f + rs);
  }

  // Subsequent values: Wilder's smoothing
  float inv = 1.0f / static_cast<float>(period);
  for (int i = period + 1; i < count; i++) {
    float change = closes[i] - closes[i - 1];
    float gain = (change > 0) ? change : 0.0f;
    float loss = (change < 0) ? -change : 0.0f;

    avgGain = (avgGain * (static_cast<float>(period) - 1.0f) + gain) * inv;
    avgLoss = (avgLoss * (static_cast<float>(period) - 1.0f) + loss) * inv;

    if (avgLoss == 0.0f) {
      rsi[static_cast<std::size_t>(i)] = 100.0f;
    } else {
      float rs = avgGain / avgLoss;
      rsi[static_cast<std::size_t>(i)] = 100.0f - 100.0f / (1.0f + rs);
    }
  }

  return rsi;
}

StochasticResult computeStochastic(const float* highs, const float* lows,
                                    const float* closes, int count,
                                    int kPeriod, int dPeriod) {
  StochasticResult result;
  auto nan = std::numeric_limits<float>::quiet_NaN();
  result.percentK.resize(static_cast<std::size_t>(count), nan);
  result.percentD.resize(static_cast<std::size_t>(count), nan);

  if (count < kPeriod || kPeriod < 1) return result;

  // Compute %K: (close - lowestLow) / (highestHigh - lowestLow) * 100
  for (int i = kPeriod - 1; i < count; i++) {
    float hh = -1e30f, ll = 1e30f;
    for (int j = i - kPeriod + 1; j <= i; j++) {
      if (highs[j] > hh) hh = highs[j];
      if (lows[j] < ll) ll = lows[j];
    }
    float range = hh - ll;
    if (range > 0.0f) {
      result.percentK[static_cast<std::size_t>(i)] = (closes[i] - ll) / range * 100.0f;
    } else {
      result.percentK[static_cast<std::size_t>(i)] = 50.0f;
    }
  }

  // Compute %D: simple moving average of %K
  if (dPeriod < 1) return result;
  for (int i = kPeriod - 1 + dPeriod - 1; i < count; i++) {
    float sum = 0.0f;
    int valid = 0;
    for (int j = i - dPeriod + 1; j <= i; j++) {
      if (!std::isnan(result.percentK[static_cast<std::size_t>(j)])) {
        sum += result.percentK[static_cast<std::size_t>(j)];
        valid++;
      }
    }
    if (valid > 0) {
      result.percentD[static_cast<std::size_t>(i)] = sum / static_cast<float>(valid);
    }
  }

  return result;
}

} // namespace dc
