#include "dc/math/Ema.hpp"

namespace dc {

void computeEma(const float* input, float* output, int count, int period) {
  if (count <= 0 || period <= 0) return;
  if (period > count) period = count;

  // Pass through values before period
  for (int i = 0; i < period - 1 && i < count; i++) {
    output[i] = input[i];
  }

  // SMA for the seed value
  float sum = 0.0f;
  for (int i = 0; i < period; i++) {
    sum += input[i];
  }
  output[period - 1] = sum / static_cast<float>(period);

  // EMA for remaining values
  float k = 2.0f / (static_cast<float>(period) + 1.0f);
  for (int i = period; i < count; i++) {
    output[i] = input[i] * k + output[i - 1] * (1.0f - k);
  }
}

} // namespace dc
