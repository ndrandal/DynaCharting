// dc_engine_host.cpp — ENC-506 (P6.5) Emscripten/emdawnwebgpu binding that
// packages the WASM `dc` core + ingest + the FULL Dawn renderer behind an
// EngineHost-shaped surface, so @repo/dc-wasm can present the SAME public API as
// @repo/engine-host and customer-layer can swap to it (ENC-507) with near-zero
// change.
//
// This is the C++ half of @repo/dc-wasm. It exposes ONE stateful Embind class,
// DcEngineHost, that owns:
//   * a Scene + ResourceRegistry + CommandProcessor   (control plane: applyControl)
//   * an IngestProcessor                              (data plane: applyDataBatch)
//   * a CpuBufferStore                                (the render-side buffer bytes)
//   * a lazily-created DawnSceneRenderer              (render + pick)
//
// The TS EngineHost wrapper (packages/dc-wasm/src/EngineHost.ts) routes its
// public methods to this object:
//   init(canvas)            -> renderer brought up lazily on first render (the
//                              browser canvas is bound JS-side; see below)
//   applyControl(jsonOrObj) -> applyControl(jsonText)  (CommandProcessor)
//   applyDataBatch(buffer)  -> applyDataBatch(bytes)   (IngestProcessor)
//   render()                -> render(w,h) -> framebuffer readback (blit to canvas)
//   pick(x,y)               -> pick(w,h,x,y) -> drawItemId (0 == miss)
//   stats                   -> the per-frame counters below
//
// CANVAS BINDING (browser): emdawnwebgpu's DawnDevice renders into an OFFSCREEN
// RGBA8 target and reads it back (readFramebufferRGBA) — exactly the proven,
// pixel-validated path from ENC-504 (dc_webgpu_all). The caller-provided
// <canvas> is bound on the JS side: the TS EngineHost holds the HTMLCanvasElement
// from init(canvas), and after each render() it blits the framebuffer bytes onto
// that canvas's 2D context via putImageData (the same blit dc_webgpu_all.html
// does). This targets an EXTERNAL canvas (the one customer-layer mounts) without
// the WASM module creating its own — the canvas selector is owned by JS, not C++.
// (A native WebGPU surface-from-canvas swapchain is a later optimization, ENC-497-
// style; the offscreen+blit path is what is validated today and keeps the device
// code identical to the headless tests.)
//
// Gated on Emscripten like dc_wasm / dc_webgpu_all; never affects the native build.

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <string>
#include <vector>

#include <cstring>
#include <map>

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/render/GpuDevice.hpp"
#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/gpu/DawnTexturedQuadBackend.hpp"
#include "dc/debug/Stats.hpp"

namespace {

// ENC-532 (T3.6) — in-memory TextureSource for the WASM EngineHost. The
// texturedQuad@1 pipeline (heatmap/spectrogram/weather colormaps) needs CPU
// pixels for a logical textureId; the browser uploads them via
// DcEngineHost::setTexturePixels and they are cached here keyed by textureId.
// The DawnTexturedQuadBackend pulls them on first use (lazily) and uploads to a
// wgpu::Texture. Mirrors core/tests/d36_1_dawn_texquad.cpp's MemTextureSource.
class MemTextureSource final : public dc::TextureSource {
public:
  struct Tex {
    std::vector<std::uint8_t> pixels;
    std::uint32_t w{0};
    std::uint32_t h{0};
    dc::TextureFormat format{dc::TextureFormat::RGBA8};
  };

  void set(std::uint32_t id, std::vector<std::uint8_t> pixels, std::uint32_t w,
           std::uint32_t h, dc::TextureFormat fmt) {
    textures_[id] = Tex{std::move(pixels), w, h, fmt};
  }

  bool getTexturePixels(std::uint32_t id, const std::uint8_t** outData,
                        std::uint32_t* outW, std::uint32_t* outH,
                        dc::TextureFormat* outFmt) const override {
    auto it = textures_.find(id);
    if (it == textures_.end()) return false;
    *outData = it->second.pixels.data();
    *outW = it->second.w;
    *outH = it->second.h;
    *outFmt = it->second.format;
    return true;
  }

private:
  std::map<std::uint32_t, Tex> textures_;
};

// Result of applyControl — mirrors the TS EngineHost.applyControl return shape
// ({ ok } | { ok:false, error }). Marshalled to a plain JS object via Embind.
struct DcControlResult {
  bool ok{true};
  std::string error;
};

// Per-frame + cumulative stats — mirrors the TS EngineStats fields the dc-wasm
// EngineHost re-exposes (frameMs, drawCalls, ingestedBytesThisFrame, ...). The
// TS wrapper fills queued/dropped batch counters (those are a JS-queue concern).
struct DcEngineStats {
  double frameMs{0};
  int drawCalls{0};
  int culledDrawCalls{0};
  double ingestedBytesThisFrame{0};
  double uploadedBytesThisFrame{0};
  int activeBuffers{0};
};

// DcEngineHost — the WASM EngineHost. One per chart canvas (mirrors the TS
// EngineHost lifecycle: construct, init, drive, dispose).
class DcEngineHost {
public:
  DcEngineHost() : cp_(scene_, reg_) {
    // The command processor needs the ingest processor so buffer-creating
    // commands (createBuffer/byteLength) and inline buffer data land in ingest_
    // (the same wiring JsonHost does), keeping the render store in sync.
    cp_.setIngestProcessor(&ingest_);
  }

