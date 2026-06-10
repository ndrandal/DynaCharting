// ENC-481 (P1.1) — Device-agnostic GPU abstraction for the DynaCharting
// WebGPU/Dawn migration.
//
// GOAL
// ----
// `GpuDevice` is the single seam through which the orchestrating Renderer (and,
// after ENC-482, each per-pipeline IRendererBackend) talks to the GPU. It hides
// the concrete graphics API behind opaque handles, enums and plain descriptor
// structs. Two concrete implementations are planned:
//
//   * GlDevice   (ENC-482) — wraps the existing OpenGL 3.3 path that lives in
//                            core/src/gl/Renderer.cpp today. It keeps GL's
//                            mutable global state model internally.
//   * DawnDevice (ENC-49x) — wraps Dawn / WebGPU. WebGPU has *immutable* render
//                            pipeline objects and an explicit render-pass /
//                            command-encoder model.
//
// DESIGN PRINCIPLES (read before extending this file)
// ---------------------------------------------------
//  1. NO graphics-API types leak through this interface. No GLuint, no
//     WGPU* handles, no glad/dawn headers. Everything crossing the boundary is
//     a POD: an opaque handle (`struct FooHandle { uint32_t id; }`), an enum,
//     or a descriptor struct. This header therefore only includes <cstdint> /
//     <cstddef> and is pure C++17. It lives in the *pure* `dc` library.
//
//  2. Shaped for the WebGPU immutable-PSO model, not GL's mutable globals.
//     In GL you flip blend/stencil/program with independent setters at draw
//     time (see Renderer::render's applyBlendMode + glStencil* calls). In
//     WebGPU all of that — shader modules, blend state, depth/stencil state,
//     color-write mask, vertex layout, primitive topology — is baked into an
//     immutable `RenderPipeline` chosen *before* the draw. So the primary draw
//     model here is: pick a pipeline (a permutation of pipeline-id × blend ×
//     clip state), bind groups, draw. See ENC-493: a permutation cache will
//     materialise one pipeline per (pipelineId, BlendMode, ClipMode) tuple so
//     the dispatcher never mutates pipeline state mid-pass.
//
//  3. v1 keeps a couple of mutable setters (setBlendMode / setClipState /
//     setViewport / setScissorRect) so the GlDevice impl in ENC-482 can be a
//     thin, behaviour-preserving wrapper over today's code without first
//     building the permutation cache. These setters are documented as a
//     transition affordance: DawnDevice will satisfy them by *selecting the
//     matching pipeline variant* rather than by mutating global state. Once the
//     ENC-493 permutation cache lands, the setters can be folded into the
//     pipeline-selection key and removed.
//
//  4. Buffer lifecycle mirrors GpuBufferManager's streaming write-range model
//     (core/include/dc/gl/GpuBufferManager.hpp): create with a capacity,
//     writeBufferRange([offset,len)) for incremental tail-appends, and a single
//     full updateBuffer() for replace. This maps to glBufferData /
//     glBufferSubData on GL and to wgpuQueueWriteBuffer on WebGPU.
//
// This is interface + design only (ENC-481). No implementation here; Renderer
// still uses raw GL and is untouched. ENC-482 introduces GlDevice and adopts it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace dc {

// ---------------------------------------------------------------------------
// Opaque resource handles
// ---------------------------------------------------------------------------
// Each handle is a trivially-copyable value type wrapping a device-local id.
// `id == 0` is reserved to mean "null / not created". The concrete device maps
// the id to its own object (a GLuint, a WGPUBuffer, a slot index, etc.). The
// caller never inspects the id beyond comparing against 0.

/// A GPU vertex/index/uniform buffer. GL: a VBO/IBO/UBO name.
/// WebGPU: a WGPUBuffer.
struct BufferHandle {
  std::uint32_t id{0};
  bool valid() const { return id != 0; }
};

