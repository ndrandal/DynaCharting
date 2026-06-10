#pragma once
// GpuBufferManager — the OpenGL backend of the streaming buffer manager.
//
// ENC-485 (P2.2): the backend-AGNOSTIC CPU side (per-id CPU bytes, capacity,
// dirty-range list + coalescing, UploadStats, reserve/writeRange/setCpuData/
// getCpuData) was extracted into dc::CpuBufferStore (dc/render/CpuBufferStore.hpp,
// pure `dc`, no GL). This class now derives from it and adds ONLY the GL-specific
// part: a VBO per buffer id and the glBufferData/glBufferSubData upload loop.
// The dirty-range coalescing is shared with the Dawn backend via the base.
//
// Public API and GL upload behaviour are unchanged from the pre-ENC-485 class
// (setCpuData/reserve/writeRange/uploadDirty/lastUploadStats/getGlBuffer/
// getCpuData/getCpuDataSize all behave identically), so the GL renderer and all
// GL tests are unaffected.
#include "dc/render/CpuBufferStore.hpp"
#include "dc/ids/Id.hpp"
#include <glad/gl.h>
#include <cstdint>
#include <unordered_map>

namespace dc {

class GpuBufferManager : public CpuBufferStore {
public:
  // Re-export UploadStats at its historical name dc::GpuBufferManager::UploadStats
  // for existing callers/tests.
  using UploadStats = CpuBufferStore::UploadStats;

  ~GpuBufferManager() override;

  // Upload any dirty buffers to GL VBOs using the shared coalescing computed by
  // the CpuBufferStore base. Full (re)allocations use glBufferData
  // (GL_DYNAMIC_DRAW); partial uploads use glBufferSubData per coalesced range.
  // Returns total bytes uploaded.
  std::uint64_t uploadDirty();

  GLuint getGlBuffer(Id bufferId) const;

private:
  std::unordered_map<Id, GLuint> vbos_;  // GL VBO per buffer id (0 == none)
};

} // namespace dc
