// ENC-612 (P2) — 256x1 COLOR-LUT TEXTURE on Dawn: a texturedQuad samples a 256x1
// color-ramp LUT in-shader, producing a SMOOTH per-pixel gradient straight from a
// ColorScale ramp (RESEARCH §4.2 last paragraph: "per-pixel color uses a 256x1
// LUT texture sampled in-shader, not per-row").
//
// THE PROOF
// ---------
// Build a 256x1 RGBA8 LUT from ColorRamp::blueRed() (t=0 pure blue -> t=1 pure
// red) via buildColorLut() — the byte-exact LUT bytes (proven in the fast suite
// test dc_enc612_color_lut). Upload it as a 256x1 RGBA8 texture with LINEAR
// filtering. Render a FULL-VIEWPORT texturedQuad whose horizontal uv runs 0..1
// left->right, so the fragment samples the LUT column under each pixel: a smooth
// blue->red gradient across the framebuffer.
//
// ASSERT: left edge ~blue, right edge ~red, the gradient is MONOTONE (R rises and
// B falls left->right), and intermediate columns are genuine blends (NOT a flat
// color, NOT a hard 2-color split) — i.e. the LUT is sampled CONTINUOUSLY.
//
// NDC: the WGSL negates clip.y (top-left framebuffer), but X is NOT flipped — so
// the horizontal gradient direction (uv.x) is the SAME as framebuffer x. A
// full-viewport quad's uv.x == 0 at the left framebuffer edge, == 1 at the right.
//
// On this headless box force lavapipe (software Vulkan):
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTexturedQuadBackend.hpp"
#include "dc/scale/ColorLut.hpp"
#include "dc/scale/ColorScale.hpp"

#include "dc/commands/CommandProcessor.hpp"
#include "dc/render/BackendRegistry.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"

#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx, r.err.code.c_str(),
                 r.err.message.c_str());
    std::exit(1);
  }
}

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

// In-memory TextureSource: textureId -> tightly-packed RGBA8 pixels.
class MemTextureSource final : public dc::TextureSource {
 public:
  struct Tex {
    std::vector<std::uint8_t> rgba;
    std::uint32_t w{0};
    std::uint32_t h{0};
  };
  void set(std::uint32_t id, std::vector<std::uint8_t> rgba, std::uint32_t w,
           std::uint32_t h) {
    textures_[id] = Tex{std::move(rgba), w, h};
  }
  bool getTexturePixels(std::uint32_t id, const std::uint8_t** outData,
                        std::uint32_t* outW, std::uint32_t* outH,
                        dc::TextureFormat* outFmt) const override {
    auto it = textures_.find(id);
    if (it == textures_.end()) return false;
    *outData = it->second.rgba.data();
    *outW = it->second.w;
    *outH = it->second.h;
    *outFmt = dc::TextureFormat::RGBA8;
    return true;
  }

 private:
  std::unordered_map<std::uint32_t, Tex> textures_;
};

int main() {
  std::printf("=== ENC-612 Dawn color-LUT (256x1 ramp -> smooth gradient) ===\n");

  constexpr std::uint32_t W = 128;
  constexpr std::uint32_t H = 16;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: set "
                 "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "to force lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // Build the 256x1 LUT from a blue->red ramp (byte-exact per dc_enc612_color_lut).
  std::vector<std::uint8_t> lut = dc::buildColorLut(dc::ColorRamp::blueRed());
  check(lut.size() == 1024u, "lut: 256x1 RGBA8 == 1024 bytes");

  MemTextureSource texSrc;
  texSrc.set(1, std::move(lut), dc::kColorLutWidth, 1);

  dc::DawnTexturedQuadBackend texQuad(&texSrc);
  if (!texQuad.init(dev)) {
    std::fprintf(stderr, "DawnTexturedQuadBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &texQuad);

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

  // Full-viewport quad in clip space (x0,y0,x1,y1)=(-1,-1,1,1). uv.x runs 0..1
  // left->right (X is not flipped), so each column samples the LUT at u=x/W.
  float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"),
            "buf");
  store.setCpuData(10, quad, sizeof(quad));
  requireOk(cp.applyJsonText(
                R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"pos2_uv4"})"),
            "geom");
  requireOk(cp.applyJsonText(
                R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"texturedQuad@1","geometryId":100})"),
            "bind");
  // White modulation: the LUT texel passes through unmodified.
  requireOk(cp.applyJsonText(
                R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})"),
            "color");
  requireOk(cp.applyJsonText(
                R"({"cmd":"setDrawItemTexture","drawItemId":3,"textureId":1})"),
            "tex");

  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = rp.clearColor[1] = rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  dev.beginRenderPass(rp);
  const dc::DrawItem* di = scene.getDrawItem(3);
  dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
  dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                           static_cast<int>(W),
                                           static_cast<int>(H));
  dev.endRenderPass();
  check(bs.drawCalls == 1, "lut: 1 textured draw call");

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };

  // Dump an ASCII grid of the gradient (R/B dominance per column) to establish
  // ground truth — one mid row sampled across the width.
  std::printf("  gradient (one row, B=blue-dominant '.', R=red-dominant '#'):\n  ");
  for (std::uint32_t x = 0; x < W; x += 4) {
    std::uint8_t c[4];
    px(x, H / 2, c);
    std::putchar(c[0] > c[2] ? '#' : '.');
  }
  std::putchar('\n');

  // Sample a few columns left->right at the mid row.
  std::uint8_t left[4], q1[4], mid[4], q3[4], right[4];
  px(1, H / 2, left);            // far left  -> ~blue
  px(W / 4, H / 2, q1);          // quarter
  px(W / 2, H / 2, mid);         // center    -> ~purple/blend
  px(3 * W / 4, H / 2, q3);      // 3/4
  px(W - 2, H / 2, right);       // far right -> ~red
  std::printf("  left  R=%u G=%u B=%u\n", left[0], left[1], left[2]);
  std::printf("  q1    R=%u G=%u B=%u\n", q1[0], q1[1], q1[2]);
  std::printf("  mid   R=%u G=%u B=%u\n", mid[0], mid[1], mid[2]);
  std::printf("  q3    R=%u G=%u B=%u\n", q3[0], q3[1], q3[2]);
  std::printf("  right R=%u G=%u B=%u\n", right[0], right[1], right[2]);

  // Endpoints: left edge ~blue (B high, R low); right edge ~red (R high, B low).
  check(left[2] > 200 && left[0] < 60, "lut: left edge ~BLUE (ramp t=0)");
  check(right[0] > 200 && right[2] < 60, "lut: right edge ~RED (ramp t=1)");

  // Continuous blend at the center: NOT pure blue, NOT pure red — both channels
  // present (the LUT is sampled per-pixel, the whole point of §4.2).
  check(mid[0] > 40 && mid[2] > 40,
        "lut: center is a genuine BLEND (per-pixel sampling, not 2-color split)");

  // Monotone gradient: R rises and B falls left -> right (a smooth ramp).
  check(left[0] < q1[0] && q1[0] < mid[0] && mid[0] < q3[0] && q3[0] < right[0],
        "lut: R channel rises monotonically left->right");
  check(left[2] > q1[2] && q1[2] > mid[2] && mid[2] > q3[2] && q3[2] > right[2],
        "lut: B channel falls monotonically left->right");

  // Non-blank: at least one fully-saturated column exists.
  check(left[2] + right[0] > 400, "lut: gradient is non-blank");

  std::printf("=== ENC-612 Dawn color-LUT: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