  ~DcEngineHost() { dispose(); }

  // ---- control plane: applyControl(json) -> CommandProcessor -------------
  // Apply one JSON command (string). Mirrors EngineHost.applyControl: returns
  // { ok } on success or { ok:false, error } on failure. The TS wrapper accepts
  // string | object and JSON.stringifies objects before calling this.
  DcControlResult applyControl(const std::string& jsonText) {
    dc::CmdResult r = cp_.applyJsonText(jsonText);
    DcControlResult out;
    out.ok = r.ok;
    if (!r.ok) {
      out.error = r.err.code.empty() ? r.err.message
                                     : (r.err.code + ": " + r.err.message);
    }
    return out;
  }

  // ---- data plane: applyDataBatch(bytes) -> IngestProcessor --------------
  // Ingest one binary DcCommand[] batch (the ENC-505 wire format). The batch
  // crosses the JS boundary as a Uint8Array (emscripten::val); we copy its bytes
  // verbatim (NOT via std::string, which would UTF-8-mangle them). Touched
  // buffers are mirrored into the render store so the next render() sees the
  // new bytes. Accumulates ingestedBytesThisFrame for stats (cleared each frame).
  void applyDataBatch(emscripten::val batch) {
    std::vector<std::uint8_t> bytes =
        emscripten::convertJSArrayToNumberVector<std::uint8_t>(batch);
    dc::IngestResult r = ingest_.processBatch(
        bytes.data(), static_cast<std::uint32_t>(bytes.size()));
    ingestedBytesAccum_ += r.payloadBytes;
    droppedBytesAccum_ += r.droppedBytes;
    // Mirror touched buffers into the CpuBufferStore (the render-side bytes),
    // exactly like JsonHost::syncTouchedBuffersToGpu.
    for (dc::Id id : r.touchedBufferIds) {
      const auto* data = ingest_.getBufferData(id);
      auto size = ingest_.getBufferSize(id);
      if (data && size > 0)
        store_.setCpuData(id, data, size);
      else
        store_.setCpuData(id, nullptr, 0);
    }
  }

  // ---- textures: setTexturePixels(...) -> TextureSource -----------------
  // ENC-532 (T3.6) — upload CPU pixels for a logical textureId so the
  // texturedQuad@1 pipeline can sample them (heatmap/spectrogram/weather
  // colormaps). The pixels cross the JS boundary as a Uint8Array
  // (emscripten::val); we copy their raw bytes verbatim (NOT via std::string,
  // which would UTF-8-mangle them) — the SAME marshalling applyDataBatch uses.
  // `format` is a dc::TextureFormat code (0 = R8, 1 = RGBA8); anything other
  // than R8 is treated as RGBA8. The DawnTexturedQuadBackend uploads these into
  // a wgpu::Texture lazily on first use, so updating a textureId between renders
  // is picked up on the next draw of any DrawItem bound to it via
  // setDrawItemTexture (the texture cache keys on textureId).
  void setTexturePixels(std::uint32_t textureId, emscripten::val pixels,
                        std::uint32_t w, std::uint32_t h, std::uint32_t format) {
    std::vector<std::uint8_t> bytes =
        emscripten::convertJSArrayToNumberVector<std::uint8_t>(pixels);
    dc::TextureFormat fmt = (format == 0) ? dc::TextureFormat::R8
                                          : dc::TextureFormat::RGBA8;
    textureSource_.set(textureId, std::move(bytes), w, h, fmt);
  }

