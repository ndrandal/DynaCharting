// ENC-480 (P0.2) — DawnDevice implementation. See DawnDevice.hpp for scope.
//
// Foundation only: device bring-up, an offscreen RGBA8Unorm target, a
// clear-via-render-pass, and a SYNCHRONOUS pixel readback. The draw path
// (pipelines / bind groups / buffers / textures / draws) is stubbed with
// TODO(ENC-484) / TODO(ENC-485) — those land when the triSolid pipeline does.
#include "dc/gpu/DawnDevice.hpp"

#include <cstring>
#include <vector>

namespace dc {
namespace {

// Usage for streaming vertex/index buffers. Vertex|Index so one slot model backs
// either; CopyDst for queue.WriteBuffer; CopySrc so a streaming test (and any
// future GPU->GPU copy) can read the buffer back via CopyBufferToBuffer.
constexpr wgpu::BufferUsage kStreamBufferUsage =
    wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Index |
    wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;

// Map a Dawn BackendType to a short human string for diagnostics/logging.
const char* backendTypeName(wgpu::BackendType b) {
  switch (b) {
    case wgpu::BackendType::Vulkan:   return "Vulkan";
    case wgpu::BackendType::Metal:    return "Metal";
    case wgpu::BackendType::D3D12:    return "D3D12";
    case wgpu::BackendType::D3D11:    return "D3D11";
    case wgpu::BackendType::OpenGL:   return "OpenGL";
    case wgpu::BackendType::OpenGLES: return "OpenGLES";
    case wgpu::BackendType::WebGPU:   return "WebGPU";
    case wgpu::BackendType::Null:     return "Null";
    default:                          return "Undefined";
  }
}

std::string toStdString(wgpu::StringView sv) {
  if (sv.data == nullptr || sv.length == 0) return {};
  // WGPU_STRLEN sentinel == data is null-terminated.
  if (sv.length == WGPU_STRLEN) return std::string(sv.data);
  return std::string(sv.data, sv.length);
}

}  // namespace

DawnDevice::~DawnDevice() {
  // wgpu handles are reference-counted; clearing them releases. The native
  // instance (instance_) is destroyed last, after all device objects, which is
  // exactly the destruction order of these members (reverse declaration order).
  pass_ = nullptr;
  encoder_ = nullptr;
  // Release ENC-484 draw-path resources before the device/instance.
  bindGroups_.clear();
  pipelines_.clear();
  buffers_.clear();
  colorView_ = nullptr;
  colorTexture_ = nullptr;
  queue_ = nullptr;
  device_ = nullptr;
}

// --- lifecycle -------------------------------------------------------------

bool DawnDevice::init() {
  if (inited_) return true;

  // Create the Dawn native instance. AllowProcessEvents-mode callbacks (used by
  // the synchronous readback) are pumped via instance.ProcessEvents(), so no
  // TimedWaitAny instance feature is required here.
  wgpu::InstanceDescriptor instanceDesc = {};
  instance_ = std::make_unique<dawn::native::Instance>(
      reinterpret_cast<const WGPUInstanceDescriptor*>(&instanceDesc));
  if (instance_->Get() == nullptr) {
    errorMessage_ = "DawnDevice: failed to create dawn::native::Instance";
    return false;
  }

  // Discover adapters. On this box the available backend is Vulkan (lavapipe =
  // software Vulkan via VK_ICD_FILENAMES). Prefer a Vulkan adapter; otherwise
  // take the first adapter found. A software adapter is fine for headless tests.
  std::vector<dawn::native::Adapter> adapters = instance_->EnumerateAdapters();
  if (adapters.empty()) {
    errorMessage_ =
        "DawnDevice: no WebGPU adapter found. On this headless box, force the "
        "lavapipe software Vulkan ICD via "
        "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json";
    return false;
  }

  dawn::native::Adapter* chosen = nullptr;
  for (auto& a : adapters) {
    wgpu::Adapter wa(a.Get());  // borrowed handle; for GetInfo only
    wgpu::AdapterInfo info = {};
    wa.GetInfo(&info);
    if (info.backendType == wgpu::BackendType::Vulkan) {
      chosen = &a;
      break;
    }
  }
  if (chosen == nullptr) chosen = &adapters.front();

  {
    wgpu::Adapter wa(chosen->Get());
    wgpu::AdapterInfo info = {};
    wa.GetInfo(&info);
    backendName_ = backendTypeName(info.backendType);
    adapterName_ = toStdString(info.device);
    if (adapterName_.empty()) adapterName_ = toStdString(info.description);
  }

  // Create the device from the chosen adapter (synchronous on dawn::native).
  WGPUDevice cDevice = chosen->CreateDevice();
  if (cDevice == nullptr) {
    errorMessage_ = "DawnDevice: Adapter::CreateDevice failed (backend=" +
                    backendName_ + ")";
    return false;
  }
  // Acquire takes ownership of the +1 ref returned by CreateDevice.
  device_ = wgpu::Device::Acquire(cDevice);
  queue_ = device_.GetQueue();

  inited_ = true;
  return true;
}

// --- handle <-> slot helpers -----------------------------------------------
// 1-based opaque handles index these vectors; id 0 == null. Matches GlDevice.

DawnDevice::BufferEntry* DawnDevice::bufferAt(BufferHandle h) {
  if (!h.valid() || h.id > buffers_.size()) return nullptr;
  return &buffers_[h.id - 1];
}
DawnDevice::PipelineEntry* DawnDevice::pipelineAt(PipelineHandle h) {
  if (!h.valid() || h.id > pipelines_.size()) return nullptr;
  return &pipelines_[h.id - 1];
}
DawnDevice::BindGroupEntry* DawnDevice::bindGroupAt(BindGroupHandle h) {
  if (!h.valid() || h.id > bindGroups_.size()) return nullptr;
  return &bindGroups_[h.id - 1];
}

// --- buffer resources (ENC-485: coalesced streaming upload) -----------------
// The Dawn half of the streaming buffer model. The CPU side (per-id bytes +
// dirty-range coalescing + UploadStats) lives in dc::CpuBufferStore (pure dc);
// it drives this device through GpuDevice::updateBuffer (full/grown buffer) and
// writeBufferRange (one call per coalesced dirty range). See CpuBufferStore and
// the d85_dawn_range_upload test.
//
// UPLOAD STRATEGY: queue.WriteBuffer (Dawn's queued staging copy) for both the
// full and the per-range path. WriteBuffer internally stages into an upload
// ring and schedules a copy into the destination buffer, which is exactly the
// "writeBuffer + staging" the ticket calls for; it needs no manual MapWrite
// buffer, no extra command encoder, and no realloc of a staging buffer. The
// only hard WebGPU constraint is 4-byte alignment of both the destination
// offset and the written size — our vertex (8B pos2) and u32-index formats are
// already 4-aligned; a non-aligned tail is rounded up to the next multiple of 4
// (the CPU store over-allocates capacity to a 4-byte multiple, so the read does
// not run past the buffer).
//
// A buffer is created with usage Vertex|Index|CopyDst (both Vertex and Index so
// the same slot model can back either; CopyDst for WriteBuffer).

BufferHandle DawnDevice::createBuffer(std::size_t capacityBytes,
                                      const void* initData,
                                      std::size_t initBytes) {
  // WebGPU requires buffer sizes be a multiple of 4. Round up the capacity.
  const std::size_t size = (capacityBytes + 3u) & ~std::size_t{3u};

  wgpu::BufferDescriptor bd = {};
  bd.label = "dc_vertex_or_index";
  bd.size = size;
  bd.usage = kStreamBufferUsage;
  wgpu::Buffer buffer = device_.CreateBuffer(&bd);

  if (initData && initBytes > 0) {
    // WriteBuffer requires the written size be a multiple of 4. The caller's
    // vertex data (pos2 = 8B/vertex, u32 indices = 4B) is already 4-aligned.
    const std::size_t writeBytes = (initBytes + 3u) & ~std::size_t{3u};
    queue_.WriteBuffer(buffer, 0, initData,
                       writeBytes <= size ? writeBytes : size);
  }

  buffers_.push_back(BufferEntry{std::move(buffer), size, /*isIndex=*/false});
  BufferHandle h;
  h.id = static_cast<std::uint32_t>(buffers_.size());  // 1-based
  return h;
}

void DawnDevice::updateBuffer(BufferHandle buf, const void* data,
                              std::size_t bytes) {
  BufferEntry* e = bufferAt(buf);
  if (!e || !data || bytes == 0) return;
  // ENC-485 streaming: full (re)upload. If the data outgrows the current
  // capacity, reallocate the Dawn buffer (a new CreateBuffer; the old handle is
  // released when overwritten) — mirrors GL's glBufferData realloc-on-grow.
  const std::size_t writeBytes = (bytes + 3u) & ~std::size_t{3u};
  if (writeBytes > e->capacity) {
    wgpu::BufferDescriptor bd = {};
    bd.label = "dc_vertex_or_index";
    bd.size = writeBytes;
    bd.usage = kStreamBufferUsage;
    e->buffer = device_.CreateBuffer(&bd);
    e->capacity = writeBytes;
  }
  queue_.WriteBuffer(e->buffer, 0, data, writeBytes);
}

void DawnDevice::writeBufferRange(BufferHandle buf, std::size_t offsetBytes,
                                  const void* data, std::size_t bytes) {
  // Partial (coalesced-range) streaming upload. offset+size must be 4-aligned;
  // round the written size up to the next multiple of 4 (the CPU store keeps a
  // 4-aligned capacity so the read stays in-bounds), and require the destination
  // offset be 4-aligned (it is for our vertex/index formats).
  BufferEntry* e = bufferAt(buf);
  if (!e || !data || bytes == 0) return;
  if ((offsetBytes & 0x3u) != 0) return;  // unaligned offset: unsupported
  const std::size_t writeBytes = (bytes + 3u) & ~std::size_t{3u};
  if (offsetBytes + writeBytes > e->capacity) return;
  queue_.WriteBuffer(e->buffer, offsetBytes, data, writeBytes);
}

void DawnDevice::destroyBuffer(BufferHandle buf) {
  BufferEntry* e = bufferAt(buf);
  if (!e) return;
  if (e->buffer) {
    e->buffer.Destroy();
    e->buffer = nullptr;
  }
  e->capacity = 0;
}

bool DawnDevice::readBuffer(BufferHandle buf, std::size_t offsetBytes,
                            std::size_t bytes, std::uint8_t* out) {
  BufferEntry* e = bufferAt(buf);
  if (!e || !e->buffer || bytes == 0 || !out) return false;
  // CopyBufferToBuffer requires 4-byte-aligned offset + size; round the copy up
  // (capacity is a 4-byte multiple) and clamp to capacity.
  const std::size_t copyOff = offsetBytes & ~std::size_t{3u};
  const std::size_t copyEnd =
      (offsetBytes + bytes + 3u) & ~std::size_t{3u};
  if (copyEnd > e->capacity) return false;
  const std::size_t copySize = copyEnd - copyOff;

  wgpu::BufferDescriptor bd = {};
  bd.label = "dc_buffer_readback";
  bd.size = copySize;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer readback = device_.CreateBuffer(&bd);

  wgpu::CommandEncoder enc = device_.CreateCommandEncoder();
  enc.CopyBufferToBuffer(e->buffer, copyOff, readback, 0, copySize);
  wgpu::CommandBuffer cmd = enc.Finish();
  queue_.Submit(1, &cmd);

  struct MapState {
    bool done = false;
    bool ok = false;
  } mapState;
  readback.MapAsync(
      wgpu::MapMode::Read, 0, copySize, wgpu::CallbackMode::AllowProcessEvents,
      [](wgpu::MapAsyncStatus status, wgpu::StringView, MapState* st) {
        st->ok = (status == wgpu::MapAsyncStatus::Success);
        st->done = true;
      },
      &mapState);
  waitUntil(&mapState.done);
  if (!mapState.ok) return false;

  const auto* base =
      static_cast<const std::uint8_t*>(readback.GetConstMappedRange(0, copySize));
  if (!base) return false;
  std::memcpy(out, base + (offsetBytes - copyOff), bytes);
  readback.Unmap();
  return true;
}

// --- texture resources (TODO(ENC-485)) -------------------------------------

TextureHandle DawnDevice::createTexture(const TextureDesc& /*desc*/) {
  return {};  // TODO(ENC-485)
}
void DawnDevice::updateTexture(TextureHandle, const std::uint8_t*) {
  // TODO(ENC-485)
}
void DawnDevice::destroyTexture(TextureHandle) {
  // TODO(ENC-485)
}

// --- pipelines & bind groups (ENC-484: triSolid render pipeline) -----------
// triSolid's PipelineDesc carries a single WGSL module string (vertexSource;
// fragmentSource is unused for WGSL — the one module holds both vs_main and
// fs_main; see DawnTriSolidBackend) and one Float32x2 vertex buffer layout.
//
// Group 0, binding 0 is a uniform buffer holding the packed transform+color.
// To avoid WGSL's mat3x3<f32> column padding (each column is 16-byte aligned,
// so a host-side 9-float mat3 doesn't map 1:1), the uniform is laid out as four
// vec4<f32>: three transform columns (xyz used, w padding) + the RGBA color.
// That's a clean 64-byte struct the host fills with no implicit padding. The
// vertex shader reconstructs mat3x3(c0.xyz, c1.xyz, c2.xyz). See the shader in
// DawnTriSolidBackend for the matching declaration.

namespace {
// Base uniform-block size: three mat3 columns padded to vec4 + a color vec4.
// Pipelines that need more (instanced rect: + viewport vec2 + cornerRadius f32)
// declare a larger PipelineDesc::uniformBytes; see the name-driven packing in
// createBindGroup.
constexpr std::size_t kBaseUniformSize = 64;  // 4 * vec4<f32>
}  // namespace

PipelineHandle DawnDevice::createPipeline(const PipelineDesc& desc) {
  if (!desc.vertexSource) return {};

  // Per-pipeline uniform-block size (defaults to the 64-byte base layout).
  const std::size_t uniformSize =
      desc.uniformBytes > 0 ? desc.uniformBytes : kBaseUniformSize;

  // Compile the WGSL module (one module, two entry points: vs_main / fs_main).
  wgpu::ShaderSourceWGSL wgsl = {};
  wgsl.code = desc.vertexSource;
  wgpu::ShaderModuleDescriptor smd = {};
  smd.nextInChain = &wgsl;
  smd.label = desc.debugName ? desc.debugName : "dc_wgsl";
  wgpu::ShaderModule module = device_.CreateShaderModule(&smd);

  // Explicit bind-group layout: group 0 / binding 0 = uniform buffer, visible
  // to the vertex stage (transform) and fragment stage (color/cornerRadius).
  wgpu::BindGroupLayoutEntry bglEntry = {};
  bglEntry.binding = 0;
  bglEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  bglEntry.buffer.type = wgpu::BufferBindingType::Uniform;
  bglEntry.buffer.minBindingSize = uniformSize;

  wgpu::BindGroupLayoutDescriptor bglDesc = {};
  bglDesc.entryCount = 1;
  bglDesc.entries = &bglEntry;
  wgpu::BindGroupLayout bgl = device_.CreateBindGroupLayout(&bglDesc);

  wgpu::PipelineLayoutDescriptor plDesc = {};
  plDesc.bindGroupLayoutCount = 1;
  plDesc.bindGroupLayouts = &bgl;
  wgpu::PipelineLayout layout = device_.CreatePipelineLayout(&plDesc);

  // Vertex buffer layouts: build one wgpu::VertexBufferLayout per PipelineDesc
  // VertexBufferLayout. This is GENERAL — a pipeline may feed several buffers,
  // and any of them may be per-instance (stepInstance == true ->
  // VertexStepMode::Instance). The instanced pipelines (ENC-488 instancedRect,
  // and ENC-489/490/491 candle/lineAA/textured) drive the per-instance path
  // through here: one instance-step buffer carrying the per-instance attributes
  // (a_rect / a_c0 / ...). Per-instance strides must be 4-byte aligned for the
  // queue.WriteBuffer streaming upload (ENC-485) — our formats (rect4 = 16B,
  // candle6 = 24B, ...) already are.
  std::vector<wgpu::VertexBufferLayout> vbLayouts;
  // attrStorage holds the per-buffer attribute arrays; vbLayouts point into it,
  // so it must outlive the CreateRenderPipeline call below (kept on the stack).
  std::vector<std::vector<wgpu::VertexAttribute>> attrStorage;
  if (desc.vertexBufferCount > 0 && desc.vertexBuffers) {
    vbLayouts.reserve(desc.vertexBufferCount);
    attrStorage.reserve(desc.vertexBufferCount);
    for (std::size_t b = 0; b < desc.vertexBufferCount; ++b) {
      const VertexBufferLayout& src = desc.vertexBuffers[b];
      attrStorage.emplace_back();
      std::vector<wgpu::VertexAttribute>& attrs = attrStorage.back();
      attrs.reserve(src.attributeCount);
      for (std::size_t i = 0; i < src.attributeCount; ++i) {
        const VertexAttribute& a = src.attributes[i];
        wgpu::VertexAttribute wa = {};
        wa.shaderLocation = a.location;
        wa.offset = a.offsetBytes;
        // Only Float32 components are used today (pos2 = Float32x2, rect4 =
        // Float32x4).
        switch (a.componentCount) {
          case 1: wa.format = wgpu::VertexFormat::Float32; break;
          case 2: wa.format = wgpu::VertexFormat::Float32x2; break;
          case 3: wa.format = wgpu::VertexFormat::Float32x3; break;
          default: wa.format = wgpu::VertexFormat::Float32x4; break;
        }
        attrs.push_back(wa);
      }
      wgpu::VertexBufferLayout vbLayout = {};
      vbLayout.arrayStride = src.strideBytes;
      vbLayout.stepMode = src.stepInstance ? wgpu::VertexStepMode::Instance
                                           : wgpu::VertexStepMode::Vertex;
      vbLayout.attributeCount = attrs.size();
      vbLayout.attributes = attrs.data();
      vbLayouts.push_back(vbLayout);
    }
  }

  // Topology (triSolid = TriangleList).
  wgpu::PrimitiveTopology topo = wgpu::PrimitiveTopology::TriangleList;
  switch (desc.topology) {
    case PrimitiveTopology::Triangles: topo = wgpu::PrimitiveTopology::TriangleList; break;
    case PrimitiveTopology::Lines:     topo = wgpu::PrimitiveTopology::LineList;     break;
    case PrimitiveTopology::Points:    topo = wgpu::PrimitiveTopology::PointList;    break;
  }

  // Color target matches the offscreen RGBA8Unorm framebuffer.
  wgpu::ColorTargetState colorTarget = {};
  colorTarget.format = wgpu::TextureFormat::RGBA8Unorm;
  colorTarget.writeMask = wgpu::ColorWriteMask::All;

  // Default Normal alpha blending — matches GL's globally-enabled
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA). Required so alpha<1
  // fragments (AA fringes, rounded-rect corners per D28.2, SDF text) composite
  // against the target instead of overwriting it with their RGB. Opaque
  // geometry (alpha==1) is unaffected. The other per-draw-item blend modes
  // (Additive/Multiply/Screen) become pipeline permutations in ENC-493.
  wgpu::BlendState blend = {};
  blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.color.operation = wgpu::BlendOperation::Add;
  blend.alpha.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.alpha.operation = wgpu::BlendOperation::Add;
  colorTarget.blend = &blend;

