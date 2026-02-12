// D8.2 — OHLC Candle Downsampler test (pure C++, no GL)
// Tests: aggregateCandles with various factors and edge cases.

#include "dc/data/CandleAggregator.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireClose(float a, float b, float eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

// candle6: x, open, high, low, close, halfWidth (6 floats = 24 bytes)
struct Candle6 {
  float x, open, high, low, close, halfWidth;
};

static std::vector<std::uint8_t> makeRawData(const std::vector<Candle6>& candles) {
  std::vector<std::uint8_t> data(candles.size() * 24);
  std::memcpy(data.data(), candles.data(), data.size());
  return data;
}

static Candle6 readCandle(const std::uint8_t* p) {
  Candle6 c;
  std::memcpy(&c, p, sizeof(Candle6));
  return c;
}

int main() {
  // --- Test 1: 12 candles × factor 3 → 4 output candles ---
  {
    std::vector<Candle6> raw;
    for (int i = 0; i < 12; ++i) {
      raw.push_back({
        static_cast<float>(i),       // x
        100.0f + i,                   // open
        105.0f + i,                   // high
        95.0f + i,                    // low
        102.0f + i,                   // close
        0.4f                          // halfWidth
      });
    }
    auto data = makeRawData(raw);
    auto result = dc::aggregateCandles(data.data(),
                                        static_cast<std::uint32_t>(data.size()), 3);

    requireTrue(result.candleCount == 4, "12/3 = 4 candles");
    requireTrue(result.data.size() == 4 * 24, "4 candles × 24 bytes");

    // First group: candles 0,1,2
    auto c0 = readCandle(result.data.data());
    requireClose(c0.x, 0.0f, 1e-5f, "group0 x = first candle x");
    requireClose(c0.open, 100.0f, 1e-5f, "group0 open = first candle open");
    requireClose(c0.high, 107.0f, 1e-5f, "group0 high = max(105,106,107)");
    requireClose(c0.low, 95.0f, 1e-5f, "group0 low = min(95,96,97)");
    requireClose(c0.close, 104.0f, 1e-5f, "group0 close = last candle close");
    requireClose(c0.halfWidth, 1.2f, 1e-5f, "group0 halfWidth = 0.4 × 3");

    // Last group: candles 9,10,11
    auto c3 = readCandle(result.data.data() + 3 * 24);
    requireClose(c3.x, 9.0f, 1e-5f, "group3 x");
    requireClose(c3.open, 109.0f, 1e-5f, "group3 open");
    requireClose(c3.high, 116.0f, 1e-5f, "group3 high");
    requireClose(c3.low, 104.0f, 1e-5f, "group3 low");
    requireClose(c3.close, 113.0f, 1e-5f, "group3 close");

    std::printf("  12×3 PASS\n");
  }

  // --- Test 2: 10 candles × factor 4 → 3 candles (tail group of 2) ---
  {
    std::vector<Candle6> raw;
    for (int i = 0; i < 10; ++i) {
      raw.push_back({
        static_cast<float>(i), 100.0f, 110.0f, 90.0f, 105.0f, 0.5f
      });
    }
    auto data = makeRawData(raw);
    auto result = dc::aggregateCandles(data.data(),
                                        static_cast<std::uint32_t>(data.size()), 4);

    requireTrue(result.candleCount == 3, "10/4 = 3 candles (ceil)");

    // Tail group (candles 8,9): halfWidth = 0.5 × 2
    auto tail = readCandle(result.data.data() + 2 * 24);
    requireClose(tail.halfWidth, 1.0f, 1e-5f, "tail halfWidth = 0.5 × 2");

    std::printf("  10×4 PASS\n");
  }

  // --- Test 3: 1 candle × factor 2 → empty ---
  {
    Candle6 raw{0, 100, 110, 90, 105, 0.5f};
    auto data = makeRawData({raw});
    auto result = dc::aggregateCandles(data.data(),
                                        static_cast<std::uint32_t>(data.size()), 2);
    requireTrue(result.candleCount == 0, "1 candle < factor → empty");
    requireTrue(result.data.empty(), "empty data");

    std::printf("  Under-factor PASS\n");
  }

  // --- Test 4: factor < 2 → empty ---
  {
    Candle6 raw{0, 100, 110, 90, 105, 0.5f};
    auto data = makeRawData({raw});

    auto r0 = dc::aggregateCandles(data.data(),
                                    static_cast<std::uint32_t>(data.size()), 0);
    requireTrue(r0.candleCount == 0, "factor 0 → empty");

    auto r1 = dc::aggregateCandles(data.data(),
                                    static_cast<std::uint32_t>(data.size()), 1);
    requireTrue(r1.candleCount == 0, "factor 1 → empty");

    std::printf("  Invalid factor PASS\n");
  }

  std::printf("\nD8.2 aggregator PASS\n");
  return 0;
}