  // ---- render: render(w,h) -> framebuffer readback ----------------------
  // Render the live scene into an offscreen RGBA8 target at (w,h) and read it
  // back so the TS wrapper can blit it onto the bound <canvas>. Brings up the
  // DawnSceneRenderer lazily on first call (device acquisition SUSPENDS via
  // ASYNCIFY). Returns 1 on success, negative on failure (renderMessage()
  // carries the reason). After a successful render the framebuffer is available
  // via framebuffer()/framebufferWidth()/framebufferHeight().
  int render(int w, int h) {
    renderMessage_.clear();
    if (w <= 0 || h <= 0) {
      renderMessage_ = "render: invalid size";
      return -1;
    }
    if (!ensureRenderer()) return -2;  // renderMessage_ set by ensureRenderer

    // Re-sync ALL non-empty ingest buffers into the store before the walk, in
    // case control commands (createBuffer with inline data) populated ingest_
    // without going through applyDataBatch. Cheap (setCpuData over a handful of
    // buffers); guarantees the render store mirrors ingest state.
    syncAllBuffers();

    dc::Stats s = renderer_->render(scene_, store_, w, h);
    lastWidth_ = w;
    lastHeight_ = h;

    // Capture per-frame stats and reset the per-frame accumulators.
    stats_.frameMs = s.frameMs;
    stats_.drawCalls = static_cast<int>(s.drawCalls);
    stats_.culledDrawCalls = static_cast<int>(s.culledDrawCalls);
    stats_.uploadedBytesThisFrame = static_cast<double>(s.uploadedBytesThisFrame);
    stats_.ingestedBytesThisFrame = static_cast<double>(ingestedBytesAccum_);
    stats_.activeBuffers = static_cast<int>(scene_.bufferIds().size());
    ingestedBytesAccum_ = 0;
    droppedBytesAccum_ = 0;

    // Read back the full framebuffer for the canvas blit (same as dc_webgpu_all).
    framebuffer_.assign(static_cast<std::size_t>(w) * h * 4u, 0);
    std::uint32_t gotW = 0, gotH = 0;
    bool fbOk = renderer_->device().readFramebufferRGBA(
        framebuffer_.data(), framebuffer_.size(), &gotW, &gotH);
    fbW_ = gotW;
    fbH_ = gotH;
    if (!fbOk) {
      renderMessage_ = "render: framebuffer readback failed";
      return -3;
    }
    renderMessage_ = "rendered " + std::to_string(s.drawCalls) +
                     " draw calls on backend=" + renderer_->device().backendName();
    return 1;
  }

  // ---- pick: pick(w,h,x,y) -> drawItemId (0 == miss) --------------------
  // GPU color-ID pick at pixel (x,y) over the scene rendered at (w,h). Mirrors
  // EngineHost.pick (which returns { drawItemId } | null); the TS wrapper maps
  // 0 -> null. Brings up the renderer lazily like render().
  double pick(int w, int h, int x, int y) {
    if (w <= 0 || h <= 0) return 0;
    if (!ensureRenderer()) return 0;
    syncAllBuffers();
    dc::DawnPickResult r =
        renderer_->renderPick(scene_, store_, w, h, x, y, nullptr);
    return static_cast<double>(r.drawItemId);
  }

  // ---- lifecycle: dispose ------------------------------------------------
  // Tear down the renderer (frees the WebGPU device + pipelines). The scene /
  // ingest / store are plain CPU state freed with the object. Mirrors
  // EngineHost.shutdown/dispose.
  void dispose() {
    renderer_.reset();
    framebuffer_.clear();
    fbW_ = fbH_ = 0;
  }

  // ---- framebuffer accessors (do not suspend) ---------------------------
  // typed_memory_view into the WASM heap; the JS caller slice()s it into a
  // standalone Uint8Array before the next render can realloc it.
  emscripten::val framebuffer() const {
    return emscripten::val(emscripten::typed_memory_view(
        framebuffer_.size(), framebuffer_.data()));
  }
  int framebufferWidth() const { return static_cast<int>(fbW_); }
  int framebufferHeight() const { return static_cast<int>(fbH_); }
  std::string renderMessage() const { return renderMessage_; }
  std::string backend() const {
    return renderer_ ? renderer_->device().backendName() : std::string();
  }

  // ---- stats -------------------------------------------------------------
  DcEngineStats stats() const { return stats_; }

  // ---- scene readback (diagnostics, mirrors DcCore) ---------------------
  int paneCount() const { return static_cast<int>(scene_.paneIds().size()); }
  int layerCount() const { return static_cast<int>(scene_.layerIds().size()); }
  int drawItemCount() const { return static_cast<int>(scene_.drawItemIds().size()); }
  int bufferCount() const { return static_cast<int>(scene_.bufferIds().size()); }
  int geometryCount() const { return static_cast<int>(scene_.geometryIds().size()); }
  std::string listResources() const { return cp_.listResourcesJson(); }

