// D44: AnnotationRenderer — generates textSDF draw items for annotations
#include "dc/metadata/AnnotationRenderer.hpp"
#include "dc/metadata/AnnotationStore.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/text/TextLayout.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace dc {

void AnnotationRenderer::setAnnotationStore(AnnotationStore* store) {
  store_ = store;
}

void AnnotationRenderer::setGlyphAtlas(GlyphAtlas* atlas) {
  atlas_ = atlas;
}

void AnnotationRenderer::setCommandProcessor(CommandProcessor* cp) {
  cp_ = cp;
}

void AnnotationRenderer::setIngestProcessor(IngestProcessor* ingest) {
  ingest_ = ingest;
}

void AnnotationRenderer::setConfig(const AnnotationRendererConfig& cfg) {
  config_ = cfg;
}

void AnnotationRenderer::dispose() {
  if (!cp_) return;

  // Delete all previously created resources in reverse order
  for (auto it = createdIds_.rbegin(); it != createdIds_.rend(); ++it) {
    std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(*it) + "}";
    cp_->applyJsonText(cmd);
  }
  createdIds_.clear();
  renderedCount_ = 0;
}

void AnnotationRenderer::update(const Scene& scene, int viewW, int viewH) {
  if (!store_ || !atlas_ || !cp_ || !ingest_) return;
  if (config_.paneId == 0 || config_.layerId == 0) return;

  (void)scene;

  // Dispose old resources first
  dispose();

  auto annotations = store_->all();
  if (annotations.empty()) return;

  // Sort annotations by drawItemId for determinism
  std::sort(annotations.begin(), annotations.end(),
            [](const Annotation& a, const Annotation& b) {
              return a.drawItemId < b.drawItemId;
            });

  // Ensure ASCII glyphs are rasterized
  atlas_->ensureAscii();

  float glyphPx = static_cast<float>(48); // default glyph rasterize size
  float fontSize = config_.fontSize;

  // Layout annotations vertically
  // Convert pixel positions to clip space: x in [-1,1], y in [-1,1]
  float pixelToClipX = 2.0f / static_cast<float>(viewW);
  float pixelToClipY = 2.0f / static_cast<float>(viewH);

  // Start from top-right corner
  float startPxX = 10.0f;
  float currentPxY = static_cast<float>(viewH) - 20.0f;
  float lineSpacing = fontSize * 1.5f;

  Id bufId = config_.baseBufferId;
  Id geomId = config_.baseGeomId;
  Id drawItemId = config_.baseDrawItemId;

  std::size_t count = 0;

  for (const auto& ann : annotations) {
    // Build display text: "label: value"
    std::string text = ann.label;
    if (!ann.value.empty()) {
      text += ": " + ann.value;
    }

    // Layout text in pixel space
    float clipBaselineX = -1.0f + startPxX * pixelToClipX;
    float clipBaselineY = -1.0f + currentPxY * pixelToClipY;

    TextLayoutResult layout = layoutText(*atlas_, text.c_str(),
                                          clipBaselineX, clipBaselineY,
                                          fontSize * pixelToClipY,
                                          glyphPx);

    if (layout.glyphCount == 0) {
      currentPxY -= lineSpacing;
      continue;
    }

    // Create buffer with glyph data
    {
      std::string cmd = "{\"cmd\":\"createBuffer\",\"id\":" + std::to_string(bufId) +
                         ",\"byteLength\":0}";
      auto r = cp_->applyJsonText(cmd);
      if (!r.ok) break;
      createdIds_.push_back(bufId);
    }

    // Upload glyph instance data to ingest buffer
    ingest_->ensureBuffer(bufId);
    ingest_->setBufferData(bufId,
                           reinterpret_cast<const std::uint8_t*>(layout.glyphInstances.data()),
                           static_cast<std::uint32_t>(layout.glyphInstances.size() * sizeof(float)));

    // Ensure glyphs command
    {
      std::string cmd = "{\"cmd\":\"ensureGlyphs\",\"text\":\"" + text + "\"}";
      cp_->applyJsonText(cmd);
    }

    // Create geometry
    {
      std::string cmd = "{\"cmd\":\"createGeometry\",\"id\":" + std::to_string(geomId) +
                         ",\"vertexBufferId\":" + std::to_string(bufId) +
                         ",\"format\":\"glyph8\"" +
                         ",\"vertexCount\":" + std::to_string(layout.glyphCount) + "}";
      auto r = cp_->applyJsonText(cmd);
      if (!r.ok) break;
      createdIds_.push_back(geomId);
    }

    // Create draw item
    {
      std::string cmd = "{\"cmd\":\"createDrawItem\",\"id\":" + std::to_string(drawItemId) +
                         ",\"layerId\":" + std::to_string(config_.layerId) +
                         ",\"name\":\"ann_" + std::to_string(ann.drawItemId) + "\"}";
      auto r = cp_->applyJsonText(cmd);
      if (!r.ok) break;
      createdIds_.push_back(drawItemId);
    }

    // Bind to textSDF pipeline
    {
      std::string cmd = "{\"cmd\":\"bindDrawItem\",\"drawItemId\":" + std::to_string(drawItemId) +
                         ",\"pipeline\":\"textSDF@1\"" +
                         ",\"geometryId\":" + std::to_string(geomId) + "}";
      cp_->applyJsonText(cmd);
    }

    // Set color
    {
      std::ostringstream ss;
      ss << "{\"cmd\":\"setDrawItemColor\",\"drawItemId\":" << drawItemId
         << ",\"r\":" << config_.color[0]
         << ",\"g\":" << config_.color[1]
         << ",\"b\":" << config_.color[2]
         << ",\"a\":" << config_.color[3] << "}";
      cp_->applyJsonText(ss.str());
    }

    // Attach transform if specified
    if (config_.transformId != 0) {
      std::string cmd = "{\"cmd\":\"attachTransform\",\"drawItemId\":" + std::to_string(drawItemId) +
                         ",\"transformId\":" + std::to_string(config_.transformId) + "}";
      cp_->applyJsonText(cmd);
    }

    bufId++;
    geomId++;
    drawItemId++;
    count++;
    currentPxY -= lineSpacing;
  }

  renderedCount_ = count;
}

std::size_t AnnotationRenderer::renderedCount() const {
  return renderedCount_;
}

} // namespace dc
