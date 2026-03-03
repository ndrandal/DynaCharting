#include "dc/session/SessionBridge.hpp"
#include <algorithm>

namespace dc {

// ---- SessionGroup ----

std::uint32_t SessionGroup::addSession() {
  std::uint32_t id = nextId_++;
  sessions_.push_back({id, {}});
  return id;
}

void SessionGroup::removeSession(std::uint32_t sessionId) {
  sessions_.erase(
    std::remove_if(sessions_.begin(), sessions_.end(),
      [sessionId](const SessionEntry& e) { return e.id == sessionId; }),
    sessions_.end());
}

void SessionGroup::subscribe(std::uint32_t sessionId, SyncEventType type, SyncCallback callback) {
  for (auto& entry : sessions_) {
    if (entry.id == sessionId) {
      entry.callbacks[static_cast<std::uint8_t>(type)] = std::move(callback);
      return;
    }
  }
}

void SessionGroup::publish(const SyncEvent& event) {
  auto typeKey = static_cast<std::uint8_t>(event.type);

  // Check if this sync type is enabled
  if (typeKey < 4 && !syncEnabled_[typeKey]) {
    return;
  }

  // Deliver to all sessions EXCEPT the source
  for (auto& entry : sessions_) {
    if (entry.id == event.sourceSessionId) continue;
    auto it = entry.callbacks.find(typeKey);
    if (it != entry.callbacks.end() && it->second) {
      it->second(event);
    }
  }
}

void SessionGroup::setSyncEnabled(SyncEventType type, bool enabled) {
  auto idx = static_cast<std::uint8_t>(type);
  if (idx < 4) {
    syncEnabled_[idx] = enabled;
  }
}

bool SessionGroup::isSyncEnabled(SyncEventType type) const {
  auto idx = static_cast<std::uint8_t>(type);
  if (idx < 4) return syncEnabled_[idx];
  return false;
}

std::size_t SessionGroup::sessionCount() const {
  return sessions_.size();
}

std::vector<std::uint32_t> SessionGroup::sessionIds() const {
  std::vector<std::uint32_t> ids;
  ids.reserve(sessions_.size());
  for (const auto& entry : sessions_) {
    ids.push_back(entry.id);
  }
  return ids;
}

// ---- SessionBridge ----

std::uint32_t SessionBridge::createGroup() {
  std::uint32_t id = nextGroupId_++;
  groups_.emplace(id, SessionGroup{});
  return id;
}

void SessionBridge::removeGroup(std::uint32_t groupId) {
  groups_.erase(groupId);
}

SessionGroup* SessionBridge::getGroup(std::uint32_t groupId) {
  auto it = groups_.find(groupId);
  if (it != groups_.end()) return &it->second;
  return nullptr;
}

bool SessionBridge::hasGroup(std::uint32_t groupId) const {
  return groups_.count(groupId) > 0;
}

std::uint32_t SessionBridge::addSessionToGroup(std::uint32_t groupId) {
  auto* group = getGroup(groupId);
  if (!group) return 0;
  return group->addSession();
}

std::size_t SessionBridge::groupCount() const {
  return groups_.size();
}

} // namespace dc
