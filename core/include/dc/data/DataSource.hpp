#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace dc {

class DataSource {
public:
  using OverflowCallback = std::function<void(std::uint64_t totalDropped)>;

  virtual ~DataSource() = default;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual bool poll(std::vector<std::uint8_t>& batch) = 0;
  virtual bool isRunning() const = 0;

  // Backpressure: default no-op; sources that buffer internally should override
  // to forward queue-drop events to the given callback. Called off the main
  // thread by default.
  virtual void setOverflowCallback(OverflowCallback /*cb*/) {}
  virtual std::uint64_t droppedCount() const { return 0; }
  virtual std::size_t queueCapacity() const { return 0; }
};

} // namespace dc