  wgpu::FragmentState fragment = {};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colorTarget;

  wgpu::RenderPipelineDescriptor rpd = {};
  rpd.label = desc.debugName ? desc.debugName : "dc_pipeline";
  rpd.layout = layout;
  rpd.vertex.module = module;
  rpd.vertex.entryPoint = "vs_main";
  rpd.vertex.bufferCount = static_cast<std::uint32_t>(vbLayouts.size());
  rpd.vertex.buffers = vbLayouts.empty() ? nullptr : vbLayouts.data();
  rpd.primitive.topology = topo;
  rpd.fragment = &fragment;
  // Single-sampled, no depth/stencil (flat 2D, painter's order).

  wgpu::RenderPipeline pipeline = device_.CreateRenderPipeline(&rpd);

  pipelines_.push_back(PipelineEntry{std::move(pipeline), std::move(bgl),
                                     uniformSize});
  PipelineHandle h;
  h.id = static_cast<std::uint32_t>(pipelines_.size());  // 1-based
  return h;
}

void DawnDevice::destroyPipeline(PipelineHandle pipe) {
  PipelineEntry* e = pipelineAt(pipe);
  if (!e) return;
  e->pipeline = nullptr;
  e->bindGroupLayout = nullptr;
}

BindGroupHandle DawnDevice::createBindGroup(const BindGroupDesc& desc) {
  PipelineEntry* pe = pipelineAt(desc.pipeline);
  if (!pe) return {};

  // The uniform-block size is the one the pipeline's shader declared (64 for the
  // base mat3+color layout; 80 for instancedRect which adds viewport+radius).
  const std::size_t uniformSize =
      pe->uniformSize > 0 ? pe->uniformSize : kBaseUniformSize;

  // Pack the bound UniformBindings into a flat float layout BY NAME. The WGSL
  // uniform struct is a fixed, std140-friendly sequence the host fills with no
  // implicit padding:
  //   bytes  0..47  : c0/c1/c2  — three mat3 columns, each a vec4 (xyz used)
  //   bytes 48..63  : color     — vec4 (rgba)
  //   bytes 64..71  : viewport  — vec2 (px width/height)   [instanced rect/lineAA]
  //   bytes 72..75  : cornerRadius — f32                   [instanced rect]
  //   bytes 76..79  : lineWidth  — f32 (clip units)        [lineAA]   (ENC-490)
  //   bytes 80..83  : aaWidth    — f32 (clip units)        [lineAA]   (ENC-490)
  //   bytes 84..87  : fringeEdge — f32 (v_dist space)      [lineAA]   (ENC-490)
  //   bytes 88..91  : dashLen    — f32 (px)                [lineAA]   (ENC-490)
  //   bytes 92..95  : gapLen     — f32 (px)                [lineAA]   (ENC-490)
  // The host mat3 is column-major 9 floats (Transform.mat3 — col0 = {m0,m1,m2},
  // col1 = {m3,m4,m5}, col2 = {m6,m7,m8}). Non-instanced pipelines pass only
  // u_transform (+ optional color) and leave the tail zero; their pipelines use
  // a 64-byte block so the tail isn't even allocated. cornerRadius (rect, byte
  // 72) and the lineAA tail (bytes 76..95) share the same flat float block but
  // never coexist in one pipeline's WGSL struct, so the slots are disjoint.
  constexpr std::size_t kMaxUniformFloats = 24;  // 96 bytes / 4
  float uniformData[kMaxUniformFloats] = {0};
  auto nameIs = [](const char* a, const char* b) {
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
  };
  for (std::size_t i = 0; i < desc.uniformCount; ++i) {
    const UniformBinding& u = desc.uniforms[i];
    if (!u.name || !u.data) continue;
    switch (u.kind) {
      case UniformBinding::Kind::Mat3:
        // 3 columns -> three padded vec4s at offset 0.
        for (int c = 0; c < 3; ++c) {
          uniformData[c * 4 + 0] = u.data[c * 3 + 0];
          uniformData[c * 4 + 1] = u.data[c * 3 + 1];
          uniformData[c * 4 + 2] = u.data[c * 3 + 2];
          uniformData[c * 4 + 3] = 0.0f;
        }
        break;
      case UniformBinding::Kind::Vec4:
        // The color vec4 lives at byte 48 (float index 12).
        uniformData[12] = u.data[0];
        uniformData[13] = u.data[1];
        uniformData[14] = u.data[2];
        uniformData[15] = u.data[3];
        break;
      case UniformBinding::Kind::Vec2:
        // u_viewportSize at byte 64 (float index 16).
        uniformData[16] = u.data[0];
        uniformData[17] = u.data[1];
        break;
      case UniformBinding::Kind::Float:
        // u_cornerRadius at byte 72 (float index 18) [instancedRect].
        // ENC-490 lineAA float tail: lineWidth/aaWidth/fringeEdge/dashLen/gapLen
        // at float indices 19..23 (bytes 76..95). Disjoint from cornerRadius —
        // no pipeline declares both.
        if (nameIs(u.name, "u_cornerRadius")) {
          uniformData[18] = u.data[0];
        } else if (nameIs(u.name, "u_lineWidth")) {
          uniformData[19] = u.data[0];
        } else if (nameIs(u.name, "u_aaWidth")) {
          uniformData[20] = u.data[0];
        } else if (nameIs(u.name, "u_fringeEdge")) {
          uniformData[21] = u.data[0];
        } else if (nameIs(u.name, "u_dashLen")) {
          uniformData[22] = u.data[0];
        } else if (nameIs(u.name, "u_gapLen")) {
          uniformData[23] = u.data[0];
        }
        break;
      case UniformBinding::Kind::Sampler2D:
        break;  // texture/sampler bindings: TODO(ENC-491 textured)
    }
  }

  wgpu::BufferDescriptor ubd = {};
  ubd.label = "dc_uniform";
  ubd.size = uniformSize;
  ubd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer uniformBuffer = device_.CreateBuffer(&ubd);
  queue_.WriteBuffer(uniformBuffer, 0, uniformData, uniformSize);

  wgpu::BindGroupEntry bge = {};
  bge.binding = 0;
  bge.buffer = uniformBuffer;
  bge.offset = 0;
  bge.size = uniformSize;

  wgpu::BindGroupDescriptor bgd = {};
  bgd.layout = pe->bindGroupLayout;
  bgd.entryCount = 1;
  bgd.entries = &bge;
  wgpu::BindGroup bindGroup = device_.CreateBindGroup(&bgd);

  BindGroupEntry entry;
  entry.bindGroup = std::move(bindGroup);
  entry.uniformBuffer = std::move(uniformBuffer);
  if (desc.vertexBufferCount > 0 && desc.vertexBuffers) {
    entry.vertexBuffer = desc.vertexBuffers[0];
  }
  entry.indexBuffer = desc.indexBuffer;
  entry.indexFormat = (desc.indexFormat == IndexFormat::Uint16)
                          ? wgpu::IndexFormat::Uint16
                          : wgpu::IndexFormat::Uint32;

  bindGroups_.push_back(std::move(entry));
  BindGroupHandle h;
  h.id = static_cast<std::uint32_t>(bindGroups_.size());  // 1-based
  return h;
}

