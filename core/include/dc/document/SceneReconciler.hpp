#pragma once
#include "dc/document/SceneDocument.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Scene.hpp"

#include <string>
#include <vector>

namespace dc {

struct ReconcileResult {
  bool ok{true};
  int created{0};
  int updated{0};
  int deleted{0};
  std::vector<std::string> errors;

  // Viewport dimensions from the document (0 = not specified)
  int viewportWidth{0};
  int viewportHeight{0};
};

class SceneReconciler {
public:
  explicit SceneReconciler(CommandProcessor& cp);

  // Reconcile newDoc against currentScene. Emits commands via CommandProcessor.
  // Wraps all changes in beginFrame/commitFrame for atomicity.
  ReconcileResult reconcile(const SceneDocument& newDoc, const Scene& currentScene);

private:
  CommandProcessor& cp_;

  // Emit a JSON command; returns false on error (appends to result.errors)
  bool emit(const std::string& json, ReconcileResult& result);

  // Per-type reconciliation
  void reconcileBuffers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void reconcileTransforms(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void reconcilePanes(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void reconcileLayers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void reconcileGeometries(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void reconcileDrawItems(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);

  // Deletion passes (reverse dependency order)
  void deleteRemovedDrawItems(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void deleteRemovedGeometries(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void deleteRemovedLayers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void deleteRemovedPanes(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void deleteRemovedTransforms(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);
  void deleteRemovedBuffers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r);

  // Helpers
  static std::string fmtFloat(float v);
  static std::string escapeJson(const std::string& s);
  static bool floatEq(float a, float b);
  static bool color4Eq(const float a[4], const float b[4]);
  static bool color2Eq(const float a[2], const float b[2]);
  static std::uint8_t anchorStringToEnum(const std::string& s);
  static std::uint8_t gradientStringToEnum(const std::string& s);
  static std::string blendModeToString(BlendMode bm);
};

} // namespace dc