/// A 2D texture (e.g. the SDF glyph atlas, or a user image for texturedQuad@1).
/// GL: a texture name bound to GL_TEXTURE_2D. WebGPU: a WGPUTexture (+ view).
struct TextureHandle {
  std::uint32_t id{0};
  bool valid() const { return id != 0; }
};

/// An immutable render pipeline (program + fixed-function state baked in).
/// GL: emulated as (linked program + cached blend/stencil/topology to apply at
/// bind time). WebGPU: a WGPURenderPipeline. Created once, never mutated.
struct PipelineHandle {
  std::uint32_t id{0};
  bool valid() const { return id != 0; }
};

/// A bind group: the concrete set of resources (buffers/textures/samplers/
/// uniforms) bound for a draw, matching a pipeline's binding layout.
/// GL: emulated as a record of attrib pointers + uniform values + texture
/// units, replayed at draw time. WebGPU: a WGPUBindGroup.
struct BindGroupHandle {
  std::uint32_t id{0};
  bool valid() const { return id != 0; }
};

/// A render target (the swapchain backbuffer, or an offscreen FBO such as the
/// D29.3 pick buffer). GL: framebuffer 0 or an FBO name. WebGPU: a set of
/// WGPUTextureView color/stencil attachments wrapped by a render pass.
struct RenderTargetHandle {
  std::uint32_t id{0};  // 0 == default/backbuffer target
  bool valid() const { return id != 0; }
};

// ---------------------------------------------------------------------------
// Enums — fixed-function state, expressed API-neutrally
// ---------------------------------------------------------------------------

/// Per-DrawItem blend mode (mirrors dc::BlendMode in scene/Types.hpp; kept as a
/// separate device-level enum so this header stays free of scene includes).
///
/// GL mapping (see Renderer::applyBlendMode):
///   Normal   -> glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
///   Additive -> glBlendFunc(SRC_ALPHA, ONE)
///   Multiply -> glBlendFuncSeparate(DST_COLOR, ZERO, ONE, ONE_MINUS_SRC_ALPHA)
///   Screen   -> glBlendFuncSeparate(ONE, ONE_MINUS_SRC_COLOR, ONE, ONE_MINUS_SRC_ALPHA)
/// WebGPU mapping: a wgpu::BlendState (color+alpha BlendComponent) baked into
/// the pipeline. This enum therefore becomes part of the pipeline permutation
/// key (ENC-493), not a runtime setter, on the Dawn backend.
enum class DeviceBlendMode : std::uint8_t {
  Normal   = 0,
  Additive = 1,
  Multiply = 2,
  Screen   = 3,
};

/// Stencil-based clip state for the two-pass clip mask (D29.2). The Renderer
/// dispatcher sets exactly one of these per DrawItem:
///
///   None       — stencil test disabled, all color channels writable.
///                GL: glDisable(GL_STENCIL_TEST); glColorMask(1,1,1,1)
///   WriteMask  — the clip *source* pass: write 1 into the stencil buffer
///                everywhere this geometry covers, but write NO color.
///                GL: glEnable(STENCIL_TEST);
///                    glStencilFunc(GL_ALWAYS, 1, 0xFF);
///                    glStencilOp(KEEP, KEEP, REPLACE);
///                    glColorMask(0,0,0,0)
///                (color mask is restored to 1,1,1,1 after the source draw)
///   UseMask    — clipped content: only draw where stencil == 1, write color.
///                GL: glEnable(STENCIL_TEST);
///                    glStencilFunc(GL_EQUAL, 1, 0xFF);
///                    glStencilOp(KEEP, KEEP, KEEP);
///                    glColorMask(1,1,1,1)
///
/// WebGPU mapping: stencil compare/op live in wgpu::DepthStencilState (baked
/// into the pipeline), and the per-draw reference value is set via
/// renderPass.SetStencilReference(1). The color-write mask is also a pipeline
/// property (wgpu::ColorTargetState.writeMask). So ClipMode, like blend mode,
/// becomes part of the pipeline permutation key on Dawn (ENC-493); WriteMask
/// and UseMask are two pipeline variants of the same shader.
enum class ClipMode : std::uint8_t {
  None      = 0,
  WriteMask = 1,  // clip source: stencil write, color masked off
  UseMask   = 2,  // clipped draw: stencil test == ref, color on
};

