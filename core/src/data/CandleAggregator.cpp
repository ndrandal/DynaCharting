#include "dc/data/CandleAggregator.hpp"
#include <algorithm>
#include <cstring>

namespace dc {

static constexpr std::uint32_t CANDLE_STRIDE = 24; // 6 floats × 4 bytes

static float readFloat(const std::uint8_t* p) {
    float v;
    std::memcpy(&v, p, sizeof(float));
    return v;
}

static void writeFloat(std::uint8_t* p, float v) {
    std::memcpy(p, &v, sizeof(float));
}

AggregateResult aggregateCandles(const std::uint8_t* rawData,
                                  std::uint32_t rawByteLen,
                                  std::uint32_t factor) {
    AggregateResult result;

    if (factor < 2) return result;

    std::uint32_t rawCount = rawByteLen / CANDLE_STRIDE;
    if (rawCount < factor) return result;

    // candle6 layout: [x, open, high, low, close, halfWidth] (6 floats)
    std::uint32_t groupCount = (rawCount + factor - 1) / factor;

    // Only include groups with at least 'factor' candles (except tail)
    result.data.resize(groupCount * CANDLE_STRIDE);
    result.candleCount = groupCount;

    for (std::uint32_t g = 0; g < groupCount; ++g) {
        std::uint32_t start = g * factor;
        std::uint32_t end = std::min(start + factor, rawCount);

        const std::uint8_t* first = rawData + start * CANDLE_STRIDE;
        const std::uint8_t* last  = rawData + (end - 1) * CANDLE_STRIDE;

        float x        = readFloat(first + 0);  // first candle's x
        float open      = readFloat(first + 4);  // first candle's open
        float high      = readFloat(first + 8);  // will take max
        float low       = readFloat(first + 12); // will take min
        float close     = readFloat(last + 16);  // last candle's close
        float halfWidth = readFloat(first + 20); // first candle's halfWidth

        // Merge high/low across all candles in group
        for (std::uint32_t i = start + 1; i < end; ++i) {
            const std::uint8_t* c = rawData + i * CANDLE_STRIDE;
            float h = readFloat(c + 8);
            float l = readFloat(c + 12);
            if (h > high) high = h;
            if (l < low) low = l;
        }

        // Scale halfWidth by actual group size
        halfWidth *= static_cast<float>(end - start);

        std::uint8_t* out = result.data.data() + g * CANDLE_STRIDE;
        writeFloat(out + 0, x);
        writeFloat(out + 4, open);
        writeFloat(out + 8, high);
        writeFloat(out + 12, low);
        writeFloat(out + 16, close);
        writeFloat(out + 20, halfWidth);
    }

    return result;
}

} // namespace dc
