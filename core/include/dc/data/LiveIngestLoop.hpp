#pragma once
#include "dc/ids/Id.hpp"

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

private:
  LiveIngestLoopConfig config_;
  std::vector<BufferGeometryBinding> bindings_;
  Viewport* viewport_{nullptr};
};

} // namespace dc
