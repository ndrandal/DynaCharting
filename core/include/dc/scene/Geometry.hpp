#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <string>

namespace dc {

enum class VertexFormat : std::uint8_t {
  Pos2_Clip = 1, // vec2 position in clip space
  Rect4     = 2, // vec4 (x0, y0, x1, y1) for instanced rects
  Candle6   = 3  // 6 floats (x, open, high, low, close, halfWidth) for instanced candles
};

inline const char* toString(VertexFormat f) {
  switch (f) {
    case VertexFormat::Pos2_Clip: return "pos2_clip";
    case VertexFormat::Rect4:    return "rect4";
    case VertexFormat::Candle6:  return "candle6";
    default: return "unknown";
  }
}

inline std::uint32_t strideOf(VertexFormat f) {
  switch (f) {
    case VertexFormat::Pos2_Clip: return 8;
    case VertexFormat::Rect4:    return 16;
    case VertexFormat::Candle6:  return 24;
    default: return 0;
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
