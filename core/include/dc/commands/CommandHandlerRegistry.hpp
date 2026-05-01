// ENC-90: registry of command handlers, mirroring the
// NodeTypeRegistry pattern from gma_v3. Adding a new command kind
// means adding a registration, not editing the central switch in
// CommandProcessor.cpp.
//
// PHASE 1: registry exists alongside the legacy switch. Migration of
// individual command kinds is incremental; each kind that registers
// here removes a corresponding case from CommandProcessor.cpp.
#pragma once

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CmdResult.hpp"

#include <rapidjson/document.h>

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dc {

/// Handler context. Kept tight — handlers receive the scene + registry
/// they're allowed to mutate, the JSON object representing the command,
/// and nothing else. Future: extend with a per-frame state argument
/// once the migration matures enough to identify what's actually needed.
struct CmdContext {
  Scene&            scene;
  ResourceRegistry& reg;
  const rapidjson::Value& obj;
};

using CommandHandlerFn = std::function<CmdResult(CmdContext&)>;

class CommandHandlerRegistry {
public:
  /// Singleton accessor. Mirrors NodeTypeRegistry::singleton() in gma_v3.
  static CommandHandlerRegistry& singleton() {
    static CommandHandlerRegistry s;
    return s;
  }

  /// Returns false if a handler with the same name was already registered.
  bool registerHandler(std::string name, CommandHandlerFn fn) {
    std::lock_guard<std::mutex> lk(mu_);
    auto [it, ok] = map_.emplace(std::move(name), std::move(fn));
    return ok;
  }

  /// Returns nullptr if no handler is registered for `name`. Caller-owned
  /// pointer; valid until the registry is cleared.
  const CommandHandlerFn* find(std::string_view name) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = map_.find(std::string(name));
    return it == map_.end() ? nullptr : &it->second;
  }

  bool contains(std::string_view name) const { return find(name) != nullptr; }

  std::size_t size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return map_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lk(mu_);
    map_.clear();
  }

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, CommandHandlerFn> map_;
};

}  // namespace dc
