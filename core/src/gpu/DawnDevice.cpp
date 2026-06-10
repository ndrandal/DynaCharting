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
  textures_.clear();  // ENC-491
  buffers_.clear();
  colorView_ = nullptr;
  colorTexture_ = nullptr;
  stencilView_ = nullptr;     // ENC-494
  stencilTexture_ = nullptr;  // ENC-494
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

// --- texture resources (ENC-491) -------------------------------------------
// The Dawn half of TextureManager (core/src/gl/TextureManager.cpp). GL loads an
// rgba image into a GL_TEXTURE_2D (RGBA8, LINEAR, CLAMP_TO_EDGE) and binds it to
// a texture unit at draw time; here we create a wgpu::Texture (RGBA8Unorm or
// R8Unorm, TextureBinding|CopyDst), upload via queue.WriteTexture, and build a
// wgpu::Sampler. The texture's view + sampler are referenced by the bind group
// (createBindGroup) for any pipeline declaring sampledTexture (texturedQuad@1;
// ENC-492 SDF glyph atlas, which uses R8 single-channel coverage). This is kept
// GENERAL over the channel count: TextureDesc::format selects RGBA8/R8 and the
// per-pixel byte count drives WriteTexture's bytesPerRow.

DawnDevice::TextureEntry* DawnDevice::textureAt(TextureHandle h) {
  if (!h.valid() || h.id > textures_.size()) return nullptr;
  return &textures_[h.id - 1];
}

namespace {
// Map the device-neutral TextureFormat to a wgpu format + per-pixel byte count.
// RGBA8 -> RGBA8Unorm (4 bytes); R8 -> R8Unorm (1 byte, the SDF atlas case).
void mapTextureFormat(TextureFormat fmt, wgpu::TextureFormat* outWgpu,
                      std::uint32_t* outBpp) {
  switch (fmt) {
    case TextureFormat::R8:
      *outWgpu = wgpu::TextureFormat::R8Unorm;
      *outBpp = 1;
      break;
    case TextureFormat::RGBA8:
    default:
      *outWgpu = wgpu::TextureFormat::RGBA8Unorm;
      *outBpp = 4;
      break;
  }
}
}  // namespace

TextureHandle DawnDevice::createTexture(const TextureDesc& desc) {
  if (desc.width == 0 || desc.height == 0) return {};

  wgpu::TextureFormat wgpuFmt;
  std::uint32_t bpp;
  mapTextureFormat(desc.format, &wgpuFmt, &bpp);

  // RGBA8Unorm / R8Unorm sampled texture. TextureBinding so a bind group can
  // reference it for sampling; CopyDst so queue.WriteTexture can upload pixels.
  wgpu::TextureDescriptor td = {};
  td.label = "dc_texture";
  td.dimension = wgpu::TextureDimension::e2D;
  td.size = {desc.width, desc.height, 1};
  td.format = wgpuFmt;
  td.mipLevelCount = 1;
  td.sampleCount = 1;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  wgpu::Texture texture = device_.CreateTexture(&td);

  wgpu::TextureView view = texture.CreateView();

  // Sampler: filter from the desc (LINEAR/NEAREST on min+mag); ClampToEdge wrap
  // to match GL's GL_CLAMP_TO_EDGE in TextureManager::load.
  const wgpu::FilterMode filter = (desc.filter == TextureFilter::Nearest)
                                      ? wgpu::FilterMode::Nearest
                                      : wgpu::FilterMode::Linear;
  wgpu::SamplerDescriptor sd = {};
  sd.label = "dc_sampler";
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = filter;
  sd.minFilter = filter;
  sd.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
  wgpu::Sampler sampler = device_.CreateSampler(&sd);

  TextureEntry entry;
  entry.texture = std::move(texture);
  entry.view = std::move(view);
  entry.sampler = std::move(sampler);
  entry.width = desc.width;
  entry.height = desc.height;
  entry.bytesPerPixel = bpp;
  entry.format = wgpuFmt;
  textures_.push_back(std::move(entry));

  TextureHandle h;
  h.id = static_cast<std::uint32_t>(textures_.size());  // 1-based

  // Optional initial upload (mirrors GL's glTexImage2D with data).
  if (desc.data) updateTexture(h, desc.data);
  return h;
}

