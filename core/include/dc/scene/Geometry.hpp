#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <string>

namespace dc {

enum class VertexFormat : std::uint8_t {
  Pos2_Clip = 1 // vec2 position in clip space
};

inline const char* toString(VertexFormat f) {
  switch (f) {
    case VertexFormat::Pos2_Clip: return "pos2_clip";
    default: return "unknown";
  }
}

struct Buffer {
  Id id{0};
  std::uint32_t byteLength{0};
};

struct Geometry {
  Id id{0};
  Id vertexBufferId{0};
  VertexFormat format{VertexFormat::Pos2_Clip};
  std::uint32_t vertexCount{0}; // number of vertices
};

} // namespace dc
