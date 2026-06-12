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

// ENC-503 (P6.2): Browser/Emscripten build path. The DEVICE-ACQUISITION half of
// DawnDevice is the only part that is backend-specific: native uses
// dawn::native::Instance + EnumerateAdapters + a synchronous CreateDevice, which
// only exists in a native Dawn build. In the browser (emdawnwebgpu) the same
// device is obtained from navigator.gpu via the ASYNC RequestAdapter /
// RequestDevice on a wgpu::Instance, and the readback event-pump yields to the
// JS event loop (ASYNCIFY) instead of calling the native ProcessEvents entry.
// Everything else — the WGSL, pipelines, buffers, draws, copy-to-buffer readback
// — is identical webgpu_cpp and compiles unchanged against emdawnwebgpu. So
// <dawn/native/DawnNative.h> (a native-only header) is included ONLY off-browser.
#ifndef __EMSCRIPTEN__
#include <dawn/native/DawnNative.h>
#endif
#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
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

  // ENC-590 (P0.2) — STORAGE buffer creation path. A SECOND, additive create
  // variant for the WebGPU-compute half (ENC-591 spike, Phases 4 & 6): the same
  // streaming/readback shape as createBuffer above, but the underlying
  // wgpu::Buffer carries BufferUsage::Storage so a compute pipeline can bind it
  // as a read/write storage binding. The existing vertex/index path
  // (createBuffer + kStreamBufferUsage) is left COMPLETELY unchanged.
  //
  // The returned handle is a normal BufferHandle into the same slot table, so
  // every existing buffer op works on it: queue.WriteBuffer (updateBuffer /
  // writeBufferRange — CopyDst is included) writes from the CPU, and readBuffer
  // (CopyBufferToBuffer -> MapRead via the waitUntil() map-pump — CopySrc is
  // included) reads it back. This ticket adds ONLY the storage usage flag + a
  // round-trip; it builds no compute pipeline (that is ENC-591).
  BufferHandle createStorageBuffer(std::size_t capacityBytes,
                                   const void* initData, std::size_t initBytes);
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

  // ENC-503 (P6.2) — full-framebuffer RGBA8 readback (top-down, tightly packed,
  // W*H*4 bytes) from the target bound by the most recent beginRenderPass. Same
  // copy-to-buffer + async-map path as readPixel, but returns the whole image so
  // the browser harness can blit it onto a <canvas> via putImageData. `out` must
  // hold W*H*4 bytes; W/H are the active target size. Returns false if no target
  // is bound or `out` is too small. Single full copy (vs N readPixel calls) keeps
  // the canvas display cheap. Identical webgpu_cpp on native and emdawnwebgpu.
  bool readFramebufferRGBA(std::uint8_t* out, std::size_t outBytes,
                           std::uint32_t* outW, std::uint32_t* outH);

  // ENC-497 (P4.1) — windowed presentation accessors. The Dawn surface +
  // swapchain (DawnWindowContext) lives OUTSIDE DawnDevice (it owns the GLFW
  // window + wgpu::Surface), but it needs the device's underlying wgpu objects to
  // build the surface, query its capabilities, and run the per-frame blit that
  // copies the offscreen scene target onto the swapchain texture. These return the
  // live wgpu handles (empty/null before init()). They are pure getters — the
  // headless offscreen path is unchanged. Available on native; the windowed path
  // (DawnWindowContext) is itself native-only.
  wgpu::Device& wgpuDevice() { return device_; }
  wgpu::Queue& wgpuQueue() { return queue_; }
#ifndef __EMSCRIPTEN__
  // The native dawn::native::Instance's underlying WGPUInstance — surface
  // creation (instance.CreateSurface) and GetCapabilities go through the same
  // instance that owns the device.
  WGPUInstance wgpuInstance() const {
    return instance_ ? instance_->Get() : nullptr;
  }
  // The adapter chosen by init() (retained for SurfaceCapabilities queries, which
  // are adapter-scoped). Null before init().
  wgpu::Adapter& wgpuAdapter() { return adapter_; }
