// D79.2: JSON host GL integration — load JSON doc, reconcile, upload inline data, render
#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

// ---- Test: End-to-end JSON → render ----
static void testEndToEndRender() {
  // A simple triangle document with inline vertex data
  const char* json = R"({
    "version": 1,
    "viewport": { "width": 64, "height": 64 },
    "buffers": {
      "100": { "data": [0.0, 0.5, 0.5, -0.5, -0.5, -0.5] }
    },
    "transforms": {
      "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 }
    },
    "panes": {
      "1": { "name": "main" }
    },
    "layers": {
      "10": { "paneId": 1, "name": "data" }
    },
    "geometries": {
      "101": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 3 }
    },
    "drawItems": {
      "102": {
        "layerId": 10, "pipeline": "triSolid@1",
        "geometryId": 101, "transformId": 50,
        "color": [1, 0, 0, 1]
      }
    }
  })";

  // 1. Parse
  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));

  int W = doc.viewportWidth > 0 ? doc.viewportWidth : 64;
  int H = doc.viewportHeight > 0 ? doc.viewportHeight : 64;

  // 2. Init OSMesa
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  // 3. Set up engine
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // 4. Reconcile
  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  assert(result.ok);
  assert(result.created > 0);

  // 5. Upload inline buffer data
  for (const auto& [id, buf] : doc.buffers) {
    if (!buf.data.empty()) {
      ingest.ensureBuffer(id);
      ingest.setBufferData(id,
        reinterpret_cast<const std::uint8_t*>(buf.data.data()),
        static_cast<std::uint32_t>(buf.data.size() * sizeof(float)));
    }
  }

  // 6. Sync to GPU
  dc::GpuBufferManager gpuBufs;
  for (const auto& [id, buf] : doc.buffers) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0) gpuBufs.setCpuData(id, data, size);
  }
  gpuBufs.uploadDirty();

  // 7. Render
  dc::Renderer renderer;
  assert(renderer.init());
  renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  // 8. Read pixels and verify non-black
  auto pixels = ctx.readPixels();
  assert(!pixels.empty());

  bool anyNonBlack = false;
  for (std::size_t i = 0; i < pixels.size(); i += 4) {
    if (pixels[i] > 0 || pixels[i+1] > 0 || pixels[i+2] > 0) {
      anyNonBlack = true;
      break;
    }
  }
  assert(anyNonBlack && "Rendered frame should have non-black pixels (red triangle)");

  // Verify we actually got red pixels
  bool hasRed = false;
  for (std::size_t i = 0; i < pixels.size(); i += 4) {
    if (pixels[i] > 200 && pixels[i+1] < 50 && pixels[i+2] < 50) {
      hasRed = true;
      break;
    }
  }
  assert(hasRed && "Should have red pixels from the triangle");

  std::printf("  PASS: end-to-end JSON → render (red triangle)\n");
}

// ---- Test: Reconcile + data upload with viewports ----
static void testReconcileWithViewport() {
  const char* json = R"({
    "version": 1,
    "viewport": { "width": 64, "height": 64 },
    "buffers": {
      "100": { "data": [-0.5, 0.0, 0.5, 0.0] }
    },
    "transforms": {
      "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 }
    },
    "panes": {
      "1": { "name": "main" }
    },
    "layers": {
      "10": { "paneId": 1, "name": "data" }
    },
    "geometries": {
      "101": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 2 }
    },
    "drawItems": {
      "102": {
        "layerId": 10, "pipeline": "line2d@1",
        "geometryId": 101, "transformId": 50,
        "color": [0, 1, 0, 1], "lineWidth": 3.0
      }
    },
    "viewports": {
      "main": {
        "transformId": 50, "paneId": 1,
        "xMin": -1, "xMax": 1, "yMin": -1, "yMax": 1
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.viewports.size() == 1);
  assert(doc.viewports.at("main").transformId == 50);

  // Basic reconcile
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  assert(result.ok);

  // Verify scene has the expected entities
  assert(scene.getPane(1) != nullptr);
  assert(scene.getLayer(10) != nullptr);
  assert(scene.getDrawItem(102) != nullptr);

  std::printf("  PASS: reconcile with viewport declarations\n");
}

int main() {
  std::printf("D79.2 — JSON host GL integration tests\n");

  testEndToEndRender();
  testReconcileWithViewport();

  std::printf("All D79.2 tests passed.\n");
  return 0;
}