/// Primitive topology for a draw. GL: the `mode` argument to glDraw*
/// (GL_TRIANGLES / GL_LINES / GL_POINTS). WebGPU: wgpu::PrimitiveTopology,
/// baked into the pipeline (so this is a pipeline-descriptor field, not a draw
/// argument, on Dawn).
enum class PrimitiveTopology : std::uint8_t {
  Triangles = 0,
  Lines     = 1,
  Points    = 2,
};

/// Index element type for indexed draws. GL: GL_UNSIGNED_INT / GL_UNSIGNED_SHORT.
/// WebGPU: wgpu::IndexFormat::Uint32 / Uint16. DynaCharting uses u32 indices
/// throughout (see GpuBufferManager / Geometry indexCount).
enum class IndexFormat : std::uint8_t {
  Uint32 = 0,
  Uint16 = 1,
};

/// Scalar component type of a vertex attribute. GL: the `type` arg to
/// glVertexAttribPointer (only GL_FLOAT is used today). WebGPU: encoded in
/// wgpu::VertexFormat (e.g. Float32x2).
enum class VertexComponentType : std::uint8_t {
  Float32 = 0,
};

/// Texture pixel format for create/upload. GL: R8 (glyph atlas, single-channel
/// coverage) and RGBA8 (user images / pick buffer). WebGPU: wgpu::TextureFormat
/// R8Unorm / RGBA8Unorm.
enum class TextureFormat : std::uint8_t {
  R8    = 0,  // single-channel; glyph atlas (GL_R8 / GL_RED upload)
  RGBA8 = 1,  // 4-channel; user textures, pick target
};

/// Texture sampling filter. GL: GL_LINEAR / GL_NEAREST on MIN/MAG.
/// WebGPU: wgpu::FilterMode in a wgpu::Sampler.
enum class TextureFilter : std::uint8_t {
  Linear  = 0,
  Nearest = 1,
};

// ---------------------------------------------------------------------------
// Descriptor structs — value-typed, plain data, no API objects
// ---------------------------------------------------------------------------

/// One vertex attribute within a vertex buffer layout. Mirrors a single
/// glVertexAttribPointer + glVertexAttribDivisor pairing.
///   GL:  glVertexAttribPointer(location, componentCount, type, false, stride,
///                              (void*)offset);
///        glVertexAttribDivisor(location, instanced ? 1 : 0);
///   WebGPU: a wgpu::VertexAttribute (+ stepMode on the wgpu::VertexBufferLayout).
struct VertexAttribute {
  std::uint32_t location{0};       // shader attribute slot (a_pos, a_rect, ...)
  std::uint32_t componentCount{2}; // 2/3/4 floats
  VertexComponentType type{VertexComponentType::Float32};
  std::uint32_t offsetBytes{0};    // byte offset within the per-vertex stride
};

/// Layout of one vertex buffer feeding a pipeline. `stepInstance == true` is the
/// instanced path (a_rect / a_c0 / a_g0 etc. advance once per instance).
///   GL: stride passed to each glVertexAttribPointer; divisor 1 when instanced.
///   WebGPU: a wgpu::VertexBufferLayout (arrayStride + stepMode + attributes).
struct VertexBufferLayout {
  std::uint32_t strideBytes{0};
  bool stepInstance{false};
  const VertexAttribute* attributes{nullptr};
  std::size_t attributeCount{0};
};

