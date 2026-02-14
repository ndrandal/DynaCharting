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
  if (config_.enableAggregation) {
    aggManager_.setConfig(config_.aggregation);
  }
}

void ChartSession::setViewport(Viewport* vp) {
  viewport_ = vp;
  loop_.setViewport(vp);
}

void ChartSession::addPaneViewport(Id paneId, Viewport* vp, Id transformId) {
  // Replace if paneId already exists
  for (auto& pv : paneViewports_) {
    if (pv.paneId == paneId) {
      pv.viewport = vp;
      pv.transformId = transformId;
      return;
    }
  }
  paneViewports_.push_back({paneId, vp, transformId});

  // Use first pane viewport for auto-scroll if no legacy viewport set
  if (!viewport_ && !paneViewports_.empty()) {
    loop_.setViewport(paneViewports_[0].viewport);
  }

  managedTransforms_.insert(transformId);
}

void ChartSession::removePaneViewport(Id paneId) {
  for (auto it = paneViewports_.begin(); it != paneViewports_.end(); ++it) {
    if (it->paneId == paneId) {
      // Check if transform is still used by another pane viewport
      Id xfId = it->transformId;
      paneViewports_.erase(it);
      bool stillUsed = false;
      for (const auto& pv : paneViewports_) {
        if (pv.transformId == xfId) { stillUsed = true; break; }
      }
      // Also check mounted slots
      if (!stillUsed) {
        for (const auto& [h, s] : slots_) {
          if (s.sharedTransformId == xfId) { stillUsed = true; break; }
        }
      }
      if (!stillUsed) managedTransforms_.erase(xfId);
      break;
    }
  }
}

void ChartSession::clearPaneViewports() {
  paneViewports_.clear();
}

void ChartSession::setLinkXAxis(bool link) {
  linkXAxis_ = link;
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

  // 5. Setup aggregation bindings (D8.5)
  if (config_.enableAggregation) {
    setupAggregation(slot);
  }

  slot.recipe = std::move(recipe);
  slots_.emplace(handle, std::move(slot));

  // 6. Rebuild all LiveIngestLoop bindings
  rebuildBindings();

  return handle;
}

void ChartSession::setupAggregation(const MountedSlot& slot) {
  for (const auto& sub : slot.buildResult.subscriptions) {
    if (sub.format != VertexFormat::Candle6) continue;

    Id aggBufId = sub.bufferId + config_.aggregation.aggBufferIdOffset;

    // Create the agg buffer in scene
    cp_.applyJsonText(
      R"({"cmd":"createBuffer","id":)" + std::to_string(aggBufId) +
      R"(,"byteLength":0})");

    ingest_.ensureBuffer(aggBufId);

    aggManager_.addBinding({sub.bufferId, aggBufId, sub.geometryId});
  }
}

