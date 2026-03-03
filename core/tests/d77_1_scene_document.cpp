// D77.1: SceneDocument parse/serialize tests
#include "dc/document/SceneDocument.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace dc;

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-6f; }

// T1: Parse a valid JSON document — verify all fields populate correctly
static void testParseValid() {
  const char* json = R"({
    "version": 1,
    "viewport": { "width": 1280, "height": 960 },
    "buffers": {
      "300": { "byteLength": 1024 }
    },
    "transforms": {
      "50": { "tx": 1.5, "ty": -0.5, "sx": 2.0, "sy": 0.5 }
    },
    "panes": {
      "1": {
        "name": "Price",
        "region": { "clipYMin": 0.0, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95 },
        "clearColor": [0.08, 0.08, 0.10, 1.0],
        "hasClearColor": true
      }
    },
    "layers": {
      "10": { "paneId": 1, "name": "Candles" }
    },
    "geometries": {
      "301": { "vertexBufferId": 300, "format": "candle6", "vertexCount": 80 }
    },
    "drawItems": {
      "302": {
        "layerId": 10,
        "name": "CandleDI",
        "pipeline": "instancedCandle@1",
        "geometryId": 301,
        "transformId": 50,
        "color": [1, 1, 1, 1],
        "colorUp": [0, 0.8, 0, 1],
        "colorDown": [0.8, 0, 0, 1],
        "pointSize": 5.0,
        "lineWidth": 2.0,
        "blendMode": "additive",
        "visible": true,
        "gradientType": "linear",
        "gradientAngle": 1.57,
        "gradientColor0": [1, 0, 0, 1],
        "gradientColor1": [0, 0, 1, 1],
        "anchorPoint": "center",
        "anchorOffsetX": 10.0,
        "anchorOffsetY": 20.0
      }
    }
  })";

  SceneDocument doc;
  bool ok = parseSceneDocument(json, doc);
  assert(ok);

  assert(doc.version == 1);
  assert(doc.viewportWidth == 1280);
  assert(doc.viewportHeight == 960);

  // Buffer
  assert(doc.buffers.size() == 1);
  assert(doc.buffers.count(300) == 1);
  assert(doc.buffers[300].byteLength == 1024);

  // Transform
  assert(doc.transforms.size() == 1);
  assert(doc.transforms.count(50) == 1);
  assert(feq(doc.transforms[50].tx, 1.5f));
  assert(feq(doc.transforms[50].sy, 0.5f));

  // Pane
  assert(doc.panes.size() == 1);
  assert(doc.panes[1].name == "Price");
  assert(feq(doc.panes[1].region.clipYMin, 0.0f));
  assert(feq(doc.panes[1].region.clipXMax, 0.95f));
  assert(doc.panes[1].hasClearColor);
  assert(feq(doc.panes[1].clearColor[0], 0.08f));

  // Layer
  assert(doc.layers.size() == 1);
  assert(doc.layers[10].paneId == 1);
  assert(doc.layers[10].name == "Candles");

  // Geometry
  assert(doc.geometries.size() == 1);
  assert(doc.geometries[301].vertexBufferId == 300);
  assert(doc.geometries[301].format == "candle6");
  assert(doc.geometries[301].vertexCount == 80);

  // DrawItem
  assert(doc.drawItems.size() == 1);
  const auto& di = doc.drawItems[302];
  assert(di.layerId == 10);
  assert(di.name == "CandleDI");
  assert(di.pipeline == "instancedCandle@1");
  assert(di.geometryId == 301);
  assert(di.transformId == 50);
  assert(feq(di.color[0], 1.0f));
  assert(feq(di.colorUp[1], 0.8f));
  assert(feq(di.colorDown[0], 0.8f));
  assert(feq(di.pointSize, 5.0f));
  assert(feq(di.lineWidth, 2.0f));
  assert(di.blendMode == "additive");
  assert(di.visible);
  assert(di.gradientType == "linear");
  assert(feq(di.gradientAngle, 1.57f));
  assert(feq(di.gradientColor0[0], 1.0f));
  assert(feq(di.gradientColor1[2], 1.0f));
  assert(di.anchorPoint == "center");
  assert(feq(di.anchorOffsetX, 10.0f));
  assert(feq(di.anchorOffsetY, 20.0f));

  std::printf("T1 parseValid: PASS\n");
}

