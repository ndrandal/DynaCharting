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

// --- buffer resources (TODO(ENC-485)) --------------------------------------
// The streaming vertex/index/uniform buffer model (mirroring GpuBufferManager)
// lands with the draw path. Stubbed here so device bring-up + clear/readback
// stay scoped to ENC-480.

BufferHandle DawnDevice::createBuffer(std::size_t /*capacityBytes*/,
                                      const void* /*initData*/,
                                      std::size_t /*initBytes*/) {
  return {};  // TODO(ENC-485)
}
void DawnDevice::updateBuffer(BufferHandle, const void*, std::size_t) {
  // TODO(ENC-485)
}
void DawnDevice::writeBufferRange(BufferHandle, std::size_t, const void*,
                                  std::size_t) {
  // TODO(ENC-485)
}
void DawnDevice::destroyBuffer(BufferHandle) {
  // TODO(ENC-485)
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

// --- pipelines & bind groups (TODO(ENC-484)) -------------------------------
// The triSolid pipeline + WGSL + bind groups are ENC-484; no shaders here.

PipelineHandle DawnDevice::createPipeline(const PipelineDesc&) {
  return {};  // TODO(ENC-484)
}
void DawnDevice::destroyPipeline(PipelineHandle) {
  // TODO(ENC-484)
}
BindGroupHandle DawnDevice::createBindGroup(const BindGroupDesc&) {
  return {};  // TODO(ENC-484)
}
void DawnDevice::destroyBindGroup(BindGroupHandle) {
  // TODO(ENC-484)
}
void DawnDevice::bindPipeline(PipelineHandle) {
  // TODO(ENC-484)
}
DeviceDrawStats DawnDevice::draw(BindGroupHandle, const DrawParams&) {
  return {};  // TODO(ENC-484)
}
DeviceDrawStats DawnDevice::drawInstanced(BindGroupHandle,
                                          const DrawInstancedParams&) {
  return {};  // TODO(ENC-484)
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