void DawnDevice::destroyBindGroup(BindGroupHandle group) {
  BindGroupEntry* e = bindGroupAt(group);
  if (!e) return;
  e->bindGroup = nullptr;
  e->uniformBuffer = nullptr;
}

void DawnDevice::bindPipeline(PipelineHandle pipe) {
  boundPipeline_ = pipe;
  PipelineEntry* pe = pipelineAt(pipe);
  if (pass_ && pe && pe->pipeline) {
    pass_.SetPipeline(pe->pipeline);
  }
}

DeviceDrawStats DawnDevice::draw(BindGroupHandle group,
                                 const DrawParams& params) {
  DeviceDrawStats stats{};
  if (!pass_) return stats;
  BindGroupEntry* bg = bindGroupAt(group);
  if (!bg) return stats;

  pass_.SetBindGroup(0, bg->bindGroup);

  BufferEntry* vb = bufferAt(bg->vertexBuffer);
  if (!vb || !vb->buffer) return stats;
  pass_.SetVertexBuffer(0, vb->buffer);

  if (params.indexCount > 0 && bg->indexBuffer.valid()) {
    BufferEntry* ib = bufferAt(bg->indexBuffer);
    if (!ib || !ib->buffer) return stats;
    pass_.SetIndexBuffer(ib->buffer, bg->indexFormat);
    pass_.DrawIndexed(params.indexCount, 1, 0, 0, 0);
    stats.drawCalls = 1;
    stats.verticesSubmitted = params.indexCount;
  } else {
    pass_.Draw(params.vertexCount, 1, params.firstVertex, 0);
    stats.drawCalls = 1;
    stats.verticesSubmitted = params.vertexCount;
  }
  return stats;
}