#endif
  // The color TextureView of render target `id` (0 == the main scene target), or
  // null if that target hasn't been created yet (call render()/beginRenderPass on
  // it first). The windowed blit samples this as its input. The target's color
  // texture already carries TextureBinding usage (ensureRenderTarget), so its view
  // is sampleable.
  wgpu::TextureView colorViewForTarget(std::uint32_t id) {
    RenderTarget* rt = targetAt(id);
    return rt ? rt->colorView : wgpu::TextureView{};
  }

  // ENC-485 — Synchronous buffer readback for streaming-upload verification.
  // Copies [offsetBytes, offsetBytes+bytes) of `buf` into a MapRead staging
  // buffer and blocks until mapped, then copies the bytes into `out` (which must
  // hold >= bytes). Returns false if the handle is invalid or the range exceeds
  // the buffer capacity. Test-facing (a streaming test reads the buffer back to
  // assert the coalesced upload landed the right bytes); not part of GpuDevice.
  bool readBuffer(BufferHandle buf, std::size_t offsetBytes, std::size_t bytes,
                  std::uint8_t* out);

  // ENC-496 (P3.4) — wrap a render target's color texture as a SAMPLEABLE
  // TextureHandle so a fullscreen post-process pass can sample it as the input
  // of the next pass. The ENC-495 RenderTarget color texture now carries
  // TextureBinding usage (see ensureRenderTarget), so its view can be bound to a
  // sampledTexture pipeline (ENC-491 binding 1 + a sampler at binding 2). This
  // returns a TextureHandle whose TextureEntry references the target's existing
  // color view plus a freshly-created sampler (filter from `filter`); it does NOT
  // copy or re-create the texture. Returns a null handle if the target `id` has
  // not been created (call beginRenderPass on it first). The post-process stack
  // ensures the scene/intermediate targets exist (by rendering into them), then
  // feeds target N's color into pass N+1's bind group via this handle.
  TextureHandle textureForRenderTarget(std::uint32_t id,
                                       TextureFilter filter = TextureFilter::Linear);

 private:
  // ENC-495 (D29.3) — multi-target render-to-texture.
  //
  // ENC-480/494 keyed a SINGLE offscreen color+stencil target on (w,h) and
  // beginRenderPass always rendered into it. D29.3 GPU picking needs a SEPARATE
  // offscreen color target (the pick buffer) so the pick pass — which draws each
  // DrawItem's id as a flat RGB color — does not clobber the visible main target.
  //
  // ensureRenderTarget/beginRenderPass now branch on RenderTargetHandle::id:
  //   * id 0  -> the main target (the visible/backbuffer-equivalent framebuffer),
  //              co-allocated with a Stencil8 attachment for the D29.2 clip mask.
  //   * id !=0 -> a distinct render-to-texture target (id 1 == the pick buffer).
  // Each target is RGBA8Unorm (RenderAttachment|CopySrc, so readPixel can copy
  // out of it) + a co-sized Stencil8 attachment (so every pipeline variant's
  // DepthStencilState has a stencil to attach, even when the target never clips).
  //
  // Targets are cached by id and recreated on resize. readPixel reads back from
  // the target bound by the most recent beginRenderPass (activeTarget_), so the
  // Renderer's pick path reads the pick buffer, not the main one. This is the
  // render-to-texture foundation ENC-496 (post-process) reuses: a post pass can
  // render the scene into a non-zero target, then sample it as a texture.
  struct RenderTarget {
    wgpu::Texture colorTexture;
    wgpu::TextureView colorView;
    wgpu::Texture stencilTexture;
    wgpu::TextureView stencilView;
    std::uint32_t w{0};
    std::uint32_t h{0};
  };

  // Lazily (re)create the offscreen color+stencil target `id` at (w, h) and
  // return it. id 0 is the main target; non-zero ids are extra render-to-texture
  // targets (id 1 == the D29.3 pick buffer). Recreated on resize.
  RenderTarget& ensureRenderTarget(std::uint32_t id, std::uint32_t w,
                                   std::uint32_t h);

  // Pump instance.ProcessEvents() (and tick the device) until *done is true.
  // The blocking primitive behind the synchronous readback.
  void waitUntil(const bool* done);

  bool inited_{false};
  std::string errorMessage_;
  std::string backendName_;
  std::string adapterName_;

#ifndef __EMSCRIPTEN__
  // NATIVE: Dawn owns the instance; it must outlive every wgpu object below. Held
  // by unique_ptr because dawn::native::Instance is non-copyable/non-assignable
  // (RAII over the underlying instance), so it can't be default-constructed as
  // a member and reassigned in init().
  std::unique_ptr<dawn::native::Instance> instance_;
  // ENC-497: the adapter init() selected, retained so the windowed presentation
  // path can query the surface's adapter-scoped capabilities (GetCapabilities).
  // Headless-only code never touches it. wgpu::Adapter is ref-counted, so holding
  // it here keeps the adapter alive past EnumerateAdapters' local vector.
  wgpu::Adapter adapter_;
#else
  // BROWSER (ENC-503): the wgpu::Instance from emdawnwebgpu (wgpu::CreateInstance,
  // backed by navigator.gpu). It owns the futures serviced by the readback pump,
  // so it must outlive the device/queue/buffers — same lifetime role as the
  // native instance_ above.
  wgpu::Instance emInstance_;
