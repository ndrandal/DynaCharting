// D80.3: BindingEvaluator GL integration — selection drives filterBuffer,
// verifies output buffer gets synced to GPU and geometry vertex count updates.
// This test runs without JsonHost's stdin loop; it exercises the same plumbing.
#include "dc/binding/BindingEvaluator.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/data/DerivedBuffer.hpp"
#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/selection/SelectionState.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

// ---- Test 1: Full pipeline — reconcile, bind, select, render ----
static void testFullPipeline() {
  // JSON: two triangles sharing a source buffer.
  // DrawItem 300 (source) renders all 3 triangles from buffer 100.
  // DrawItem 310 (filtered) renders from buffer 110 (output of binding).
  // Binding 1001: selection on DrawItem 300 → filterBuffer 100→110, geom 210.
  const char* json = R"({
    "version": 1,
    "viewport": { "width": 64, "height": 64 },
    "buffers": {
      "100": {
        "data": [
          -0.5, -0.5,   0.0, 0.5,   0.5, -0.5,
          -0.8, -0.8,  -0.3, 0.2,   0.2, -0.8,
           0.3, -0.3,   0.8, 0.7,   0.8, -0.3
        ]
      },
      "110": { "byteLength": 0 }
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
      "200": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 9 },
      "210": { "vertexBufferId": 110, "format": "pos2_clip", "vertexCount": 0 }
    },
    "drawItems": {
      "300": {
        "layerId": 10, "pipeline": "triSolid@1", "geometryId": 200,
        "transformId": 50, "color": [0.2, 0.2, 0.8, 1.0]
      },
      "310": {
        "layerId": 10, "pipeline": "triSolid@1", "geometryId": 210,
        "transformId": 50, "color": [1.0, 0.0, 0.0, 1.0]
      }
    },
    "bindings": {
      "1001": {
        "trigger": { "type": "selection", "drawItemId": 300 },
        "effect": {
          "type": "filterBuffer",
          "sourceBufferId": 100,
          "outputBufferId": 110,
          "recordStride": 8,
          "geometryId": 210
        }
      }
    }
  })";

  // 1. Parse
  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));

  // 2. Init OSMesa
  dc::OsMesaContext ctx;
  assert(ctx.init(64, 64));

  // 3. Engine
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // 4. Reconcile
  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  assert(result.ok);

  // 5. Upload inline buffers
  for (const auto& [id, buf] : doc.buffers) {
    if (!buf.data.empty()) {
      ingest.ensureBuffer(id);
      ingest.setBufferData(id,
        reinterpret_cast<const std::uint8_t*>(buf.data.data()),
        static_cast<std::uint32_t>(buf.data.size() * sizeof(float)));
    }
  }
  ingest.ensureBuffer(110);

  // 6. Setup bindings
  dc::DerivedBufferManager derivedBufs;
  dc::SelectionState selection;
  dc::EventBus eventBus;
  dc::BindingEvaluator binder(cp, ingest, derivedBufs);
  binder.loadBindings(doc.bindings);

  // 7. GPU setup
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  renderer.init();

  // Sync source buffer to GPU
  for (const auto& [id, buf] : doc.buffers) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0) gpuBufs.setCpuData(id, data, size);
  }
  gpuBufs.uploadDirty();

  // 8. Render initial frame — DrawItem 310 has vertexCount=0, invisible
  renderer.render(scene, gpuBufs, 64, 64);
  ctx.swapBuffers();
  auto pixels1 = ctx.readPixels();

  // 9. Select record 2 (third triangle: vertices 4,5 i.e. indices 2 in triangle-terms)
  // pos2_clip stride is 8 bytes. The source has 9 vertices = 3 triangles.
  // Record index 2 means vertex offset 2*3=6..8 (the third triangle).
  // Actually, for filterBuffer with recordStride=8, each "record" is one vertex.
  // Select vertex indices 6,7,8 (the third triangle).
  selection.setMode(dc::SelectionMode::Toggle);
  selection.toggle({300, 6});
  selection.toggle({300, 7});
  selection.toggle({300, 8});
  auto touched = binder.onSelectionChanged(selection);

  // Output buffer should now have 3 vertices (24 bytes)
  assert(ingest.getBufferSize(110) == 24);
  assert(scene.getGeometry(210)->vertexCount == 3);

  // Sync to GPU
  for (dc::Id id : touched) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0)
      gpuBufs.setCpuData(id, data, size);
    else
      gpuBufs.setCpuData(id, nullptr, 0);
  }
  gpuBufs.uploadDirty();

  // 10. Render post-selection — DrawItem 310 now draws the third triangle in red
  renderer.render(scene, gpuBufs, 64, 64);
  ctx.swapBuffers();
  auto pixels2 = ctx.readPixels();

  // Frames should differ (the red triangle is now visible)
  bool differ = false;
  for (std::size_t i = 0; i < pixels1.size() && i < pixels2.size(); i++) {
    if (pixels1[i] != pixels2[i]) { differ = true; break; }
  }
  assert(differ);

  // 11. Clear selection → output buffer empty, vertex count 0
  selection.clear();
  touched = binder.onSelectionChanged(selection);
  assert(ingest.getBufferSize(110) == 0);
  assert(scene.getGeometry(210)->vertexCount == 0);

  std::printf("  PASS: full pipeline (reconcile → bind → select → render)\n");
}

// ---- Test 2: setVisible binding via selection in reconciled scene ----
static void testVisibilityBinding() {
  const char* json = R"({
    "version": 1,
    "buffers": {
      "100": { "data": [-0.5, -0.5, 0.0, 0.5, 0.5, -0.5] }
    },
    "panes": { "1": { "name": "p" } },
    "layers": { "10": { "paneId": 1 } },
    "geometries": {
      "200": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 3 }
    },
    "drawItems": {
      "300": { "layerId": 10, "pipeline": "triSolid@1", "geometryId": 200 },
      "310": { "layerId": 10, "pipeline": "triSolid@1", "geometryId": 200, "visible": false }
    },
    "bindings": {
      "2001": {
        "trigger": { "type": "selection", "drawItemId": 300 },
        "effect": {
          "type": "setVisible",
          "drawItemId": 310,
          "visible": true,
          "defaultVisible": false
        }
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  dc::SceneReconciler reconciler(cp);
  assert(reconciler.reconcile(doc, scene).ok);

  dc::DerivedBufferManager derivedBufs;
  dc::BindingEvaluator binder(cp, ingest, derivedBufs);
  binder.loadBindings(doc.bindings);

  // Initially DrawItem 310 is invisible
  assert(scene.getDrawItem(310)->visible == false);

  // Select on 300 → 310 becomes visible
  dc::SelectionState sel;
  sel.select({300, 0});
  binder.onSelectionChanged(sel);
  assert(scene.getDrawItem(310)->visible == true);

  // Clear → 310 reverts to invisible
  sel.clear();
  binder.onSelectionChanged(sel);
  assert(scene.getDrawItem(310)->visible == false);

  std::printf("  PASS: setVisible binding in reconciled scene\n");
}

int main() {
  std::printf("D80.3: Binding GL integration\n");
  testFullPipeline();
  testVisibilityBinding();
  std::printf("All D80.3 tests passed.\n");
  return 0;
}