DeviceDrawStats DawnDevice::drawInstanced(BindGroupHandle group,
                                          const DrawInstancedParams& params) {
  // ENC-488 instanced draw foundation (reused by ENC-489/490/491). The bound
  // pipeline carries a per-instance step-mode vertex buffer layout (built in
  // createPipeline from VertexBufferLayout::stepInstance); here we bind the
  // per-instance buffer(s) and issue a single instanced draw. WebGPU's
  // RenderPassEncoder::Draw(vertexCount, instanceCount, firstVertex,
  // firstInstance) is the equivalent of GL's glDrawArraysInstanced(topology, 0,
  // vertsPerInstance, instanceCount) — the unit quad's 6 vertices are generated
  // from @builtin(vertex_index) in the WGSL (no per-vertex buffer needed), and
  // the per-instance attributes (a_rect, ...) advance once per instance.
  DeviceDrawStats stats{};
  if (!pass_) return stats;
  BindGroupEntry* bg = bindGroupAt(group);
  if (!bg) return stats;
  if (params.instanceCount == 0 || params.vertexCountPerInstance == 0) return stats;

  pass_.SetBindGroup(0, bg->bindGroup);

  // Bind the per-instance vertex buffer in slot 0 (the instance attributes). The
  // pipeline's slot-0 layout has VertexStepMode::Instance, so the buffer is read
  // once per instance. (Future split-attribute pipelines — candle a_c0/a_c1 —
  // will bind additional slots here; the layout supports multiple buffers.)
  BufferEntry* vb = bufferAt(bg->vertexBuffer);
  if (!vb || !vb->buffer) return stats;
  pass_.SetVertexBuffer(0, vb->buffer);

  // Draw vertsPerInstance vertices, instanceCount instances. firstVertex selects
  // the start vertex (0 for the unit quad); firstInstance is 0 — the gathered /
  // visible-subset selection is done CPU-side by the backend (D26 indexed gather
  // packs the selected instances into the bound buffer), so every bound instance
  // is drawn.
  pass_.Draw(params.vertexCountPerInstance, params.instanceCount,
             params.firstVertex, 0);
  stats.drawCalls = 1;
  stats.verticesSubmitted = params.vertexCountPerInstance * params.instanceCount;
  return stats;
}

