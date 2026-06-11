// ENC-497 (P4.1) — DawnWindowContext implementation. See the header for the
// surface-creation / swapchain-blit design and the GL hello_glfw parallel.
//
// Gated on DC_DAWN_WINDOWED at the CMake level (the windowed build path that
// re-enables DAWN_USE_X11 and links the system GLFW); this translation unit is
// only compiled into dc_gpu in that configuration.
#include "dc/gpu/DawnWindowContext.hpp"

#include "dc/gpu/DawnSceneRenderer.hpp"

// Force the X11 native accessors. We set GLFW_PLATFORM=X11 at runtime, and Dawn's
// re-enabled surface support is Xlib (DAWN_USE_X11). Define the expose macro
// BEFORE including glfw3native.h so glfwGetX11Display/glfwGetX11Window are visible.
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace dc {
namespace {

// ---------------------------------------------------------------------------
// Minimal self-contained PNG encoder (RGBA8, top-down). No zlib/stb dependency:
// emits a zlib stream using DEFLATE "stored" (uncompressed) blocks, which every
// PNG reader accepts. Used only by the windowed demo's proof-of-render readback,
// so encode speed/size do not matter. Keeps the windowed path free of any new
// third-party dep (the repo has no PNG writer; demos use PPM).
// ---------------------------------------------------------------------------
std::uint32_t crc32_of(const std::uint8_t* data, std::size_t len,
                       std::uint32_t crc) {
  static std::uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (std::uint32_t n = 0; n < 256; ++n) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      table[n] = c;
    }
    init = true;
  }
  crc ^= 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; ++i)
    crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

void put32(std::vector<std::uint8_t>& v, std::uint32_t x) {
  v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xFF));
  v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
  v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
  v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}

void writeChunk(std::vector<std::uint8_t>& out, const char tag[4],
                const std::vector<std::uint8_t>& data) {
  put32(out, static_cast<std::uint32_t>(data.size()));
  std::size_t crcStart = out.size();
  out.insert(out.end(), tag, tag + 4);
  out.insert(out.end(), data.begin(), data.end());
  std::uint32_t crc = crc32_of(out.data() + crcStart, out.size() - crcStart, 0);
  put32(out, crc);
}

}  // namespace

bool DawnWindowContext::writeRgbaPng(const std::string& path, int w, int h,
                                     const std::vector<std::uint8_t>& rgba) {
  if (w <= 0 || h <= 0) return false;
  if (static_cast<std::size_t>(w) * h * 4u != rgba.size()) return false;

  // Raw scanlines: each row prefixed by a filter byte (0 == None).
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(h) * (1 + w * 4));
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);
    const std::uint8_t* row = rgba.data() + static_cast<std::size_t>(y) * w * 4;
    raw.insert(raw.end(), row, row + static_cast<std::size_t>(w) * 4);
  }

  // zlib stream: 2-byte header + DEFLATE stored blocks + Adler-32.
  std::vector<std::uint8_t> zlib;
  zlib.push_back(0x78);
  zlib.push_back(0x01);
  std::size_t pos = 0;
  while (pos < raw.size()) {
    std::size_t block = std::min<std::size_t>(65535, raw.size() - pos);
    bool last = (pos + block) >= raw.size();
    zlib.push_back(last ? 1 : 0);
    std::uint16_t len = static_cast<std::uint16_t>(block);
    std::uint16_t nlen = static_cast<std::uint16_t>(~len);
    zlib.push_back(static_cast<std::uint8_t>(len & 0xFF));
    zlib.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
    zlib.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
    zlib.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xFF));
    zlib.insert(zlib.end(), raw.begin() + pos, raw.begin() + pos + block);
    pos += block;
  }
  // Adler-32 over the raw (uncompressed) data.
  std::uint32_t a = 1, b = 0;
  for (std::uint8_t byte : raw) {
    a = (a + byte) % 65521;
    b = (b + a) % 65521;
  }
  put32(zlib, (b << 16) | a);

  std::vector<std::uint8_t> out;
  const std::uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  out.insert(out.end(), sig, sig + 8);

  std::vector<std::uint8_t> ihdr;
  put32(ihdr, static_cast<std::uint32_t>(w));
  put32(ihdr, static_cast<std::uint32_t>(h));
  ihdr.push_back(8);   // bit depth
  ihdr.push_back(6);   // color type RGBA
  ihdr.push_back(0);   // compression
  ihdr.push_back(0);   // filter
  ihdr.push_back(0);   // interlace
  writeChunk(out, "IHDR", ihdr);
  writeChunk(out, "IDAT", zlib);
  writeChunk(out, "IEND", {});

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::size_t n = std::fwrite(out.data(), 1, out.size(), f);
  std::fclose(f);
  return n == out.size();
}

