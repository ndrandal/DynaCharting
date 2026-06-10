// ENC-501 (P5 cutover) — Dawn-only golden render harness.
//
// WHAT THIS IS / WHY IT EXISTS
// ----------------------------
// The ENC-510/511/512/500 parity tests originally rendered every scene through
// BOTH the GL backend (Renderer + OsMesaContext + GpuBufferManager) and the Dawn
// backend (DawnSceneRenderer) and compared the two readbacks pixel-by-pixel — the
// GL output was the reference. Dawn is now the proven default renderer and the GL
// backend (dc_gl) is being deleted (ENC-501 Part 2). To preserve the rendering
// regression coverage WITHOUT dc_gl, those tests are converted to render ONLY via
// Dawn and assert the readback against a captured GOLDEN.
//
// The goldens are the SAME Dawn pixels the parity tests validated against GL while
// dc_gl still existed: the parity suite passed (Dawn matched GL within tolerance),
// so the current Dawn output IS the GL-validated reference. We bake a representative
// set of probe pixels (position -> expected RGB) captured from that passing run.
//
// This header is the Dawn-only render core: it builds a Scene from a SceneBuilder
// closure, renders it through DawnSceneRenderer into the headless offscreen target,
// and returns the TOP-LEFT-origin RGBA readback so a test can probe known pixels.
// It deliberately pulls in NO GL headers and links only dc_gpu (no dc_gl / OSMesa).
//
// ORIGIN CONVENTION
// -----------------
// DawnDevice::readPixel is TOP-LEFT origin (row 0 = top). The Dawn backends negate
// clip-space y so a scene lands on-screen the same way GL drew it; with the
// top-left readback that means a clip-space point (x, y) maps to readback row
// (H-1)/2*(1-y)... — i.e. higher clip y => smaller row index (toward the top).
// The d79_dawn_json_host test already established (and documented) that the Dawn
// readback matches GL orientation with NO extra flip, so the golden probe
// coordinates below are just the on-screen pixel positions.
//
// SKIP-GRACEFULLY
// ---------------
// If Dawn can't bring up an adapter the helper returns a SKIPPED result and the
// test exits 0 — matching every other Dawn test's graceful-skip behavior.
#pragma once

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/gpu/DawnTexturedQuadBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace dc {
namespace golden {

// One logical buffer's CPU bytes (a vertex/index/instance buffer the scene's
// geometry references by id), fed to the Dawn CpuBufferStore.
struct BufferData {
  Id id{0};
  std::vector<std::uint8_t> bytes;

  BufferData() = default;
  BufferData(Id i, const void* data, std::size_t n)
      : id(i),
        bytes(reinterpret_cast<const std::uint8_t*>(data),
              reinterpret_cast<const std::uint8_t*>(data) + n) {}
};

// One logical texture supplied to the Dawn texturedQuad backend (the GL side that
// used to mirror these texels into a GL TextureManager is gone — Dawn-only now).
struct TextureInput {
  std::uint32_t id{1};
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::vector<std::uint8_t> rgba;  // tightly-packed RGBA8 (width*height*4)
  bool valid() const {
    return width > 0 && height > 0 &&
           rgba.size() == static_cast<std::size_t>(width) * height * 4;
  }
};

// In-memory TextureSource backing the Dawn texturedQuad backend.
class MemTextureSource final : public TextureSource {
 public:
  void set(const TextureInput& t) { tex_ = t; }
  bool getTexturePixels(std::uint32_t id, const std::uint8_t** outData,
                        std::uint32_t* outW, std::uint32_t* outH,
                        TextureFormat* outFmt) const override {
    if (!tex_.valid() || id != tex_.id) return false;
    *outData = tex_.rgba.data();
    *outW = tex_.width;
    *outH = tex_.height;
    *outFmt = TextureFormat::RGBA8;
    return true;
  }

 private:
  TextureInput tex_;
};

// Build the Scene (identical CommandProcessor JSON the parity tests used) and
// return the per-buffer CPU bytes.
using SceneBuilder =
    std::function<std::vector<BufferData>(CommandProcessor& cp, Scene& scene)>;

// Optional D78 pane border/separator style applied to the Dawn renderer.
struct GoldenStyle {
  bool enabled{false};
  float paneBorderColor[4] = {0, 0, 0, 0};
  float paneBorderWidth{0.0f};
  float separatorColor[4] = {0, 0, 0, 0};
  float separatorWidth{0.0f};
};

struct GoldenFrame {
  bool skipped{false};
  std::string skipReason;
  int width{0};
  int height{0};
  std::string dawnBackend;
  std::vector<std::uint8_t> rgba;  // top-left origin, row-major RGBA, W*H*4

  // RGBA at (x,y) (top-left origin). Out-of-range returns {0,0,0,0}.
  const std::uint8_t* at(int x, int y) const {
    static const std::uint8_t kZero[4] = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= width || y >= height) return kZero;
    return &rgba[(static_cast<std::size_t>(y) * width + x) * 4];
  }
};

// Render `builder` through Dawn into a top-left-origin RGBA readback.
inline GoldenFrame renderDawn(const char* name, const SceneBuilder& builder, int W,
                              int H, GlyphAtlas* atlas = nullptr,
                              const GoldenStyle& style = {},
                              const TextureInput* texture = nullptr) {
  GoldenFrame f;
  f.width = W;
  f.height = H;

  MemTextureSource texSrc;
  const TextureSource* texSrcPtr = nullptr;
  if (texture && texture->valid()) {
    texSrc.set(*texture);
    texSrcPtr = &texSrc;
  }

  DawnSceneRenderer renderer(atlas, texSrcPtr);
  if (!renderer.init()) {
    f.skipped = true;
    f.skipReason = "Dawn adapter unavailable: " + renderer.errorMessage();
    std::printf("[golden %s] SKIP: %s\n", name, f.skipReason.c_str());
    return f;
  }
  f.dawnBackend = renderer.device().backendName();

  if (style.enabled) {
    DawnRenderStyle rs;
    rs.paneBorderColor[0] = style.paneBorderColor[0];
    rs.paneBorderColor[1] = style.paneBorderColor[1];
    rs.paneBorderColor[2] = style.paneBorderColor[2];
    rs.paneBorderColor[3] = style.paneBorderColor[3];
    rs.paneBorderWidth = style.paneBorderWidth;
    rs.separatorColor[0] = style.separatorColor[0];
    rs.separatorColor[1] = style.separatorColor[1];
    rs.separatorColor[2] = style.separatorColor[2];
    rs.separatorColor[3] = style.separatorColor[3];
    rs.separatorWidth = style.separatorWidth;
    renderer.setRenderStyle(rs);
  }

  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  if (atlas) cp.setGlyphAtlas(atlas);

  std::vector<BufferData> buffers = builder(cp, scene);

  CpuBufferStore store;
  for (const auto& b : buffers) {
    store.setCpuData(b.id, b.bytes.data(),
                     static_cast<std::uint32_t>(b.bytes.size()));
  }

  renderer.render(scene, store, W, H);

  f.rgba.assign(static_cast<std::size_t>(W) * H * 4, 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      std::uint8_t px[4] = {0, 0, 0, 0};
      renderer.device().readPixel(x, y, px);
      std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4;
      f.rgba[idx + 0] = px[0];
      f.rgba[idx + 1] = px[1];
      f.rgba[idx + 2] = px[2];
      f.rgba[idx + 3] = px[3];
    }
  }
  return f;
}

