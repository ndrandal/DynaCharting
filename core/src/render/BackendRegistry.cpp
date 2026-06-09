// ENC-483 (P1.3) — BackendRegistry implementation. See BackendRegistry.hpp.
#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"

namespace dc {

void BackendRegistry::registerBackend(DeviceKind kind,
                                      IRendererBackend* backend) {
  if (!backend) return;
  std::string id(backend->pipelineId());
  // Replace any existing registration for the same (kind, pipelineId).
  for (Entry& e : entries_) {
    if (e.kind == kind && e.pipelineId == id) {
      e.backend = backend;
      return;
    }
  }
  entries_.push_back(Entry{kind, std::move(id), backend});
}

IRendererBackend* BackendRegistry::find(DeviceKind kind,
                                        std::string_view pipelineId) const {
  for (const Entry& e : entries_) {
    if (e.kind == kind && e.pipelineId == pipelineId) {
      return e.backend;
    }
  }
  return nullptr;
}

}  // namespace dc
