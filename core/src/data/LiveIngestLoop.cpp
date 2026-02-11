#include "dc/data/LiveIngestLoop.hpp"
#include "dc/data/DataSource.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/viewport/Viewport.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>

namespace dc {

void LiveIngestLoop::setConfig(const LiveIngestLoopConfig& cfg) {
  config_ = cfg;
}

void LiveIngestLoop::addBinding(const BufferGeometryBinding& binding) {
  bindings_.push_back(binding);
}

void LiveIngestLoop::clearBindings() { bindings_.clear(); }

void LiveIngestLoop::setViewport(Viewport* vp) { viewport_ = vp; }

std::vector<Id> LiveIngestLoop::consumeAndUpdate(DataSource& source,
                                                  IngestProcessor& ingest,
                                                  CommandProcessor& cp) {
  // 1. Drain all available batches
  std::vector<std::uint8_t> batch;
  std::unordered_set<Id> touchedSet;

  while (source.poll(batch)) {
    auto result =
        ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
    for (Id id : result.touchedBufferIds) {
      touchedSet.insert(id);
    }
  }

  if (touchedSet.empty()) return {};

  // 2. Update vertex counts for all bindings whose buffers were touched
  for (auto& b : bindings_) {
    if (touchedSet.count(b.bufferId) == 0) continue;
    auto sz = ingest.getBufferSize(b.bufferId);
    std::uint32_t vc = (b.bytesPerVertex > 0) ? (sz / b.bytesPerVertex) : 0;
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":)" +
                     std::to_string(b.geometryId) + R"(,"vertexCount":)" +
                     std::to_string(vc) + "}");
  }

  // 3. Auto-scroll and auto-scale viewport
  if (viewport_) {
    for (auto& b : bindings_) {
      if (b.bytesPerVertex != 24) continue; // candle6 only
      auto sz = ingest.getBufferSize(b.bufferId);
      if (sz < 24) continue;

      std::uint32_t numCandles = sz / 24;
      const auto* data = ingest.getBufferData(b.bufferId);

      if (config_.autoScrollX) {
        float lastX;
        std::memcpy(&lastX, data + (numCandles - 1) * 24, sizeof(float));
        const auto& dr = viewport_->dataRange();
        double xSpan = dr.xMax - dr.xMin;
        double margin = xSpan * static_cast<double>(config_.scrollMargin);
        double newXMax = static_cast<double>(lastX) + margin;
        double newXMin = newXMax - xSpan;
        viewport_->setDataRange(newXMin, newXMax, dr.yMin, dr.yMax);
      }

      if (config_.autoScaleY) {
        const auto& dr = viewport_->dataRange();
        float yMin = 1e9f, yMax = -1e9f;
        for (std::uint32_t i = 0; i < numCandles; i++) {
          float x, high, low;
          std::memcpy(&x, data + i * 24, sizeof(float));
          std::memcpy(&high, data + i * 24 + 8, sizeof(float));
          std::memcpy(&low, data + i * 24 + 12, sizeof(float));

          if (static_cast<double>(x) < dr.xMin ||
              static_cast<double>(x) > dr.xMax)
            continue;
          yMin = std::min(yMin, low);
          yMax = std::max(yMax, high);
        }
        if (yMin < yMax) {
          float padding = (yMax - yMin) * 0.05f;
          viewport_->setDataRange(
              dr.xMin, dr.xMax,
              static_cast<double>(yMin - padding),
              static_cast<double>(yMax + padding));
        }
      }
      break; // only process first candle binding
    }
  }

  return std::vector<Id>(touchedSet.begin(), touchedSet.end());
}

} // namespace dc
