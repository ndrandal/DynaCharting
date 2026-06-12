#pragma once
#include "dc/ids/Id.hpp"
#include "dc/layout/PaneLayout.hpp"
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

// ENC-613 (P2.3) — coordinate mode (RESEARCH §4.2/§4.3). The affine mat3 is a
// CARTESIAN transform (data x,y -> clip x,y); it cannot express POLAR (data
// angle,radius -> clip x,y = center + r*(cos theta, sin theta)). The polar slot
// is therefore a distinct coords mode, applied by the encode pass at COMPILE time
// (the renderer stays dumb — it only ever sees clip-space geometry). This is what
// radial / pie / rose charts need.
enum class CoordsMode : std::uint8_t {
  Cartesian = 0,  // data (x,y) -> clip (x,y) via the affine mat3 (the default)
  Polar     = 1,  // data (angle,radius) -> clip (cx + r*cos, cy + r*sin)
};

// ENC-613 — polar coordinate parameters: the clip-space center the angle/radius
// is measured from. Angles are in RADIANS (0 = +x axis, CCW). The radius is in
// CLIP units (so a unit circle r=1 fills the [-1,1] viewport). A scale upstream
// maps the data radius column into clip radius; the angle column maps to radians.
struct PolarParams {
  float centerX{0.0f};  // clip-space center x
  float centerY{0.0f};  // clip-space center y
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
  PaneRegion region{-1.0f, 1.0f, -1.0f, 1.0f};
  float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  bool hasClearColor{false};
};

struct Layer {
  Id id{0};
  Id paneId{0};
  std::string name;
};

// D29.1: per-DrawItem blend mode
enum class BlendMode : std::uint8_t {
  Normal   = 0,
  Additive = 1,
  Multiply = 2,
  Screen   = 3
};

struct DrawItem {
  Id id{0};
  Id layerId{0};
  std::string name;

  // bindings for pipeline execution
  std::string pipeline;  // e.g. "triSolid@1"
  Id geometryId{0};      // must refer to a Geometry resource
  Id transformId{0};     // optional; 0 = identity

  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // RGBA, default white

  // Style fields (D10.1)
  float colorUp[4]   = {0.0f, 0.8f, 0.0f, 1.0f};  // candle up color (green)
  float colorDown[4] = {0.8f, 0.0f, 0.0f, 1.0f};  // candle down color (red)
  float pointSize{4.0f};
  float lineWidth{1.0f};

  // D28.1: dash pattern for lineAA@1 (0 = solid, no dash)
  float dashLength{0.0f};   // pixels
  float gapLength{0.0f};    // pixels

  // D28.2: rounded corner radius for instancedRect@1 (0 = sharp corners)
  float cornerRadius{0.0f}; // pixels

  // D29.1: blend mode
  BlendMode blendMode{BlendMode::Normal};

  // D29.2: stencil-based clipping masks
  bool isClipSource{false};  // writes to stencil, no color output
  bool useClipMask{false};   // only renders where stencil == 1

  // D36: texture binding
  std::uint32_t textureId{0};

  // D37: anchoring
  std::uint8_t anchorPoint{0};  // 0=TopLeft..8=BottomRight (matches AnchorPoint enum)
  float anchorOffsetX{0};       // pixels
  float anchorOffsetY{0};       // pixels
  bool hasAnchor{false};

  bool visible{true};  // D14.2: visibility toggle

  // D46: gradient fill
  std::uint8_t gradientType{0};  // 0=None, 1=Linear, 2=Radial
  float gradientAngle{0.0f};     // radians, for linear
  float gradientColor0[4] = {1,1,1,1};
  float gradientColor1[4] = {0,0,0,1};
  float gradientCenter[2] = {0.5f, 0.5f};  // normalized 0-1
  float gradientRadius{0.5f};
};



} // namespace dc