// --- render target ---------------------------------------------------------

void DawnDevice::ensureRenderTarget(std::uint32_t w, std::uint32_t h) {
  if (colorTexture_ && targetW_ == w && targetH_ == h) return;

  wgpu::TextureDescriptor td = {};
  td.label = "dc_offscreen_color";
  td.dimension = wgpu::TextureDimension::e2D;
  td.size = {w, h, 1};
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.mipLevelCount = 1;
  td.sampleCount = 1;
  // RenderAttachment so we can clear/draw into it; CopySrc so readback can
  // copyTextureToBuffer out of it.
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;

  colorTexture_ = device_.CreateTexture(&td);
  colorView_ = colorTexture_.CreateView();
  targetW_ = w;
  targetH_ = h;
}

// --- render pass -----------------------------------------------------------

void DawnDevice::beginRenderPass(const RenderPassDesc& desc) {
  const std::uint32_t w = desc.viewportWidth;
  const std::uint32_t h = desc.viewportHeight;
  ensureRenderTarget(w, h);

  encoder_ = device_.CreateCommandEncoder();

  wgpu::RenderPassColorAttachment color = {};
  color.view = colorView_;
  color.loadOp = desc.clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
  color.storeOp = wgpu::StoreOp::Store;
  color.clearValue = wgpu::Color{desc.clearColor[0], desc.clearColor[1],
                                 desc.clearColor[2], desc.clearColor[3]};

  wgpu::RenderPassDescriptor rp = {};
  rp.colorAttachmentCount = 1;
  rp.colorAttachments = &color;

  pass_ = encoder_.BeginRenderPass(&rp);
  // The pass viewport defaults to the full attachment; set explicitly to match
  // the GpuDevice contract / GlDevice's glViewport(0,0,w,h).
  if (w > 0 && h > 0) {
    pass_.SetViewport(0.0f, 0.0f, static_cast<float>(w),
                      static_cast<float>(h), 0.0f, 1.0f);
  }
  // TODO(ENC-484): SetPipeline / SetBindGroup / Draw happen here on the draw
  // path. ENC-480 only clears (LoadOp::Clear above) and ends the pass.
}

