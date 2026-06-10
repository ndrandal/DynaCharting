// D79: Generic JSON Chart Host
// Single binary that reads a .json SceneDocument, renders it headless,
// and serves frames over the TEXT/FRME protocol on stdin/stdout.
//
// Usage: dc_json_host <chart.json>
//
// ENC-500 (P5.1) — Dawn cutover. The embedding/render path is now Dawn by
// DEFAULT: when the WebGPU/Dawn backend is available (DC_HAS_DAWN), JsonHost
// renders through DawnDevice + CpuBufferStore + DawnSceneRenderer and reads the
// frame back via DawnDevice::readPixel. When Dawn is OFF (DC_HAS_DAWN undefined)
// it falls back to the original GL path (OsMesaContext + GpuBufferManager +
// Renderer + OSMesa readPixels), so JsonHost still builds and works without
// Dawn. The two paths are isolated behind a single HostRenderBackend interface;
// everything else (parse, reconcile, ingest, bindings, viewports, the input
// loop) is backend-agnostic and shared.
//
// GL is the conformance reference (ENC-509..512); deleting dc_gl is a SEPARATE
// ticket (ENC-501).

#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"

#include "dc/binding/BindingEvaluator.hpp"
#include "dc/data/DerivedBuffer.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/export/ChartSnapshot.hpp"

// Backend-agnostic CPU-side buffer store (base of the GL GpuBufferManager and
// the store the Dawn render path uses directly).
#include "dc/render/CpuBufferStore.hpp"

#if DC_HAS_DAWN
// ENC-500 default render path: Dawn (WebGPU native).
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnSceneRenderer.hpp"
#else
// Fallback render path: GL (OSMesa).
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#endif

#include <rapidjson/document.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Protocol helpers (same as live_server.cpp)
// ---------------------------------------------------------------------------

static std::string readLine() {
  std::string line;
  int c;
  while ((c = std::fgetc(stdin)) != EOF && c != '\n') {
    line += static_cast<char>(c);
  }
  if (c == EOF && line.empty()) return {};
  return line;
}

static void writeTextOverlay(const dc::DocTextOverlay& overlay, int W, int H) {
  if (overlay.labels.empty()) return;

  std::string json = R"({"fontSize":)";
  json += std::to_string(overlay.fontSize);
  json += R"(,"color":")" + overlay.color + R"(","labels":[)";

  bool first = true;
  for (const auto& lbl : overlay.labels) {
    float px = (lbl.clipX + 1.0f) / 2.0f * static_cast<float>(W);
    float py = (1.0f - lbl.clipY) / 2.0f * static_cast<float>(H);
    if (!first) json += ",";
    first = false;
    char buf[512];
    std::snprintf(buf, sizeof(buf),
      R"({"x":%.1f,"y":%.1f,"t":"%s","a":"%s")",
      static_cast<double>(px), static_cast<double>(py),
      lbl.text.c_str(), lbl.align.c_str());
    json += buf;
    if (!lbl.color.empty()) {
      json += R"(,"c":")" + lbl.color + R"(")";
    }
    if (lbl.fontSize > 0) {
      json += R"(,"fs":)" + std::to_string(lbl.fontSize);
    }
    json += "}";
  }
  json += "]}";

  std::fwrite("TEXT", 1, 4, stdout);
  std::uint32_t len = static_cast<std::uint32_t>(json.size());
  std::fwrite(&len, 4, 1, stdout);
  std::fwrite(json.data(), 1, json.size(), stdout);
}

// ---------------------------------------------------------------------------
// Render backend abstraction (ENC-500 cutover seam).
//
// The host's input loop only needs four operations from a render backend:
//   * render the current scene at (W,H) and produce a TOP-DOWN RGBA frame,
//   * GPU-pick at a top-down pixel and return the hit DrawItem id,
//   * resize to a new (W,H),
//   * push CPU buffer bytes into the backend's buffer store.
// Two concrete backends implement it: Dawn (default, DC_HAS_DAWN) and GL
// (fallback). Both expose a CpuBufferStore-derived store so the shared buffer-
// sync helpers below stay backend-neutral.
// ---------------------------------------------------------------------------

