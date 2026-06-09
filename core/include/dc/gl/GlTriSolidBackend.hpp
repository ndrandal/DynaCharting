// ENC-483 (P1.3) — GL backend for the `triSolid@1` pipeline.
//
// This is the FIRST pipeline ported out of Renderer.cpp's central
// `if (di->pipeline == "triSolid@1") drawPos2(..., GL_TRIANGLES, ...)` dispatch
// into a registered `IRendererBackend`, proving the registry seam end-to-end
// (see BackendRegistry.hpp). It owns its own GL program (the shared Pos2 shader)
// and issues the triSolid draw itself.
//
// SCOPE: triSolid@1 ONLY. line2d@1 / points@1 reuse the same Pos2 shader but are
// NOT ported here (different topology); they stay on Renderer's legacy inline
// `drawPos2` path until ENC-486+. The remaining 7 pipelines are likewise
// untouched (ENC-487..492).
//
// DEVICE ROUTING (matches ENC-482's scope note): per-DrawItem viewport, scissor,
// blend and clip state are applied by the Renderer dispatcher through GpuDevice
// BEFORE calling renderDrawItem (and the shared VAO is bound at pass scope by the
// device). The actual vertex/uniform/draw for triSolid is issued via raw GL here,
// exactly as the old drawPos2 helper did — GpuDevice::draw / bindPipeline are
// still ENC-484 stubs, so the bind-group/pipeline path is not yet available. When
// ENC-484 lands the GL draw path on GpuDevice, this backend's body moves onto
// device.bindPipeline + device.draw(bindGroup, ...).
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/gl/ShaderProgram.hpp"

namespace dc {

class GlTriSolidBackend final : public IRendererBackend {
public:
  std::string_view pipelineId() const override { return "triSolid@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device,
                              const Scene& scene,
                              GpuBufferManager& gpu,
                              const DrawItem& item,
                              int viewW, int viewH) override;

private:
  ShaderProgram prog_;  // the Pos2 program (a_pos + u_transform/u_color/u_pointSize)
};

}  // namespace dc