/// Descriptor for an immutable render pipeline. Everything that GL would set via
/// independent global calls is captured here so a WebGPU impl can bake one
/// immutable object. The shader sources are passed as null-terminated GLSL/WGSL
/// strings; the concrete device compiles/translates them (GlDevice compiles
/// GLSL directly; DawnDevice will translate via the shared shader spec, ENC-49x).
///
/// NOTE (ENC-493 permutation cache): `blend`, `clip` and `topology` are part of
/// this descriptor precisely because they are immutable pipeline state in
/// WebGPU. The future cache keys created pipelines on
/// (debugName | logical pipelineId, blend, clip, topology) and hands back a
/// PipelineHandle per permutation, so the dispatcher selects rather than mutates.
struct PipelineDesc {
  const char* debugName{nullptr};   // e.g. "triSolid@1"; for logging + cache key
  const char* vertexSource{nullptr};
  const char* fragmentSource{nullptr};

  // Vertex input layout (may be multiple buffers for split attributes, e.g.
  // candle a_c0/a_c1). Pointer to a caller-owned array valid for the call.
  const VertexBufferLayout* vertexBuffers{nullptr};
  std::size_t vertexBufferCount{0};

  PrimitiveTopology topology{PrimitiveTopology::Triangles};

  // Immutable fixed-function state (WebGPU bakes these into the pipeline; the GL
  // impl records them and re-applies on bindPipeline()).
  DeviceBlendMode blend{DeviceBlendMode::Normal};
  ClipMode clip{ClipMode::None};

  // Size (bytes) of the group-0 uniform buffer this pipeline's shader declares.
  // Drives the bind-group layout's minBindingSize and the per-draw uniform
  // buffer allocation on the Dawn backend (the GL backend ignores it). The
  // device packs the bound UniformBindings into this buffer by name (see the
  // Dawn createBindGroup packing). Defaults to 0 == "the device's base uniform
  // layout" (three mat3 columns + a color vec4 = 64 bytes), which is what the
  // non-instanced pipelines (triSolid/triGradient/triAA) use. Instanced
  // pipelines that need extra fields (viewport size, corner radius, ...) set a
  // larger size — e.g. instancedRect@1 uses 80 (adds a vec2 viewport + f32
  // cornerRadius). Per-instance strides are unrelated to this (those live in the
  // vertex buffer layout); this is purely the uniform-block size.
  std::size_t uniformBytes{0};
};

/// A single uniform value to bind for a draw. Kept as a tagged union of the few
/// shapes the shaders use (mat3, vec4, vec2, float, int/sampler-unit). The
/// `data` pointer is borrowed and must outlive the bind-group creation /draw.
///   GL: ShaderProgram::setUniform* (glUniformMatrix3fv, glUniform4f, ...).
///   WebGPU: packed into a uniform buffer referenced by the bind group; sampler
///           bindings (Sampler2D) become texture+sampler bind-group entries.
struct UniformBinding {
  enum class Kind : std::uint8_t { Mat3, Vec4, Vec2, Float, Sampler2D } kind{Kind::Float};
  const char* name{nullptr};  // uniform name (GL) / binding label (WGPU)
  const float* data{nullptr}; // Mat3:9, Vec4:4, Vec2:2, Float:1 floats; unused for Sampler2D
  TextureHandle texture{};    // valid only when kind == Sampler2D
  std::uint32_t textureUnit{0};
};

/// Descriptor for a bind group: the resources a draw needs, matching the
/// pipeline's binding layout. Pointers are borrowed for the duration of the call.
///   GL: replayed as glBindBuffer + glVertexAttribPointer + setUniform* +
///       glActiveTexture/glBindTexture at draw time.
///   WebGPU: a wgpu::BindGroup (uniform buffer + texture views + samplers).
struct BindGroupDesc {
  PipelineHandle pipeline{};           // layout this group satisfies

  const BufferHandle* vertexBuffers{nullptr};
  std::size_t vertexBufferCount{0};

  BufferHandle indexBuffer{};          // optional; invalid() == non-indexed
  IndexFormat indexFormat{IndexFormat::Uint32};

  const UniformBinding* uniforms{nullptr};
  std::size_t uniformCount{0};
};