namespace {

// Fullscreen-triangle blit: sample the offscreen scene color texture (binding 0)
// with a sampler (binding 1) and write it to the swapchain texture. Mirrors the
// DawnPostProcess fullscreen-triangle vertex trick.
const char* kBlitWgsl = R"WGSL(
@group(0) @binding(0) var src_tex : texture_2d<f32>;
@group(0) @binding(1) var src_smp : sampler;

struct VsOut {
  @builtin(position) pos : vec4<f32>,
  @location(0)       uv  : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32) -> VsOut {
  let x = -1.0 + f32((vid & 1u) << 2u);
  let y = -1.0 + f32((vid & 2u) << 1u);
  var o : VsOut;
  o.pos = vec4<f32>(x, y, 0.0, 1.0);
  // The offscreen scene target is stored top-down (row 0 == top) exactly like the
  // swapchain, so UV maps clip-space directly without a vertical flip:
  //   x in [-1,1] -> u in [0,1];  y in [-1,1] (clip, +y up) -> v in [0,1] (top=0).
  o.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
  return o;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> {
  return textureSample(src_tex, src_smp, uv);
}
)WGSL";

}  // namespace

DawnWindowContext::~DawnWindowContext() {
  blitPipeline_ = nullptr;
  blitBindGroupLayout_ = nullptr;
  blitSampler_ = nullptr;
  // Unconfigure the surface (release the swapchain) BEFORE dropping the surface
  // handle and the window, so the swapchain teardown runs while the X11 display +
  // the device are still fully alive. (Some Mesa/NVK X11 swapchain destroy paths
  // are fragile if torn down lazily during device release; doing it explicitly
  // here is the clean order.)
  if (surface_) surface_.Unconfigure();
  surface_ = nullptr;
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  if (glfwInited_) {
    glfwTerminate();
    glfwInited_ = false;
  }
}

