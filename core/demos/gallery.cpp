// Gallery Demo — renders multiple mini-scenes from JSON documents
//
// Each "card" is a self-contained JSON scene document that gets reconciled
// and rendered to its own PPM. Demonstrates the declarative system driving
// various pipeline types and styles purely from text descriptions.

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

// --- Helpers ---

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
}

// --- Gallery card: a JSON document + vertex data producer ---

struct GalleryCard {
  const char* name;
  const char* json;
  // Callback to fill vertex data for each buffer ID
  void (*fillData)(dc::IngestProcessor& ingest);
};

// Fill: two colourful triangles (triSolid@1)
static void fillTriangles(dc::IngestProcessor& ingest) {
  float verts[] = {
    // Tri 1 (left)
    -0.8f,  0.7f,  -0.2f,  0.7f,  -0.5f,  0.1f,
    // Tri 2 (right)
     0.2f, -0.1f,   0.8f, -0.1f,   0.5f, -0.7f
  };
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, verts, sizeof(verts));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// Fill: cross-hatch line pattern (line2d@1)
static void fillLines(dc::IngestProcessor& ingest) {
  float verts[40]; // 10 line segments = 20 points = 40 floats
  int idx = 0;
  // Vertical lines
  for (int i = 0; i < 5; i++) {
    float x = -0.8f + static_cast<float>(i) * 0.4f;
    verts[idx++] = x; verts[idx++] = -0.8f;
    verts[idx++] = x; verts[idx++] =  0.8f;
  }
  // Horizontal lines
  for (int i = 0; i < 5; i++) {
    float y = -0.8f + static_cast<float>(i) * 0.4f;
    verts[idx++] = -0.8f; verts[idx++] = y;
    verts[idx++] =  0.8f; verts[idx++] = y;
  }
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, verts, sizeof(verts));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// Fill: scatter plot (points@1)
static void fillPoints(dc::IngestProcessor& ingest) {
  float verts[50]; // 25 points
  int idx = 0;
  for (int i = 0; i < 25; i++) {
    // Simple deterministic pseudo-random scatter
    float t = static_cast<float>(i) / 24.0f;
    float x = -0.8f + t * 1.6f;
    // Sine wave with some variation
    float y = 0.5f * std::sin(t * 6.28f + static_cast<float>(i % 3) * 0.5f);
    verts[idx++] = x;
    verts[idx++] = y;
  }
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, verts, static_cast<std::uint32_t>(idx * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// Fill: bar chart (instancedRect@1, rect4 format: x0, y0, x1, y1)
static void fillBars(dc::IngestProcessor& ingest) {
  const int N = 8;
  float rects[N * 4];
  for (int i = 0; i < N; i++) {
    float x0 = -0.9f + static_cast<float>(i) * 0.225f;
    float x1 = x0 + 0.18f;
    // Varying heights
    float heights[] = {0.3f, 0.7f, 0.5f, 0.9f, 0.4f, 0.8f, 0.6f, 0.2f};
    float h = heights[i];
    rects[i * 4 + 0] = x0;
    rects[i * 4 + 1] = -0.8f;
    rects[i * 4 + 2] = x1;
    rects[i * 4 + 3] = -0.8f + h * 1.6f;
  }
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, rects, sizeof(rects));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// Fill: candle sticks (instancedCandle@1, candle6: x, open, high, low, close, halfWidth)
static void fillCandles(dc::IngestProcessor& ingest) {
  const int N = 10;
  float candles[N * 6];
  float base = 0.0f;
  for (int i = 0; i < N; i++) {
    float x = -0.85f + static_cast<float>(i) * 0.18f;
    // Deterministic price pattern
    float open = base + (i % 2 == 0 ? 0.1f : -0.05f);
    float close = open + (i % 3 == 0 ? 0.2f : -0.15f);
    float high = std::max(open, close) + 0.1f;
    float low = std::min(open, close) - 0.08f;
    base = close;

    candles[i * 6 + 0] = x;
    candles[i * 6 + 1] = open;
    candles[i * 6 + 2] = high;
    candles[i * 6 + 3] = low;
    candles[i * 6 + 4] = close;
    candles[i * 6 + 5] = 0.06f; // half-width
  }
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, candles, sizeof(candles));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

// --- JSON scene documents for each card ---

static const char* kTrianglesJson = R"({
  "version": 1,
  "buffers": { "100": { "byteLength": 0 } },
  "transforms": { "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 } },
  "panes": {
    "1": {
      "name": "Triangles",
      "hasClearColor": true,
      "clearColor": [0.04, 0.04, 0.06, 1.0]
    }
  },
  "layers": { "10": { "paneId": 1, "name": "Data" } },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 6 }
  },
  "drawItems": {
    "200": {
      "layerId": 10, "name": "Tri1", "pipeline": "triSolid@1",
      "geometryId": 101, "transformId": 50,
      "color": [0.2, 0.6, 1.0, 1.0]
    },
    "201": {
      "layerId": 10, "name": "Tri2", "pipeline": "triSolid@1",
      "geometryId": 101, "transformId": 50,
      "color": [1.0, 0.3, 0.1, 1.0]
    }
  }
})";

static const char* kLinesJson = R"({
  "version": 1,
  "buffers": { "100": { "byteLength": 0 } },
  "transforms": { "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 } },
  "panes": {
    "1": {
      "name": "Grid Lines",
      "hasClearColor": true,
      "clearColor": [0.02, 0.02, 0.04, 1.0]
    }
  },
  "layers": { "10": { "paneId": 1, "name": "Grid" } },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 20 }
  },
  "drawItems": {
    "200": {
      "layerId": 10, "name": "GridLines", "pipeline": "line2d@1",
      "geometryId": 101, "transformId": 50,
      "color": [0.3, 0.8, 0.3, 0.6],
      "lineWidth": 1.5
    }
  }
})";