// T2: Parse empty document — struct defaults
static void testParseEmpty() {
  SceneDocument doc;
  bool ok = parseSceneDocument("{}", doc);
  assert(ok);
  assert(doc.version == 1);
  assert(doc.buffers.empty());
  assert(doc.transforms.empty());
  assert(doc.panes.empty());
  assert(doc.layers.empty());
  assert(doc.geometries.empty());
  assert(doc.drawItems.empty());

  std::printf("T2 parseEmpty: PASS\n");
}

// T3: Parse invalid JSON — returns false
static void testParseInvalid() {
  SceneDocument doc;
  assert(!parseSceneDocument("not json", doc));
  assert(!parseSceneDocument("{invalid", doc));
  assert(!parseSceneDocument("", doc));

  std::printf("T3 parseInvalid: PASS\n");
}

// T4: Round-trip: serialize → parse → compare
static void testRoundTrip() {
  SceneDocument doc;
  doc.version = 1;
  doc.viewportWidth = 800;
  doc.viewportHeight = 600;

  doc.buffers[100] = {512};
  doc.transforms[200] = {1.0f, 2.0f, 3.0f, 4.0f};
  DocPane p;
  p.name = "Main";
  p.region = {-0.5f, 0.5f, -0.9f, 0.9f};
  p.clearColor[0] = 0.1f; p.clearColor[1] = 0.2f;
  p.clearColor[2] = 0.3f; p.clearColor[3] = 1.0f;
  p.hasClearColor = true;
  doc.panes[1] = p;

  doc.layers[10] = {1, "Layer0"};

  DocGeometry g;
  g.vertexBufferId = 100;
  g.format = "pos2_clip";
  g.vertexCount = 6;
  doc.geometries[101] = g;

  DocDrawItem di;
  di.layerId = 10;
  di.pipeline = "triSolid@1";
  di.geometryId = 101;
  di.transformId = 200;
  di.color[0] = 0.5f; di.color[1] = 0.6f;
  di.color[2] = 0.7f; di.color[3] = 1.0f;
  di.visible = true;
  di.blendMode = "screen";
  doc.drawItems[102] = di;

  // Serialize
  std::string json = serializeSceneDocument(doc);
  assert(!json.empty());

  // Parse back
  SceneDocument doc2;
  bool ok = parseSceneDocument(json, doc2);
  assert(ok);

  // Compare
  assert(doc2.version == doc.version);
  assert(doc2.viewportWidth == doc.viewportWidth);
  assert(doc2.viewportHeight == doc.viewportHeight);

  assert(doc2.buffers.size() == 1);
  assert(doc2.buffers[100].byteLength == 512);

  assert(doc2.transforms.size() == 1);
  assert(feq(doc2.transforms[200].tx, 1.0f));
  assert(feq(doc2.transforms[200].sy, 4.0f));

  assert(doc2.panes.size() == 1);
  assert(doc2.panes[1].name == "Main");
  assert(doc2.panes[1].hasClearColor);
  assert(feq(doc2.panes[1].clearColor[0], 0.1f));

  assert(doc2.layers.size() == 1);
  assert(doc2.layers[10].paneId == 1);

  assert(doc2.geometries.size() == 1);
  assert(doc2.geometries[101].format == "pos2_clip");
  assert(doc2.geometries[101].vertexCount == 6);

  assert(doc2.drawItems.size() == 1);
  assert(doc2.drawItems[102].pipeline == "triSolid@1");
  assert(doc2.drawItems[102].blendMode == "screen");
  assert(feq(doc2.drawItems[102].color[0], 0.5f));

  std::printf("T4 roundTrip: PASS\n");
}

// T5: Parse with string ID keys (as the format specifies)
static void testStringIdKeys() {
  const char* json = R"({
    "buffers": { "42": { "byteLength": 100 } },
    "transforms": { "7": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 } }
  })";

  SceneDocument doc;
  bool ok = parseSceneDocument(json, doc);
  assert(ok);
  assert(doc.buffers.count(42) == 1);
  assert(doc.transforms.count(7) == 1);

  std::printf("T5 stringIdKeys: PASS\n");
}

