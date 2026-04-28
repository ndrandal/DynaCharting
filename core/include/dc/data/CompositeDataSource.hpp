#pragma once
#include "dc/data/DataSource.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace dc {

// A DataSource that owns N child sources and drains them round-robin on poll().
// Used for charts that combine multiple independent feeds (e.g. price +
// orderbook + news). No API change to ChartSession: callers wrap their sources
// in a CompositeDataSource and pass it to ChartSession::update() as usual.
//
// Backpressure (D81): each child reports its own drop events via the
// OverflowCallback set by setOverflowCallback(); the composite forwards the
// same callback to every child, so a single EventBus sees drops from any
// source.
class CompositeDataSource : public DataSource {
public:
  CompositeDataSource() = default;
  ~CompositeDataSource() override = default;

  // Takes ownership. Must not be called while the composite is running.
  void add(std::unique_ptr<DataSource> child);
  std::size_t childCount() const { return children_.size(); }

  void start() override;
  void stop() override;
  bool poll(std::vector<std::uint8_t>& batch) override;
  bool isRunning() const override;

  void setOverflowCallback(OverflowCallback cb) override;
  // Sum of dropped counts across all children.
  std::uint64_t droppedCount() const override;
  // Max capacity across children (informational only).
  std::size_t queueCapacity() const override;

private:
  std::vector<std::unique_ptr<DataSource>> children_;
  std::size_t nextIdx_{0};
};

} // namespace dc
