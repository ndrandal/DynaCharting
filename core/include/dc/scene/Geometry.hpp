#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <string>

namespace dc {

enum class VertexFormat : std::uint8_t {
  Pos2_Clip = 1, // vec2 position in clip space
  Rect4     = 2, // vec4 (x0, y0, x1, y1) for instanced rects
  Candle6   = 3, // 6 floats (x, open, high, low, close, halfWidth) for instanced candles
  Glyph8    = 4, // 8 floats (x0, y0, x1, y1, u0, v0, u1, v1) for text SDF
  Pos2Alpha  = 5, // 3 floats (x, y, alpha) for edge-fringe AA triangles
  Pos2Color4 = 6, // 6 floats (x, y, r, g, b, a) for per-vertex color gradient
  Pos2Uv4   = 7, // 4 floats (x, y, u, v) for textured quads
  // ENC-608 (P2.1) — the KEYSTONE per-instance-color rect format. 24 bytes:
  //   vec4 f32 rect (x0,y0,x1,y1)  @ 0  (16B)
  //   RGBA8  packed color           @ 16 (4B, Unorm8x4)
  //   f32    scalar lane            @ 20 (4B; optional per-instance scalar —
  //                                  reserved so a future per-instance ROW ID
  //                                  for picking can ride this lane, ENC-594)
  // Generalizes Candle6's per-instance multi-channel proof so ~4 walled views
  // (weather-radar / correlation / footprint / pie) collapse to native with
  // ZERO compute. See DawnInstancedRectColorBackend + EncodePass Mark::RectColor.
  Rect4Color = 8,
  // ENC-609 (P2.2) — the per-POINT color+size scatter format. 16 bytes:
  //   vec2 f32 position (x,y)       @ 0  (8B)
  //   RGBA8  packed color           @ 8  (4B, Unorm8x4)
  //   f32    size (point diameter,  @ 12 (4B; pixels — quad half-extent)
  //          in pixels)
  // The per-point sibling of Rect4Color: one instanced quad per point, sized in
  // pixels (so the scatter scales with the viewport, not the affine transform)
  // and colored from the per-instance packed RGBA8. See
  // DawnInstancedPointColorBackend + EncodePass Mark::PointColor.
  Point4Color = 9
};

inline const char* toString(VertexFormat f) {
  switch (f) {
    case VertexFormat::Pos2_Clip: return "pos2_clip";
    case VertexFormat::Rect4:    return "rect4";
    case VertexFormat::Candle6:  return "candle6";
    case VertexFormat::Glyph8:   return "glyph8";
    case VertexFormat::Pos2Alpha:  return "pos2_alpha";
    case VertexFormat::Pos2Color4: return "pos2_color4";
    case VertexFormat::Pos2Uv4:   return "pos2_uv4";
    case VertexFormat::Rect4Color: return "rect4_color";
    case VertexFormat::Point4Color: return "point4_color";
    default: return "unknown";
  }
}

inline std::uint32_t strideOf(VertexFormat f) {
  switch (f) {
    case VertexFormat::Pos2_Clip: return 8;
    case VertexFormat::Rect4:    return 16;
    case VertexFormat::Candle6:  return 24;
    case VertexFormat::Glyph8:   return 32;
    case VertexFormat::Pos2Alpha:  return 12;
    case VertexFormat::Pos2Color4: return 24;
    case VertexFormat::Pos2Uv4:   return 16;
    case VertexFormat::Rect4Color: return 24;  // rect4 (16) + rgba8 (4) + scalar/rowid lane (4)
    case VertexFormat::Point4Color: return 16; // pos2 (8) + rgba8 (4) + size (4)
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

  // Index buffer (D26): 0 = no index buffer (draw all vertices)
  Id indexBufferId{0};
  std::uint32_t indexCount{0};

  // Axis-aligned bounding box in data space (D10.5)
  float boundsMin[2] = {-1e30f, -1e30f};
  float boundsMax[2] = { 1e30f,  1e30f};
  bool boundsValid{false};
};

} // namespace dc