bool DawnWindowContext::init(DawnDevice& device, int width, int height,
                             const char* title) {
  device_ = &device;
  width_ = width;
  height_ = height;

  glfwSetErrorCallback([](int code, const char* msg) {
    std::fprintf(stderr, "[GLFW error %d] %s\n", code, msg ? msg : "");
  });

  // Force the X11 backend (XWayland under a Wayland session) BEFORE glfwInit —
  // Dawn's re-enabled Vulkan surface support is Xlib, not Wayland.
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  if (!glfwInit()) {
    errorMessage_ =
        "DawnWindowContext: glfwInit() failed (could not open the X11 display). "
        "Ensure DISPLAY is set (e.g. DISPLAY=:0) and X11/XWayland is reachable.";
    return false;
  }
  glfwInited_ = true;

  if (glfwGetPlatform() != GLFW_PLATFORM_X11) {
    errorMessage_ =
        "DawnWindowContext: GLFW did not select the X11 platform (got platform "
        "code " + std::to_string(glfwGetPlatform()) +
        "). The Dawn surface path requires X11.";
    return false;
  }

  // WebGPU owns the context; GLFW must not create a GL context on the window.
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // keep the swapchain size fixed
  window_ = glfwCreateWindow(width_, height_, title ? title : "DynaCharting",
                             nullptr, nullptr);
  if (!window_) {
    errorMessage_ =
        "DawnWindowContext: glfwCreateWindow failed (no X11 window could be "
        "created on this display).";
    return false;
  }

  // Build the wgpu::Surface from the X11 display + window (mirrors Dawn's own
  // webgpu_glfw utils.cpp). The native WGPUInstance that owns the device also
  // owns the surface.
  WGPUInstance cInstance = device_->wgpuInstance();
  if (!cInstance) {
    errorMessage_ = "DawnWindowContext: device has no native instance (init()?).";
    return false;
  }
  // Wrap the borrowed WGPUInstance: bump its refcount via the C API and Acquire
  // that ref so the local `instance` owns its own reference (the C++ wrapper's
  // AddRef is private in this Dawn revision).
  wgpuInstanceAddRef(cInstance);
  wgpu::Instance instance = wgpu::Instance::Acquire(cInstance);

  wgpu::SurfaceSourceXlibWindow xlibDesc = {};
  xlibDesc.display = glfwGetX11Display();
  xlibDesc.window = static_cast<std::uint64_t>(glfwGetX11Window(window_));

  wgpu::SurfaceDescriptor surfaceDesc = {};
  surfaceDesc.nextInChain = &xlibDesc;
  surface_ = instance.CreateSurface(&surfaceDesc);
  if (!surface_) {
    errorMessage_ =
        "DawnWindowContext: instance.CreateSurface(Xlib) returned null.";
    return false;
  }

  // Query capabilities + Configure the swapchain.
  wgpu::SurfaceCapabilities caps = {};
  surface_.GetCapabilities(device_->wgpuAdapter(), &caps);
  if (caps.formatCount == 0) {
    errorMessage_ =
        "DawnWindowContext: surface reports no supported formats (the adapter "
        "cannot present to this X11 surface).";
    return false;
  }
  surfaceFormat_ = caps.formats[0];

  // Prefer Fifo (vsync) if offered, else the first present mode.
  wgpu::PresentMode present = (caps.presentModeCount > 0)
                                  ? caps.presentModes[0]
                                  : wgpu::PresentMode::Fifo;
  for (std::size_t i = 0; i < caps.presentModeCount; ++i) {
    if (caps.presentModes[i] == wgpu::PresentMode::Fifo) {
      present = wgpu::PresentMode::Fifo;
      break;
    }
  }

  wgpu::SurfaceConfiguration config = {};
  config.device = device_->wgpuDevice();
  config.format = surfaceFormat_;
  config.usage = wgpu::TextureUsage::RenderAttachment;
  config.width = static_cast<std::uint32_t>(width_);
  config.height = static_cast<std::uint32_t>(height_);
  config.presentMode = present;
  config.alphaMode = wgpu::CompositeAlphaMode::Auto;
  surface_.Configure(&config);

  if (!buildBlitPipeline()) return false;  // errorMessage_ set inside
  return true;
}