// Render the Dawn pick pass and return decoded DrawItem ids at probe pixels.
struct PickProbe {
  int x{0};
  int y{0};
  std::uint32_t expectId{0};
};

struct PickResultRow {
  int x{0}, y{0};
  std::uint32_t expectId{0};
  std::uint32_t gotId{0};
  bool match{false};
};

struct PickFrame {
  bool skipped{false};
  std::string skipReason;
  std::string dawnBackend;
  std::vector<PickResultRow> rows;
};

inline PickFrame pickDawn(const char* name, const SceneBuilder& builder, int W,
                          int H, const std::vector<PickProbe>& probes) {
  PickFrame f;
  DawnSceneRenderer renderer(nullptr, nullptr);
  if (!renderer.init()) {
    f.skipped = true;
    f.skipReason = "Dawn adapter unavailable: " + renderer.errorMessage();
    std::printf("[golden-pick %s] SKIP: %s\n", name, f.skipReason.c_str());
    return f;
  }
  f.dawnBackend = renderer.device().backendName();

  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  std::vector<BufferData> buffers = builder(cp, scene);
  CpuBufferStore store;
  for (const auto& b : buffers)
    store.setCpuData(b.id, b.bytes.data(),
                     static_cast<std::uint32_t>(b.bytes.size()));

  for (const auto& p : probes) {
    DawnPickResult pr = renderer.renderPick(scene, store, W, H, p.x, p.y);
    PickResultRow row;
    row.x = p.x;
    row.y = p.y;
    row.expectId = p.expectId;
    row.gotId = pr.drawItemId;
    row.match = (row.gotId == row.expectId);
    f.rows.push_back(row);
  }
  return f;
}

}  // namespace golden
}  // namespace dc