static const char* kPointsJson = R"({
  "version": 1,
  "buffers": { "100": { "byteLength": 0 } },
  "transforms": { "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 } },
  "panes": {
    "1": {
      "name": "Scatter",
      "hasClearColor": true,
      "clearColor": [0.03, 0.03, 0.05, 1.0]
    }
  },
  "layers": { "10": { "paneId": 1, "name": "Points" } },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 25 }
  },
  "drawItems": {
    "200": {
      "layerId": 10, "name": "Dots", "pipeline": "points@1",
      "geometryId": 101, "transformId": 50,
      "color": [1.0, 0.8, 0.2, 1.0],
      "pointSize": 8.0
    }
  }
})";

static const char* kBarsJson = R"({
  "version": 1,
  "buffers": { "100": { "byteLength": 0 } },
  "transforms": { "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 } },
  "panes": {
    "1": {
      "name": "Bar Chart",
      "hasClearColor": true,
      "clearColor": [0.05, 0.03, 0.05, 1.0]
    }
  },
  "layers": { "10": { "paneId": 1, "name": "Bars" } },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "rect4", "vertexCount": 8 }
  },
  "drawItems": {
    "200": {
      "layerId": 10, "name": "BarSeries", "pipeline": "instancedRect@1",
      "geometryId": 101, "transformId": 50,
      "color": [0.4, 0.7, 1.0, 0.9],
      "cornerRadius": 4.0
    }
  }
})";

static const char* kCandlesJson = R"({
  "version": 1,
  "buffers": { "100": { "byteLength": 0 } },
  "transforms": { "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 } },
  "panes": {
    "1": {
      "name": "Candlesticks",
      "hasClearColor": true,
      "clearColor": [0.06, 0.04, 0.02, 1.0]
    }
  },
  "layers": { "10": { "paneId": 1, "name": "OHLC" } },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "candle6", "vertexCount": 10 }
  },
  "drawItems": {
    "200": {
      "layerId": 10, "name": "CandleSeries", "pipeline": "instancedCandle@1",
      "geometryId": 101, "transformId": 50,
      "color": [1, 1, 1, 1],
      "colorUp": [0.2, 0.9, 0.3, 1.0],
      "colorDown": [0.9, 0.2, 0.2, 1.0]
    }
  }
})";

static GalleryCard kCards[] = {
  {"gallery_triangles", kTrianglesJson, fillTriangles},
  {"gallery_lines",     kLinesJson,     fillLines},
  {"gallery_points",    kPointsJson,    fillPoints},
  {"gallery_bars",      kBarsJson,      fillBars},
  {"gallery_candles",   kCandlesJson,   fillCandles},
};

// --- Main ---

int main() {
#ifndef DC_HAS_OSMESA
  std::printf("Gallery demo requires OSMesa — skipping.\n");
  return 0;
#else
  const int W = 400, H = 300;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Failed to init OSMesa context\n");
    return 1;
  }

  dc::Renderer renderer;
  renderer.init();

  const int numCards = static_cast<int>(sizeof(kCards) / sizeof(kCards[0]));
  int rendered = 0;

  for (int i = 0; i < numCards; i++) {
    const auto& card = kCards[i];
    std::printf("[%d/%d] %s ... ", i + 1, numCards, card.name);

    // Fresh engine per card (isolate scenes)
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);
    dc::GpuBufferManager gpuBuf;

    // Parse JSON
    dc::SceneDocument doc;
    if (!dc::parseSceneDocument(card.json, doc)) {
      std::printf("PARSE FAILED\n");
      continue;
    }

    // Reconcile
    dc::SceneReconciler reconciler(cp);
    auto result = reconciler.reconcile(doc, scene);
    if (!result.ok) {
      std::printf("RECONCILE FAILED\n");
      for (const auto& e : result.errors) std::fprintf(stderr, "  %s\n", e.c_str());
      continue;
    }

    // Fill vertex data
    card.fillData(ingest);

    // Upload to GPU
    for (dc::Id bid : scene.bufferIds()) {
      if (ingest.getBufferSize(bid) > 0) {
        gpuBuf.setCpuData(bid, ingest.getBufferData(bid), ingest.getBufferSize(bid));
      }
    }
    gpuBuf.uploadDirty();

    // Render
    renderer.render(scene, gpuBuf, W, H);

    // Read pixels and write PPM
    auto pixels = ctx.readPixels();
    std::string path = std::string(card.name) + ".ppm";
    writePPM(path.c_str(), W, H, pixels);

    std::printf("OK (%d created)\n", result.created);
    rendered++;
  }

  // Summary with compact serialization demo
  std::printf("\n--- Gallery complete: %d/%d cards rendered ---\n", rendered, numCards);

  // Bonus: demonstrate compact serialization
  dc::SceneDocument doc;
  dc::parseSceneDocument(kBarsJson, doc);
  std::string full = dc::serializeSceneDocument(doc, false);
  std::string compact = dc::serializeSceneDocument(doc, true);
  std::printf("\nCompact serialization demo (bars scene):\n");
  std::printf("  Full:    %zu bytes\n", full.size());
  std::printf("  Compact: %zu bytes (%.0f%% smaller)\n",
              compact.size(),
              100.0 * (1.0 - static_cast<double>(compact.size()) / static_cast<double>(full.size())));

  return 0;
#endif
}