struct HostRenderBackend {
  virtual ~HostRenderBackend() = default;

  // The backend's CPU-side buffer store (GpuBufferManager for GL, CpuBufferStore
  // for Dawn). setCpuData/getCpuData live on the shared base.
  virtual dc::CpuBufferStore& store() = 0;

  // Render `scene` at (W,H) into `outRgba` as TOP-DOWN, row-major RGBA
  // (outRgba.size() == W*H*4). This unifies the two backends' opposite readback
  // origins (GL is bottom-up; Dawn is top-down) so the protocol/PNG writers see
  // one convention.
  virtual void render(const dc::Scene& scene, int W, int H,
                      std::vector<std::uint8_t>& outRgba) = 0;

  // GPU pick at a TOP-DOWN pixel (px, pyTop). Returns the hit DrawItem id (0 ==
  // background). Each backend converts to its native origin internally.
  virtual dc::Id pick(const dc::Scene& scene, int W, int H,
                      int px, int pyTop, dc::EventBus* bus) = 0;

  // Recreate device-side resources at a new (W,H). The caller re-syncs buffers.
  virtual bool resize(int W, int H) = 0;
};

#if DC_HAS_DAWN
// --- Dawn render backend (DEFAULT) -----------------------------------------
//
// Owns a DawnSceneRenderer (which owns a DawnDevice + all 10 pipeline backends)
// and a CpuBufferStore. The scene's CPU buffer bytes are pushed into the store;
// render() walks the whole scene through the Dawn backend registry and reads the
// offscreen target back per-pixel via DawnDevice::readPixel (already top-down).
class DawnHostBackend final : public HostRenderBackend {
 public:
  // Optional glyph atlas (textSDF@1) + texture source (texturedQuad@1) are wired
  // into the renderer so text/textured charts render. JsonHost charts that use
  // neither leave both null (the renderer simply skips those pipelines).
  explicit DawnHostBackend(dc::GlyphAtlas* atlas = nullptr,
                           const dc::TextureSource* textures = nullptr)
      : atlas_(atlas), textures_(textures) {}

  bool init() {
    renderer_ = std::make_unique<dc::DawnSceneRenderer>(atlas_, textures_);
    if (!renderer_->init()) return false;
    return true;
  }

  dc::CpuBufferStore& store() override { return store_; }

  void render(const dc::Scene& scene, int W, int H,
              std::vector<std::uint8_t>& outRgba) override {
    renderer_->render(scene, store_, W, H);
    // DawnDevice::readPixel is TOP-DOWN origin already.
    outRgba.assign(static_cast<std::size_t>(W) * H * 4, 0);
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        std::uint8_t px[4] = {0, 0, 0, 0};
        renderer_->device().readPixel(x, y, px);
        std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4;
        outRgba[idx + 0] = px[0];
        outRgba[idx + 1] = px[1];
        outRgba[idx + 2] = px[2];
        outRgba[idx + 3] = px[3];
      }
    }
  }

  dc::Id pick(const dc::Scene& scene, int W, int H, int px, int pyTop,
              dc::EventBus* bus) override {
    // DawnSceneRenderer::renderPick reads its pick target top-down, matching the
    // top-down (px, pyTop) the input loop already computed.
    dc::DawnPickResult r =
        renderer_->renderPick(scene, store_, W, H, px, pyTop, bus);
    return static_cast<dc::Id>(r.drawItemId);
  }

  bool resize(int /*W*/, int /*H*/) override {
    // The DawnDevice recreates its offscreen target lazily at the new (W,H) on
    // the next beginRenderPass, so no explicit teardown is needed. The store is
    // re-synced by the caller.
    return true;
  }

 private:
  dc::GlyphAtlas* atlas_{nullptr};
  const dc::TextureSource* textures_{nullptr};
  std::unique_ptr<dc::DawnSceneRenderer> renderer_;
  dc::CpuBufferStore store_;
};
#else
// --- GL render backend (FALLBACK, Dawn OFF) --------------------------------
//
// The original D79 path: OsMesaContext + GpuBufferManager + Renderer, OSMesa
// readback (bottom-up) flipped to top-down here so the rest of the host sees the
// same convention as the Dawn path.
class GlHostBackend final : public HostRenderBackend {
 public:
  bool init(int W, int H) {
    ctx_ = std::make_unique<dc::OsMesaContext>();
    if (!ctx_->init(W, H)) return false;
    renderer_ = std::make_unique<dc::Renderer>();
    return renderer_->init();
  }