/// Parameters for a non-instanced draw. GL: glDrawArrays / glDrawElements.
/// WebGPU: renderPass.Draw / DrawIndexed.
struct DrawParams {
  std::uint32_t vertexCount{0};
  std::uint32_t indexCount{0};   // >0 => indexed draw (uses bind group's index buffer)
  std::uint32_t firstVertex{0};
};

/// Parameters for an instanced draw. GL: glDrawArraysInstanced (6 verts/quad,
/// 12 verts/candle) with `instanceCount` instances. WebGPU: renderPass.Draw
/// with the instanceCount argument.
struct DrawInstancedParams {
  std::uint32_t vertexCountPerInstance{0}; // 6 (quad/glyph/line) or 12 (candle)
  std::uint32_t instanceCount{0};
  std::uint32_t firstVertex{0};
};

/// Descriptor passed to beginRenderPass. Captures the per-pass setup the
/// Renderer does once at the top of render()/renderPick(): bind target, set the
/// viewport, optionally clear.
///   GL:  glBindFramebuffer(target); glViewport(0,0,w,h);
///        if(clear){ glClearColor(...); glClear(COLOR|STENCIL); }
///   WebGPU: the wgpu::RenderPassDescriptor's color/stencil attachments carry
///           loadOp=Clear + clearValue; the pass also sets the viewport.
struct RenderPassDesc {
  RenderTargetHandle target{};   // invalid()/id==0 => default backbuffer
  std::uint32_t viewportWidth{0};
  std::uint32_t viewportHeight{0};

  bool clear{true};
  float clearColor[4]{0.0f, 0.0f, 0.0f, 1.0f};
  bool clearStencil{true};
};

/// Integer scissor rectangle in framebuffer pixels (origin bottom-left, GL
/// convention — DawnDevice flips to top-left internally). GL: glScissor.
/// WebGPU: renderPass.SetScissorRect.
struct ScissorRect {
  std::int32_t x{0};
  std::int32_t y{0};
  std::int32_t width{0};
  std::int32_t height{0};
};

/// Descriptor for creating/uploading a 2D texture.
///   GL: glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, UBYTE, data)
///       + glTexParameteri filters/wrap. WebGPU: wgpu::Texture + queueWriteTexture.
struct TextureDesc {
  std::uint32_t width{0};
  std::uint32_t height{0};
  TextureFormat format{TextureFormat::RGBA8};
  TextureFilter filter{TextureFilter::Linear};
  const std::uint8_t* data{nullptr};  // tightly packed pixels; may be null (alloc only)
};

// ---------------------------------------------------------------------------
// GpuDevice — the abstraction
// ---------------------------------------------------------------------------

/// Returned by each draw so the dispatcher can accumulate frame stats (mirrors
/// dc::Stats.drawCalls). Kept device-neutral here.
struct DeviceDrawStats {
  std::uint32_t drawCalls{0};
  std::uint32_t verticesSubmitted{0};
};

/// Device-agnostic GPU interface. Concrete impls: GlDevice (ENC-482),
/// DawnDevice (ENC-49x). All methods are documented with the GL semantics they
/// abstract and the WebGPU shape they map to.
///
/// Threading: single-threaded, called from the render thread, same contract as
/// the current Renderer. Lifetimes: created handles are owned by the device and
/// freed by the matching destroy*() (or device teardown).
class GpuDevice {
public:
  virtual ~GpuDevice() = default;

  // --- lifecycle ----------------------------------------------------------

  /// Bring the device up against an already-current context. GL: assumes a
  /// current GL context (GLAD loaded), creates the shared VAO, enables
  /// program-point-size, etc. WebGPU: acquires the wgpu::Device/Queue and
  /// configures the surface. Returns false on failure.
  virtual bool init() = 0;

  // --- buffer resources ---------------------------------------------------
  // Mirrors GpuBufferManager's streaming model (write-range coalescing).

