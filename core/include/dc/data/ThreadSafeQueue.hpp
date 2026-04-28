#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>

namespace dc {

template <typename T>
class ThreadSafeQueue {
public:
  // Invoked after a push drops an older item due to capacity.
  // Argument is the cumulative dropped count. Called outside the internal lock.
  using OverflowCallback = std::function<void(std::uint64_t totalDropped)>;

  explicit ThreadSafeQueue(std::size_t maxCapacity = 256)
      : maxCap_(maxCapacity) {}

  bool push(T item) {
    bool overflowed = false;
    std::uint64_t droppedNow = 0;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (queue_.size() >= maxCap_) {
        queue_.pop();
        ++dropped_;
        overflowed = true;
        droppedNow = dropped_;
      }
      queue_.push(std::move(item));
    }
    if (overflowed && overflowCb_) {
      overflowCb_(droppedNow);
    }
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

  std::size_t capacity() const { return maxCap_; }

  std::uint64_t droppedCount() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return dropped_;
  }

  void setOverflowCallback(OverflowCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    overflowCb_ = std::move(cb);
  }

private:
  mutable std::mutex mtx_;
  std::queue<T> queue_;
  std::size_t maxCap_;
  std::uint64_t dropped_{0};
  OverflowCallback overflowCb_;
};

} // namespace dc