#endif
  wgpu::Device device_;
  wgpu::Queue queue_;

  // ENC-495: the offscreen targets, keyed by RenderTargetHandle::id. The main
  // target (id 0) is the headless framebuffer (ENC-480); id 1 is the D29.3 pick
  // buffer. Each carries its own RGBA8Unorm color + Stencil8 attachment (the
  // D29.2 clip mask). Recreated on resize. A small flat vector (only a couple of
  // targets ever exist) keyed by id keeps lookup trivial.
  std::vector<std::pair<std::uint32_t, RenderTarget>> targets_;
  RenderTarget* targetAt(std::uint32_t id);

  // The target bound by the most recent beginRenderPass — readPixel reads back
  // from it (so the pick path reads the pick buffer, not the main target).
  std::uint32_t activeTarget_{0};
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

  // Render pipelines. Each PipelineDesc -> one base wgpu::RenderPipeline (built
  // with the DrawItem's default Normal blend) + its implicit bind-group layout
  // (group 0) and a uniform-buffer size. Cached by the backend (it creates one
  // per pipelineId and reuses it), so we never rebuild per draw.
  //
  // ENC-493 (D29.1) — blend-mode permutation cache. WebGPU bakes blend state
  // into the immutable pipeline (unlike GL's per-draw glBlendFunc), so the four
  // per-DrawItem blend modes (Normal/Additive/Multiply/Screen) become pipeline
  // VARIANTS of the same base pipeline. Each PipelineEntry caches up to four
  // wgpu::RenderPipelines keyed by DeviceBlendMode; the Normal variant is the
  // base built in createPipeline (byte-identical to pre-ENC-493 behavior). The
  // Additive/Multiply/Screen variants are built LAZILY on first use (when
  // bindPipeline is asked for that mode) and then cached — never rebuilt per
  // frame. Building a variant reuses the already-compiled WGSL module and the
  // pipeline layout/vertex layouts captured here, changing ONLY the baked
  // wgpu::BlendState. NOTE(ENC-494): stencil clipping also turns createPipeline
  // into a permutation source (ClipMode); it will extend this same variant model
  // (the cache key grows to (blend, clip)). Keep the variant builder factored so
  // ENC-494 can add the clip axis without re-plumbing.
  struct PipelineEntry {
    wgpu::BindGroupLayout bindGroupLayout;
    std::size_t uniformSize{0};  // bytes of the group-0 uniform buffer
    // ENC-491: when true the pipeline's group-0 layout adds a texture (binding
    // 1) + sampler (binding 2) alongside the uniform buffer (binding 0), and
    // createBindGroup must supply the bound texture's view+sampler. Set when the
    // PipelineDesc declares a Sampler2D uniform (texturedQuad@1; future SDF text).
    bool hasTexture{false};

    // ENC-493/494 variant cache + the immutable inputs needed to (re)build a
    // variant. ENC-493 keyed the cache on DeviceBlendMode alone (4 slots);
    // ENC-494 adds the ClipMode axis (the variant builder was deliberately
    // factored so the clip axis drops in — see the class-level note), so the key
    // is now (blendMode, clipMode) and the table is a 2D grid:
    //   variants[blendIndex(Normal..Screen)][clipIndex(None/WriteMask/UseMask)]
    // A null slot has not been built yet. variants[Normal][None] is the base
    // pipeline (byte-identical to the pre-ENC-493 default: Normal blend, no
    // stencil, full color write). Every (blend,clip) cell is built LAZILY on
    // first bindPipeline request for that combination and then cached. Building a
    // cell reuses the already-compiled WGSL module + pipeline layout + vertex
    // layouts; it bakes only the per-blend wgpu::BlendState, the per-clip
    // wgpu::DepthStencilState, and (for the WriteMask clip source) sets
    // colorTarget.writeMask = None so the clip geometry writes stencil only.
    wgpu::RenderPipeline variants[4][3];
    // Captured pipeline build inputs (shared by every blend variant): the
    // compiled WGSL module, the group-0 pipeline layout, the topology, and a
    // self-owned snapshot of the vertex buffer layouts (the original
    // PipelineDesc arrays are caller-owned and not valid after createPipeline,
    // so the variant builder owns its own copy that the wgpu layouts point into).
    wgpu::ShaderModule module;
    wgpu::PipelineLayout pipelineLayout;
    wgpu::PrimitiveTopology topology{wgpu::PrimitiveTopology::TriangleList};
    std::vector<wgpu::VertexBufferLayout> vbLayouts;
    std::vector<std::vector<wgpu::VertexAttribute>> vbAttrStorage;
  };
  std::vector<PipelineEntry> pipelines_;

  // ENC-493/494: lazily build (and cache) the wgpu::RenderPipeline variant of
  // `pe` for the (blend, clip) combination, returning it. The (Normal, None)
  // variant is built eagerly in createPipeline; the rest are built here on first
  // request. Reuses pe.module / pe.pipelineLayout / pe.topology / pe.vbLayouts
  // and bakes only the per-blend BlendState + per-clip DepthStencilState +
  // color-write mask.
  wgpu::RenderPipeline& pipelineVariant(PipelineEntry& pe, DeviceBlendMode blend,
                                        ClipMode clip);

  // The blend mode selected by the most recent setBlendMode(); applied by
  // bindPipeline() to pick the matching pipeline variant. Defaults to Normal so
  // a backend that never calls setBlendMode gets the byte-identical base.
  DeviceBlendMode currentBlendMode_{DeviceBlendMode::Normal};

  // ENC-494 (D29.2): the clip/stencil mode selected by the most recent
  // setClipState(); applied by bindPipeline() to pick the matching pipeline
  // variant. Defaults to None (stencil disabled, full color write) so a backend
  // that never calls setClipState gets the byte-identical base — the existing
  // Dawn tests stay green.
  ClipMode currentClipMode_{ClipMode::None};

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