void DawnDevice::endRenderPass() {
  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }
  if (encoder_) {
    wgpu::CommandBuffer cmd = encoder_.Finish();
    queue_.Submit(1, &cmd);
    encoder_ = nullptr;
  }
}

void DawnDevice::setViewport(std::uint32_t width, std::uint32_t height) {
  if (pass_ && width > 0 && height > 0) {
    pass_.SetViewport(0.0f, 0.0f, static_cast<float>(width),
                      static_cast<float>(height), 0.0f, 1.0f);
  }
}

void DawnDevice::setScissorRect(const ScissorRect& rect) {
  if (pass_ && rect.width > 0 && rect.height > 0) {
    // GpuDevice scissor is bottom-left origin (GL convention); WebGPU is
    // top-left. Flip y against the current target height.
    const std::int32_t flippedY =
        static_cast<std::int32_t>(targetH_) - rect.y - rect.height;
    pass_.SetScissorRect(static_cast<std::uint32_t>(rect.x),
                         static_cast<std::uint32_t>(flippedY < 0 ? 0 : flippedY),
                         static_cast<std::uint32_t>(rect.width),
                         static_cast<std::uint32_t>(rect.height));
  }
}

// --- per-draw mutable state (TODO(ENC-484/ENC-493)) ------------------------
// On Dawn these select an immutable pipeline variant rather than mutate global
// state (see GpuDevice.hpp note 3). Implemented with the draw path.

