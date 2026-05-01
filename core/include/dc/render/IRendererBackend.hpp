// ENC-90: per-pipeline rendering backend. The orchestrating Renderer
// becomes a small dispatcher keyed on pipeline kind; each concrete
// backend owns the GL state for one pipeline (`triSolid@1`,
// `instancedCandle@1`, etc.). Adding a pipeline kind = adding a backend
// and registering it; no edit to Renderer.cpp's central dispatch.
//
// PHASE 1: interface published; concrete backends migrate from the
// existing switch in Renderer.cpp incrementally.
#pragma once

#include <cstdint>
#include <string>

namespace dc {

class Scene;
class GpuBufferManager;
struct DrawItem;

/// Statistics each render() pass returns to the dispatcher.
struct BackendStats {
  std::uint32_t drawCalls{0};
  std::uint32_t verticesSubmitted{0};
};

/// Per-pipeline rendering backend. One instance per pipeline kind, owned
/// by Renderer.
class IRendererBackend {
public:
  virtual ~IRendererBackend() = default;

  /// Stable pipeline identifier this backend handles, e.g. "triSolid@1".
  virtual std::string_view pipelineId() const = 0;

  /// Set up GL state, link/use shader program, etc. Called once at engine
  /// boot; safe to call again to re-init after a context loss.
  virtual bool initGL() = 0;

  /// Draw a single DrawItem. Caller (Renderer) has already filtered
  /// drawItems whose `pipeline` matches this backend's `pipelineId`.
  /// The viewport size is forwarded for backends that need it (text /
  /// instanced layouts).
  virtual BackendStats renderDrawItem(const Scene& scene,
                                      GpuBufferManager& gpu,
                                      const DrawItem& item,
                                      int viewW, int viewH) = 0;
};

}  // namespace dc
