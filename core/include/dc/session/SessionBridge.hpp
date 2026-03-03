#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

// Events that can be synchronized across chart instances
enum class SyncEventType : std::uint8_t {
  CrosshairMove = 0,   // crosshair position changed
  TimeRangeChange = 1, // visible time range changed (pan/zoom X)
  SymbolChange = 2,    // symbol/instrument changed
  IntervalChange = 3   // timeframe changed
};

struct SyncEvent {
  SyncEventType type;
  std::uint32_t sourceSessionId{0};
  double dataX{0}, dataY{0};          // for CrosshairMove
  double rangeStart{0}, rangeEnd{0};  // for TimeRangeChange
  std::string symbol;                  // for SymbolChange
  std::string interval;                // for IntervalChange
};

using SyncCallback = std::function<void(const SyncEvent&)>;

// A group of linked sessions
class SessionGroup {
public:
  std::uint32_t addSession();
  void removeSession(std::uint32_t sessionId);

  // Subscribe to sync events
  void subscribe(std::uint32_t sessionId, SyncEventType type, SyncCallback callback);

  // Publish an event (broadcasts to all other sessions in group)
  void publish(const SyncEvent& event);

  // Enable/disable specific sync types
  void setSyncEnabled(SyncEventType type, bool enabled);
  bool isSyncEnabled(SyncEventType type) const;

  std::size_t sessionCount() const;
  std::vector<std::uint32_t> sessionIds() const;

private:
  struct SessionEntry {
    std::uint32_t id;
    std::unordered_map<std::uint8_t, SyncCallback> callbacks;
  };
  std::vector<SessionEntry> sessions_;
  std::uint32_t nextId_{1};
  bool syncEnabled_[4] = {true, true, true, true};
};

// Global bridge that manages multiple groups
class SessionBridge {
public:
  std::uint32_t createGroup();
  void removeGroup(std::uint32_t groupId);

  // Returns pointer to group. WARNING: pointer is invalidated by
  // createGroup() or removeGroup() (unordered_map rehash). Do not
  // store the returned pointer across calls that modify groups_.
  SessionGroup* getGroup(std::uint32_t groupId);

  bool hasGroup(std::uint32_t groupId) const;

  // Convenience: add a session to a group
  std::uint32_t addSessionToGroup(std::uint32_t groupId);

  std::size_t groupCount() const;

private:
  std::unordered_map<std::uint32_t, SessionGroup> groups_;
  std::uint32_t nextGroupId_{1};
};

} // namespace dc