void ChartSession::unmount(RecipeHandle handle) {
  auto it = slots_.find(handle);
  if (it == slots_.end()) return;

  auto& slot = it->second;

  // 1. Apply dispose commands
  for (const auto& cmd : slot.buildResult.disposeCommands) {
    cp_.applyJsonText(cmd);
  }

  // 2. Clean up aggregation buffers
  if (config_.enableAggregation) {
    for (const auto& sub : slot.buildResult.subscriptions) {
      if (sub.format != VertexFormat::Candle6) continue;
      Id aggBufId = sub.bufferId + config_.aggregation.aggBufferIdOffset;
      cp_.applyJsonText(R"({"cmd":"delete","id":)" + std::to_string(aggBufId) + "}");
    }
    // Rebuild aggregation bindings from remaining slots
    aggManager_.clearBindings();
  }

  // 3. Remove compute dependencies for this handle
  for (auto& [bufId, handles] : computeDeps_) {
    handles.erase(
      std::remove(handles.begin(), handles.end(), handle),
      handles.end());
  }

  // 4. Check if the shared transform is still used by another slot
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

  // 5. Erase slot
  slots_.erase(it);

  // 6. Rebuild aggregation bindings from remaining slots
  if (config_.enableAggregation) {
    for (const auto& [h, s] : slots_) {
      for (const auto& sub : s.buildResult.subscriptions) {
        if (sub.format != VertexFormat::Candle6) continue;
        Id aggBufId = sub.bufferId + config_.aggregation.aggBufferIdOffset;
        aggManager_.addBinding({sub.bufferId, aggBufId, sub.geometryId});
      }
    }
  }

  // 7. Rebuild bindings from remaining mounted recipes
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

void ChartSession::setRecomputeOnViewportChange(RecipeHandle handle, bool enable) {
  auto it = slots_.find(handle);
  if (it != slots_.end()) {
    it->second.recomputeOnViewportChange = enable;
  }
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

    // 2b. Aggregation: recompute agg buffers for touched raw data (D8.5)
    if (config_.enableAggregation) {
      auto aggTouched = aggManager_.onRawDataChanged(touched, ingest_);
      for (Id id : aggTouched) {
        touchedSet.insert(id);
      }
    }

    result.touchedBufferIds.assign(touchedSet.begin(), touchedSet.end());
  }

  // 3. Sync managed transforms
  if (!paneViewports_.empty()) {
    // Multi-viewport mode: each pane viewport syncs its own transform
    for (const auto& pv : paneViewports_) {
      if (pv.viewport && pv.transformId != 0) {
        syncTransformFromViewport(pv.transformId, pv.viewport);
      }
    }

    // X-axis linking: propagate primary viewport X range to all others
    if (linkXAxis_ && paneViewports_.size() > 1) {
      const auto& primary = paneViewports_[0];
      if (primary.viewport) {
        const auto& pdr = primary.viewport->dataRange();
        for (std::size_t i = 1; i < paneViewports_.size(); i++) {
          if (paneViewports_[i].viewport) {
            const auto& dr = paneViewports_[i].viewport->dataRange();
            paneViewports_[i].viewport->setDataRange(pdr.xMin, pdr.xMax, dr.yMin, dr.yMax);
          }
        }
      }
    }

    result.viewportChanged = true;
  } else if (viewport_ && !managedTransforms_.empty()) {
    // Legacy single-viewport mode
    for (Id xfId : managedTransforms_) {
      syncTransform(xfId);
    }
    result.viewportChanged = true;
  }

  // 3b. Viewport-triggered recompute (D10.3)
  if (result.viewportChanged) {
    for (auto& [h, slot] : slots_) {
      if (slot.recomputeOnViewportChange && slot.computeCallback) {
        auto computedIds = slot.computeCallback();
        for (Id id : computedIds) {
          bool found = false;
          for (Id existing : result.touchedBufferIds) {
            if (existing == id) { found = true; break; }
          }
          if (!found) result.touchedBufferIds.push_back(id);
        }
      }
    }
  }

  // 4. Aggregation: respond to viewport changes (D8.5)
  Viewport* primaryVp = viewport_;
  if (!primaryVp && !paneViewports_.empty()) {
    primaryVp = paneViewports_[0].viewport;
  }
  if (config_.enableAggregation && primaryVp) {
    double ppdu = primaryVp->pixelsPerDataUnitX();
    auto aggVpTouched = aggManager_.onViewportChanged(ppdu, ingest_, cp_);
    if (!aggVpTouched.empty()) {
      result.resolutionChanged = true;
      for (Id id : aggVpTouched) {
        // Add to touched set
        bool found = false;
        for (Id existing : result.touchedBufferIds) {
          if (existing == id) { found = true; break; }
        }
        if (!found) result.touchedBufferIds.push_back(id);
      }
    }
  }

  // 5. Smart retention: dynamically adjust maxBytes (D8.5)
  if (config_.enableSmartRetention && primaryVp) {
    double visW = primaryVp->visibleDataWidth();
    for (const auto& [handle, slot] : slots_) {
      for (const auto& sub : slot.buildResult.subscriptions) {
        std::uint32_t stride = strideOf(sub.format);
        double records = visW; // 1 record per data unit (approximation)
        double bytes = records * stride * config_.smartRetention.retentionMultiplier;
        auto clamped = static_cast<std::uint32_t>(
          std::max(static_cast<double>(config_.smartRetention.minRetention),
            std::min(bytes, static_cast<double>(config_.smartRetention.maxRetention))));
        ingest_.setMaxBytes(sub.bufferId, clamped);
      }
    }
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

void ChartSession::syncTransformFromViewport(Id transformId, Viewport* vp) {
  if (!vp) return;
  auto tp = vp->computeTransformParams();
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
