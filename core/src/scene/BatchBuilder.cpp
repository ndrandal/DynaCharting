// D56: Render state batching — group consecutive draw items by pipeline/blend/texture
#include "dc/gl/BatchBuilder.hpp"
#include <algorithm>

namespace dc {

BatchedFrame BatchBuilder::build(const Scene& scene) {
  BatchedFrame frame;

  auto paneIds = scene.paneIds(); // sorted ascending
  for (Id paneId : paneIds) {
    PaneBatches pb;
    pb.paneId = paneId;

    auto layerIds = scene.layerIds(); // sorted ascending

    for (Id layerId : layerIds) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;

      auto drawItemIds = scene.drawItemIds(); // sorted ascending

      for (Id diId : drawItemIds) {
        const DrawItem* di = scene.getDrawItem(diId);
        if (!di || di->layerId != layerId) continue;
        if (!di->visible) continue;
        if (di->pipeline.empty()) continue;

        // Check if we can merge with the last batch
        if (!pb.batches.empty()) {
          DrawBatch& lastBatch = pb.batches.back();
          if (lastBatch.pipeline == di->pipeline &&
              lastBatch.blendMode == di->blendMode &&
              lastBatch.textureId == di->textureId) {
            // Merge into existing batch
            lastBatch.items.push_back(di);
            continue;
          }
        }

        // Start a new batch
        DrawBatch batch;
        batch.pipeline = di->pipeline;
        batch.blendMode = di->blendMode;
        batch.textureId = di->textureId;
        batch.items.push_back(di);
        pb.batches.push_back(std::move(batch));
      }
    }

    frame.panes.push_back(std::move(pb));
  }

  return frame;
}

} // namespace dc
