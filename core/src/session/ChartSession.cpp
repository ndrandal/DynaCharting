#include "dc/session/ChartSession.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/data/DataSource.hpp"
#include "dc/viewport/Viewport.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_set>

namespace dc {

ChartSession::ChartSession(CommandProcessor& cp, IngestProcessor& ingest)
  : cp_(cp), ingest_(ingest) {}

void ChartSession::setConfig(const ChartSessionConfig& cfg) {
  config_ = cfg;
}

void ChartSession::setViewport(Viewport* vp) {
  viewport_ = vp;
  loop_.setViewport(vp);
}

RecipeHandle ChartSession::mount(std::unique_ptr<Recipe> recipe,
                                  Id sharedTransformId) {
  RecipeHandle handle = nextHandle_++;
  MountedSlot slot;
  slot.sharedTransformId = sharedTransformId;

  // 1. Build the recipe
  slot.buildResult = recipe->build();

  // 2. Apply create commands
  for (const auto& cmd : slot.buildResult.createCommands) {
    cp_.applyJsonText(cmd);
  }

  // 3. Attach shared transform to all draw items if specified
  if (sharedTransformId != 0) {
    for (Id diId : recipe->drawItemIds()) {
      cp_.applyJsonText(
        R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(diId) +
        R"(,"transformId":)" + std::to_string(sharedTransformId) + "}");
    }
    managedTransforms_.insert(sharedTransformId);
  }

  // 4. Apply retention caps for each subscription buffer
  for (const auto& sub : slot.buildResult.subscriptions) {
    ingest_.setMaxBytes(sub.bufferId, config_.retention.maxBytesPerBuffer);
  }

  slot.recipe = std::move(recipe);
  slots_.emplace(handle, std::move(slot));

  // 5. Rebuild all LiveIngestLoop bindings
  rebuildBindings();

  return handle;
}

void ChartSession::unmount(RecipeHandle handle) {
  auto it = slots_.find(handle);
  if (it == slots_.end()) return;

  auto& slot = it->second;

  // 1. Apply dispose commands
  for (const auto& cmd : slot.buildResult.disposeCommands) {
    cp_.applyJsonText(cmd);
  }

  // 2. Remove compute dependencies for this handle
  for (auto& [bufId, handles] : computeDeps_) {
    handles.erase(
      std::remove(handles.begin(), handles.end(), handle),
      handles.end());
  }

  // 3. Check if the shared transform is still used by another slot
  if (slot.sharedTransformId != 0) {
    bool stillUsed = false;
    for (const auto& [h, s] : slots_) {
      if (h != handle && s.sharedTransformId == slot.sharedTransformId) {
        stillUsed = true;
        break;
      }
    }
    if (!stillUsed) {
      managedTransforms_.erase(slot.sharedTransformId);
    }
  }

  // 4. Erase slot
  slots_.erase(it);

  // 5. Rebuild bindings from remaining mounted recipes
  rebuildBindings();
}

void ChartSession::unmountAll() {
  // Collect handles first to avoid modifying map while iterating
  std::vector<RecipeHandle> handles;
  handles.reserve(slots_.size());
  for (const auto& [h, s] : slots_) {
    handles.push_back(h);
  }
  for (auto h : handles) {
    unmount(h);
  }
}

bool ChartSession::isMounted(RecipeHandle handle) const {
  return slots_.count(handle) > 0;
}

Recipe* ChartSession::getRecipe(RecipeHandle handle) {
  auto it = slots_.find(handle);
  return (it != slots_.end()) ? it->second.recipe.get() : nullptr;
}

void ChartSession::setComputeCallback(RecipeHandle handle,
                                       std::function<std::vector<Id>()> cb) {
  auto it = slots_.find(handle);
  if (it != slots_.end()) {
    it->second.computeCallback = std::move(cb);
  }
}

void ChartSession::addComputeDependency(RecipeHandle handle,
                                         Id upstreamBufferId) {
  computeDeps_[upstreamBufferId].push_back(handle);
}

FrameResult ChartSession::update(DataSource& source) {
  FrameResult result;

  // 1. Drain data via LiveIngestLoop
  auto touched = loop_.consumeAndUpdate(source, ingest_, cp_);

  if (!touched.empty()) {
    result.dataChanged = true;

    // 2. Run compute callbacks for dependent recipes
    std::unordered_set<Id> touchedSet(touched.begin(), touched.end());
    std::vector<RecipeHandle> toCompute;
    for (Id bufId : touched) {
      auto depIt = computeDeps_.find(bufId);
      if (depIt != computeDeps_.end()) {
        for (RecipeHandle h : depIt->second) {
          toCompute.push_back(h);
        }
      }
    }

    // Deduplicate
    std::sort(toCompute.begin(), toCompute.end());
    toCompute.erase(std::unique(toCompute.begin(), toCompute.end()),
                    toCompute.end());

    for (RecipeHandle h : toCompute) {
      auto it = slots_.find(h);
      if (it != slots_.end() && it->second.computeCallback) {
        auto computedIds = it->second.computeCallback();
        for (Id id : computedIds) {
          touchedSet.insert(id);
        }
      }
    }

    result.touchedBufferIds.assign(touchedSet.begin(), touchedSet.end());
  }

  // 3. Sync managed transforms
  if (viewport_ && !managedTransforms_.empty()) {
    for (Id xfId : managedTransforms_) {
      syncTransform(xfId);
    }
    result.viewportChanged = true;
  }

  return result;
}

void ChartSession::syncTransform(Id transformId) {
  if (!viewport_) return;
  auto tp = viewport_->computeTransformParams();
  char buf[512];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setTransform","id":%llu,"tx":%.9g,"ty":%.9g,"sx":%.9g,"sy":%.9g})",
    static_cast<unsigned long long>(transformId),
    static_cast<double>(tp.tx), static_cast<double>(tp.ty),
    static_cast<double>(tp.sx), static_cast<double>(tp.sy));
  cp_.applyJsonText(buf);
}

void ChartSession::rebuildBindings() {
  loop_.clearBindings();
  for (const auto& [handle, slot] : slots_) {
    for (const auto& sub : slot.buildResult.subscriptions) {
      loop_.addBinding({sub.bufferId, sub.geometryId, strideOf(sub.format)});
    }
  }
}

} // namespace dc
