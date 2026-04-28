// D81.2: CompositeDataSource — round-robin drain across multiple child sources.
#include "dc/data/CompositeDataSource.hpp"
#include "dc/data/DataSource.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <queue>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

// Deterministic in-memory DataSource backed by a pre-loaded queue.
class CannedSource : public dc::DataSource {
public:
  explicit CannedSource(std::uint8_t tag) : tag_(tag) {}
  void start() override { running_ = true; started_++; }
  void stop() override { running_ = false; stopped_++; }
  bool poll(std::vector<std::uint8_t>& batch) override {
    if (batches_.empty()) return false;
    batch = batches_.front();
    batches_.pop();
    return true;
  }
  bool isRunning() const override { return running_; }
  void setOverflowCallback(OverflowCallback cb) override { overflowCb_ = std::move(cb); }
  std::uint64_t droppedCount() const override { return dropped_; }
  std::size_t queueCapacity() const override { return 99; }

  void preload(int n) {
    for (int i = 0; i < n; ++i) {
      batches_.push(std::vector<std::uint8_t>{tag_, static_cast<std::uint8_t>(i)});
    }
  }
  void simulateDrop(std::uint64_t total) {
    dropped_ = total;
    if (overflowCb_) overflowCb_(total);
  }

  int started_{0}, stopped_{0};
private:
  std::uint8_t tag_;
  bool running_{false};
  std::queue<std::vector<std::uint8_t>> batches_;
  OverflowCallback overflowCb_;
  std::uint64_t dropped_{0};
};

static void testRoundRobinPoll() {
  auto a = std::make_unique<CannedSource>(0xAA);
  auto b = std::make_unique<CannedSource>(0xBB);
  a->preload(3);
  b->preload(3);
  auto* aPtr = a.get();
  auto* bPtr = b.get();

  dc::CompositeDataSource composite;
  composite.add(std::move(a));
  composite.add(std::move(b));
  requireTrue(composite.childCount() == 2, "childCount==2");

  composite.start();
  requireTrue(aPtr->started_ == 1 && bPtr->started_ == 1, "start propagates to both");
  requireTrue(composite.isRunning(), "composite running when any child running");

  // Round-robin: first poll returns from A (idx 0), second from B (idx 1), ...
  std::vector<std::uint8_t> batch;
  int aCount = 0, bCount = 0;
  while (composite.poll(batch)) {
    if (!batch.empty() && batch[0] == 0xAA) ++aCount;
    else if (!batch.empty() && batch[0] == 0xBB) ++bCount;
    batch.clear();
  }
  requireTrue(aCount == 3, "drained all of A");
  requireTrue(bCount == 3, "drained all of B");

  composite.stop();
  requireTrue(aPtr->stopped_ == 1 && bPtr->stopped_ == 1, "stop propagates to both");
  requireTrue(!composite.isRunning(), "composite not running when all children stopped");
}

static void testOverflowFanOut() {
  auto a = std::make_unique<CannedSource>(0xAA);
  auto b = std::make_unique<CannedSource>(0xBB);
  auto* aPtr = a.get();
  auto* bPtr = b.get();

  dc::CompositeDataSource composite;
  composite.add(std::move(a));
  composite.add(std::move(b));

  std::uint64_t lastSeen = 0;
  int callCount = 0;
  composite.setOverflowCallback([&](std::uint64_t n) {
    lastSeen = n;
    ++callCount;
  });

  aPtr->simulateDrop(5);
  requireTrue(callCount == 1 && lastSeen == 5, "drop from child A reaches listener");

  bPtr->simulateDrop(7);
  requireTrue(callCount == 2 && lastSeen == 7, "drop from child B reaches listener");

  requireTrue(composite.droppedCount() == 12, "composite droppedCount sums children");
}

static void testEmptyComposite() {
  dc::CompositeDataSource composite;
  std::vector<std::uint8_t> batch;
  requireTrue(!composite.poll(batch), "empty composite poll returns false");
  requireTrue(!composite.isRunning(), "empty composite not running");
  composite.start();
  composite.stop();           // must not crash
  requireTrue(composite.childCount() == 0, "still zero children");
}

int main() {
  testRoundRobinPoll();
  testOverflowFanOut();
  testEmptyComposite();
  std::fprintf(stdout, "D81.2 composite source: OK\n");
  return 0;
}
