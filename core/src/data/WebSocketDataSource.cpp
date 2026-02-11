#include "dc/data/WebSocketDataSource.hpp"
#include <easywsclient/easywsclient.hpp>

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace dc {

WebSocketDataSource::WebSocketDataSource(const WebSocketDataSourceConfig& config)
    : config_(config), queue_(config.maxQueueSize) {}

WebSocketDataSource::~WebSocketDataSource() { stop(); }

void WebSocketDataSource::start() {
  if (running_.load()) return;
  running_.store(true);
  status_.store(Status::connecting);
  thread_ = std::thread(&WebSocketDataSource::receiveLoop, this);
}

void WebSocketDataSource::stop() {
  running_.store(false);
  if (thread_.joinable()) thread_.join();
  status_.store(Status::disconnected);
}

bool WebSocketDataSource::poll(std::vector<std::uint8_t>& batch) {
  return queue_.pop(batch);
}

bool WebSocketDataSource::isRunning() const { return running_.load(); }

WebSocketDataSource::Status WebSocketDataSource::status() const {
  return status_.load();
}

void WebSocketDataSource::receiveLoop() {
  while (running_.load()) {
    status_.store(Status::connecting);
    std::fprintf(stderr, "[WebSocketDataSource] connecting to %s\n",
                 config_.url.c_str());

    std::unique_ptr<easywsclient::WebSocket,
                     void (*)(easywsclient::WebSocket*)>
        ws(easywsclient::WebSocket::from_url(config_.url),
           [](easywsclient::WebSocket* p) {
             if (p && p != easywsclient::WebSocket::create_dummy()) delete p;
           });

    if (!ws || ws->getReadyState() == easywsclient::WebSocket::CLOSED) {
      std::fprintf(stderr,
                   "[WebSocketDataSource] connection failed, retrying in %dms\n",
                   config_.reconnectIntervalMs);
      status_.store(Status::error);
      // Wait for reconnect interval, checking running_ periodically
      auto deadline = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(config_.reconnectIntervalMs);
      while (running_.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      continue;
    }

    status_.store(Status::connected);
    std::fprintf(stderr, "[WebSocketDataSource] connected\n");

    while (running_.load() &&
           ws->getReadyState() != easywsclient::WebSocket::CLOSED) {
      ws->poll(10); // 10ms poll timeout
      ws->dispatchBinary([this](const std::vector<std::uint8_t>& msg) {
        queue_.push(std::vector<std::uint8_t>(msg));
      });
    }

    if (ws->getReadyState() != easywsclient::WebSocket::CLOSED) {
      ws->close();
      ws->poll(0);
    }

    if (running_.load()) {
      std::fprintf(stderr,
                   "[WebSocketDataSource] disconnected, reconnecting in %dms\n",
                   config_.reconnectIntervalMs);
      status_.store(Status::error);
      auto deadline = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(config_.reconnectIntervalMs);
      while (running_.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
  }

  status_.store(Status::disconnected);
}

} // namespace dc
