// D81.1: Ingest backpressure — ThreadSafeQueue drop counter + overflow callback,
// plus ChartSession -> EventBus wiring.
#include "dc/data/ThreadSafeQueue.hpp"
#include "dc/data/FakeDataSource.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/session/ChartSession.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireEq(std::uint64_t got, std::uint64_t want, const char* ctx) {
  if (got != want) {
    std::fprintf(stderr, "ASSERT FAIL [%s]: got=%llu want=%llu\n",
                 ctx, (unsigned long long)got, (unsigned long long)want);
    std::exit(1);
  }
}

// Case 1: Pure ThreadSafeQueue — floods past capacity, drop counter and callback fire.
static void testDropCounting() {
  dc::ThreadSafeQueue<int> q(4);
  std::atomic<std::uint64_t> lastCbCount{0};
  std::atomic<int> cbFired{0};
  q.setOverflowCallback([&](std::uint64_t n) {
    lastCbCount.store(n);
    cbFired.fetch_add(1);
  });

  for (int i = 0; i < 100; ++i) q.push(i);

  requireEq(q.droppedCount(), 96, "droppedCount after 100 pushes, cap=4");
  requireTrue(cbFired.load() == 96, "callback fired once per drop");
  requireEq(lastCbCount.load(), 96, "callback saw final cumulative count");

  // The queue still holds 4 items (the most recent ones).
  requireEq(q.size(), 4, "queue size stays at cap");

  // Pop order is oldest-first of the surviving tail.
  int out = 0;
  q.pop(out); requireEq(out, 96, "first survivor");
  q.pop(out); requireEq(out, 97, "second survivor");
  q.pop(out); requireEq(out, 98, "third survivor");
  q.pop(out); requireEq(out, 99, "fourth survivor");
  requireTrue(!q.pop(out), "empty after draining");
}

// Case 2: FakeDataSource forwards queue overflow callback through its own API.
static void testFakeSourceOverflow() {
  dc::FakeDataSourceConfig cfg{};
  cfg.candleBufferId = 100;
  cfg.lineBufferId = 101;
  cfg.tickIntervalMs = 1;          // produce fast
  cfg.candleIntervalMs = 10;
  dc::FakeDataSource src(cfg);

  std::atomic<std::uint64_t> dropped{0};
  src.setOverflowCallback([&](std::uint64_t n) { dropped.store(n); });

  requireTrue(src.queueCapacity() > 0, "fake source has a queue capacity");

  src.start();
  // Let it produce without draining — queue must overflow eventually.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  src.stop();

  requireTrue(dropped.load() > 0,
              "FakeDataSource should report dropped batches when never drained");
  requireEq(src.droppedCount(), dropped.load(),
            "droppedCount() matches last callback value");
}

// Case 3: ChartSession drains drop counters off the background thread
// and emits EventType::IngestDropped on the main thread via update().
static void testSessionEmitsDropEvent() {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  dc::EventBus bus;
  dc::ChartSession session(cp, ingest);
  session.setEventBus(&bus);

  std::atomic<int> eventCount{0};
  std::atomic<std::uint64_t> lastDroppedSeen{0};
  bus.subscribe(dc::EventType::IngestDropped,
                [&](const dc::EventData& ev) {
                  eventCount.fetch_add(1);
                  lastDroppedSeen.store(static_cast<std::uint64_t>(ev.payload[0]));
                });

  dc::FakeDataSourceConfig cfg{};
  cfg.candleBufferId = 200;
  cfg.lineBufferId = 201;
  cfg.tickIntervalMs = 1;
  cfg.candleIntervalMs = 10;
  dc::FakeDataSource src(cfg);
  session.attachDataSource(src);

  src.start();
  // Let the source flood its queue (session.update not called).
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Now pump update() once on the main thread — this should emit exactly one
  // IngestDropped event reflecting the latched drop count.
  session.update(src);
  src.stop();

  requireTrue(eventCount.load() >= 1,
              "session emitted IngestDropped on first update() after overflow");
  requireTrue(lastDroppedSeen.load() > 0,
              "IngestDropped payload[0] is the cumulative drop count");
}

// Case 4: deterministic re-emit guard — without a background thread, verify
// that update() only emits once when the drop count hasn't advanced.
static void testSessionDoesNotReEmit() {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  dc::EventBus bus;
  dc::ChartSession session(cp, ingest);
  session.setEventBus(&bus);

  int eventCount = 0;
  bus.subscribe(dc::EventType::IngestDropped,
                [&](const dc::EventData&) { ++eventCount; });

  // Minimal DataSource that latches a drop count via the session's callback
  // exactly once, then never advances it.
  struct StaticSource : public dc::DataSource {
    OverflowCallback cb;
    bool running{false};
    std::size_t cap{16};
    void start() override { running = true; }
    void stop() override { running = false; }
    bool poll(std::vector<std::uint8_t>&) override { return false; }
    bool isRunning() const override { return running; }
    void setOverflowCallback(OverflowCallback c) override { cb = std::move(c); }
    std::uint64_t droppedCount() const override { return 0; }
    std::size_t queueCapacity() const override { return cap; }
  } src;

  session.attachDataSource(src);
  src.cb(42);                    // simulate one overflow

  session.update(src);
  requireTrue(eventCount == 1, "first update emits IngestDropped");
  session.update(src);
  requireTrue(eventCount == 1, "second update does not re-emit");

  src.cb(43);                    // one more overflow
  session.update(src);
  requireTrue(eventCount == 2, "advancing drop count re-emits once");
}

int main() {
  testDropCounting();
  testFakeSourceOverflow();
  testSessionEmitsDropEvent();
  testSessionDoesNotReEmit();
  std::fprintf(stdout, "D81.1 backpressure: OK\n");
  return 0;
}
