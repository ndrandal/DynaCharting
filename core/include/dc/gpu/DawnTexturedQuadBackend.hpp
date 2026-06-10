// ENC-491 (P2.8) — Dawn/WebGPU backend for the `texturedQuad@1` pipeline.
//
// The Dawn mirror of Renderer::drawTexturedQuad (kTexQuadVert/kTexQuadFrag in
// core/src/gl/Renderer.cpp). It samples a 2D texture over an instanced unit quad
// with per-DrawItem color modulation, reusing the shared instanced foundation
// (DawnDevice::drawInstanced, ENC-488) and the new DawnDevice texture/sampler
// support (ENC-491).
//
// BEHAVIOURAL PARITY with the GL drawTexturedQuad path:
//   * geometry      — a unit quad (6 vertices, two triangles) generated in the
//                     vertex shader from @builtin(vertex_index) % 6; NO per-vertex
//                     buffer (matches the GL gl_VertexID % 6 quad).
//   * per-instance  — one Pos2Uv4 attribute a_pos_uv = vec4(x0,y0,x1,y1) at
//                     location 0, VertexStepMode::Instance (GL divisor 1). The
//                     quad corner mixes between the rect min/max by the unit uv;
//                     that SAME uv is the texture coordinate (GL's v_uv). Stride
//                     16B (4-byte aligned, ENC-485).
//   * texture       — the DrawItem's textureId. The backend creates a Dawn
//                     texture from the TextureManager bytes on first use (lazily,
//                     cached per textureId) and binds its view+sampler.
//   * uniforms      — mat3 transform + vec4 color (the 64-byte base uniform
//                     block) + the Sampler2D texture binding.
//   * color modulation — fs out = texel * u_color (verbatim from kTexQuadFrag).
//   * D26 indexed gather — when the geometry carries an index buffer, the visible
//                     instances are GATHERED CPU-side into a scratch per-instance
//                     buffer; only that subset is drawn (mirrors the GL gather).
//
// WGSL NOTES (vs the GLSL): NDC Y-FLIP — clip.y is negated (same convention as
// triSolid/instancedRect) so the WebGPU top-left framebuffer matches the GL
// bottom-left readback. Default Normal alpha blend is already applied by
// DawnDevice::createPipeline.
//
// TEXTURE SOURCE: the backend needs the raw RGBA bytes for a textureId so it can
// upload them into a Dawn texture. The Renderer-owned TextureManager (dc_gl)
// holds GL texture names, not CPU bytes, so this backend takes a small
// device-neutral provider seam (TextureSource) the caller wires up — for the
// test this is a tiny in-memory map of id -> rgba pixels.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

/// Device-neutral provider of a texture's CPU pixels for a logical textureId.
/// The Dawn backend uploads these into a wgpu::Texture on first use. (The GL
/// TextureManager keeps GL names, not CPU bytes; this seam lets the Dawn path
/// obtain the source pixels without a GL dependency. ENC-492's glyph atlas will
/// supply its R8 coverage bytes the same way.)
class TextureSource {
 public:
  virtual ~TextureSource() = default;

  /// Pixels for `textureId`. Returns the tightly-packed pixel bytes (RGBA8 =
  /// 4B/px, or R8 = 1B/px) and the dimensions/format. Returns false if unknown.
  virtual bool getTexturePixels(std::uint32_t textureId,
                                const std::uint8_t** outData,
                                std::uint32_t* outWidth,
                                std::uint32_t* outHeight,
                                TextureFormat* outFormat) const = 0;
};

class DawnTexturedQuadBackend final : public IRendererBackend {
 public:
  explicit DawnTexturedQuadBackend(const TextureSource* textures)
      : textures_(textures) {}

  std::string_view pipelineId() const override { return "texturedQuad@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // Source of CPU texture pixels (non-owning; supplied by the caller).
  const TextureSource* textures_{nullptr};

  // The cached texturedQuad render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // Per-geometry instance buffers (the Pos2Uv4 rect records), created lazily and
  // reused. The non-indexed path uploads the geometry bytes directly; the
  // indexed (D26) path uploads a CPU-gathered scratch buffer of only the selected
  // instances.
  struct GeoBuffers {
    BufferHandle instanceBuffer{};
    std::uint32_t instanceCount{0};
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  // Per-textureId Dawn textures, created lazily from the TextureSource on first
  // use and reused across frames/draws.
  std::vector<std::pair<std::uint32_t, TextureHandle>> textureHandles_;

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
  TextureHandle ensureTexture(GpuDevice& device, std::uint32_t textureId);
};

}  // namespace dc
