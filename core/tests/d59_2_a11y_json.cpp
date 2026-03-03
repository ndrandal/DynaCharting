// D59.2 — AccessibilityBridge JSON serialization
#include "dc/metadata/AccessibilityBridge.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/metadata/AnnotationStore.hpp"

#include <rapidjson/document.h>

#include <cstdio>
#include <string>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    ++failed;
  }
}

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D59.2 AccessibilityBridge JSON Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::AnnotationStore annotations;

  // Build scene: 1 pane, 1 layer, 2 annotated drawItems.
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Chart"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di1");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "di2");

  annotations.set(3, "series", "Candles", "OHLC");
  annotations.set(4, "axis", "Y Axis", "price");

  dc::AccessibilityBridge bridge;
  dc::AccessibilityConfig config;
  config.viewW = 800;
  config.viewH = 600;

  auto tree = bridge.buildTree(scene, annotations, config);

  // Test 1: toJSON produces valid JSON.
  std::string json = dc::AccessibilityBridge::toJSON(tree);
  check(!json.empty(), "toJSON produces non-empty string");
  std::printf("    JSON length: %zu\n", json.size());

  // Test 2: Parse JSON and verify structure.
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  check(!doc.HasParseError(), "JSON parses without error");
  check(doc.IsArray(), "root is array");

  if (doc.IsArray() && doc.Size() > 0) {
    // Test 3: First element is the pane root.
    const auto& root = doc[0];
    check(root.IsObject(), "root element is object");
    check(root.HasMember("id"), "root has id field");
    check(root.HasMember("role"), "root has role field");
    check(root.HasMember("name"), "root has name field");
    check(root.HasMember("value"), "root has value field");
    check(root.HasMember("boundingBox"), "root has boundingBox field");

    if (root.HasMember("role") && root["role"].IsString()) {
      check(std::string(root["role"].GetString()) == "group", "root role is group");
    }
    if (root.HasMember("name") && root["name"].IsString()) {
      check(std::string(root["name"].GetString()) == "Chart", "root name is Chart");
    }

    // Test 4: boundingBox is an array of 4 numbers.
    if (root.HasMember("boundingBox") && root["boundingBox"].IsArray()) {
      check(root["boundingBox"].Size() == 4, "boundingBox has 4 elements");
    }

    // Test 5: Children array exists with annotated items.
    check(root.HasMember("children"), "root has children");
    if (root.HasMember("children") && root["children"].IsArray()) {
      check(root["children"].Size() == 2, "children has 2 elements");

      if (root["children"].Size() >= 2) {
        const auto& child0 = root["children"][0];
        check(child0.HasMember("id"), "child0 has id");
        check(child0.HasMember("role"), "child0 has role");
        check(child0.HasMember("name"), "child0 has name");
        check(child0.HasMember("value"), "child0 has value");
        check(child0.HasMember("boundingBox"), "child0 has boundingBox");

        if (child0.HasMember("role") && child0["role"].IsString()) {
          std::string role = child0["role"].GetString();
          check(role == "series" || role == "axis", "child0 role is series or axis");
        }

        // Children should not have a children key (leaf nodes).
        check(!child0.HasMember("children"), "leaf nodes have no children key");
      }
    }
  }

  // Test 6: Empty tree serializes to empty array.
  {
    std::vector<dc::AccessibleNode> emptyTree;
    std::string emptyJson = dc::AccessibilityBridge::toJSON(emptyTree);
    check(emptyJson == "[]", "empty tree produces []");
  }

  // Test 7: Single pane with no children.
  {
    dc::Scene s2;
    dc::ResourceRegistry r2;
    dc::CommandProcessor cp2(s2, r2);
    dc::AnnotationStore ann2;

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":10,"name":"Empty"})"), "pane2");

    auto tree2 = bridge.buildTree(s2, ann2);
    std::string json2 = dc::AccessibilityBridge::toJSON(tree2);

    rapidjson::Document doc2;
    doc2.Parse(json2.c_str());
    check(doc2.IsArray() && doc2.Size() == 1, "single pane produces 1-element array");
    if (doc2.Size() > 0) {
      check(!doc2[0].HasMember("children"), "pane with no children omits children key");
    }
  }

  // Test 8: Nested structure with includeUnannotated.
  {
    dc::AccessibilityConfig cfg3;
    cfg3.viewW = 800;
    cfg3.viewH = 600;
    cfg3.includeUnannotated = true;
    cfg3.defaultRole = "presentation";

    // Add an unannotated drawItem.
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di3");

    auto tree3 = bridge.buildTree(scene, annotations, cfg3);
    std::string json3 = dc::AccessibilityBridge::toJSON(tree3);

    rapidjson::Document doc3;
    doc3.Parse(json3.c_str());
    check(!doc3.HasParseError(), "includeUnannotated JSON is valid");
    if (doc3.IsArray() && doc3.Size() > 0
        && doc3[0].HasMember("children") && doc3[0]["children"].IsArray()) {
      check(doc3[0]["children"].Size() == 3, "includeUnannotated: 3 children in JSON");

      // Find the presentation child.
      bool foundPresentation = false;
      for (unsigned i = 0; i < doc3[0]["children"].Size(); ++i) {
        const auto& c = doc3[0]["children"][i];
        if (c.HasMember("role") && c["role"].IsString()
            && std::string(c["role"].GetString()) == "presentation") {
          foundPresentation = true;
        }
      }
      check(foundPresentation, "unannotated child has presentation role in JSON");
    }
  }

  // Test 9: JSON contains numeric id values.
  {
    check(json.find("\"id\"") != std::string::npos, "JSON contains id fields");
    check(json.find("\"role\"") != std::string::npos, "JSON contains role fields");
  }

  // Test 10: Value field is serialized.
  {
    check(json.find("\"OHLC\"") != std::string::npos, "JSON contains annotation value OHLC");
    check(json.find("\"price\"") != std::string::npos, "JSON contains annotation value price");
  }

  std::printf("=== D59.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
