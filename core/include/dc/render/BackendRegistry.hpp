// ENC-483 (P1.3) — Backend registry + dispatcher seam.
//
// GOAL
// ----
// Replace Renderer's central string-matched `if (di->pipeline == "...") ...`
// dispatch with a registry keyed on (device kind × pipelineId). For each
// DrawItem the Renderer dispatcher asks the registry for the backend that owns
// `di->pipeline` and routes the per-pipeline draw through that
// `IRendererBackend` (see IRendererBackend.hpp). The per-pane / per-layer scene
// walk, culling, scissor, blend and clip-state application stay in the Renderer
// dispatcher (they are already routed through GpuDevice in ENC-482); only the
// *pipeline selection + per-pipeline draw dispatch* moves here.
//
// KEYING
// ------
// The registry is keyed on (DeviceKind, pipelineId). A device kind selects which
// backend *set* is active: today only `DeviceKind::Gl` backends exist; ENC-484+
// will register `DeviceKind::Dawn` backends for the same pipelineIds without
// editing any central switch. `find(kind, pipelineId)` returns the matching
// backend or nullptr (caller falls back to the legacy inline path while pipelines
// are being ported one at a time).
//
// OWNERSHIP
// ---------
// The registry stores *non-owning* `IRendererBackend*`. Concrete backends are
// owned by whoever registers them (the Renderer owns its GL backends as members),
// and must outlive the registry. This keeps the registry header-light and free of
// any graphics-API include — it is pure C++17 and lives in the `dc` library.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dc {

class IRendererBackend;

/// Which device's backend set a registration belongs to. A backend implements a
/// pipeline for exactly one device kind; the dispatcher looks up backends for the
/// device kind it is currently rendering with.
///   Gl   — the OpenGL 3.3 backends (this ticket: GlTriSolidBackend).
///   Dawn — the future WebGPU/Dawn backends (ENC-484+; none registered yet).
enum class DeviceKind : std::uint8_t {
  Gl   = 0,
  Dawn = 1,
};

/// Maps (DeviceKind, pipelineId) -> IRendererBackend*. Small, linear-scan map:
/// the pipeline count is tiny (10) and lookups happen once per DrawItem, so a
/// flat vector keeps the header free of <unordered_map> hashing of a composite
/// key while staying O(pipelines) per draw. Registration is one-time at boot.
class BackendRegistry {
public:
  BackendRegistry() = default;

  /// Register `backend` under (kind, backend->pipelineId()). A later registration
  /// for the same (kind, pipelineId) replaces the earlier one. The registry does
  /// NOT take ownership; `backend` must outlive the registry.
  void registerBackend(DeviceKind kind, IRendererBackend* backend);

  /// Look up the backend handling `pipelineId` for `kind`, or nullptr if none is
  /// registered (caller falls back to the legacy inline path).
  IRendererBackend* find(DeviceKind kind, std::string_view pipelineId) const;

  /// Number of registered backends (for tests / diagnostics).
  std::size_t size() const { return entries_.size(); }

private:
  struct Entry {
    DeviceKind kind;
    std::string pipelineId;     // owned copy of backend->pipelineId()
    IRendererBackend* backend;  // non-owning
  };
  std::vector<Entry> entries_;
};

}  // namespace dc