void DawnDevice::setBlendMode(DeviceBlendMode) {
  // TODO(ENC-484/ENC-493)
}
void DawnDevice::setClipState(ClipMode) {
  // TODO(ENC-484/ENC-493)
}

// --- synchronous readback --------------------------------------------------

void DawnDevice::waitUntil(const bool* done) {
  // Pump the native instance until *done flips. Tick the device too so its
  // internal queues advance (the copy must complete before the map callback
  // fires). AllowProcessEvents-mode futures are serviced by ProcessEvents.
  while (!*done) {
    device_.Tick();
    // dawn::native::Instance has no C++ ProcessEvents; use the C entry point on
    // its underlying WGPUInstance.
    wgpuInstanceProcessEvents(instance_->Get());
  }
}

void DawnDevice::readPixel(std::int32_t x, std::int32_t y,
                           std::uint8_t* outRgba) {
  outRgba[0] = outRgba[1] = outRgba[2] = outRgba[3] = 0;
  if (!colorTexture_) return;

  // WebGPU requires bytesPerRow to be a multiple of 256. We read the whole
  // target so the center pixel can be located after mapping; one padded row is
  // enough for a 1x1 read, but a full copy keeps this simple and reusable.
  const std::uint32_t kBytesPerPixel = 4;
  const std::uint32_t unpaddedRow = targetW_ * kBytesPerPixel;
  const std::uint32_t kAlign = 256;
  const std::uint32_t paddedRow = (unpaddedRow + (kAlign - 1)) & ~(kAlign - 1);
  const std::uint64_t bufSize =
      static_cast<std::uint64_t>(paddedRow) * targetH_;

  wgpu::BufferDescriptor bd = {};
  bd.label = "dc_readback";
  bd.size = bufSize;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer readback = device_.CreateBuffer(&bd);

  // Copy the offscreen texture into the readback buffer.
  wgpu::CommandEncoder enc = device_.CreateCommandEncoder();
  wgpu::TexelCopyTextureInfo src = {};
  src.texture = colorTexture_;
  src.mipLevel = 0;
  src.origin = {0, 0, 0};
  src.aspect = wgpu::TextureAspect::All;

  wgpu::TexelCopyBufferInfo dst = {};
  dst.buffer = readback;
  dst.layout.offset = 0;
  dst.layout.bytesPerRow = paddedRow;
  dst.layout.rowsPerImage = targetH_;

  wgpu::Extent3D copySize = {targetW_, targetH_, 1};
  enc.CopyTextureToBuffer(&src, &dst, &copySize);
  wgpu::CommandBuffer cmd = enc.Finish();
  queue_.Submit(1, &cmd);

  // Map the readback buffer SYNCHRONOUSLY: kick off the async map, then block
  // in waitUntil() pumping instance events until the callback sets `done`.
  // A non-capturing lambda + userdata pointer is the "repeatable callback" form
  // Dawn's webgpu_cpp.h wants (capturing lambdas are rejected at compile time).
  struct MapState {
    bool done = false;
    bool ok = false;
  } mapState;
  readback.MapAsync(
      wgpu::MapMode::Read, 0, bufSize, wgpu::CallbackMode::AllowProcessEvents,
      [](wgpu::MapAsyncStatus status, wgpu::StringView, MapState* st) {
        st->ok = (status == wgpu::MapAsyncStatus::Success);
        st->done = true;
      },
      &mapState);
  waitUntil(&mapState.done);
  const bool mapOk = mapState.ok;

  if (mapOk) {
    const auto* base =
        static_cast<const std::uint8_t*>(readback.GetConstMappedRange(0, bufSize));
    if (base) {
      const std::int32_t cx = (x < 0) ? 0 : x;
      const std::int32_t cy = (y < 0) ? 0 : y;
      if (static_cast<std::uint32_t>(cx) < targetW_ &&
          static_cast<std::uint32_t>(cy) < targetH_) {
        const std::uint8_t* px =
            base + static_cast<std::size_t>(cy) * paddedRow +
            static_cast<std::size_t>(cx) * kBytesPerPixel;
        std::memcpy(outRgba, px, 4);
      }
    }
    readback.Unmap();
  }
}

}  // namespace dc