void DawnDevice::updateTexture(TextureHandle tex, const std::uint8_t* data) {
  TextureEntry* e = textureAt(tex);
  if (!e || !e->texture || !data) return;
  // queue.WriteTexture: the GL glTexImage2D re-upload equivalent. The source is
  // tightly packed (bytesPerRow == width * bytesPerPixel) — WriteTexture (unlike
  // a buffer copy) has NO 256-byte row-pitch constraint, so a tight upload is
  // fine for both RGBA8 (4B/px) and R8 (1B/px, the SDF atlas).
  wgpu::TexelCopyTextureInfo dst = {};
  dst.texture = e->texture;
  dst.mipLevel = 0;
  dst.origin = {0, 0, 0};
  dst.aspect = wgpu::TextureAspect::All;

  wgpu::TexelCopyBufferLayout layout = {};
  layout.offset = 0;
  layout.bytesPerRow = e->width * e->bytesPerPixel;
  layout.rowsPerImage = e->height;

  wgpu::Extent3D writeSize = {e->width, e->height, 1};
  const std::size_t byteCount =
      static_cast<std::size_t>(e->width) * e->height * e->bytesPerPixel;
  queue_.WriteTexture(&dst, data, byteCount, &layout, &writeSize);
}

void DawnDevice::destroyTexture(TextureHandle tex) {
  TextureEntry* e = textureAt(tex);
  if (!e) return;
  if (e->texture) {
    e->texture.Destroy();
    e->texture = nullptr;
  }
  e->view = nullptr;
  e->sampler = nullptr;
  e->width = 0;
  e->height = 0;
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

// ENC-493 (D29.1) — translate a DeviceBlendMode to the wgpu::BlendState baked
// into a pipeline variant. These are the exact WebGPU equivalents of the GL
// blend funcs in Renderer/GlDevice::setBlendMode:
//
//   Normal   glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
//            color & alpha: src*SrcAlpha + dst*(1-SrcAlpha)  — standard src-over.
//            BYTE-IDENTICAL to the ENC-488 default BlendState, so the existing
//            10 Dawn tests' Normal-blended output is unchanged.
//   Additive glBlendFunc(SRC_ALPHA, ONE)
//            color & alpha: src*SrcAlpha + dst*1            — brightens toward sum.
//   Multiply glBlendFuncSeparate(DST_COLOR, ZERO, ONE, ONE_MINUS_SRC_ALPHA)
//            color: src*Dst + dst*0 = src*dst              — darkens toward product.
//            alpha: src*1   + dst*(1-SrcAlpha)             — standard src-over alpha.
//   Screen   glBlendFuncSeparate(ONE, ONE_MINUS_SRC_COLOR, ONE, ONE_MINUS_SRC_ALPHA)
//            color: src*1 + dst*(1-Src) = src + dst - src*dst — lightens.
//            alpha: src*1 + dst*(1-SrcAlpha)               — standard src-over alpha.
//
// All ops are Add (GL_FUNC_ADD, GL's implicit default). Multiply/Screen need
// SEPARATE color vs alpha BlendComponents (the GL funcs use glBlendFuncSeparate);
// Normal/Additive use the same factors for color and alpha.
wgpu::BlendState blendStateFor(DeviceBlendMode mode) {
  wgpu::BlendState bs = {};
  bs.color.operation = wgpu::BlendOperation::Add;
  bs.alpha.operation = wgpu::BlendOperation::Add;
  switch (mode) {
    case DeviceBlendMode::Normal:
      bs.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
      bs.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      bs.alpha.srcFactor = wgpu::BlendFactor::SrcAlpha;
      bs.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      break;
    case DeviceBlendMode::Additive:
      bs.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
      bs.color.dstFactor = wgpu::BlendFactor::One;
      bs.alpha.srcFactor = wgpu::BlendFactor::SrcAlpha;
      bs.alpha.dstFactor = wgpu::BlendFactor::One;
      break;
    case DeviceBlendMode::Multiply:
      bs.color.srcFactor = wgpu::BlendFactor::Dst;   // GL_DST_COLOR
      bs.color.dstFactor = wgpu::BlendFactor::Zero;  // GL_ZERO
      bs.alpha.srcFactor = wgpu::BlendFactor::One;   // GL_ONE
      bs.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      break;
    case DeviceBlendMode::Screen:
      bs.color.srcFactor = wgpu::BlendFactor::One;            // GL_ONE
      bs.color.dstFactor = wgpu::BlendFactor::OneMinusSrc;    // GL_ONE_MINUS_SRC_COLOR
      bs.alpha.srcFactor = wgpu::BlendFactor::One;            // GL_ONE
      bs.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      break;
  }
  return bs;
}

// Index a PipelineEntry::variants[][] slot by blend mode (Normal=0 .. Screen=3).
constexpr std::size_t blendIndex(DeviceBlendMode mode) {
  return static_cast<std::size_t>(mode);
}

// ENC-494 (D29.2) — index a PipelineEntry::variants[][] slot by clip mode
// (None=0, WriteMask=1, UseMask=2). Mirrors the ClipMode enum values.
constexpr std::size_t clipIndex(ClipMode mode) {
  return static_cast<std::size_t>(mode);
}

// ENC-494 (D29.2) — the Stencil8 format used for the offscreen depth-stencil
// target and the per-ClipMode DepthStencilState below. We only need a stencil
// (no depth: the 2D painter's-order draw has no depth test), so Stencil8 is the
// minimal attachment. DepthStencilState requires `depthCompare` be set to a
// valid value even when depth is unused; Always + depthWriteEnabled=false makes
// depth a no-op.
constexpr wgpu::TextureFormat kStencilFormat = wgpu::TextureFormat::Stencil8;

// ENC-494 (D29.2) — translate a ClipMode to the wgpu::DepthStencilState baked
// into a pipeline variant. These are the exact WebGPU equivalents of GL's
// two-pass stencil clip in GlDevice::setClipState:
//
//   None      glDisable(GL_STENCIL_TEST); glColorMask(1,1,1,1)
//             -> stencil disabled (compare Always, ops Keep — a no-op pass),
//                color writes on. (writeMask handled by the caller as All.)
//   WriteMask glStencilFunc(GL_ALWAYS,1,0xFF); glStencilOp(KEEP,KEEP,REPLACE);
//             glColorMask(0,0,0,0)
//             -> compare Always, passOp Replace (writes the SetStencilReference
//                value, 1, where geometry covers), color writes OFF (the caller
//                sets colorTarget.writeMask = None for this clip). The clip
//                SOURCE pass: paint the mask into the stencil, no color.
//   UseMask   glStencilFunc(GL_EQUAL,1,0xFF); glStencilOp(KEEP,KEEP,KEEP);
//             glColorMask(1,1,1,1)
//             -> compare Equal (against SetStencilReference == 1), all ops Keep
//                (don't disturb the mask), color writes on. The CLIPPED pass:
//                content draws only where stencil == 1.
//
// `stencilReadMask`/`stencilWriteMask` are 0xFF (GL's 0xFF mask argument).
// `depthCompare = Always` + `depthWriteEnabled = false` make the (unused) depth
// aspect a no-op; Stencil8 has no depth aspect, but the field must be valid.
wgpu::DepthStencilState depthStencilStateFor(ClipMode mode) {
  wgpu::DepthStencilState ds = {};
  ds.format = kStencilFormat;
  ds.depthWriteEnabled = false;
  ds.depthCompare = wgpu::CompareFunction::Always;
  ds.stencilReadMask = 0xFF;
  ds.stencilWriteMask = 0xFF;

  wgpu::StencilFaceState face = {};
  switch (mode) {
    case ClipMode::None:
      // Stencil effectively disabled: Always pass, Keep everything.
      face.compare = wgpu::CompareFunction::Always;
      face.failOp = wgpu::StencilOperation::Keep;
      face.depthFailOp = wgpu::StencilOperation::Keep;
      face.passOp = wgpu::StencilOperation::Keep;
      ds.stencilWriteMask = 0;  // never write stencil in None mode
      break;
    case ClipMode::WriteMask:
      // Clip source: write ref (1) everywhere geometry covers.
      face.compare = wgpu::CompareFunction::Always;
      face.failOp = wgpu::StencilOperation::Keep;
      face.depthFailOp = wgpu::StencilOperation::Keep;
      face.passOp = wgpu::StencilOperation::Replace;  // GL_REPLACE
      break;
    case ClipMode::UseMask:
      // Clipped draw: pass only where stencil == ref (1); keep the mask.
      face.compare = wgpu::CompareFunction::Equal;
      face.failOp = wgpu::StencilOperation::Keep;
      face.depthFailOp = wgpu::StencilOperation::Keep;
      face.passOp = wgpu::StencilOperation::Keep;
      break;
  }
  // Single-sided clip geometry; apply the same state to front + back faces (GL's
  // glStencilFunc/glStencilOp without glStencilOpSeparate is two-sided too).
  ds.stencilFront = face;
  ds.stencilBack = face;
  return ds;
}
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
  // ENC-491: a sampledTexture pipeline (texturedQuad@1; future SDF text) ALSO
  // declares binding 1 = a sampled 2D float texture and binding 2 = a filtering
  // sampler, both fragment-visible (the WGSL samples in fs_main). createBindGroup
  // supplies the bound texture's view+sampler for those bindings.
  std::vector<wgpu::BindGroupLayoutEntry> bglEntries;
  {
    wgpu::BindGroupLayoutEntry uniformEntry = {};
    uniformEntry.binding = 0;
    uniformEntry.visibility =
        wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    uniformEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniformEntry.buffer.minBindingSize = uniformSize;
    bglEntries.push_back(uniformEntry);
  }
  if (desc.sampledTexture) {
    wgpu::BindGroupLayoutEntry texEntry = {};
    texEntry.binding = 1;
    texEntry.visibility = wgpu::ShaderStage::Fragment;
    texEntry.texture.sampleType = wgpu::TextureSampleType::Float;
    texEntry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    texEntry.texture.multisampled = false;
    bglEntries.push_back(texEntry);

    wgpu::BindGroupLayoutEntry sampEntry = {};
    sampEntry.binding = 2;
    sampEntry.visibility = wgpu::ShaderStage::Fragment;
    sampEntry.sampler.type = wgpu::SamplerBindingType::Filtering;
    bglEntries.push_back(sampEntry);
  }

  wgpu::BindGroupLayoutDescriptor bglDesc = {};
  bglDesc.entryCount = static_cast<std::uint32_t>(bglEntries.size());
  bglDesc.entries = bglEntries.data();
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

  // ENC-493: stash the immutable build inputs on the PipelineEntry so blend
  // variants can be built lazily later without recompiling the WGSL or
  // re-deriving the layout. The wgpu::VertexBufferLayout structs point into
  // attrStorage (caller-owned VertexAttribute arrays are NOT valid after this
  // call), so the entry OWNS its own copy of both the attribute arrays and the
  // layouts that reference them; we rebuild the layout->attribute pointers after
  // the move so they point at the entry-owned storage.
  PipelineEntry entry;
  entry.bindGroupLayout = std::move(bgl);
  entry.uniformSize = uniformSize;
  entry.hasTexture = desc.sampledTexture;
  entry.module = std::move(module);
  entry.pipelineLayout = std::move(layout);
  entry.topology = topo;
  entry.vbAttrStorage = std::move(attrStorage);
  entry.vbLayouts = std::move(vbLayouts);
  for (std::size_t b = 0; b < entry.vbLayouts.size(); ++b) {
    entry.vbLayouts[b].attributes = entry.vbAttrStorage[b].data();
    entry.vbLayouts[b].attributeCount = entry.vbAttrStorage[b].size();
  }

  pipelines_.push_back(std::move(entry));
  PipelineHandle h;
  h.id = static_cast<std::uint32_t>(pipelines_.size());  // 1-based

  // Eagerly build the (Normal, None) variant (the base pipeline). This is the
  // byte-identical equivalent of the pre-ENC-493 single CreateRenderPipeline:
  // same module / layout / topology / vertex layouts, blendStateFor(Normal) ==
  // the old hardcoded SrcAlpha/OneMinusSrcAlpha BlendState, and ClipMode::None
  // attaches the stencil aspect but disables the stencil test (every fragment
  // passes, nothing is written) so output matches the no-stencil base. The other
  // (blend, clip) cells are built lazily by pipelineVariant() on first
  // bindPipeline request.
  pipelineVariant(pipelines_.back(), DeviceBlendMode::Normal, ClipMode::None);
  return h;
}

