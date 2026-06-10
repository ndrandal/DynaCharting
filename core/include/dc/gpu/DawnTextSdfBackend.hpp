// ENC-492 (P2.9) — Dawn/WebGPU backend for the `textSDF@1` pipeline.
//
// The Dawn mirror of Renderer::drawTextSdf (kTextSdfVert/kTextSdfFrag in
// core/src/gl/Renderer.cpp). It renders instanced glyph quads sampling a single
// R8 SDF glyph atlas, with the fragment doing the SDF alpha reconstruction
// (smoothstep around 0.5 of the distance field) so the text edges are crisp and
// anti-aliased. It reuses the shared instanced foundation
// (DawnDevice::drawInstanced, ENC-488) and the DawnDevice R8 texture/sampler
// support (ENC-491) — uploading the GlyphAtlas R8 SDF bitmap as a Dawn R8
// texture and sampling it in the SDF shader.
//
// BEHAVIOURAL PARITY with the GL drawTextSdf path:
//   * geometry      — a unit quad (6 vertices, two triangles) generated in the
//                     vertex shader from @builtin(vertex_index) % 6; NO per-vertex
//                     buffer (matches the GL gl_VertexID % 6 quad).
//   * per-instance  — the Glyph8 record: a_g0 = vec4(x0,y0,x1,y1) (quad pos/size)
//                     at location 0 / offset 0, a_g1 = vec4(u0,v0,u1,v1) (atlas UV
//                     rect) at location 1 / offset 16. Stride 32B,
//                     VertexStepMode::Instance (GL divisor 1). The quad corner
//                     mixes between the rect min/max by the unit uv; the atlas UV
//                     is mixed the same way to produce the per-fragment v_uv.
//   * atlas         — the GlyphAtlas R8 SDF bitmap (atlasSize x atlasSize). The
//                     backend creates the Dawn R8 texture on first use and
//                     re-uploads it when the atlas is dirty (grows/changes),
//                     mirroring the GL uploadAtlasIfDirty().
//   * uniforms      — mat3 transform + vec4 color (the 64-byte base uniform
//                     block) + the Sampler2D atlas binding.
//   * SDF alpha     — fs: val = atlas.r; a = smoothstep(0.45,0.55,val);
//                     out = vec4(u_color.rgb, u_color.a * a) — verbatim from
//                     kTextSdfFrag's SDF branch (useSdf()==true). The default
//                     Normal alpha blend (applied by DawnDevice) then composites
//                     the SDF edges — essential for text.
//   * D26 indexed gather — when the geometry carries an index buffer, the visible
//                     glyph instances are GATHERED CPU-side into a scratch
//                     per-instance buffer; only that subset is drawn (mirrors the
//                     GL gather).
//
// WGSL NOTES (vs the GLSL): NDC Y-FLIP — clip.y is negated (same convention as
// triSolid/instancedRect/texturedQuad) so the WebGPU top-left framebuffer matches
// the GL bottom-left readback.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class GlyphAtlas;

class DawnTextSdfBackend final : public IRendererBackend {
 public:
  explicit DawnTextSdfBackend(GlyphAtlas* atlas) : atlas_(atlas) {}

  std::string_view pipelineId() const override { return "textSDF@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The glyph atlas (non-owning; supplied by the caller). Source of the R8 SDF
  // bitmap + per-glyph UV rects. Mirrors Renderer::atlas_.
  GlyphAtlas* atlas_{nullptr};

  // The cached textSDF render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // The Dawn R8 atlas texture, created lazily on first upload (once the atlas
  // size is known) and re-uploaded whenever the atlas is dirty. Mirrors
  // Renderer::atlasTex_ + uploadAtlasIfDirty().
  TextureHandle atlasTex_{};

  // Per-geometry instance buffers (the Glyph8 records), created lazily and
  // reused. The non-indexed path uploads the geometry bytes directly; the
  // indexed (D26) path uploads a CPU-gathered scratch buffer of only the
  // selected glyph instances.
  struct GeoBuffers {
    BufferHandle instanceBuffer{};
    std::uint32_t instanceCount{0};
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  // Create-on-first-use / re-upload-on-dirty the R8 atlas texture (mirrors GL
  // uploadAtlasIfDirty). Returns the valid texture handle, or an invalid one if
  // there is no atlas.
  TextureHandle uploadAtlasIfDirty(GpuDevice& device);

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
};

}  // namespace dc
