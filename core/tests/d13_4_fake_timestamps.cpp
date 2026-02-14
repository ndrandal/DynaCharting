// D13.4 — FakeDataSource timestamps test (pure C++)

#include "dc/data/FakeDataSource.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int tests = 0;
static int passed = 0;

static void check(bool cond, const char* msg) {
  tests++;
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    std::exit(1);
  }
  passed++;
  std::printf("  OK: %s\n", msg);
}

int main() {
  // Test with timestamps enabled
  {
    dc::FakeDataSourceConfig cfg;
    cfg.candleBufferId = 1;
    cfg.useTimestamps = true;
    cfg.startTimestamp = 1700000000.0f;
    cfg.candleIntervalSec = 300.0f;

    dc::FakeDataSource source(cfg);
    dc::IngestProcessor ingest;
    ingest.ensureBuffer(1);

    // Start and collect a few candles
    source.start();

    // Poll until we get at least 1 batch
    std::vector<std::uint8_t> batch;
    int attempts = 0;
    while (attempts < 100) {
      if (source.poll(batch)) break;
      // Small busy-wait
      attempts++;
    }
    source.stop();

    // Drain remaining
    while (source.poll(batch)) {
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }

    // Process the collected batch
    if (!batch.empty()) {
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }

    // Read the first candle from the buffer
    auto size = ingest.getBufferSize(1);
    check(size >= 24, "timestamp candle buffer has data");

    const auto* data = ingest.getBufferData(1);
    float candle[6];
    std::memcpy(candle, data, sizeof(candle));

    // x[0] should be near startTimestamp
    float x0 = candle[0];
    check(std::fabs(x0 - 1700000000.0f) < 1.0f,
          "first candle X ≈ startTimestamp");

    float hw = candle[5];
    check(std::fabs(hw - 120.0f) < 1.0f,
          "halfWidth ≈ 120 (300 * 0.4)");

    std::printf("  x[0] = %.1f, halfWidth = %.1f\n",
                static_cast<double>(x0), static_cast<double>(hw));
  }

  // Test with default config (no timestamps)
  {
    dc::FakeDataSourceConfig cfg;
    cfg.candleBufferId = 2;

    dc::FakeDataSource source(cfg);
    dc::IngestProcessor ingest;
    ingest.ensureBuffer(2);

    source.start();

    std::vector<std::uint8_t> batch;
    int attempts = 0;
    while (attempts < 100) {
      if (source.poll(batch)) break;
      attempts++;
    }
    source.stop();

    while (source.poll(batch)) {
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }

    if (!batch.empty()) {
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    }

    auto size = ingest.getBufferSize(2);
    check(size >= 24, "default candle buffer has data");

    const auto* data = ingest.getBufferData(2);
    float candle[6];
    std::memcpy(candle, data, sizeof(candle));

    float x0 = candle[0];
    check(x0 < 1000.0f, "default config: X is small integer index");

    float hw = candle[5];
    check(std::fabs(hw - 0.4f) < 0.01f, "default halfWidth = 0.4");

    std::printf("  default x[0] = %.1f, halfWidth = %.2f\n",
                static_cast<double>(x0), static_cast<double>(hw));
  }

  std::printf("D13.4 fake_timestamps: %d/%d PASS\n", passed, tests);
  return 0;
}
