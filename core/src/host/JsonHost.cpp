// D79: Generic JSON Chart Host
// Single binary that reads a .json SceneDocument, renders via OSMesa,
// and serves frames over the TEXT/FRME protocol on stdin/stdout.
//
// Usage: dc_json_host <chart.json>

#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/viewport/Viewport.hpp"

#include "dc/binding/BindingEvaluator.hpp"
#include "dc/data/DerivedBuffer.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/export/ChartSnapshot.hpp"

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

static void writeFrame(const std::uint8_t* pixels, int w, int h) {
  std::fwrite("FRME", 1, 4, stdout);
  std::uint32_t width  = static_cast<std::uint32_t>(w);
  std::uint32_t height = static_cast<std::uint32_t>(h);
  std::fwrite(&width, 4, 1, stdout);
  std::fwrite(&height, 4, 1, stdout);
  // OSMesa readPixels is bottom-up; write top-down
  for (int y = h - 1; y >= 0; y--) {
    std::fwrite(pixels + y * w * 4, 1, static_cast<std::size_t>(w) * 4, stdout);
  }
  std::fflush(stdout);
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
// Buffer upload helpers
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
                              dc::GpuBufferManager& gpuBufs) {
  for (const auto& [id, buf] : doc.buffers) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0) gpuBufs.setCpuData(id, data, size);
  }
}

static void syncTouchedBuffersToGpu(const std::vector<dc::Id>& touchedIds,
                                     dc::IngestProcessor& ingest,
                                     dc::GpuBufferManager& gpuBufs) {
  for (dc::Id id : touchedIds) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0)
      gpuBufs.setCpuData(id, data, size);
    else
      gpuBufs.setCpuData(id, nullptr, 0);
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

  // ---- 3. Init OSMesa ----
  auto ctx = std::make_unique<dc::OsMesaContext>();
  if (!ctx->init(W, H)) return 1;

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

  // ---- 8. Init renderer and GPU buffers ----
  auto gpuBufs = std::make_unique<dc::GpuBufferManager>();
  auto renderer = std::make_unique<dc::Renderer>();
  renderer->init();

  syncAllGpuBuffers(doc, ingest, *gpuBufs);
  gpuBufs->uploadDirty();

  // ---- 9. Render initial frame ----
  renderer->render(scene, *gpuBufs, W, H);
  ctx->swapBuffers();
  auto pixels = ctx->readPixels();

  // --png mode: write PNG and exit (no protocol, no input loop)
  if (!pngPath.empty()) {
    bool ok = dc::writePNGFlipped(pngPath, pixels.data(), W, H);
    return ok ? 0 : 1;
  }

  writeTextOverlay(doc.textOverlay, W, H);
  writeFrame(pixels.data(), W, H);

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
            // GPU pick at click position (flip Y: browser=top-down, GL=bottom-up)
            gpuBufs->uploadDirty();
            int pickY = H - 1 - static_cast<int>(py);
            auto pick = renderer->renderPick(scene, *gpuBufs, W, H,
              static_cast<int>(px), pickY);
            if (pick.drawItemId != 0) {
              // Toggle selection on the picked DrawItem
              dc::SelectionKey key{pick.drawItemId, 0};
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
            syncTouchedBuffersToGpu(touched, ingest, *gpuBufs);
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

        ctx = std::make_unique<dc::OsMesaContext>();
        if (!ctx->init(W, H)) break;

        renderer = std::make_unique<dc::Renderer>();
        renderer->init();

        gpuBufs = std::make_unique<dc::GpuBufferManager>();

        for (auto& [name, hv] : viewports) {
          hv.viewport.setPixelViewport(W, H);
          syncTransform(cp, hv.transformId, hv.viewport);
        }

        syncAllGpuBuffers(doc, ingest, *gpuBufs);
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
        syncTouchedBuffersToGpu(touched, ingest, *gpuBufs);
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
        syncTouchedBuffersToGpu(touched, ingest, *gpuBufs);
        needsRender = true;
      }
    }
    else if (cmd == "clearSelection") {
      selectionState.clear();
      auto touched = binder.onSelectionChanged(selectionState);
      syncTouchedBuffersToGpu(touched, ingest, *gpuBufs);
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
        syncTouchedBuffersToGpu(touched, ingest, *gpuBufs);
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
      syncTouchedBuffersToGpu(touched, ingest, *gpuBufs);
      if (!touched.empty()) needsRender = true;
    }

    if (needsRender) {
      gpuBufs->uploadDirty();
      renderer->render(scene, *gpuBufs, W, H);
      ctx->swapBuffers();
      pixels = ctx->readPixels();
      writeTextOverlay(doc.textOverlay, W, H);
      writeFrame(pixels.data(), W, H);
    }
  }

  return 0;
}
