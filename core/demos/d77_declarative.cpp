// D77 — Declarative JSON Scene Document Demo
//
// Demonstrates the "JSON in, pixels out" pattern:
// 1. Parse a JSON document describing the entire scene
// 2. Reconcile against an empty scene → creates everything
// 3. Ingest vertex data (binary batch)
// 4. Upload GPU buffers + render → output PPM
// 5. Modify the document (change colors, hide items)
// 6. Reconcile again → incremental update
// 7. Re-render → second PPM

#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>

// --- Binary ingest helpers ---

static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

static void appendRecord(std::vector<std::uint8_t>& batch,
                          std::uint8_t op, std::uint32_t bufferId,
                          std::uint32_t offset, const void* payload, std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

static void writePPM(const char* path, int w, int h, const std::vector<std::uint8_t>& rgba) {
  std::ofstream f(path, std::ios::binary);
  f << "P6\n" << w << " " << h << "\n255\n";
  for (int y = h - 1; y >= 0; --y) {
    for (int x = 0; x < w; ++x) {
      std::size_t idx = static_cast<std::size_t>((y * w + x) * 4);
      f.put(static_cast<char>(rgba[idx + 0]));
      f.put(static_cast<char>(rgba[idx + 1]));
      f.put(static_cast<char>(rgba[idx + 2]));
    }
  }
  std::printf("Wrote %s\n", path);
}

// --- The scene as a JSON document ---

static const char* kSceneJson = R"({
  "version": 1,
  "viewport": { "width": 640, "height": 480 },
  "buffers": {
    "100": { "byteLength": 0 }
  },
  "transforms": {
    "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 }
  },
  "panes": {
    "1": {
      "name": "Main",
      "region": { "clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95 },
      "clearColor": [0.05, 0.05, 0.08, 1.0],
      "hasClearColor": true
    }
  },
  "layers": {
    "10": { "paneId": 1, "name": "Data" }
  },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 6 }
  },
  "drawItems": {
    "200": {
      "layerId": 10,
      "name": "Triangle1",
      "pipeline": "triSolid@1",
      "geometryId": 101,
      "transformId": 50,
      "color": [0.2, 0.6, 1.0, 1.0],
      "visible": true
    },
    "201": {
      "layerId": 10,
      "name": "Triangle2",
      "pipeline": "triSolid@1",
      "geometryId": 101,
      "transformId": 50,
      "color": [1.0, 0.3, 0.1, 1.0],
      "visible": true
    }
  }
})";

int main() {
#ifndef DC_HAS_OSMESA
  std::printf("D77 demo requires OSMesa — skipping.\n");
  return 0;
#else
  const int W = 640, H = 480;

  // 1. Create GL context
  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Failed to init OSMesa context\n");
    return 1;
  }

  // 2. Create engine objects
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  dc::GpuBufferManager gpuBuf;
  dc::Renderer renderer;
  renderer.init();

  // 3. Parse the JSON document
  dc::SceneDocument doc;
  if (!dc::parseSceneDocument(kSceneJson, doc)) {
    std::fprintf(stderr, "Failed to parse scene JSON\n");
    return 1;
  }

  // 4. Reconcile (cold start — creates everything)
  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  if (!result.ok) {
    std::fprintf(stderr, "Reconcile failed:\n");
    for (const auto& e : result.errors) std::fprintf(stderr, "  %s\n", e.c_str());
    return 1;
  }
  std::printf("Cold start: %d created, %d updated, %d deleted\n",
              result.created, result.updated, result.deleted);

  // 5. Ingest vertex data (two triangles into buffer 100)
  {
    // Triangle 1: upper left
    float tri1[] = {
      -0.8f,  0.8f,
      -0.2f,  0.8f,
      -0.5f,  0.2f
    };
    // Triangle 2: lower right
    float tri2[] = {
       0.2f, -0.2f,
       0.8f, -0.2f,
       0.5f, -0.8f
    };

    // Combine into one buffer (6 vertices)
    float verts[12];
    std::memcpy(verts, tri1, sizeof(tri1));
    std::memcpy(verts + 6, tri2, sizeof(tri2));

    std::vector<std::uint8_t> batch;
    appendRecord(batch, 1, 100, 0, verts, sizeof(verts));
    auto ir = ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

    // Upload touched buffers to GPU
    for (dc::Id bid : ir.touchedBufferIds) {
      gpuBuf.setCpuData(bid, ingest.getBufferData(bid), ingest.getBufferSize(bid));
    }
    gpuBuf.uploadDirty();
  }

  // 6. Render
  renderer.render(scene, gpuBuf, W, H);

  // 7. Read pixels + write PPM
  auto pixels = ctx.readPixels();
  writePPM("d77_initial.ppm", W, H, pixels);

  // --- Incremental update ---

  // 8. Modify document: change colors, hide triangle2
  doc.drawItems[200].color[0] = 0.0f;
  doc.drawItems[200].color[1] = 1.0f;
  doc.drawItems[200].color[2] = 0.0f;
  doc.drawItems[201].visible = false;

  // 9. Reconcile (incremental)
  auto r2 = reconciler.reconcile(doc, scene);
  if (!r2.ok) {
    std::fprintf(stderr, "Incremental reconcile failed\n");
    return 1;
  }
  std::printf("Incremental: %d created, %d updated, %d deleted\n",
              r2.created, r2.updated, r2.deleted);

  // 10. Re-render
  renderer.render(scene, gpuBuf, W, H);
  pixels = ctx.readPixels();
  writePPM("d77_updated.ppm", W, H, pixels);

  // Verify: triangle1 is now green, triangle2 is hidden
  const dc::DrawItem* di200 = scene.getDrawItem(200);
  const dc::DrawItem* di201 = scene.getDrawItem(201);
  assert(di200 && std::fabs(di200->color[1] - 1.0f) < 0.01f);
  assert(di201 && !di201->visible);

  std::printf("\nD77 declarative demo completed successfully.\n");
  return 0;
#endif
}
