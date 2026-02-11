#pragma once
#include "dc/ids/Id.hpp"
#include <string>

namespace dc {

enum class ResourceKind : std::uint8_t {
  Pane,
  Layer,
  DrawItem,
  Buffer,
  Geometry,
  Transform
};

inline const char* toString(ResourceKind k) {
  switch (k) {
    case ResourceKind::Pane: return "pane";
    case ResourceKind::Layer: return "layer";
    case ResourceKind::DrawItem: return "drawItem";
    case ResourceKind::Buffer: return "buffer";
    case ResourceKind::Geometry: return "geometry";
    case ResourceKind::Transform: return "transform";
    default: return "unknown";
  }
}

struct TransformParams {
  float tx{0}, ty{0}, sx{1}, sy{1};
};

struct Transform {
  Id id{0};
  TransformParams params{};
  float mat3[9] = {1,0,0, 0,1,0, 0,0,1};
};

inline void recomputeMat3(Transform& t) {
  t.mat3[0] = t.params.sx; t.mat3[1] = 0;           t.mat3[2] = 0;
  t.mat3[3] = 0;           t.mat3[4] = t.params.sy;  t.mat3[5] = 0;
  t.mat3[6] = t.params.tx; t.mat3[7] = t.params.ty;  t.mat3[8] = 1;
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
  Id transformId{0};     // optional; 0 = identity
};



} // namespace dc
