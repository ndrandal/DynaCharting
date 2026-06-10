// ENC-90: per-pipeline rendering backend. The orchestrating Renderer
// becomes a small dispatcher keyed on pipeline kind; each concrete
// backend owns the GPU state for one pipeline (`triSolid@1`,
// `instancedCandle@1`, etc.). Adding a pipeline kind = adding a backend
// and registering it; no edit to Renderer.cpp's central dispatch.
//
// PHASE 1: interface published; concrete backends migrate from the
// existing switch in Renderer.cpp incrementally.
//
// ENC-481 (P1.1): this interface is now *device-agnostic*. A backend no longer
// implies raw GL — it is initialised with, and renders through, a `GpuDevice`
// (see GpuDevice.hpp). That keeps the seam usable by both the GlDevice (ENC-482)
// and the future DawnDevice (ENC-49x). The old GL-named `initGL()` is gone;
// `init(GpuDevice&)` replaces it. This is interface/design only — Renderer.cpp
// is NOT migrated to backends here (that is ENC-482); today nothing implements
// this interface yet, so finalizing the signatures breaks no compilation.
#pragma once

#include <cstdint>
#include <string>

namespace dc {

class Scene;
class CpuBufferStore;
class GpuDevice;
struct DrawItem;

/// Statistics each render() pass returns to the dispatcher.
struct BackendStats {
  std::uint32_t drawCalls{0};
  std::uint32_t verticesSubmitted{0};
};

/// Per-pipeline rendering backend. One instance per pipeline kind, owned
/// by Renderer. Device-agnostic: a backend talks to the GPU only through the
/// `GpuDevice` it is handed, never via raw GL/Dawn calls.
class IRendererBackend {
public:
  virtual ~IRendererBackend() = default;

  /// Stable pipeline identifier this backend handles, e.g. "triSolid@1".
  virtual std::string_view pipelineId() const = 0;

  /// Set up device resources for this pipeline: create the render pipeline(s)
  /// (and any clip/blend permutations — see GpuDevice / ENC-493), allocate
  /// long-lived bind groups, etc. Called once at engine boot; safe to call
  /// again to re-init after a context/device loss.
  ///
  /// Replaces the GL-specific `initGL()` (ENC-481). The backend retains the
  /// `device` reference (owned by the Renderer) for use in renderDrawItem.
  /// Returns false on failure (e.g. pipeline/shader creation failed).
  virtual bool init(GpuDevice& device) = 0;

  /// Draw a single DrawItem through the device. Caller (Renderer) has already
  /// filtered drawItems whose `pipeline` matches this backend's `pipelineId`,
  /// and has already applied the per-item viewport/scissor/blend/clip state on
  /// the device. The backend selects/binds its pipeline + bind group and issues
  /// the draw via `device`. `gpu` is the device-agnostic CPU byte source
  /// (CpuBufferStore) for this item's geometry — the GL backend downcasts it to
  /// its GpuBufferManager to reach getGlBuffer(); the Dawn backend reads CPU
  /// bytes straight from the base. `viewW/viewH` are forwarded for backends that
  /// need pixel-
  /// space sizing (text / instanced / AA-line layouts).
  ///
  /// TODO(ENC-482): adopt this in Renderer.cpp — port each draw* helper in
  /// core/src/gl/Renderer.cpp into a concrete IRendererBackend that routes
  /// through `device` instead of calling glDraw* directly.
  virtual BackendStats renderDrawItem(GpuDevice& device,
                                      const Scene& scene,
                                      CpuBufferStore& gpu,
                                      const DrawItem& item,
                                      int viewW, int viewH) = 0;
};

}  // namespace dc
