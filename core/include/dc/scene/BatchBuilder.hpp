#pragma once
// D56: Render state batching — group consecutive draw items by pipeline/blend/texture
#include "dc/scene/Scene.hpp"
#include "dc/scene/Types.hpp"
#include <vector>

namespace dc {

struct DrawBatch {
  std::string pipeline;
  BlendMode blendMode{BlendMode::Normal};
  std::uint32_t textureId{0};
  std::vector<const DrawItem*> items;
};

struct PaneBatches {
  Id paneId{0};
  std::vector<DrawBatch> batches;
};

struct BatchedFrame {
  std::vector<PaneBatches> panes;
};

class BatchBuilder {
public:
  BatchedFrame build(const Scene& scene);
};

} // namespace dc
