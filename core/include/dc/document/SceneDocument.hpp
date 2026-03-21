#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dc {

// --- Document struct definitions ---
// These mirror the Scene graph types exactly, but use ordered maps (keyed by Id)
// for deterministic diffing and serialization.

struct DocBuffer {
  std::uint32_t byteLength{0};
  std::vector<float> data;  // inline vertex floats; when non-empty, byteLength is derived
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

// --- Viewport declaration (for interactive pan/zoom) ---
struct DocViewport {
  Id transformId{0};
  Id paneId{0};
  double xMin{0}, xMax{1}, yMin{0}, yMax{1};
  std::string linkGroup;
  bool panX{true}, panY{true}, zoomX{true}, zoomY{true};
};

// --- Text overlay declarations ---
struct DocTextLabel {
  float clipX{0}, clipY{0};
  std::string text;
  std::string align{"l"};  // "l", "c", "r"
  std::string color;        // hex, empty = default
  int fontSize{0};          // 0 = use overlay default
};

struct DocTextOverlay {
  int fontSize{12};
  std::string color{"#b2b5bc"};
  std::vector<DocTextLabel> labels;
};

// --- Binding declarations (D80: reactive data relationships) ---

struct DocBindingTrigger {
  std::string type;                  // "selection", "hover", "viewport", "threshold"
  Id drawItemId{0};                  // for selection/hover triggers
  std::string viewportName;          // for viewport trigger
  Id sourceBufferId{0};              // for threshold trigger
  std::uint32_t fieldOffset{0};      // byte offset to float field in record
  std::string condition;             // "greaterThan", "lessThan", "crossingUp", "crossingDown"
  double value{0};                   // threshold value
};

struct DocBindingEffect {
  std::string type;                  // "filterBuffer", "rangeBuffer", "setVisible", "setColor"

  // filterBuffer / rangeBuffer
  Id sourceBufferId{0};
  Id outputBufferId{0};
  std::uint32_t recordStride{0};
  Id geometryId{0};                  // auto-update vertexCount
  std::uint32_t xFieldOffset{0};     // rangeBuffer: byte offset of x-coordinate

  // setVisible
  Id drawItemId{0};
  bool visible{true};
  bool defaultVisible{true};

  // setColor
  float color[4] = {1, 1, 1, 1};
  float defaultColor[4] = {1, 1, 1, 1};
};

struct DocBinding {
  DocBindingTrigger trigger;
  DocBindingEffect effect;
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

  std::map<std::string, DocViewport> viewports;
  DocTextOverlay textOverlay;

  std::map<Id, DocBinding> bindings;
};

// Parse a JSON string into a SceneDocument. Returns true on success.
bool parseSceneDocument(const std::string& json, SceneDocument& out);

// Serialize a SceneDocument to JSON string.
// When compact=true, fields at their default values are omitted for concise output.
std::string serializeSceneDocument(const SceneDocument& doc, bool compact = false);

} // namespace dc
