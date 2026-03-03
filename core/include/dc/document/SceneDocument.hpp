#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace dc {

// --- Document struct definitions ---
// These mirror the Scene graph types exactly, but use ordered maps (keyed by Id)
// for deterministic diffing and serialization.

struct DocBuffer {
  std::uint32_t byteLength{0};
};

struct DocTransform {
  float tx{0}, ty{0}, sx{1}, sy{1};
};

struct DocPaneRegion {
  float clipYMin{-1.0f}, clipYMax{1.0f};
  float clipXMin{-1.0f}, clipXMax{1.0f};
};

struct DocPane {
  std::string name;
  DocPaneRegion region;
  float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  bool hasClearColor{false};
};

struct DocLayer {
  Id paneId{0};
  std::string name;
};

struct DocGeometry {
  Id vertexBufferId{0};
  std::string format{"pos2_clip"};
  std::uint32_t vertexCount{1};
  Id indexBufferId{0};
  std::uint32_t indexCount{0};
};

struct DocDrawItem {
  Id layerId{0};
  std::string name;
  std::string pipeline;
  Id geometryId{0};
  Id transformId{0};

  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float colorUp[4] = {0.0f, 0.8f, 0.0f, 1.0f};
  float colorDown[4] = {0.8f, 0.0f, 0.0f, 1.0f};
  float pointSize{4.0f};
  float lineWidth{1.0f};
  float dashLength{0.0f};
  float gapLength{0.0f};
  float cornerRadius{0.0f};

  std::string blendMode{"normal"};
  bool isClipSource{false};
  bool useClipMask{false};

  std::uint32_t textureId{0};

  // Anchor
  std::string anchorPoint;  // empty = no anchor
  float anchorOffsetX{0};
  float anchorOffsetY{0};

  bool visible{true};

  // Gradient
  std::string gradientType;  // empty or "none" = no gradient, "linear", "radial"
  float gradientAngle{0.0f};
  float gradientColor0[4] = {1, 1, 1, 1};
  float gradientColor1[4] = {0, 0, 0, 1};
  float gradientCenter[2] = {0.5f, 0.5f};
  float gradientRadius{0.5f};
};

struct SceneDocument {
  int version{1};
  int viewportWidth{0};
  int viewportHeight{0};

  std::map<Id, DocBuffer> buffers;
  std::map<Id, DocTransform> transforms;
  std::map<Id, DocPane> panes;
  std::map<Id, DocLayer> layers;
  std::map<Id, DocGeometry> geometries;
  std::map<Id, DocDrawItem> drawItems;
};

// Parse a JSON string into a SceneDocument. Returns true on success.
bool parseSceneDocument(const std::string& json, SceneDocument& out);

// Serialize a SceneDocument to JSON string.
// When compact=true, fields at their default values are omitted for concise output.
std::string serializeSceneDocument(const SceneDocument& doc, bool compact = false);

} // namespace dc
