// ENC-480 (P0.2) — DawnDevice: the headless WebGPU/Dawn implementation of the
// GpuDevice seam (foundation only).
//
// This is the first concrete `GpuDevice` over WebGPU via Dawn's native backend.
// SCOPE (ENC-480): the DEVICE-LEVEL foundation that every later Dawn render
// test (ENC-484+) builds on:
//   * init()           — bring up a dawn::native::Instance, pick an adapter
//                        (Vulkan / lavapipe software here), create the
//                        wgpu::Device + wgpu::Queue.
//   * an offscreen render target (RGBA8Unorm, RenderAttachment|CopySrc) — the
//     headless framebuffer. Created lazily by beginRenderPass at (w,h).
//   * beginRenderPass / endRenderPass — clear the target via a render pass and
//     submit (LoadOp::Clear with the desc's clearColor).
//   * readPixel() — SYNCHRONOUS readback: copyTextureToBuffer into a MapRead
//     buffer, then map it and pump the native instance's events until the map
//     callback fires (Dawn's map is async; we present a blocking API to match
//     OsMesaContext::readPixels for the GL tests).
//
// OUT OF SCOPE here (minimal stubs with TODO refs):
//   * createPipeline / createBindGroup / bindPipeline / draw / drawInstanced —
//     the triSolid pipeline + WGSL land in ENC-484 (no shaders/PSOs here).
//   * setBlendMode / setClipState — pipeline-variant selection, ENC-484/ENC-493.
//   * buffers / textures — vertex/index/uniform buffer + texture upload, the
//     streaming write-range model, land with the draw path (TODO ENC-485).
//
// Mirrors core/include/dc/gl/GlDevice.hpp in shape so the GL and Dawn devices
// stay parallel. Like GlDevice (which includes <glad/gl.h>), this header pulls
// in the Dawn/WebGPU headers directly — it lives in the dc_gpu library, which
// is the one place Dawn types are allowed.
#pragma once

#include "dc/render/GpuDevice.hpp"

#include <dawn/native/DawnNative.h>
#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dc {

class DawnDevice final : public GpuDevice {
 public:
  DawnDevice() = default;
  ~DawnDevice() override;

  // --- lifecycle ----------------------------------------------------------
  // Create the Dawn instance, request an adapter, create the device + queue.
  // Returns false (with errorMessage() populated) if no adapter is found.
  bool init() override;

  /// Human-readable reason init() returned false (empty on success). Lets the
  /// test print a clear message when no Vulkan/lavapipe adapter is available.
  const std::string& errorMessage() const { return errorMessage_; }

  /// Backend the selected adapter runs on, as a string (e.g. "Vulkan"), and the
  /// adapter's device name. Populated by init(); for test diagnostics/logging.
  const std::string& backendName() const { return backendName_; }
  const std::string& adapterName() const { return adapterName_; }

  // --- buffer resources (TODO(ENC-485) — draw path) -----------------------
  BufferHandle createBuffer(std::size_t capacityBytes, const void* initData,
                            std::size_t initBytes) override;
  void updateBuffer(BufferHandle buf, const void* data,
                    std::size_t bytes) override;
  void writeBufferRange(BufferHandle buf, std::size_t offsetBytes,
                        const void* data, std::size_t bytes) override;
  void destroyBuffer(BufferHandle buf) override;

  // --- texture resources (TODO(ENC-485) — draw path) ----------------------
  TextureHandle createTexture(const TextureDesc& desc) override;
  void updateTexture(TextureHandle tex, const std::uint8_t* data) override;
  void destroyTexture(TextureHandle tex) override;

  // --- pipelines & bind groups (TODO(ENC-484) — triSolid pipeline) --------
  PipelineHandle createPipeline(const PipelineDesc& desc) override;
  void destroyPipeline(PipelineHandle pipe) override;
  BindGroupHandle createBindGroup(const BindGroupDesc& desc) override;
  void destroyBindGroup(BindGroupHandle group) override;

  // --- render pass --------------------------------------------------------
  void beginRenderPass(const RenderPassDesc& desc) override;
  void endRenderPass() override;
  void setViewport(std::uint32_t width, std::uint32_t height) override;
  void setScissorRect(const ScissorRect& rect) override;

  // --- per-draw mutable state (TODO(ENC-484/ENC-493) — pipeline variants) --
  void setBlendMode(DeviceBlendMode mode) override;
  void setClipState(ClipMode mode) override;

  // --- draw (TODO(ENC-484) — triSolid pipeline) ---------------------------
  void bindPipeline(PipelineHandle pipe) override;
  DeviceDrawStats draw(BindGroupHandle group,
                       const DrawParams& params) override;
  DeviceDrawStats drawInstanced(BindGroupHandle group,
                                const DrawInstancedParams& params) override;

  // --- readback -----------------------------------------------------------
  // Synchronous RGBA8 readback at (x, y) from the offscreen target. Blocks by
  // pumping the native instance's events until the buffer map completes.
  void readPixel(std::int32_t x, std::int32_t y,
                 std::uint8_t* outRgba) override;

