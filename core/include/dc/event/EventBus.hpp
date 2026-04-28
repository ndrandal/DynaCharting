#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace dc {

enum class EventType : std::uint8_t {
  ViewportChanged,
  SelectionChanged,
  DataChanged,
  HoverChanged,
  GeometryClicked,
  DrawItemVisibilityChanged,
  FrameCommitted,
  // Emitted when an ingest queue drops an item due to capacity.
  // payload[0] = cumulative dropped count, payload[1] = queue capacity.
  IngestDropped
};

struct EventData {
  EventType type;
  Id targetId{0};
  double payload[4] = {0, 0, 0, 0};
};

using EventCallback = std::function<void(const EventData&)>;
using SubscriptionId = std::uint32_t;

class EventBus {
public:
  SubscriptionId subscribe(EventType type, EventCallback cb);
  void unsubscribe(SubscriptionId id);
  void emit(const EventData& event);
  void clear();
  std::size_t subscriberCount(EventType type) const;

private:
  struct Subscription {
    SubscriptionId id;
    EventType type;
    EventCallback callback;
  };
  SubscriptionId nextId_{1};
  std::vector<Subscription> subs_;
};

} // namespace dc
