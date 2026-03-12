// D79.1: SceneDocument extended fields — parse/serialize round-trip tests
#include "dc/document/SceneDocument.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-5f; }
static bool deq(double a, double b) { return std::fabs(a - b) < 1e-9; }

// ---- Test 1: Inline buffer data parse ----
static void testInlineBufferParse() {
  const char* json = R"({
    "version": 1,
    "buffers": {
      "100": { "data": [1.0, 2.0, 3.0, 4.0, 5.0, 6.0] }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.buffers.count(100) == 1);

  const auto& b = doc.buffers.at(100);
  assert(b.data.size() == 6);
  assert(feq(b.data[0], 1.0f));
  assert(feq(b.data[5], 6.0f));
  // byteLength derived from data
  assert(b.byteLength == 6 * sizeof(float));

  std::printf("  PASS: inline buffer data parse\n");
}

// ---- Test 2: Inline buffer data with explicit byteLength ----
static void testInlineBufferWithByteLength() {
  const char* json = R"({
    "buffers": {
      "200": { "byteLength": 1024, "data": [10.0, 20.0] }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  // Explicit byteLength takes precedence (not overridden by data size)
  assert(doc.buffers.at(200).byteLength == 1024);
  assert(doc.buffers.at(200).data.size() == 2);

  std::printf("  PASS: inline buffer with explicit byteLength\n");
}

// ---- Test 3: Buffer without data (backward compat) ----
static void testBufferNoData() {
  const char* json = R"({
    "buffers": {
      "300": { "byteLength": 512 }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.buffers.at(300).byteLength == 512);
  assert(doc.buffers.at(300).data.empty());

  std::printf("  PASS: buffer without inline data (backward compat)\n");
}

// ---- Test 4: Viewport declarations parse ----
static void testViewportParse() {
  const char* json = R"({
    "viewports": {
      "price": {
        "transformId": 50, "paneId": 1,
        "xMin": 0, "xMax": 100, "yMin": 10, "yMax": 200,
        "linkGroup": "time",
        "panX": true, "panY": false, "zoomX": true, "zoomY": false
      },
      "volume": {
        "transformId": 51, "paneId": 2,
        "xMin": 0, "xMax": 100, "yMin": 0, "yMax": 5000,
        "linkGroup": "time"
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.viewports.size() == 2);

  const auto& pv = doc.viewports.at("price");
  assert(pv.transformId == 50);
  assert(pv.paneId == 1);
  assert(deq(pv.xMin, 0));
  assert(deq(pv.xMax, 100));
  assert(deq(pv.yMin, 10));
  assert(deq(pv.yMax, 200));
  assert(pv.linkGroup == "time");
  assert(pv.panX == true);
  assert(pv.panY == false);
  assert(pv.zoomX == true);
  assert(pv.zoomY == false);

  const auto& vv = doc.viewports.at("volume");
  assert(vv.transformId == 51);
  // Defaults when not specified
  assert(vv.panX == true);
  assert(vv.panY == true);
  assert(vv.zoomX == true);
  assert(vv.zoomY == true);

  std::printf("  PASS: viewport declarations parse\n");
}

// ---- Test 5: Text overlay parse ----
static void testTextOverlayParse() {
  const char* json = R"({
    "textOverlay": {
      "fontSize": 14,
      "color": "#ff0000",
      "labels": [
        { "clipX": -0.9, "clipY": 0.5, "text": "100.00", "align": "r", "color": "#00ff00" },
        { "clipX": 0.0, "clipY": -0.8, "text": "12:00", "align": "c", "fontSize": 11 }
      ]
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.textOverlay.fontSize == 14);
  assert(doc.textOverlay.color == "#ff0000");
  assert(doc.textOverlay.labels.size() == 2);

  const auto& lbl0 = doc.textOverlay.labels[0];
  assert(feq(lbl0.clipX, -0.9f));
  assert(feq(lbl0.clipY, 0.5f));
  assert(lbl0.text == "100.00");
  assert(lbl0.align == "r");
  assert(lbl0.color == "#00ff00");
  assert(lbl0.fontSize == 0); // not specified

  const auto& lbl1 = doc.textOverlay.labels[1];
  assert(lbl1.align == "c");
  assert(lbl1.fontSize == 11);

  std::printf("  PASS: text overlay parse\n");
}

// ---- Test 6: Serialize round-trip ----
static void testSerializeRoundTrip() {
  dc::SceneDocument doc;
  doc.version = 1;
  doc.viewportWidth = 800;
  doc.viewportHeight = 600;

  // Buffer with inline data
  dc::DocBuffer buf;
  buf.data = {1.0f, 2.0f, 3.0f};
  buf.byteLength = static_cast<std::uint32_t>(buf.data.size() * sizeof(float));
  doc.buffers[100] = buf;

  // Viewport
  dc::DocViewport vp;
  vp.transformId = 50;
  vp.paneId = 1;
  vp.xMin = 0; vp.xMax = 100;
  vp.yMin = -10; vp.yMax = 200;
  vp.linkGroup = "time";
  vp.panY = false;
  doc.viewports["price"] = vp;

  // Text overlay
  doc.textOverlay.fontSize = 16;
  doc.textOverlay.color = "#abcdef";
  dc::DocTextLabel lbl;
  lbl.clipX = 0.5f; lbl.clipY = -0.5f;
  lbl.text = "Hello";
  lbl.align = "c";
  doc.textOverlay.labels.push_back(lbl);

  // Serialize
  std::string json = dc::serializeSceneDocument(doc);

  // Re-parse
  dc::SceneDocument doc2;
  assert(dc::parseSceneDocument(json, doc2));

  // Verify buffer data
  assert(doc2.buffers.at(100).data.size() == 3);
  assert(feq(doc2.buffers.at(100).data[0], 1.0f));
  assert(feq(doc2.buffers.at(100).data[2], 3.0f));

  // Verify viewport
  assert(doc2.viewports.size() == 1);
  assert(doc2.viewports.count("price") == 1);
  const auto& vp2 = doc2.viewports.at("price");
  assert(vp2.transformId == 50);
  assert(vp2.paneId == 1);
  assert(deq(vp2.xMax, 100));
  assert(vp2.linkGroup == "time");

  // Verify text overlay
  assert(doc2.textOverlay.fontSize == 16);
  assert(doc2.textOverlay.color == "#abcdef");
  assert(doc2.textOverlay.labels.size() == 1);
  assert(doc2.textOverlay.labels[0].text == "Hello");
  assert(doc2.textOverlay.labels[0].align == "c");

  std::printf("  PASS: serialize round-trip\n");
}

// ---- Test 7: Compact serialize omits defaults ----
static void testCompactSerialize() {
  dc::SceneDocument doc;
  // Empty doc with defaults
  std::string json = dc::serializeSceneDocument(doc, true);

  // Should not contain "viewports" or "textOverlay" or "buffers" when empty
  assert(json.find("viewports") == std::string::npos);
  assert(json.find("buffers") == std::string::npos);

  // Now add a viewport — it should appear
  dc::DocViewport vp;
  vp.transformId = 1; vp.paneId = 1;
  doc.viewports["test"] = vp;
  json = dc::serializeSceneDocument(doc, true);
  assert(json.find("viewports") != std::string::npos);
  // Default panX/panY/zoomX/zoomY should be omitted in compact
  assert(json.find("panX") == std::string::npos);

  std::printf("  PASS: compact serialize omits defaults\n");
}

// ---- Test 8: Full document with all sections ----
static void testFullDocument() {
  const char* json = R"({
    "version": 1,
    "viewport": { "width": 900, "height": 600 },
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
    },
    "viewports": {
      "main": { "transformId": 50, "paneId": 1, "xMin": -1, "xMax": 1, "yMin": -1, "yMax": 1 }
    },
    "textOverlay": {
      "fontSize": 13,
      "color": "#b2b5bc",
      "labels": [
        { "clipX": 0.0, "clipY": 0.9, "text": "Hello Chart", "align": "c" }
      ]
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.viewportWidth == 900);
  assert(doc.viewportHeight == 600);
  assert(doc.buffers.size() == 1);
  assert(doc.transforms.size() == 1);
  assert(doc.panes.size() == 1);
  assert(doc.layers.size() == 1);
  assert(doc.geometries.size() == 1);
  assert(doc.drawItems.size() == 1);
  assert(doc.viewports.size() == 1);
  assert(doc.textOverlay.labels.size() == 1);

  // Buffer data check
  assert(doc.buffers.at(100).data.size() == 6);
  assert(feq(doc.buffers.at(100).data[1], 0.5f));

  std::printf("  PASS: full document with all sections\n");
}

int main() {
  std::printf("D79.1 — JSON host parse/serialize tests\n");

  testInlineBufferParse();
  testInlineBufferWithByteLength();
  testBufferNoData();
  testViewportParse();
  testTextOverlayParse();
  testSerializeRoundTrip();
  testCompactSerialize();
  testFullDocument();

  std::printf("All D79.1 tests passed.\n");
  return 0;
}
