#pragma once
#include <cstddef>
#include <mutex>
#include <queue>

namespace dc {

template <typename T>
class ThreadSafeQueue {
public:
  explicit ThreadSafeQueue(std::size_t maxCapacity = 256)
      : maxCap_(maxCapacity) {}

  bool push(T item) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.size() >= maxCap_) {
      queue_.pop(); // drop oldest
    }
    queue_.push(std::move(item));
    return true;
  }

  bool pop(T& out) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    while (!queue_.empty()) queue_.pop();
  }

private:
  mutable std::mutex mtx_;
  std::queue<T> queue_;
  std::size_t maxCap_;
};

} // namespace dc
