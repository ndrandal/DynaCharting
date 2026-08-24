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

void LiveIngestLoop::clearBindings() {
  bindings_.clear();
  // Bindings define which buffer the streaming auto-domain tracks; dropping them
  // invalidates the running [min,max]. Reset so a fresh set of bindings re-folds
  // from scratch instead of carrying a stale domain. (autoScaleFoldedCandles_ is a
  // cumulative lifetime counter — intentionally NOT reset.)
  autoScaleDomain_.reset();
  autoScaleBufferId_ = kInvalidId;
  autoScaleConsumedCandles_ = 0;
}

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
      if (numCandles == 0) continue;
      const auto* data = ingest.getBufferData(b.bufferId);
      if (!data) continue;

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
        // ENC-607 (P1.16) — O(Δ) streaming auto-domain via the P1.5 RunningDomain
        // reducer. Instead of rescanning all `numCandles` every tick (the removed
        // O(N) loop), fold ONLY the candles appended since the previous tick into a
        // persistent running [min,max]. Each candle's low and high are folded once,
        // ever; over an append-only stream the result is bit-identical to a full
        // brute-force min(low)/max(high). (Deliberate P1.5 semantic: this is a
        // full-data-domain auto-scale over the whole growing column — NOT the legacy
        // visible-X-window filter, which stays on AutoScale::computeYRange.)
        if (b.bufferId != autoScaleBufferId_ ||
            numCandles < autoScaleConsumedCandles_) {
          // Buffer swapped, or the row count went backwards (replaced/truncated):
          // the running state is stale — restart the reducer for this buffer.
          autoScaleDomain_.reset();
          autoScaleConsumedCandles_ = 0;
          autoScaleBufferId_ = b.bufferId;
        }
        for (std::uint32_t i = autoScaleConsumedCandles_; i < numCandles; i++) {
          float high, low;
          std::memcpy(&high, data + i * 24 + 8, sizeof(float));
          std::memcpy(&low, data + i * 24 + 12, sizeof(float));
          // low <= high per candle, so folding both yields domain.min == min(lows)
          // and domain.max == max(highs) in a single running domain.
          autoScaleDomain_.foldOne(static_cast<double>(low));
          autoScaleDomain_.foldOne(static_cast<double>(high));
          ++autoScaleFoldedCandles_;
        }
        autoScaleConsumedCandles_ = numCandles;

        const Domain& dom = autoScaleDomain_.domain();
        if (!dom.empty && dom.min < dom.max) {
          const auto& dr = viewport_->dataRange();
          double padding = (dom.max - dom.min) * 0.05;
          viewport_->setDataRange(dr.xMin, dr.xMax, dom.min - padding,
                                  dom.max + padding);
        }
      }
      break; // only process first candle binding
    }
  }

  return std::vector<Id>(touchedSet.begin(), touchedSet.end());
}

} // namespace dc