  // ENC-485 — Synchronous buffer readback for streaming-upload verification.
  // Copies [offsetBytes, offsetBytes+bytes) of `buf` into a MapRead staging
  // buffer and blocks until mapped, then copies the bytes into `out` (which must
  // hold >= bytes). Returns false if the handle is invalid or the range exceeds
  // the buffer capacity. Test-facing (a streaming test reads the buffer back to
  // assert the coalesced upload landed the right bytes); not part of GpuDevice.
  bool readBuffer(BufferHandle buf, std::size_t offsetBytes, std::size_t bytes,
                  std::uint8_t* out);

 private:
  // Lazily (re)create the offscreen RGBA8Unorm color target at (w, h).
  void ensureRenderTarget(std::uint32_t w, std::uint32_t h);

  // Pump instance.ProcessEvents() (and tick the device) until *done is true.
  // The blocking primitive behind the synchronous readback.
  void waitUntil(const bool* done);

  bool inited_{false};
  std::string errorMessage_;
  std::string backendName_;
  std::string adapterName_;

  // Dawn owns the instance; it must outlive every wgpu object below. Held by
  // unique_ptr because dawn::native::Instance is non-copyable/non-assignable
  // (RAII over the underlying instance), so it can't be default-constructed as
  // a member and reassigned in init().
  std::unique_ptr<dawn::native::Instance> instance_;
  wgpu::Device device_;
  wgpu::Queue queue_;

  // Offscreen color target (the headless framebuffer). Recreated on resize.
  wgpu::Texture colorTexture_;
  wgpu::TextureView colorView_;
  std::uint32_t targetW_{0};
  std::uint32_t targetH_{0};

  // Active render pass scratch (valid between beginRenderPass/endRenderPass).
  wgpu::CommandEncoder encoder_;
  wgpu::RenderPassEncoder pass_;

  // --- ENC-484 draw-path resource tables ---------------------------------
  // Opaque handles index into these vectors (1-based; id 0 == null). Mirrors
  // GlDevice's BufferEntry/TextureEntry slot model so the Dawn and GL devices
  // stay parallel.
  //
  // Buffers (ENC-484: static create+write is enough for triSolid; the full
  // streaming/coalescing write-range model is TODO(ENC-485)).
  struct BufferEntry {
    wgpu::Buffer buffer;
    std::size_t capacity{0};
    bool isIndex{false};  // chosen at create time so usage flags match
  };
  std::vector<BufferEntry> buffers_;

  // --- ENC-491 texture resources -----------------------------------------
  // A 2D texture (texturedQuad@1 user images, and the ENC-492 SDF glyph atlas).
  // Owns the wgpu::Texture, a default view, and a sampler. RGBA8 (4-channel
  // user images / pick) and R8 (single-channel coverage / SDF atlas) are both
  // supported via the format/bytesPerPixel recorded at create time so
  // updateTexture re-uploads with the right row pitch. The sampler's
  // FilterMode (Linear/Nearest) comes from the TextureDesc; address mode is
  // ClampToEdge (matches GL's GL_CLAMP_TO_EDGE in TextureManager).
  struct TextureEntry {
    wgpu::Texture texture;
    wgpu::TextureView view;
    wgpu::Sampler sampler;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t bytesPerPixel{4};  // 4 (RGBA8) or 1 (R8)
    wgpu::TextureFormat format{wgpu::TextureFormat::RGBA8Unorm};
  };
  std::vector<TextureEntry> textures_;
  TextureEntry* textureAt(TextureHandle h);

  // Render pipelines. Each PipelineDesc -> one wgpu::RenderPipeline + its
  // implicit bind-group layout (group 0) and a uniform-buffer size. Cached by
  // the backend (it creates one per pipelineId and reuses it), so we never
  // rebuild per draw.
  struct PipelineEntry {
    wgpu::RenderPipeline pipeline;
    wgpu::BindGroupLayout bindGroupLayout;
    std::size_t uniformSize{0};  // bytes of the group-0 uniform buffer
    // ENC-491: when true the pipeline's group-0 layout adds a texture (binding
    // 1) + sampler (binding 2) alongside the uniform buffer (binding 0), and
    // createBindGroup must supply the bound texture's view+sampler. Set when the
    // PipelineDesc declares a Sampler2D uniform (texturedQuad@1; future SDF text).
    bool hasTexture{false};
  };
  std::vector<PipelineEntry> pipelines_;

  // Bind groups. A bind group owns a small uniform buffer (the packed
  // transform+color) plus the wgpu::BindGroup referencing it, and records the
  // vertex/index buffers + index format to bind at draw time (WebGPU sets
  // vertex/index buffers on the render pass, not in the bind group).
  struct BindGroupEntry {
    wgpu::BindGroup bindGroup;
    wgpu::Buffer uniformBuffer;
    BufferHandle vertexBuffer{};
    BufferHandle indexBuffer{};
    wgpu::IndexFormat indexFormat{wgpu::IndexFormat::Uint32};
  };
  std::vector<BindGroupEntry> bindGroups_;

  // The pipeline bound by the most recent bindPipeline(), applied by draw().
  PipelineHandle boundPipeline_{};

  BufferEntry* bufferAt(BufferHandle h);
  PipelineEntry* pipelineAt(PipelineHandle h);
  BindGroupEntry* bindGroupAt(BindGroupHandle h);
};

}  // namespace dc