bool DawnWindowContext::buildBlitPipeline() {
  wgpu::Device dev = device_->wgpuDevice();

  wgpu::ShaderSourceWGSL wgsl = {};
  wgsl.code = kBlitWgsl;
  wgpu::ShaderModuleDescriptor smd = {};
  smd.nextInChain = &wgsl;
  smd.label = "dc_blit_module";
  wgpu::ShaderModule module = dev.CreateShaderModule(&smd);
  if (!module) {
    errorMessage_ = "DawnWindowContext: blit shader module creation failed.";
    return false;
  }

  // Bind group 0: sampled texture (binding 0) + filtering sampler (binding 1).
  wgpu::BindGroupLayoutEntry bgle[2] = {};
  bgle[0].binding = 0;
  bgle[0].visibility = wgpu::ShaderStage::Fragment;
  bgle[0].texture.sampleType = wgpu::TextureSampleType::Float;
  bgle[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  bgle[1].binding = 1;
  bgle[1].visibility = wgpu::ShaderStage::Fragment;
  bgle[1].sampler.type = wgpu::SamplerBindingType::Filtering;

  wgpu::BindGroupLayoutDescriptor bgld = {};
  bgld.entryCount = 2;
  bgld.entries = bgle;
  blitBindGroupLayout_ = dev.CreateBindGroupLayout(&bgld);

  wgpu::PipelineLayoutDescriptor pld = {};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &blitBindGroupLayout_;
  wgpu::PipelineLayout layout = dev.CreatePipelineLayout(&pld);

  wgpu::SamplerDescriptor sd = {};
  sd.label = "dc_blit_sampler";
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.addressModeW = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  sd.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
  blitSampler_ = dev.CreateSampler(&sd);

  // Color target == the CONFIGURED surface format (BGRA8Unorm on X11/Vulkan here).
  wgpu::ColorTargetState colorTarget = {};
  colorTarget.format = surfaceFormat_;
  colorTarget.writeMask = wgpu::ColorWriteMask::All;

  wgpu::FragmentState fragment = {};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colorTarget;

  wgpu::RenderPipelineDescriptor rpd = {};
  rpd.label = "dc_blit_pipeline";
  rpd.layout = layout;
  rpd.vertex.module = module;
  rpd.vertex.entryPoint = "vs_main";
  rpd.vertex.bufferCount = 0;
  rpd.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  rpd.fragment = &fragment;
  // No depth/stencil for the blit (the swapchain texture has color only).
  blitPipeline_ = dev.CreateRenderPipeline(&rpd);
  if (!blitPipeline_) {
    errorMessage_ = "DawnWindowContext: blit pipeline creation failed.";
    return false;
  }
  return true;
}

bool DawnWindowContext::shouldClose() const {
  return !window_ || glfwWindowShouldClose(window_);
}

bool DawnWindowContext::presentFrame(DawnSceneRenderer& renderer,
                                     const Scene& scene, CpuBufferStore& store) {
  if (!device_ || !surface_ || !blitPipeline_) return false;

  // 1) Render the whole scene into the device's offscreen target id 0 (unchanged
  //    headless path; this ends its render pass + submits internally).
  renderer.render(scene, store, width_, height_);

  // 2) Acquire the swapchain texture for this frame.
  wgpu::SurfaceTexture surfaceTexture = {};
  surface_.GetCurrentTexture(&surfaceTexture);
  if (!surfaceTexture.texture ||
      (surfaceTexture.status !=
           wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
       surfaceTexture.status !=
           wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)) {
    return false;  // window resized/lost; caller decides whether to reconfigure
  }

  // 3) Blit the offscreen scene color view onto the swapchain texture.
  wgpu::TextureView srcView = device_->colorViewForTarget(0);
  if (!srcView) return false;

  wgpu::Device dev = device_->wgpuDevice();
  wgpu::BindGroupEntry bge[2] = {};
  bge[0].binding = 0;
  bge[0].textureView = srcView;
  bge[1].binding = 1;
  bge[1].sampler = blitSampler_;
  wgpu::BindGroupDescriptor bgd = {};
  bgd.layout = blitBindGroupLayout_;
  bgd.entryCount = 2;
  bgd.entries = bge;
  wgpu::BindGroup bindGroup = dev.CreateBindGroup(&bgd);

  wgpu::TextureView dstView = surfaceTexture.texture.CreateView();

  wgpu::RenderPassColorAttachment color = {};
  color.view = dstView;
  color.loadOp = wgpu::LoadOp::Clear;
  color.storeOp = wgpu::StoreOp::Store;
  color.clearValue = wgpu::Color{0.0, 0.0, 0.0, 1.0};

  wgpu::RenderPassDescriptor rp = {};
  rp.colorAttachmentCount = 1;
  rp.colorAttachments = &color;

  wgpu::CommandEncoder enc = dev.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
  pass.SetPipeline(blitPipeline_);
  pass.SetBindGroup(0, bindGroup);
  pass.Draw(3, 1, 0, 0);  // fullscreen triangle
  pass.End();
  wgpu::CommandBuffer cmd = enc.Finish();
  device_->wgpuQueue().Submit(1, &cmd);

  // 4) Present + pump window events.
  surface_.Present();
  glfwPollEvents();
  return true;
}

bool DawnWindowContext::readSceneToPng(const std::string& path) {
  if (!device_) return false;
  std::uint32_t w = 0, h = 0;
  // First query the active target dimensions via a sizing call (out=nullptr makes
  // readFramebufferRGBA return false but still fills outW/outH).
  device_->readFramebufferRGBA(nullptr, 0, &w, &h);
  if (w == 0 || h == 0) return false;
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4u);
  if (!device_->readFramebufferRGBA(rgba.data(), rgba.size(), &w, &h)) return false;
  return writeRgbaPng(path, static_cast<int>(w), static_cast<int>(h), rgba);
}

}  // namespace dc
