#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scale/Scale.hpp"  // RunningDomain — the P1.5 (ENC-596) O(Δ) reducer

#include <cstdint>
#include <vector>

namespace dc {

class DataSource;
class IngestProcessor;
class CommandProcessor;
class Viewport;

struct BufferGeometryBinding {
  Id bufferId;
  Id geometryId;
  std::uint32_t bytesPerVertex; // e.g., 24 for candle6, 8 for pos2_clip
};

struct LiveIngestLoopConfig {
  bool autoScrollX{true};
  bool autoScaleY{true};
  float scrollMargin{0.1f}; // fraction of X range to keep as right margin
};

class LiveIngestLoop {
public:
  void setConfig(const LiveIngestLoopConfig& cfg);
  void addBinding(const BufferGeometryBinding& binding);
  void clearBindings();
  void setViewport(Viewport* vp);

  // Called each frame from render loop.
  // Returns IDs of touched buffers (empty if no data changed).
  // Caller is responsible for syncing touched buffers to GPU.
  std::vector<Id> consumeAndUpdate(DataSource& source,
                                    IngestProcessor& ingest,
                                    CommandProcessor& cp);

  // ENC-607 (P1.16) — cumulative number of candles the auto-scale-Y path has
  // FOLDED into its running domain across all ticks. For an append-only stream
  // this equals the total candle count ever appended (each candle folded EXACTLY
  // once), which is the proof the auto-domain is O(Δ) per tick, not an O(N)
  // per-tick rescan (that would make this grow ~N² over the run). Testing hook.
  std::uint64_t autoScaleFoldedCandles() const { return autoScaleFoldedCandles_; }

private:
  LiveIngestLoopConfig config_;
  std::vector<BufferGeometryBinding> bindings_;
  Viewport* viewport_{nullptr};

  // ENC-607 — streaming O(Δ) auto-domain-Y state. The RunningDomain (P1.5,
  // ENC-596) keeps a running [min,max] over the candle low/high stream and folds
  // ONLY the candles appended since the last tick. `autoScaleBufferId_` /
  // `autoScaleConsumedCandles_` track which candle buffer the domain is bound to
  // and how many of its candles have been folded, so a buffer swap or a shrink
  // (row count going backwards) resets the reducer instead of reading stale state.
  RunningDomain autoScaleDomain_;
  Id autoScaleBufferId_{kInvalidId};
  std::uint32_t autoScaleConsumedCandles_{0};
  std::uint64_t autoScaleFoldedCandles_{0};
};

} // namespace dc
