#include "dc/event/EventBus.hpp"
#include <algorithm>

namespace dc {

SubscriptionId EventBus::subscribe(EventType type, EventCallback cb) {
  SubscriptionId id = nextId_++;
  subs_.push_back({id, type, std::move(cb)});
  return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
  subs_.erase(
    std::remove_if(subs_.begin(), subs_.end(),
      [id](const Subscription& s) { return s.id == id; }),
    subs_.end());
}

void EventBus::emit(const EventData& event) {
  auto copy = subs_;
  for (const auto& s : copy) {
    if (s.type == event.type && s.callback) {
      s.callback(event);
    }
  }
}

void EventBus::clear() {
  subs_.clear();
}

std::size_t EventBus::subscriberCount(EventType type) const {
  std::size_t count = 0;
  for (const auto& s : subs_) {
    if (s.type == type) ++count;
  }
  return count;
}

} // namespace dc