  /// Create a GPU buffer of `capacityBytes`, optionally seeded with `initData`
  /// (may be null). GL: glGenBuffers + glBufferData(capacity, initData,
  /// STREAM_DRAW). WebGPU: device.CreateBuffer({size, usage:Vertex|Index|
  /// CopyDst}) + optional queueWriteBuffer.
  virtual BufferHandle createBuffer(std::size_t capacityBytes,
                                    const void* initData,
                                    std::size_t initBytes) = 0;

  /// Full-buffer replace. If `bytes` exceeds the current capacity the device
  /// reallocates. GL: glBufferData (realloc) or full glBufferSubData.
  /// WebGPU: recreate-if-grown + queueWriteBuffer(0, data, bytes).
  virtual void updateBuffer(BufferHandle buf,
                            const void* data, std::size_t bytes) = 0;

  /// Incremental write of `bytes` at `offsetBytes` — the live-tick tail-append
  /// path. Matches GpuBufferManager::writeRange's [offset, offset+bytes) dirty
  /// semantics. GL: glBufferSubData(offset, bytes, data). WebGPU:
  /// queueWriteBuffer(offset, data, bytes). Caller guarantees the range fits
  /// within the buffer's capacity.
  virtual void writeBufferRange(BufferHandle buf, std::size_t offsetBytes,
                                const void* data, std::size_t bytes) = 0;

  /// Destroy a buffer. GL: glDeleteBuffers. WebGPU: buffer.Destroy().
  virtual void destroyBuffer(BufferHandle buf) = 0;

  // --- texture resources --------------------------------------------------

  /// Create (and optionally upload) a 2D texture per `desc`. GL: glGenTextures +
  /// glTexImage2D + filter/wrap params. WebGPU: CreateTexture + queueWriteTexture
  /// + a default sampler. Used for the SDF glyph atlas (R8) and texturedQuad@1
  /// user images / the pick RGBA8 attachment.
  virtual TextureHandle createTexture(const TextureDesc& desc) = 0;

  /// Re-upload pixels into an existing texture (e.g. the glyph atlas when dirty,
  /// see Renderer::uploadAtlasIfDirty). GL: glBindTexture + glTexImage2D.
  /// WebGPU: queueWriteTexture. Dimensions/format must match the creation desc.
  virtual void updateTexture(TextureHandle tex,
                             const std::uint8_t* data) = 0;

  /// Destroy a texture. GL: glDeleteTextures. WebGPU: texture.Destroy().
  virtual void destroyTexture(TextureHandle tex) = 0;

  // --- pipelines & bind groups -------------------------------------------

  /// Create an immutable render pipeline from `desc` (shaders + vertex layout +
  /// topology + blend + clip state). GL: build a ShaderProgram and cache the
  /// fixed-function state to re-apply at bindPipeline(). WebGPU:
  /// device.CreateRenderPipeline (truly immutable). This is the object the
  /// ENC-493 permutation cache will mint one-per-(pipelineId,blend,clip).
  virtual PipelineHandle createPipeline(const PipelineDesc& desc) = 0;

  /// Destroy a pipeline. GL: glDeleteProgram. WebGPU: handle release.
  virtual void destroyPipeline(PipelineHandle pipe) = 0;

  /// Create a bind group (the resource set for a draw) against a pipeline's
  /// layout. GL: record the buffers/uniforms/textures to replay at draw time.
  /// WebGPU: device.CreateBindGroup. Bind groups are cheap and may be created
  /// per-frame; long-lived ones (e.g. the atlas) can be cached by the caller.
  virtual BindGroupHandle createBindGroup(const BindGroupDesc& desc) = 0;

  /// Destroy a bind group. GL: no-op (just drops the recorded state).
  /// WebGPU: handle release.
  virtual void destroyBindGroup(BindGroupHandle group) = 0;

  // --- render pass --------------------------------------------------------

  /// Begin a render pass against `desc.target`, set the viewport, and apply the
  /// clear (if requested). Exactly one pass may be active at a time. GL:
  /// glBindFramebuffer + glViewport + glClear*. WebGPU: encoder.BeginRenderPass
  /// with the descriptor's load/clear ops; the pass also sets the viewport.
  virtual void beginRenderPass(const RenderPassDesc& desc) = 0;