// T6: Compact serialization omits default fields
static void testCompactSerialize() {
  SceneDocument doc;
  doc.version = 1;

  // Minimal draw item with only structural fields set
  doc.buffers[100] = {0};
  doc.panes[1] = {};  // all defaults
  doc.layers[10] = {1, ""};

  DocGeometry g;
  g.vertexBufferId = 100;
  // format defaults to "pos2_clip", vertexCount=1
  doc.geometries[101] = g;

  DocDrawItem di;
  di.layerId = 10;
  di.pipeline = "triSolid@1";
  di.geometryId = 101;
  // Everything else stays at default
  doc.drawItems[200] = di;

  // Full serialization
  std::string full = serializeSceneDocument(doc, false);
  // Compact serialization
  std::string compact = serializeSceneDocument(doc, true);

  // Compact should be significantly shorter
  assert(compact.size() < full.size());

  // Compact should NOT contain default fields
  assert(compact.find("\"dashLength\"") == std::string::npos);
  assert(compact.find("\"gapLength\"") == std::string::npos);
  assert(compact.find("\"cornerRadius\"") == std::string::npos);
  assert(compact.find("\"gradientType\"") == std::string::npos);
  assert(compact.find("\"textureId\"") == std::string::npos);
  assert(compact.find("\"anchorPoint\"") == std::string::npos);
  assert(compact.find("\"isClipSource\"") == std::string::npos);
  assert(compact.find("\"useClipMask\"") == std::string::npos);
  assert(compact.find("\"indexBufferId\"") == std::string::npos);
  assert(compact.find("\"viewport\"") == std::string::npos);

  // Compact SHOULD still contain structural fields
  assert(compact.find("\"pipeline\"") != std::string::npos);
  assert(compact.find("\"layerId\"") != std::string::npos);
  assert(compact.find("\"vertexBufferId\"") != std::string::npos);

  // Round-trip: compact JSON should parse back correctly
  SceneDocument doc2;
  bool ok = parseSceneDocument(compact, doc2);
  assert(ok);
  assert(doc2.drawItems.count(200) == 1);
  assert(doc2.drawItems[200].pipeline == "triSolid@1");
  assert(doc2.drawItems[200].geometryId == 101);
  // Default fields should still parse to defaults
  assert(feq(doc2.drawItems[200].dashLength, 0.0f));
  assert(feq(doc2.drawItems[200].pointSize, 4.0f));
  assert(doc2.drawItems[200].visible);  // default true

  std::printf("T6 compactSerialize: PASS\n");
}

// T7: Compact serialization includes non-default fields
static void testCompactNonDefaults() {
  SceneDocument doc;
  doc.version = 1;
  doc.viewportWidth = 1280;  // non-default
  doc.viewportHeight = 720;

  doc.buffers[100] = {0};
  DocPane p;
  p.name = "Test";
  p.hasClearColor = true;
  p.clearColor[0] = 0.5f;
  doc.panes[1] = p;

  doc.layers[10] = {1, "Layer0"};

  DocGeometry g;
  g.vertexBufferId = 100;
  g.format = "rect4";  // non-default format
  g.vertexCount = 10;
  doc.geometries[101] = g;

  DocDrawItem di;
  di.layerId = 10;
  di.pipeline = "instancedRect@1";
  di.geometryId = 101;
  di.blendMode = "additive";  // non-default
  di.cornerRadius = 5.0f;     // non-default
  di.gradientType = "linear";
  di.gradientAngle = 1.57f;
  di.anchorPoint = "center";
  di.textureId = 42;
  di.visible = false;  // non-default
  doc.drawItems[200] = di;

  std::string compact = serializeSceneDocument(doc, true);

  // Non-default fields SHOULD be present
  assert(compact.find("\"viewport\"") != std::string::npos);
  assert(compact.find("\"blendMode\"") != std::string::npos);
  assert(compact.find("\"cornerRadius\"") != std::string::npos);
  assert(compact.find("\"gradientType\"") != std::string::npos);
  assert(compact.find("\"anchorPoint\"") != std::string::npos);
  assert(compact.find("\"textureId\"") != std::string::npos);
  assert(compact.find("\"visible\"") != std::string::npos);
  assert(compact.find("\"format\"") != std::string::npos);
  assert(compact.find("\"hasClearColor\"") != std::string::npos);

  // Round-trip
  SceneDocument doc2;
  bool ok = parseSceneDocument(compact, doc2);
  assert(ok);
  assert(doc2.viewportWidth == 1280);
  assert(doc2.drawItems[200].blendMode == "additive");
  assert(feq(doc2.drawItems[200].cornerRadius, 5.0f));
  assert(doc2.drawItems[200].gradientType == "linear");
  assert(doc2.drawItems[200].textureId == 42);
  assert(!doc2.drawItems[200].visible);

  std::printf("T7 compactNonDefaults: PASS\n");
}

int main() {
  testParseValid();
  testParseEmpty();
  testParseInvalid();
  testRoundTrip();
  testStringIdKeys();
  testCompactSerialize();
  testCompactNonDefaults();

  std::printf("\nAll D77.1 tests passed.\n");
  return 0;
}