  dc::CpuBufferStore& store() override { return gpuBufs_; }

  void render(const dc::Scene& scene, int W, int H,
              std::vector<std::uint8_t>& outRgba) override {
    gpuBufs_.uploadDirty();
    renderer_->render(scene, gpuBufs_, W, H);
    ctx_->swapBuffers();
    auto pixels = ctx_->readPixels();  // bottom-up RGBA
    // Flip to top-down so writeFrame/PNG see the same origin as Dawn.
    outRgba.assign(static_cast<std::size_t>(W) * H * 4, 0);
    for (int y = 0; y < H; ++y) {
      std::memcpy(&outRgba[(static_cast<std::size_t>(y) * W) * 4],
                  &pixels[(static_cast<std::size_t>(H - 1 - y) * W) * 4],
                  static_cast<std::size_t>(W) * 4);
    }
  }

  dc::Id pick(const dc::Scene& scene, int W, int H, int px, int pyTop,
              dc::EventBus* /*bus*/) override {
    gpuBufs_.uploadDirty();
    // GL pick target is bottom-up; convert the top-down probe row.
    int pickY = H - 1 - pyTop;
    auto r = renderer_->renderPick(scene, gpuBufs_, W, H, px, pickY);
    return r.drawItemId;
  }

  bool resize(int W, int H) override {
    ctx_ = std::make_unique<dc::OsMesaContext>();
    if (!ctx_->init(W, H)) return false;
    renderer_ = std::make_unique<dc::Renderer>();
    if (!renderer_->init()) return false;
    // A fresh GpuBufferManager (new GL context => new VBOs); caller re-syncs.
    gpuBufs_ = dc::GpuBufferManager();
    return true;
  }

 private:
  std::unique_ptr<dc::OsMesaContext> ctx_;
  std::unique_ptr<dc::Renderer> renderer_;
  dc::GpuBufferManager gpuBufs_;
};
#endif

// Write a TOP-DOWN RGBA frame as a FRME message (the protocol's native order).
static void writeFrame(const std::uint8_t* topDownRgba, int w, int h) {
  std::fwrite("FRME", 1, 4, stdout);
  std::uint32_t width  = static_cast<std::uint32_t>(w);
  std::uint32_t height = static_cast<std::uint32_t>(h);
  std::fwrite(&width, 4, 1, stdout);
  std::fwrite(&height, 4, 1, stdout);
  std::fwrite(topDownRgba, 1, static_cast<std::size_t>(w) * h * 4, stdout);
  std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// Viewport management
// ---------------------------------------------------------------------------

struct HostViewport {
  dc::Viewport viewport;
  dc::Id transformId{0};
  dc::Id paneId{0};
  std::string linkGroup;
  bool panX{true}, panY{true}, zoomX{true}, zoomY{true};
  // Initial ranges for Home-key reset
  double initXMin{0}, initXMax{1}, initYMin{0}, initYMax{1};
};

static void syncTransform(dc::CommandProcessor& cp, dc::Id transformId,
                          const dc::Viewport& vp) {
  auto tp = vp.computeTransformParams();
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setTransform","id":%u,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
    static_cast<unsigned>(transformId),
    static_cast<double>(tp.sx), static_cast<double>(tp.sy),
    static_cast<double>(tp.tx), static_cast<double>(tp.ty));
  cp.applyJsonText(buf);
}

