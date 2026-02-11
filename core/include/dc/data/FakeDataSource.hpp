#pragma once
#include "dc/data/DataSource.hpp"
#include "dc/data/ThreadSafeQueue.hpp"
#include "dc/ids/Id.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace dc {

struct FakeDataSourceConfig {
  Id candleBufferId{0};
  Id lineBufferId{0};
  int tickIntervalMs{100};
  int candleIntervalMs{2000};
  float startPrice{100.0f};
  float volatility{0.5f};
  std::size_t maxCandles{200};
};

class FakeDataSource : public DataSource {
public:
  explicit FakeDataSource(const FakeDataSourceConfig& config);
  ~FakeDataSource() override;

  void start() override;
  void stop() override;
  bool poll(std::vector<std::uint8_t>& batch) override;
  bool isRunning() const override;

  std::uint32_t candleCount() const;
  float priceMin() const;
  float priceMax() const;

private:
  void producerLoop();
  void emitAppend();
  void emitUpdate();

  // Binary batch helpers
  static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v);
  static void appendRecord(std::vector<std::uint8_t>& batch,
                            std::uint8_t op, std::uint32_t bufferId,
                            std::uint32_t offset, const void* payload,
                            std::uint32_t len);

  FakeDataSourceConfig config_;
  ThreadSafeQueue<std::vector<std::uint8_t>> queue_{256};
  std::thread thread_;
  std::atomic<bool> running_{false};

  // Producer state (only accessed from producer thread)
  std::uint32_t seed_{42};
  float price_;
  float currentOpen_;
  float currentHigh_;
  float currentLow_;
  float currentClose_;

  // Shared state (mutex-protected reads from main thread)
  mutable std::mutex stateMtx_;
  std::uint32_t candleCount_{0};
  float priceMin_{1e9f};
  float priceMax_{-1e9f};
};

} // namespace dc
