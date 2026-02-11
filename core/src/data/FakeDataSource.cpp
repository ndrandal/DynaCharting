#include "dc/data/FakeDataSource.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace dc {

FakeDataSource::FakeDataSource(const FakeDataSourceConfig& config)
    : config_(config), price_(config.startPrice),
      currentOpen_(config.startPrice), currentHigh_(config.startPrice),
      currentLow_(config.startPrice), currentClose_(config.startPrice) {}

FakeDataSource::~FakeDataSource() { stop(); }

void FakeDataSource::start() {
  if (running_.load()) return;
  running_.store(true);
  thread_ = std::thread(&FakeDataSource::producerLoop, this);
}

void FakeDataSource::stop() {
  running_.store(false);
  if (thread_.joinable()) thread_.join();
}

bool FakeDataSource::poll(std::vector<std::uint8_t>& batch) {
  return queue_.pop(batch);
}

bool FakeDataSource::isRunning() const { return running_.load(); }

std::uint32_t FakeDataSource::candleCount() const {
  std::lock_guard<std::mutex> lock(stateMtx_);
  return candleCount_;
}

float FakeDataSource::priceMin() const {
  std::lock_guard<std::mutex> lock(stateMtx_);
  return priceMin_;
}

float FakeDataSource::priceMax() const {
  std::lock_guard<std::mutex> lock(stateMtx_);
  return priceMax_;
}

void FakeDataSource::producerLoop() {
  using Clock = std::chrono::steady_clock;

  auto nextTick = Clock::now();
  auto nextCandle = Clock::now() +
      std::chrono::milliseconds(config_.candleIntervalMs);

  // Emit the first candle immediately
  emitAppend();

  while (running_.load()) {
    nextTick += std::chrono::milliseconds(config_.tickIntervalMs);
    std::this_thread::sleep_until(nextTick);
    if (!running_.load()) break;

    // Check if we need a new candle
    if (Clock::now() >= nextCandle) {
      emitAppend();
      nextCandle += std::chrono::milliseconds(config_.candleIntervalMs);
    } else {
      emitUpdate();
    }
  }
}

void FakeDataSource::emitAppend() {
  // RNG: simple LCG
  auto rng = [this]() -> float {
    seed_ = seed_ * 1103515245u + 12345u;
    return static_cast<float>((seed_ >> 16) & 0x7FFF) / 32767.0f;
  };

  // Start a new candle
  float change = (rng() - 0.5f) * config_.volatility * 2.0f;
  price_ += change;
  currentOpen_ = price_;
  currentHigh_ = price_ + rng() * config_.volatility * 0.5f;
  currentLow_ = price_ - rng() * config_.volatility * 0.5f;
  currentClose_ = price_;

  std::uint32_t idx;
  {
    std::lock_guard<std::mutex> lock(stateMtx_);
    idx = candleCount_;
    candleCount_++;
    priceMin_ = std::min(priceMin_, currentLow_);
    priceMax_ = std::max(priceMax_, currentHigh_);
  }

  // candle6 format: {x, open, high, low, close, halfWidth}
  float halfWidth = 0.4f;
  float candle[6] = {static_cast<float>(idx), currentOpen_, currentHigh_,
                     currentLow_, currentClose_, halfWidth};

  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, // OP_APPEND
               static_cast<std::uint32_t>(config_.candleBufferId), 0,
               candle, sizeof(candle));

  // Also append a close-price line point (pos2_clip: {x, y})
  if (config_.lineBufferId != 0) {
    float linePoint[2] = {static_cast<float>(idx), currentClose_};
    appendRecord(batch, 1, static_cast<std::uint32_t>(config_.lineBufferId),
                 0, linePoint, sizeof(linePoint));
  }

  queue_.push(std::move(batch));
}

void FakeDataSource::emitUpdate() {
  auto rng = [this]() -> float {
    seed_ = seed_ * 1103515245u + 12345u;
    return static_cast<float>((seed_ >> 16) & 0x7FFF) / 32767.0f;
  };

  // Tick the current candle
  float tick = (rng() - 0.5f) * config_.volatility;
  currentClose_ += tick;
  currentHigh_ = std::max(currentHigh_, currentClose_);
  currentLow_ = std::min(currentLow_, currentClose_);
  price_ = currentClose_;

  std::uint32_t idx;
  {
    std::lock_guard<std::mutex> lock(stateMtx_);
    idx = candleCount_ > 0 ? candleCount_ - 1 : 0;
    priceMin_ = std::min(priceMin_, currentLow_);
    priceMax_ = std::max(priceMax_, currentHigh_);
  }

  // Update last candle via OP_UPDATE_RANGE
  float candle[6] = {static_cast<float>(idx), currentOpen_, currentHigh_,
                     currentLow_, currentClose_, 0.4f};
  std::uint32_t offset = idx * static_cast<std::uint32_t>(sizeof(candle));

  std::vector<std::uint8_t> batch;
  appendRecord(batch, 2, // OP_UPDATE_RANGE
               static_cast<std::uint32_t>(config_.candleBufferId), offset,
               candle, sizeof(candle));

  // Update last line point
  if (config_.lineBufferId != 0) {
    float linePoint[2] = {static_cast<float>(idx), currentClose_};
    std::uint32_t lineOffset = idx * static_cast<std::uint32_t>(sizeof(linePoint));
    appendRecord(batch, 2, static_cast<std::uint32_t>(config_.lineBufferId),
                 lineOffset, linePoint, sizeof(linePoint));
  }

  queue_.push(std::move(batch));
}

void FakeDataSource::writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void FakeDataSource::appendRecord(std::vector<std::uint8_t>& batch,
                                   std::uint8_t op, std::uint32_t bufferId,
                                   std::uint32_t offset, const void* payload,
                                   std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

} // namespace dc