// ---------------------------------------------------------------------------
// Buffer upload helpers (backend-agnostic — operate on CpuBufferStore)
// ---------------------------------------------------------------------------

static void uploadInlineBuffers(const dc::SceneDocument& doc,
                                dc::IngestProcessor& ingest) {
  for (const auto& [id, buf] : doc.buffers) {
    if (!buf.data.empty()) {
      ingest.ensureBuffer(id);
      ingest.setBufferData(id,
        reinterpret_cast<const std::uint8_t*>(buf.data.data()),
        static_cast<std::uint32_t>(buf.data.size() * sizeof(float)));
    }
  }
}

static void syncAllGpuBuffers(const dc::SceneDocument& doc,
                              dc::IngestProcessor& ingest,
                              dc::CpuBufferStore& store) {
  for (const auto& [id, buf] : doc.buffers) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0) store.setCpuData(id, data, size);
  }
}

static void syncTouchedBuffersToGpu(const std::vector<dc::Id>& touchedIds,
                                     dc::IngestProcessor& ingest,
                                     dc::CpuBufferStore& store) {
  for (dc::Id id : touchedIds) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0)
      store.setCpuData(id, data, size);
    else
      store.setCpuData(id, nullptr, 0);
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  // Redirect stderr to /dev/null to prevent debug output from corrupting protocol
  std::freopen("/dev/null", "w", stderr);

  if (argc < 2) return 1;

  // Parse flags: dc_json_host [--png output.png] <chart.json>
  std::string pngPath;
  std::string jsonPath;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--png" && i + 1 < argc) {
      pngPath = argv[++i];
    } else if (jsonPath.empty()) {
      jsonPath = arg;
    }
  }
  if (jsonPath.empty()) return 1;

  // ---- 1. Read JSON file ----
  std::ifstream ifs(jsonPath);
  if (!ifs.is_open()) return 1;
  std::ostringstream ss;
  ss << ifs.rdbuf();
  std::string jsonStr = ss.str();

  // ---- 2. Parse SceneDocument ----
  dc::SceneDocument doc;
  if (!dc::parseSceneDocument(jsonStr, doc)) return 1;

  int W = doc.viewportWidth > 0 ? doc.viewportWidth : 900;
  int H = doc.viewportHeight > 0 ? doc.viewportHeight : 600;

  // ---- 3. Init the render backend (Dawn by default; GL fallback) ----
#if DC_HAS_DAWN
  auto backend = std::make_unique<DawnHostBackend>();
  if (!backend->init()) return 1;
#else
  auto backend = std::make_unique<GlHostBackend>();
  if (!backend->init(W, H)) return 1;