// ENC-493/494 — lazily build + cache one (blend, clip) variant of a base
// pipeline. ENC-493 keyed on blend alone; ENC-494 adds the clip axis: the same
// builder now also bakes a per-ClipMode wgpu::DepthStencilState and, for the
// WriteMask clip source, masks color writes off (colorTarget.writeMask = None).
wgpu::RenderPipeline& DawnDevice::pipelineVariant(PipelineEntry& pe,
                                                  DeviceBlendMode blend,
                                                  ClipMode clip) {
  wgpu::RenderPipeline& slot = pe.variants[blendIndex(blend)][clipIndex(clip)];
  if (slot) return slot;  // already built

  // Color target matches the offscreen RGBA8Unorm framebuffer. The BlendState is
  // the per-blend permutation; the color-write mask is the per-clip permutation:
  // the WriteMask clip SOURCE writes the stencil only (no color), matching GL's
  // glColorMask(0,0,0,0). Everything else (module / layout / topology / vertex
  // layouts) is shared across variants.
  wgpu::BlendState blendState = blendStateFor(blend);
  wgpu::ColorTargetState colorTarget = {};
  colorTarget.format = wgpu::TextureFormat::RGBA8Unorm;
  colorTarget.writeMask = (clip == ClipMode::WriteMask)
                              ? wgpu::ColorWriteMask::None
                              : wgpu::ColorWriteMask::All;
  colorTarget.blend = &blendState;

  wgpu::FragmentState fragment = {};
  fragment.module = pe.module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colorTarget;

  // ENC-494: per-ClipMode DepthStencilState. ALL variants carry the Stencil8
  // depth-stencil state (the render pass always attaches the stencil target, so
  // every pipeline drawing into the pass must declare a matching depth-stencil
  // format — including the None variants, which simply disable the test).
  wgpu::DepthStencilState depthStencil = depthStencilStateFor(clip);

  wgpu::RenderPipelineDescriptor rpd = {};
  rpd.label = "dc_pipeline";
  rpd.layout = pe.pipelineLayout;
  rpd.vertex.module = pe.module;
  rpd.vertex.entryPoint = "vs_main";
  rpd.vertex.bufferCount = static_cast<std::uint32_t>(pe.vbLayouts.size());
  rpd.vertex.buffers = pe.vbLayouts.empty() ? nullptr : pe.vbLayouts.data();
  rpd.primitive.topology = pe.topology;
  rpd.fragment = &fragment;
  rpd.depthStencil = &depthStencil;  // ENC-494: Stencil8, per-clip stencil state

  slot = device_.CreateRenderPipeline(&rpd);
  return slot;
}

