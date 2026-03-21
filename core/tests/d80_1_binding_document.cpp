// D80.1: Binding declarations — parse/serialize round-trip tests
#include "dc/document/SceneDocument.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-5f; }
static bool deq(double a, double b) { return std::fabs(a - b) < 1e-9; }

// ---- Test 1: Parse a filterBuffer binding ----
static void testParseFilterBinding() {
  const char* json = R"({
    "version": 1,
    "bindings": {
      "1001": {
        "trigger": { "type": "selection", "drawItemId": 300 },
        "effect": {
          "type": "filterBuffer",
          "sourceBufferId": 100,
          "outputBufferId": 110,
          "recordStride": 16,
          "geometryId": 501
        }
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.bindings.size() == 1);
  assert(doc.bindings.count(1001) == 1);

  const auto& b = doc.bindings.at(1001);
  assert(b.trigger.type == "selection");
  assert(b.trigger.drawItemId == 300);
  assert(b.effect.type == "filterBuffer");
  assert(b.effect.sourceBufferId == 100);
  assert(b.effect.outputBufferId == 110);
  assert(b.effect.recordStride == 16);
  assert(b.effect.geometryId == 501);

  std::printf("  PASS: parse filterBuffer binding\n");
}

// ---- Test 2: Parse a setVisible binding with threshold trigger ----
static void testParseThresholdBinding() {
  const char* json = R"({
    "bindings": {
      "2001": {
        "trigger": {
          "type": "threshold",
          "sourceBufferId": 100,
          "fieldOffset": 4,
          "condition": "greaterThan",
          "value": 50.0
        },
        "effect": {
          "type": "setVisible",
          "drawItemId": 600,
          "visible": true,
          "defaultVisible": false
        }
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.bindings.count(2001) == 1);

  const auto& b = doc.bindings.at(2001);
  assert(b.trigger.type == "threshold");
  assert(b.trigger.sourceBufferId == 100);
  assert(b.trigger.fieldOffset == 4);
  assert(b.trigger.condition == "greaterThan");
  assert(deq(b.trigger.value, 50.0));

  assert(b.effect.type == "setVisible");
  assert(b.effect.drawItemId == 600);
  assert(b.effect.visible == true);
  assert(b.effect.defaultVisible == false);

  std::printf("  PASS: parse threshold/setVisible binding\n");
}

// ---- Test 3: Parse a setColor binding with hover trigger ----
static void testParseColorBinding() {
  const char* json = R"({
    "bindings": {
      "3001": {
        "trigger": { "type": "hover", "drawItemId": 400 },
        "effect": {
          "type": "setColor",
          "drawItemId": 500,
          "color": [1, 0.8, 0, 1],
          "defaultColor": [0.5, 0.5, 0.5, 1]
        }
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));

  const auto& b = doc.bindings.at(3001);
  assert(b.trigger.type == "hover");
  assert(b.trigger.drawItemId == 400);
  assert(b.effect.type == "setColor");
  assert(b.effect.drawItemId == 500);
  assert(feq(b.effect.color[0], 1.0f));
  assert(feq(b.effect.color[1], 0.8f));
  assert(feq(b.effect.defaultColor[0], 0.5f));

  std::printf("  PASS: parse hover/setColor binding\n");
}

// ---- Test 4: Parse a viewport/rangeBuffer binding ----
static void testParseViewportBinding() {
  const char* json = R"({
    "bindings": {
      "4001": {
        "trigger": { "type": "viewport", "viewportName": "main" },
        "effect": {
          "type": "rangeBuffer",
          "sourceBufferId": 100,
          "outputBufferId": 111,
          "recordStride": 24,
          "geometryId": 502,
          "xFieldOffset": 0
        }
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));

  const auto& b = doc.bindings.at(4001);
  assert(b.trigger.type == "viewport");
  assert(b.trigger.viewportName == "main");
  assert(b.effect.type == "rangeBuffer");
  assert(b.effect.recordStride == 24);
  assert(b.effect.xFieldOffset == 0);

  std::printf("  PASS: parse viewport/rangeBuffer binding\n");
}

// ---- Test 5: Serialize round-trip ----
static void testSerializeRoundTrip() {
  dc::SceneDocument doc;
  doc.version = 1;

  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 16;
  b.effect.geometryId = 501;
  doc.bindings[1001] = b;

  std::string json = dc::serializeSceneDocument(doc, false);

  // Parse it back
  dc::SceneDocument doc2;
  assert(dc::parseSceneDocument(json, doc2));
  assert(doc2.bindings.size() == 1);
  assert(doc2.bindings.count(1001) == 1);

  const auto& b2 = doc2.bindings.at(1001);
  assert(b2.trigger.type == "selection");
  assert(b2.trigger.drawItemId == 300);
  assert(b2.effect.type == "filterBuffer");
  assert(b2.effect.sourceBufferId == 100);
  assert(b2.effect.outputBufferId == 110);
  assert(b2.effect.recordStride == 16);
  assert(b2.effect.geometryId == 501);

  std::printf("  PASS: serialize round-trip\n");
}

// ---- Test 6: Compact serialize omits defaults ----
static void testCompactSerialize() {
  dc::SceneDocument doc;

  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 16;
  b.effect.geometryId = 501;
  doc.bindings[1001] = b;

  std::string json = dc::serializeSceneDocument(doc, true);

  // Compact should not contain "viewportName" (it's empty/default)
  assert(json.find("viewportName") == std::string::npos);
  // But should contain the essential fields
  assert(json.find("\"selection\"") != std::string::npos);
  assert(json.find("\"filterBuffer\"") != std::string::npos);

  std::printf("  PASS: compact serialize omits defaults\n");
}

// ---- Test 7: Multiple bindings ordered by ID ----
static void testMultipleBindings() {
  const char* json = R"({
    "bindings": {
      "5001": {
        "trigger": { "type": "selection", "drawItemId": 10 },
        "effect": { "type": "setVisible", "drawItemId": 20, "visible": false }
      },
      "5002": {
        "trigger": { "type": "hover", "drawItemId": 30 },
        "effect": { "type": "setColor", "drawItemId": 40 }
      }
    }
  })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.bindings.size() == 2);

  // std::map preserves order
  auto it = doc.bindings.begin();
  assert(it->first == 5001);
  ++it;
  assert(it->first == 5002);

  std::printf("  PASS: multiple bindings ordered by ID\n");
}

// ---- Test 8: Empty bindings section parses OK ----
static void testEmptyBindings() {
  const char* json = R"({ "bindings": {} })";

  dc::SceneDocument doc;
  assert(dc::parseSceneDocument(json, doc));
  assert(doc.bindings.empty());

  std::printf("  PASS: empty bindings section\n");
}

int main() {
  std::printf("D80.1: Binding document parse/serialize\n");
  testParseFilterBinding();
  testParseThresholdBinding();
  testParseColorBinding();
  testParseViewportBinding();
  testSerializeRoundTrip();
  testCompactSerialize();
  testMultipleBindings();
  testEmptyBindings();
  std::printf("All D80.1 tests passed.\n");
  return 0;
}