#endif

  // ---- 4. Set up engine ----
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // ---- 5. Reconcile scene from document ----
  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  if (!result.ok) return 1;

  // ---- 6. Upload inline buffer data ----
  uploadInlineBuffers(doc, ingest);

  // ---- 7. Build viewports ----
  std::map<std::string, HostViewport> viewports;
  for (const auto& [name, dv] : doc.viewports) {
    HostViewport hv;
    hv.transformId = dv.transformId;
    hv.paneId = dv.paneId;
    hv.linkGroup = dv.linkGroup;
    hv.panX = dv.panX; hv.panY = dv.panY;
    hv.zoomX = dv.zoomX; hv.zoomY = dv.zoomY;
    hv.initXMin = dv.xMin; hv.initXMax = dv.xMax;
    hv.initYMin = dv.yMin; hv.initYMax = dv.yMax;

    hv.viewport.setPixelViewport(W, H);
    const auto* pane = scene.getPane(dv.paneId);
    if (pane) hv.viewport.setClipRegion(pane->region);
    hv.viewport.setDataRange(dv.xMin, dv.xMax, dv.yMin, dv.yMax);

    syncTransform(cp, dv.transformId, hv.viewport);
    viewports[name] = std::move(hv);
  }

  // ---- 7b. Set up bindings (D80) ----
  dc::DerivedBufferManager derivedBufs;
  dc::SelectionState selectionState;
  dc::EventBus eventBus;
  dc::BindingEvaluator binder(cp, ingest, derivedBufs);

  if (!doc.bindings.empty()) {
    // Ensure output buffers exist in IngestProcessor
    for (const auto& [id, b] : doc.bindings) {
      if (b.effect.outputBufferId != 0)
        ingest.ensureBuffer(b.effect.outputBufferId);
    }
    binder.loadBindings(doc.bindings);
    binder.attach(eventBus, selectionState);
  }

  // ---- 8. Push CPU buffer bytes into the backend store ----
  syncAllGpuBuffers(doc, ingest, backend->store());

  // ---- 9. Render initial frame ----
  std::vector<std::uint8_t> frame;  // top-down RGBA
  backend->render(scene, W, H, frame);

  // --png mode: write PNG and exit (no protocol, no input loop). The backend
  // already produced a top-down frame, so write it without the extra Y flip.
  if (!pngPath.empty()) {
    bool ok = dc::writePNG(pngPath, frame.data(), W, H);
    return ok ? 0 : 1;
  }

  writeTextOverlay(doc.textOverlay, W, H);
  writeFrame(frame.data(), W, H);

  // ---- 10. Input loop ----
  bool dragging = false;
  bool didDrag = false;
  double lastMouseX = 0.0, lastMouseY = 0.0;
  double mouseDownX = 0.0, mouseDownY = 0.0;
  bool hasBindings = !doc.bindings.empty();

  while (true) {
    std::string line = readLine();
    if (line.empty()) break;

    rapidjson::Document jdoc;
    jdoc.Parse(line.c_str());
    if (jdoc.HasParseError() || !jdoc.IsObject()) continue;
    if (!jdoc.HasMember("cmd")) continue;

    std::string cmd = jdoc["cmd"].GetString();
    bool needsRender = false;

    if (cmd == "render") {
      needsRender = true;
    }
    else if (cmd == "mouse") {
      double px = jdoc["x"].GetDouble();
      double py = jdoc["y"].GetDouble();
      int buttons = jdoc.HasMember("buttons") ? jdoc["buttons"].GetInt() : 0;
      std::string type = jdoc.HasMember("type") ? jdoc["type"].GetString() : "";

      if (type == "down") {
        dragging = (buttons & 1) != 0;
        didDrag = false;
        lastMouseX = px; lastMouseY = py;
        mouseDownX = px; mouseDownY = py;
      }
      else if (type == "up") {
        // Click detection: if mouse didn't move much, treat as a pick-click
        if (hasBindings) {
          double dist = std::sqrt((px - mouseDownX) * (px - mouseDownX) +
                                   (py - mouseDownY) * (py - mouseDownY));
          if (dist < 20.0) {
            // GPU pick at click position. The browser reports a top-down y; the
            // backend converts to its native origin internally.
            dc::Id hitId = backend->pick(scene, W, H,
                static_cast<int>(px), static_cast<int>(py), &eventBus);
            if (hitId != 0) {
              // Toggle selection on the picked DrawItem
              dc::SelectionKey key{hitId, 0};
              if (selectionState.isSelected(key)) {
                selectionState.clear();
              } else {
                selectionState.clear();
                selectionState.select(key);
              }
            } else {
              selectionState.clear();
            }
            auto touched = binder.onSelectionChanged(selectionState);
            syncTouchedBuffersToGpu(touched, ingest, backend->store());
            needsRender = true;
          }
        }
        dragging = false;
        didDrag = false;
      }
      else if (type == "move" && (buttons & 1) != 0) {
        if (!dragging) {
          dragging = true;
          lastMouseX = px; lastMouseY = py;
        }
        double dx = px - lastMouseX;
        double dy = py - lastMouseY;
        lastMouseX = px; lastMouseY = py;

        if (std::fabs(dx) > 0.001 || std::fabs(dy) > 0.001) {
          didDrag = true;
          // Find viewport under cursor
          std::string activeVp;
          for (auto& [name, hv] : viewports) {
            if (hv.viewport.containsPixel(px, py)) {
              activeVp = name;
              break;
            }
          }

          for (auto& [name, hv] : viewports) {
            bool isActive = (name == activeVp);
            // Pan X: if in same link group as active, or is active
            bool shouldPanX = false;
            if (isActive) shouldPanX = hv.panX;
            else if (!activeVp.empty() && !hv.linkGroup.empty() &&
                     hv.linkGroup == viewports[activeVp].linkGroup)
              shouldPanX = hv.panX;

            bool shouldPanY = isActive && hv.panY;

            double panDx = shouldPanX ? dx : 0;
            double panDy = shouldPanY ? dy : 0;
            if (std::fabs(panDx) > 0.001 || std::fabs(panDy) > 0.001) {
              hv.viewport.pan(panDx, panDy);
              syncTransform(cp, hv.transformId, hv.viewport);
              needsRender = true;
            }
          }
        }
      }
    }
    else if (cmd == "scroll") {
      double px = jdoc["x"].GetDouble();
      double py = jdoc["y"].GetDouble();
      double dy = jdoc.HasMember("dy") ? jdoc["dy"].GetDouble() : 0.0;

      if (std::fabs(dy) > 0.001) {
        double factor = dy * 0.1;

        // Find viewport under cursor
        std::string activeVp;
        for (auto& [name, hv] : viewports) {
          if (hv.viewport.containsPixel(px, py)) {
            activeVp = name;
            break;
          }
        }

        for (auto& [name, hv] : viewports) {
          bool isActive = (name == activeVp);
          bool linked = (!activeVp.empty() && !hv.linkGroup.empty() &&
                        hv.linkGroup == viewports[activeVp].linkGroup);

          if (isActive || linked) {
            // Apply zoom with viewport's axis constraints
            double zx = (hv.zoomX && (isActive || linked)) ? factor : 0;
            double zy = (hv.zoomY && isActive) ? factor : 0;
            // Use separate X/Y zoom if needed, or combined
            if (std::fabs(zx) > 0.001 || std::fabs(zy) > 0.001) {
              // For linked viewports, only zoom X
              if (!isActive) zy = 0;
              hv.viewport.zoom(std::max(std::fabs(zx), std::fabs(zy)) *
                              (factor > 0 ? 1.0 : -1.0), px, py);
              syncTransform(cp, hv.transformId, hv.viewport);
              needsRender = true;
            }
          }
        }
      }
    }
    else if (cmd == "key") {
      std::string code = jdoc.HasMember("code") ? jdoc["code"].GetString() : "";
      double panAmount = 30.0;

      if (code == "ArrowRight" || code == "ArrowLeft") {
        double dx = (code == "ArrowLeft") ? panAmount : -panAmount;
        for (auto& [name, hv] : viewports) {
          if (hv.panX) {
            hv.viewport.pan(dx, 0);
            syncTransform(cp, hv.transformId, hv.viewport);
          }
        }
        needsRender = true;
      }
      else if (code == "ArrowUp" || code == "ArrowDown") {
        double f = (code == "ArrowUp") ? 0.2 : -0.2;
        double cx = static_cast<double>(W) / 2.0;
        double cy = static_cast<double>(H) / 2.0;
        for (auto& [name, hv] : viewports) {
          hv.viewport.zoom(f, cx, cy);
          syncTransform(cp, hv.transformId, hv.viewport);
        }
        needsRender = true;
      }
      else if (code == "Home") {
        for (auto& [name, hv] : viewports) {
          hv.viewport.setDataRange(hv.initXMin, hv.initXMax,
                                   hv.initYMin, hv.initYMax);
          syncTransform(cp, hv.transformId, hv.viewport);
        }
        needsRender = true;
      }
    }
    else if (cmd == "resize") {
      int newW = jdoc.HasMember("w") ? jdoc["w"].GetInt() : W;
      int newH = jdoc.HasMember("h") ? jdoc["h"].GetInt() : H;

      if (newW != W || newH != H) {
        W = newW; H = newH;

        if (!backend->resize(W, H)) break;

        for (auto& [name, hv] : viewports) {
          hv.viewport.setPixelViewport(W, H);
          syncTransform(cp, hv.transformId, hv.viewport);
        }

        syncAllGpuBuffers(doc, ingest, backend->store());
        needsRender = true;
      }
    }

    // ---- D80: Selection commands (drive bindings) ----
    else if (cmd == "select") {
      dc::Id diId = jdoc.HasMember("drawItemId")
        ? static_cast<dc::Id>(jdoc["drawItemId"].GetUint64()) : 0;
      std::uint32_t recIdx = jdoc.HasMember("recordIndex")
        ? static_cast<std::uint32_t>(jdoc["recordIndex"].GetUint()) : 0;
      if (diId != 0) {
        selectionState.select({diId, recIdx});
        auto touched = binder.onSelectionChanged(selectionState);
        syncTouchedBuffersToGpu(touched, ingest, backend->store());
        needsRender = true;
      }
    }
    else if (cmd == "deselect") {
      dc::Id diId = jdoc.HasMember("drawItemId")
        ? static_cast<dc::Id>(jdoc["drawItemId"].GetUint64()) : 0;
      std::uint32_t recIdx = jdoc.HasMember("recordIndex")
        ? static_cast<std::uint32_t>(jdoc["recordIndex"].GetUint()) : 0;
      if (diId != 0) {
        selectionState.deselect({diId, recIdx});
        auto touched = binder.onSelectionChanged(selectionState);
        syncTouchedBuffersToGpu(touched, ingest, backend->store());
        needsRender = true;
      }
    }
    else if (cmd == "clearSelection") {
      selectionState.clear();
      auto touched = binder.onSelectionChanged(selectionState);
      syncTouchedBuffersToGpu(touched, ingest, backend->store());
      needsRender = true;
    }
    else if (cmd == "toggleSelection") {
      dc::Id diId = jdoc.HasMember("drawItemId")
        ? static_cast<dc::Id>(jdoc["drawItemId"].GetUint64()) : 0;
      std::uint32_t recIdx = jdoc.HasMember("recordIndex")
        ? static_cast<std::uint32_t>(jdoc["recordIndex"].GetUint()) : 0;
      if (diId != 0) {
        selectionState.toggle({diId, recIdx});
        auto touched = binder.onSelectionChanged(selectionState);
        syncTouchedBuffersToGpu(touched, ingest, backend->store());
        needsRender = true;
      }
    }
    else if (cmd == "hover") {
      dc::Id diId = jdoc.HasMember("drawItemId")
        ? static_cast<dc::Id>(jdoc["drawItemId"].GetUint64()) : 0;
      std::uint32_t recIdx = jdoc.HasMember("recordIndex")
        ? static_cast<std::uint32_t>(jdoc["recordIndex"].GetUint())
        : static_cast<std::uint32_t>(-1);
      auto touched = binder.onHoverChanged(diId, recIdx);
      syncTouchedBuffersToGpu(touched, ingest, backend->store());
      if (!touched.empty()) needsRender = true;
    }

    if (needsRender) {
      backend->render(scene, W, H, frame);
      writeTextOverlay(doc.textOverlay, W, H);
      writeFrame(frame.data(), W, H);
    }
  }

  return 0;
}