void DawnDevice::destroyPipeline(PipelineHandle pipe) {
  PipelineEntry* e = pipelineAt(pipe);
  if (!e) return;
  // ENC-493/494: release every (blend, clip) variant cell.
  for (auto& row : e->variants)
    for (auto& v : row) v = nullptr;
  e->bindGroupLayout = nullptr;
  e->module = nullptr;
  e->pipelineLayout = nullptr;
  e->vbLayouts.clear();
  e->vbAttrStorage.clear();
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
  // a 64-byte block so the tail isn't even allocated. cornerRadius (rect),
  // wickHalf (candle), and the lineAA tail (lineWidth/aaWidth/fringeEdge/dashLen/
  // gapLen) share this flat 96-byte float block but never coexist in one
  // pipeline's WGSL struct, so overlapping slots are safe.
  constexpr std::size_t kMaxUniformFloats = 24;  // 96 bytes / 4
  float uniformData[kMaxUniformFloats] = {0};
  auto nameIs = [](const char* a, const char* b) {
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
  };
  // ENC-491: capture the Sampler2D binding's texture handle (texturedQuad@1's
  // u_texture / future SDF atlas). The texture's view+sampler are added to the
  // bind group below when the pipeline declared a texture binding.
  TextureHandle boundTexture{};
  for (std::size_t i = 0; i < desc.uniformCount; ++i) {
    const UniformBinding& u = desc.uniforms[i];
    if (u.kind == UniformBinding::Kind::Sampler2D) {
      boundTexture = u.texture;
      continue;  // Sampler2D carries no float `data`; handled as a texture entry.
    }
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
        if (nameIs(u.name, "u_colorDown")) {
          // ENC-489 (instancedCandle): second color vec4 at byte 64 (float 16).
          uniformData[16] = u.data[0];
          uniformData[17] = u.data[1];
          uniformData[18] = u.data[2];
          uniformData[19] = u.data[3];
        } else {
          // The primary color vec4 (u_color / u_colorUp) at byte 48 (float 12).
          uniformData[12] = u.data[0];
          uniformData[13] = u.data[1];
          uniformData[14] = u.data[2];
          uniformData[15] = u.data[3];
        }
        break;
      case UniformBinding::Kind::Vec2:
        // u_viewportSize at byte 64 (float index 16) — instancedRect.
        uniformData[16] = u.data[0];
        uniformData[17] = u.data[1];
        break;
      case UniformBinding::Kind::Float:
        // u_wickHalf (candle, float 20), u_cornerRadius (rect, float 18), and the
        // lineAA tail (lineWidth/aaWidth/fringeEdge/dashLen/gapLen, floats 19..23)
        // share this flat block but never coexist in one pipeline's WGSL struct,
        // so overlapping slots (e.g. wickHalf@20 vs aaWidth@20) are safe.
        if (nameIs(u.name, "u_wickHalf")) {
          // ENC-489 (instancedCandle): wick half-width (clip space), dedicated
          // field at byte 80 (float index 20) — survives bind-group packing
          // (not smuggled into a mat3 padding lane).
          uniformData[20] = u.data[0];
        } else if (nameIs(u.name, "u_cornerRadius")) {
          // u_cornerRadius at byte 72 (float index 18) — instancedRect.
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
        break;  // texture handle already captured above (boundTexture).
    }
  }

  wgpu::BufferDescriptor ubd = {};
  ubd.label = "dc_uniform";
  ubd.size = uniformSize;
  ubd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer uniformBuffer = device_.CreateBuffer(&ubd);
  queue_.WriteBuffer(uniformBuffer, 0, uniformData, uniformSize);

  std::vector<wgpu::BindGroupEntry> bgEntries;
  {
    wgpu::BindGroupEntry bge = {};
    bge.binding = 0;
    bge.buffer = uniformBuffer;
    bge.offset = 0;
    bge.size = uniformSize;
    bgEntries.push_back(bge);
  }
  // ENC-491: a sampledTexture pipeline binds the texture's view (binding 1) and
  // its sampler (binding 2). The layout was built with these entries in
  // createPipeline; supplying them here completes the texturedQuad bind group.
  if (pe->hasTexture) {
    TextureEntry* te = textureAt(boundTexture);
    if (!te || !te->view || !te->sampler) return {};  // texture not created
    wgpu::BindGroupEntry texE = {};
    texE.binding = 1;
    texE.textureView = te->view;
    bgEntries.push_back(texE);

    wgpu::BindGroupEntry sampE = {};
    sampE.binding = 2;
    sampE.sampler = te->sampler;
    bgEntries.push_back(sampE);
  }

  wgpu::BindGroupDescriptor bgd = {};
  bgd.layout = pe->bindGroupLayout;
  bgd.entryCount = static_cast<std::uint32_t>(bgEntries.size());
  bgd.entries = bgEntries.data();
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
  if (!pass_ || !pe) return;
  // ENC-493/494 (D29.1/D29.2): WebGPU bakes blend AND stencil state into the
  // immutable pipeline, so honoring the per-DrawItem blend mode + clip state
  // means selecting the matching pipeline VARIANT at bind time rather than
  // mutating global state. currentBlendMode_/currentClipMode_ are whatever the
  // dispatcher last requested via setBlendMode()/setClipState() (Normal/None by
  // default if the backend never calls them). pipelineVariant() returns the
  // cached (blend, clip) variant, building it lazily on first use.
  wgpu::RenderPipeline& variant =
      pipelineVariant(*pe, currentBlendMode_, currentClipMode_);
  if (variant) pass_.SetPipeline(variant);
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

  // ENC-494 (D29.2): co-sized Stencil8 depth-stencil target for the two-pass
  // stencil clip mask. RenderAttachment only (it's never copied/read back — the
  // pixel readback reads the color target). Recreated with the color target on
  // resize. Every pipeline drawn into the pass declares this same Stencil8
  // format in its DepthStencilState (see pipelineVariant); the pass clears it
  // per-pane on beginRenderPass (matching GL's GL_STENCIL_BUFFER_BIT clear).
  wgpu::TextureDescriptor sd = {};
  sd.label = "dc_offscreen_stencil";
  sd.dimension = wgpu::TextureDimension::e2D;
  sd.size = {w, h, 1};
  sd.format = kStencilFormat;
  sd.mipLevelCount = 1;
  sd.sampleCount = 1;
  sd.usage = wgpu::TextureUsage::RenderAttachment;
  stencilTexture_ = device_.CreateTexture(&sd);
  stencilView_ = stencilTexture_.CreateView();

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

  // ENC-494 (D29.2): the stencil attachment for the two-pass clip mask. The
  // stencil is cleared per-pass when desc.clearStencil is set (matching GL's
  // per-pane GL_STENCIL_BUFFER_BIT clear in GlDevice::beginRenderPass: a pass
  // that clears color also clears stencil to 0), otherwise loaded. clearValue 0
  // means "no clip mask written yet"; the WriteMask source pass replaces it with
  // 1 where clip geometry covers, and the UseMask pass tests == 1. We have no
  // depth aspect (Stencil8), so depthLoadOp/depthStoreOp are Undefined and the
  // depth clear value is irrelevant. storeOp = Store so the mask written by the
  // source pass survives for the clipped pass within the same render pass.
  wgpu::RenderPassDepthStencilAttachment stencil = {};
  stencil.view = stencilView_;
  const bool clearStencil = desc.clear && desc.clearStencil;
  stencil.stencilLoadOp = clearStencil ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
  stencil.stencilStoreOp = wgpu::StoreOp::Store;
  stencil.stencilClearValue = 0;
  // Stencil8 has no depth aspect: leave depth load/store Undefined.

  wgpu::RenderPassDescriptor rp = {};
  rp.colorAttachmentCount = 1;
  rp.colorAttachments = &color;
  rp.depthStencilAttachment = &stencil;

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

void DawnDevice::setBlendMode(DeviceBlendMode mode) {
  // ENC-493 (D29.1): on WebGPU this does NOT mutate global blend state (there is
  // none — blend is baked into the immutable pipeline). It records the mode the
  // next bindPipeline() should select the variant for. The dispatcher calls this
  // before binding/drawing each DrawItem (Renderer sets it from di.blendMode);
  // bindPipeline() then resolves (basePipeline, currentBlendMode_) -> the cached
  // wgpu::RenderPipeline variant. Defaults to Normal, so a backend that never
  // calls setBlendMode keeps the byte-identical base pipeline.
  currentBlendMode_ = mode;
}
void DawnDevice::setClipState(ClipMode mode) {
  // ENC-494 (D29.2): on WebGPU this does NOT mutate global stencil state (there
  // is none — the stencil compare/op + color-write mask are baked into the
  // immutable pipeline). It records the mode the next bindPipeline() should
  // select the (blend, clip) variant for. The dispatcher calls this before
  // binding/drawing each DrawItem (WriteMask for the clip source, UseMask for
  // the clipped content, None otherwise), mirroring GL's two-pass
  // glStencilFunc/glStencilOp/glColorMask. The per-draw stencil REFERENCE value
  // (GL's `ref` argument, 1) is a render-pass property, not a pipeline one, so
  // it's set on the active pass here: WriteMask writes 1, UseMask compares == 1.
  currentClipMode_ = mode;
  if (pass_) pass_.SetStencilReference(1);
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
