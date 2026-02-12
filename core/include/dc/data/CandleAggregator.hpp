#pragma once
#include <cstdint>
#include <vector>

namespace dc {

struct AggregateResult {
    std::vector<std::uint8_t> data;  // candle6 format (24B per candle)
    std::uint32_t candleCount{0};
};

// Merges N consecutive raw candles (candle6: 24B each) into aggregated candles.
// Returns empty result if factor < 2 or rawCount < factor.
AggregateResult aggregateCandles(const std::uint8_t* rawData,
                                  std::uint32_t rawByteLen,
                                  std::uint32_t factor);

} // namespace dc
