#pragma once
#include "dc/ids/Id.hpp"
#include <string>

namespace dc {

enum class ResourceKind : std::uint8_t {
  Pane,
  Layer,
  DrawItem,
  Buffer,
  Geometry
};

inline const char* toString(ResourceKind k) {
  switch (k) {
    case ResourceKind::Pane: return "pane";
    case ResourceKind::Layer: return "layer";
    case ResourceKind::DrawItem: return "drawItem";
    case ResourceKind::Buffer: return "buffer";
    case ResourceKind::Geometry: return "geometry";
    default: return "unknown";
  }
}

struct Pane {
  Id id{0};
  std::string name;
};

struct Layer {
  Id id{0};
  Id paneId{0};
  std::string name;
};

struct DrawItem {
  Id id{0};
  Id layerId{0};
  std::string name;

  // bindings for pipeline execution
  std::string pipeline;  // e.g. "triSolid@1"
  Id geometryId{0};      // must refer to a Geometry resource
};



} // namespace dc
