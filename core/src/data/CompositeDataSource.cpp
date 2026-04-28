#include "dc/data/CompositeDataSource.hpp"

namespace dc {

void CompositeDataSource::add(std::unique_ptr<DataSource> child) {
  if (child) children_.push_back(std::move(child));
}

void CompositeDataSource::start() {
  for (auto& c : children_) c->start();
}

void CompositeDataSource::stop() {
  for (auto& c : children_) c->stop();
}

bool CompositeDataSource::isRunning() const {
  for (const auto& c : children_) {
    if (c->isRunning()) return true;
  }
  return false;
}

bool CompositeDataSource::poll(std::vector<std::uint8_t>& batch) {
  // Round-robin one poll per call. Scan up to N children starting from
  // nextIdx_ so we don't starve a child when earlier ones are empty.
  const std::size_t n = children_.size();
  if (n == 0) return false;
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t idx = (nextIdx_ + i) % n;
    if (children_[idx]->poll(batch)) {
      nextIdx_ = (idx + 1) % n;
      return true;
    }
  }
  return false;
}

void CompositeDataSource::setOverflowCallback(OverflowCallback cb) {
  // Each child receives the same callback; drops from any source reach the
  // same listener. The child is responsible for invoking the callback on its
  // own cumulative count.
  for (auto& c : children_) c->setOverflowCallback(cb);
}

std::uint64_t CompositeDataSource::droppedCount() const {
  std::uint64_t sum = 0;
  for (const auto& c : children_) sum += c->droppedCount();
  return sum;
}

std::size_t CompositeDataSource::queueCapacity() const {
  std::size_t maxCap = 0;
  for (const auto& c : children_) {
    auto cap = c->queueCapacity();
    if (cap > maxCap) maxCap = cap;
  }
  return maxCap;
}

} // namespace dc
