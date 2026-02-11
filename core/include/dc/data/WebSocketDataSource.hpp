#pragma once
#include "dc/data/DataSource.hpp"
#include "dc/data/ThreadSafeQueue.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace dc {

struct WebSocketDataSourceConfig {
  std::string url;                // ws://host:port/path
  int reconnectIntervalMs{3000};  // auto-reconnect delay
  std::size_t maxQueueSize{64};   // max pending batches
};

class WebSocketDataSource : public DataSource {
public:
  explicit WebSocketDataSource(const WebSocketDataSourceConfig& config);
  ~WebSocketDataSource() override;

  void start() override;
  void stop() override;
  bool poll(std::vector<std::uint8_t>& batch) override;
  bool isRunning() const override;

  enum class Status { disconnected, connecting, connected, error };
  Status status() const;

private:
  void receiveLoop();

  WebSocketDataSourceConfig config_;
  ThreadSafeQueue<std::vector<std::uint8_t>> queue_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<Status> status_{Status::disconnected};
};

} // namespace dc
