#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/metadata/AnnotationStore.hpp"
#include <string>
#include <vector>

namespace dc {

struct AccessibleNode {
  Id id{0};
  std::string role;
  std::string name;
  std::string value;
  float boundingBox[4] = {0,0,0,0}; // x, y, w, h
  std::vector<AccessibleNode> children;
};

struct AccessibilityConfig {
  int viewW{800}, viewH{600};
  bool includeUnannotated{false};
  std::string defaultRole{"presentation"};
};

class AccessibilityBridge {
public:
  std::vector<AccessibleNode> buildTree(const Scene& scene,
                                         const AnnotationStore& annotations,
                                         const AccessibilityConfig& config = {});
  static std::string toJSON(const std::vector<AccessibleNode>& tree);
};

} // namespace dc
