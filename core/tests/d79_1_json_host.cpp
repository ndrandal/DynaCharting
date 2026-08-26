// D79.1: SceneDocument extended fields — parse/serialize round-trip tests
#include "dc/document/SceneDocument.hpp"

#include "dc_check.hpp"
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
  DC_CHECK(dc::parseSceneDocument(json, doc));
  DC_CHECK(doc.buffers.count(100) == 1);

  const auto& b = doc.buffers.at(100);
  DC_CHECK(b.data.size() == 6);
  DC_CHECK(feq(b.data[0], 1.0f));
  DC_CHECK(feq(b.data[5], 6.0f));
  // byteLength derived from data
  DC_CHECK(b.byteLength == 6 * sizeof(float));

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
  DC_CHECK(dc::parseSceneDocument(json, doc));
  // Explicit byteLength takes precedence (not overridden by data size)
  DC_CHECK_CONTAINS(doc.buffers, 200);
  DC_CHECK(doc.buffers.at(200).byteLength == 1024);
  DC_CHECK(doc.buffers.at(200).data.size() == 2);

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
  DC_CHECK(dc::parseSceneDocument(json, doc));
  DC_CHECK_CONTAINS(doc.buffers, 300);
  DC_CHECK(doc.buffers.at(300).byteLength == 512);
  DC_CHECK(doc.buffers.at(300).data.empty());

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
  DC_CHECK(dc::parseSceneDocument(json, doc));
  DC_CHECK(doc.viewports.size() == 2);

  DC_CHECK_CONTAINS(doc.viewports, "price");
  const auto& pv = doc.viewports.at("price");
  DC_CHECK(pv.transformId == 50);
  DC_CHECK(pv.paneId == 1);
  DC_CHECK(deq(pv.xMin, 0));
  DC_CHECK(deq(pv.xMax, 100));
  DC_CHECK(deq(pv.yMin, 10));
  DC_CHECK(deq(pv.yMax, 200));
  DC_CHECK(pv.linkGroup == "time");
  DC_CHECK(pv.panX == true);
  DC_CHECK(pv.panY == false);
  DC_CHECK(pv.zoomX == true);
  DC_CHECK(pv.zoomY == false);

  DC_CHECK_CONTAINS(doc.viewports, "volume");
  const auto& vv = doc.viewports.at("volume");
  DC_CHECK(vv.transformId == 51);
  // Defaults when not specified
  DC_CHECK(vv.panX == true);
  DC_CHECK(vv.panY == true);
  DC_CHECK(vv.zoomX == true);
  DC_CHECK(vv.zoomY == true);

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
  DC_CHECK(dc::parseSceneDocument(json, doc));
  DC_CHECK(doc.textOverlay.fontSize == 14);
  DC_CHECK(doc.textOverlay.color == "#ff0000");
  DC_CHECK(doc.textOverlay.labels.size() == 2);

  const auto& lbl0 = doc.textOverlay.labels[0];
  DC_CHECK(feq(lbl0.clipX, -0.9f));
  DC_CHECK(feq(lbl0.clipY, 0.5f));
  DC_CHECK(lbl0.text == "100.00");
  DC_CHECK(lbl0.align == "r");
  DC_CHECK(lbl0.color == "#00ff00");
  DC_CHECK(lbl0.fontSize == 0); // not specified

  const auto& lbl1 = doc.textOverlay.labels[1];
  DC_CHECK(lbl1.align == "c");
  DC_CHECK(lbl1.fontSize == 11);

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
  DC_CHECK(dc::parseSceneDocument(json, doc2));

  // Verify buffer data
  DC_CHECK_CONTAINS(doc2.buffers, 100);
  DC_CHECK(doc2.buffers.at(100).data.size() == 3);
  DC_CHECK(feq(doc2.buffers.at(100).data[0], 1.0f));
  DC_CHECK(feq(doc2.buffers.at(100).data[2], 3.0f));

  // Verify viewport
  DC_CHECK(doc2.viewports.size() == 1);
  DC_CHECK(doc2.viewports.count("price") == 1);
  const auto& vp2 = doc2.viewports.at("price");
  DC_CHECK(vp2.transformId == 50);
  DC_CHECK(vp2.paneId == 1);
  DC_CHECK(deq(vp2.xMax, 100));
  DC_CHECK(vp2.linkGroup == "time");

  // Verify text overlay
  DC_CHECK(doc2.textOverlay.fontSize == 16);
  DC_CHECK(doc2.textOverlay.color == "#abcdef");
  DC_CHECK(doc2.textOverlay.labels.size() == 1);
  DC_CHECK(doc2.textOverlay.labels[0].text == "Hello");
  DC_CHECK(doc2.textOverlay.labels[0].align == "c");

  std::printf("  PASS: serialize round-trip\n");
}

// ---- Test 7: Compact serialize omits defaults ----
static void testCompactSerialize() {
  dc::SceneDocument doc;
  // Empty doc with defaults
  std::string json = dc::serializeSceneDocument(doc, true);

  // Should not contain "viewports" or "textOverlay" or "buffers" when empty
  DC_CHECK(json.find("viewports") == std::string::npos);
  DC_CHECK(json.find("buffers") == std::string::npos);

  // Now add a viewport — it should appear
  dc::DocViewport vp;
  vp.transformId = 1; vp.paneId = 1;
  doc.viewports["test"] = vp;
  json = dc::serializeSceneDocument(doc, true);
  DC_CHECK(json.find("viewports") != std::string::npos);
  // Default panX/panY/zoomX/zoomY should be omitted in compact
  DC_CHECK(json.find("panX") == std::string::npos);

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
  DC_CHECK(dc::parseSceneDocument(json, doc));
  DC_CHECK(doc.viewportWidth == 900);
  DC_CHECK(doc.viewportHeight == 600);
  DC_CHECK(doc.buffers.size() == 1);
  DC_CHECK(doc.transforms.size() == 1);
  DC_CHECK(doc.panes.size() == 1);
  DC_CHECK(doc.layers.size() == 1);
  DC_CHECK(doc.geometries.size() == 1);
  DC_CHECK(doc.drawItems.size() == 1);
  DC_CHECK(doc.viewports.size() == 1);
  DC_CHECK(doc.textOverlay.labels.size() == 1);

  // Buffer data check
  DC_CHECK_CONTAINS(doc.buffers, 100);
  DC_CHECK(doc.buffers.at(100).data.size() == 6);
  DC_CHECK(feq(doc.buffers.at(100).data[1], 0.5f));

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
