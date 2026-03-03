#pragma once
// D44: AnnotationRenderer — generates textSDF draw items from AnnotationStore
#include "dc/ids/Id.hpp"
#include "dc/scene/Scene.hpp"
#include <vector>

namespace dc {
class CommandProcessor;
class IngestProcessor;
class GlyphAtlas;
class AnnotationStore;

struct AnnotationRendererConfig {
  Id paneId{0}, layerId{0};
  Id baseBufferId{0}, baseGeomId{0}, baseDrawItemId{0};
  Id transformId{0};
  float fontSize{14.0f};
  float color[4] = {1,1,1,1};
};

class AnnotationRenderer {
public:
  void setAnnotationStore(AnnotationStore* store);
  void setGlyphAtlas(GlyphAtlas* atlas);
  void setCommandProcessor(CommandProcessor* cp);
  void setIngestProcessor(IngestProcessor* ingest);
  void setConfig(const AnnotationRendererConfig& cfg);

  void update(const Scene& scene, int viewW, int viewH);
  void dispose();
  std::size_t renderedCount() const;

private:
  AnnotationStore* store_{nullptr};
  GlyphAtlas* atlas_{nullptr};
  CommandProcessor* cp_{nullptr};
  IngestProcessor* ingest_{nullptr};
  AnnotationRendererConfig config_;
  std::vector<Id> createdIds_;
  std::size_t renderedCount_{0};
};
} // namespace dc
