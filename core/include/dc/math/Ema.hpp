#pragma once

namespace dc {

// Exponential Moving Average.
// output[0..period-2] = input[0..period-2] (pass-through, no EMA yet).
// output[period-1] = SMA of first `period` values.
// output[i] for i >= period uses EMA formula.
void computeEma(const float* input, float* output, int count, int period);

} // namespace dc