  // Read back a buffer's CPU bytes (lets node validation assert ingest results
  // byte-for-byte, reusing the ENC-505 pattern).
  emscripten::val getBufferBytes(double bufferId) const {
    auto id = static_cast<dc::Id>(bufferId);
    const std::uint8_t* data = ingest_.getBufferData(id);
    std::uint32_t size = ingest_.getBufferSize(id);
    return emscripten::val(emscripten::typed_memory_view(size, data));
  }
  double bufferSize(double bufferId) const {
    return static_cast<double>(
        ingest_.getBufferSize(static_cast<dc::Id>(bufferId)));
  }

private:
  // Bring up the DawnSceneRenderer on first use (device acquisition SUSPENDS via
  // ASYNCIFY in the browser). No atlas / textures supplied, so textSDF@1 /
  // texturedQuad@1 are skipped (the other 8 pipelines register) — identical to
  // dc_webgpu_all. Returns false (renderMessage_ set) if device/pipeline init
  // fails.
  bool ensureRenderer() {
    if (renderer_) return true;
    // ENC-532: supply the in-memory TextureSource so texturedQuad@1 registers
    // and can sample browser-uploaded colormap pixels (setTexturePixels). The
    // glyph atlas (textSDF@1) is still null. The source is owned by this host
    // and outlives the renderer; the backend holds it non-owning.
    renderer_ = std::make_unique<dc::DawnSceneRenderer>(
        /*atlas=*/nullptr, &textureSource_);
    if (!renderer_->init()) {
      renderMessage_ =
          "DawnSceneRenderer::init failed: " + renderer_->errorMessage();
      renderer_.reset();
      return false;
    }
    return true;
  }

  // Push every non-empty ingest buffer's current bytes into the render store.
  void syncAllBuffers() {
    for (dc::Id id : scene_.bufferIds()) {
      const auto* data = ingest_.getBufferData(id);
      auto size = ingest_.getBufferSize(id);
      if (data && size > 0) store_.setCpuData(id, data, size);
    }
  }

  dc::Scene scene_{};
  dc::ResourceRegistry reg_{};
  dc::CommandProcessor cp_;
  dc::IngestProcessor ingest_{};
  dc::CpuBufferStore store_{};
  // ENC-532: CPU pixels for texturedQuad@1, keyed by textureId. Owned here so it
  // outlives renderer_ (the backend references it non-owning). Declared BEFORE
  // renderer_ so it is destroyed AFTER it.
  MemTextureSource textureSource_{};
  std::unique_ptr<dc::DawnSceneRenderer> renderer_;

  std::vector<std::uint8_t> framebuffer_;
  std::uint32_t fbW_{0};
  std::uint32_t fbH_{0};
  int lastWidth_{0};
  int lastHeight_{0};
  std::string renderMessage_;

  DcEngineStats stats_{};
  std::uint64_t ingestedBytesAccum_{0};
  std::uint64_t droppedBytesAccum_{0};
};

}  // namespace

EMSCRIPTEN_BINDINGS(dc_engine_host) {
  namespace em = emscripten;

  em::value_object<DcControlResult>("DcControlResult")
      .field("ok", &DcControlResult::ok)
      .field("error", &DcControlResult::error);

  em::value_object<DcEngineStats>("DcEngineStats")
      .field("frameMs", &DcEngineStats::frameMs)
      .field("drawCalls", &DcEngineStats::drawCalls)
      .field("culledDrawCalls", &DcEngineStats::culledDrawCalls)
      .field("ingestedBytesThisFrame", &DcEngineStats::ingestedBytesThisFrame)
      .field("uploadedBytesThisFrame", &DcEngineStats::uploadedBytesThisFrame)
      .field("activeBuffers", &DcEngineStats::activeBuffers);

  em::class_<DcEngineHost>("DcEngineHost")
      .constructor<>()
      .function("applyControl", &DcEngineHost::applyControl)
      .function("applyDataBatch", &DcEngineHost::applyDataBatch)
      .function("setTexturePixels", &DcEngineHost::setTexturePixels)
      .function("render", &DcEngineHost::render)
      .function("pick", &DcEngineHost::pick)
      .function("dispose", &DcEngineHost::dispose)
      .function("framebuffer", &DcEngineHost::framebuffer)
      .function("framebufferWidth", &DcEngineHost::framebufferWidth)
      .function("framebufferHeight", &DcEngineHost::framebufferHeight)
      .function("renderMessage", &DcEngineHost::renderMessage)
      .function("backend", &DcEngineHost::backend)
      .function("stats", &DcEngineHost::stats)
      .function("paneCount", &DcEngineHost::paneCount)
      .function("layerCount", &DcEngineHost::layerCount)
      .function("drawItemCount", &DcEngineHost::drawItemCount)
      .function("bufferCount", &DcEngineHost::bufferCount)
      .function("geometryCount", &DcEngineHost::geometryCount)
      .function("listResources", &DcEngineHost::listResources)
      .function("getBufferBytes", &DcEngineHost::getBufferBytes)
      .function("bufferSize", &DcEngineHost::bufferSize);
}