  /// End the active render pass. GL: a flush/unbind boundary (glFlush, unbind
  /// VAO/FBO). WebGPU: pass.End() then queue.Submit(encoder.Finish()).
  virtual void endRenderPass() = 0;

  /// Set the viewport within the active pass. GL: glViewport(0,0,w,h).
  /// WebGPU: renderPass.SetViewport(0,0,w,h,0,1). Usually redundant with the
  /// pass desc but exposed for completeness.
  virtual void setViewport(std::uint32_t width, std::uint32_t height) = 0;

  /// Set the scissor rect within the active pass (per-pane clipping, D9.2).
  /// GL: glScissor (with GL_SCISSOR_TEST kept enabled across the pass).
  /// WebGPU: renderPass.SetScissorRect.
  virtual void setScissorRect(const ScissorRect& rect) = 0;

  // --- per-draw mutable state (TRANSITION affordance; see file header note 3) -
  // These exist so the GlDevice (ENC-482) can wrap today's mutable-global code
  // 1:1. On DawnDevice they are implemented by *selecting the matching immutable
  // pipeline variant*, NOT by mutating global state — and the ENC-493
  // permutation cache will ultimately fold them into the pipeline key so these
  // setters can be removed.

  /// Set the active blend mode. GL: applyBlendMode (glBlendFunc*).
  /// WebGPU/ENC-493: choose the pipeline variant whose baked blend == `mode`.
  virtual void setBlendMode(DeviceBlendMode mode) = 0;

  /// Set the active clip/stencil state for the two-pass clip mask (D29.2).
  /// GL: glEnable/Disable(STENCIL_TEST) + glStencilFunc/Op + glColorMask per the
  /// ClipMode doc above. WebGPU/ENC-493: choose the matching pipeline variant
  /// and SetStencilReference(1); WriteMask/UseMask are pipeline variants.
  virtual void setClipState(ClipMode mode) = 0;

  // --- draw ---------------------------------------------------------------

  /// Bind an immutable pipeline as the active one for subsequent draws. GL:
  /// program.use() + re-apply the pipeline's cached blend/stencil/topology.
  /// WebGPU: renderPass.SetPipeline. Must be called before draw/drawInstanced.
  virtual void bindPipeline(PipelineHandle pipe) = 0;

  /// Non-instanced draw using `group`'s vertex/index buffers + uniforms.
  /// GL: bind VBO/IBO + attrib pointers + uniforms, then glDrawArrays /
  /// glDrawElements (topology from the bound pipeline). WebGPU:
  /// renderPass.SetBindGroup + SetVertexBuffer + (SetIndexBuffer) + Draw/
  /// DrawIndexed. Returns the draw-call/vertex stats to accumulate.
  virtual DeviceDrawStats draw(BindGroupHandle group,
                               const DrawParams& params) = 0;

  /// Instanced draw (bars / candles / glyphs / AA lines). GL:
  /// glVertexAttribDivisor(...,1) + glDrawArraysInstanced(topology, 0,
  /// vertsPerInstance, instanceCount). WebGPU: renderPass.Draw(vertsPerInstance,
  /// instanceCount). Returns the draw-call/vertex stats to accumulate.
  virtual DeviceDrawStats drawInstanced(BindGroupHandle group,
                                        const DrawInstancedParams& params) = 0;

  // --- readback -----------------------------------------------------------

  /// Read a 1x1 RGBA8 pixel at (x, y) from the active/last target — the D29.3
  /// GPU-pick readback. GL: glReadPixels(x,y,1,1,RGBA,UNSIGNED_BYTE,out).
  /// WebGPU: copyTextureToBuffer + mapAsync (async on real WebGPU; DawnDevice
  /// can block via the native instance). `outRgba` must point to >= 4 bytes.
  virtual void readPixel(std::int32_t x, std::int32_t y,
                         std::uint8_t* outRgba) = 0;
};

}  // namespace dc
