// ENC-491 (P2.8) — texturedQuad@1 + texture/sampler support on Dawn.
//
// The Dawn counterpart of the GL textured-quad paths:
//   * d36_1_texture_quad.cpp / d41_1_texquad_render.cpp — the GL texturedQuad
//     baselines (Renderer::drawTexturedQuad, kTexQuadVert/kTexQuadFrag): an
//     instanced unit quad sampling a 2D texture at interpolated UVs, modulated by
//     the per-DrawItem color.
//
// Renders, through the backend registry with DeviceKind::Dawn into the headless
// DawnDevice offscreen RGBA8 target:
//
//   1. SAMPLED TEXELS: a full-viewport quad sampling a 2x2 texture with four
//      DISTINCT colored texels (R, G, B, Y). With CLAMP_TO_EDGE + linear
//      filtering on a 2x2 image, the four screen corners sample the four corner
//      texels. The WGSL negates clip-y (top-left framebuffer), so uv.y=0
//      (clip-bottom) lands at framebuffer-TOP — the corner->texel mapping is
//      checked accordingly. White color modulation => texels pass through.
//
//   2. COLOR MODULATION: the same texture with a half-green color
//      (u_color = (1, 0.5, 0, 1)). A texel that is pure white reads back the
//      modulated color (texel * u_color), proving fs out = texel * u_color.
//
//   3. INDEXED GATHER (D26): four quadrant quads with an index buffer selecting a
//      diagonal pair; only the two selected quadrants are textured, the others
//      stay clear — proving the CPU-side gather draws exactly the selected subset
//      through the shared drawInstanced foundation (ENC-488).
//
// This also exercises the new DawnDevice texture/sampler support (ENC-491):
// createTexture (RGBA8Unorm, TextureBinding|CopyDst) + queue.WriteTexture upload
// + a sampler + the texture/sampler bind-group entries.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTexturedQuadBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx,
                 r.err.code.c_str(), r.err.message.c_str());
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

// In-memory TextureSource: maps a textureId -> tightly-packed RGBA8 pixels. The
// Dawn backend uploads these into a wgpu::Texture on first use.
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
  std::printf("=== D36.1 Dawn texturedQuad + texture/sampler ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;

  // --- Bring up the headless Dawn device. ---------------------------------
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

  // 2x2 texture, four DISTINCT corner texels. Row-major from texel (col,row):
  //   texel (0,0) = RED    texel (1,0) = GREEN
  //   texel (0,1) = BLUE   texel (1,1) = WHITE
  // Texel rows are bottom-up in UV terms here only as a label; the actual UV ->
  // texel mapping is what we assert against (with the WGSL y-flip accounted for).
  MemTextureSource texSrc;
  {
    auto rgba = [](std::uint8_t r, std::uint8_t g, std::uint8_t b) {
      return std::vector<std::uint8_t>{r, g, b, 255};
    };
    std::vector<std::uint8_t> px;
    auto push = [&](std::vector<std::uint8_t> c) {
      px.insert(px.end(), c.begin(), c.end());
    };
    // WriteTexture row order: row 0 first. Texture rows map to UV v via the
    // texture coordinate; row 0 == v near 0, row 1 == v near 1.
    // row 0 (v~0): RED, GREEN
    push(rgba(255, 0, 0));  // (col0,row0) RED
    push(rgba(0, 255, 0));  // (col1,row0) GREEN
    // row 1 (v~1): BLUE, WHITE
    push(rgba(0, 0, 255));  // (col0,row1) BLUE
    push(rgba(255, 255, 255));  // (col1,row1) WHITE
    texSrc.set(1, std::move(px), 2, 2);
  }

  // Backend + registry (registered ADDITIVELY under DeviceKind::Dawn).
  dc::DawnTexturedQuadBackend texQuad(&texSrc);
  if (!texQuad.init(dev)) {
    std::fprintf(stderr, "DawnTexturedQuadBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &texQuad);
  check(backends.find(dc::DeviceKind::Dawn, "texturedQuad@1") == &texQuad,
        "registry: texturedQuad@1 -> DawnTexturedQuadBackend");

  auto makeRp = [&]() {
    dc::RenderPassDesc rp;
    rp.target = {};
    rp.viewportWidth = W;
    rp.viewportHeight = H;
    rp.clear = true;
    rp.clearColor[0] = 0.0f;
    rp.clearColor[1] = 0.0f;
    rp.clearColor[2] = 0.0f;
    rp.clearColor[3] = 1.0f;
    return rp;
  };

  auto renderDi = [&](dc::Scene& scene, dc::CpuBufferStore& store,
                      std::uint32_t diId) -> std::uint32_t {
    const dc::DrawItem* di = scene.getDrawItem(diId);
    if (!di) return 0;
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
    if (!be) return 0;
    dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                             static_cast<int>(W),
                                             static_cast<int>(H));
    return bs.drawCalls;
  };

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };

  // =====================================================================
  // Case 1: sampled texels — four corners read back the four corner texels.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // Full-viewport quad in clip space (x0,y0,x1,y1) = (-1,-1, 1,1).
    float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    store.setCpuData(10, quad, sizeof(quad));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"pos2_uv4"})"),
        "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"texturedQuad@1","geometryId":100})"),
        "bind");
    // White color: texel passes through unmodified.
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})"), "color");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemTexture","drawItemId":3,"textureId":1})"), "tex");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 3);
    dev.endRenderPass();
    check(calls == 1, "sampled: 1 draw call");

    // UV->screen mapping (with the WGSL clip-y negation):
    //   uv=(0,0) [clip bottom-left] -> framebuffer TOP-left      -> texel(0,row0)=RED
    //   uv=(1,0) [clip bottom-right]-> framebuffer TOP-right     -> texel(1,row0)=GREEN
    //   uv=(0,1) [clip top-left]    -> framebuffer BOTTOM-left   -> texel(0,row1)=BLUE
    //   uv=(1,1) [clip top-right]   -> framebuffer BOTTOM-right  -> texel(1,row1)=WHITE
    // Sample a few px in from each corner so linear+clamp lands on the edge texel.
    std::uint8_t tl[4], tr[4], bl[4], br[4];
    px(3, 3, tl);            // top-left
    px(W - 4, 3, tr);        // top-right
    px(3, H - 4, bl);        // bottom-left
    px(W - 4, H - 4, br);    // bottom-right
    std::printf("  corner TL R=%u G=%u B=%u A=%u (expect RED)\n",   tl[0], tl[1], tl[2], tl[3]);
    std::printf("  corner TR R=%u G=%u B=%u A=%u (expect GREEN)\n", tr[0], tr[1], tr[2], tr[3]);
    std::printf("  corner BL R=%u G=%u B=%u A=%u (expect BLUE)\n",  bl[0], bl[1], bl[2], bl[3]);
    std::printf("  corner BR R=%u G=%u B=%u A=%u (expect WHITE)\n", br[0], br[1], br[2], br[3]);

    check(tl[0] > 220 && tl[1] < 40 && tl[2] < 40, "sampled TL == RED texel");
    check(tr[1] > 220 && tr[0] < 40 && tr[2] < 40, "sampled TR == GREEN texel");
    check(bl[2] > 220 && bl[0] < 40 && bl[1] < 40, "sampled BL == BLUE texel");
    check(br[0] > 220 && br[1] > 220 && br[2] > 220, "sampled BR == WHITE texel");
  }

  // =====================================================================
  // Case 2: color modulation — white texel * half-color reads modulated.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    // A 1x1 pure-white texture so every sampled texel == white; the readback
    // therefore equals u_color * white == u_color.
    {
      std::vector<std::uint8_t> wpx = {255, 255, 255, 255};
      texSrc.set(2, std::move(wpx), 1, 1);
    }

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":16})"), "buf2");
    store.setCpuData(30, quad, sizeof(quad));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":1,"format":"pos2_uv4"})"),
        "geom2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"texturedQuad@1","geometryId":200})"),
        "bind2");
    // Half-green modulation: u_color = (1, 0.5, 0, 1). texel(white)*color = color.
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":22,"r":1,"g":0.5,"b":0,"a":1})"), "color2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemTexture","drawItemId":22,"textureId":2})"), "tex2");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 22);
    dev.endRenderPass();
    check(calls == 1, "modulate: 1 draw call");

    std::uint8_t cen[4];
    px(W / 2, H / 2, cen);
    std::printf("  modulated center R=%u G=%u B=%u A=%u (expect ~255,128,0)\n",
                cen[0], cen[1], cen[2], cen[3]);
    check(cen[0] > 240, "modulate: R full (1.0 * white)");
    check(cen[1] > 110 && cen[1] < 150, "modulate: G ~half (0.5 * white)");
    check(cen[2] < 24, "modulate: B zero (0.0 * white)");
  }

  // =====================================================================
  // Case 3: D26 indexed gather — texture a selected diagonal pair of quadrants.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":40,"name":"P3"})"), "pane3");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":41,"paneId":40})"), "layer3");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":42,"layerId":41})"), "di3");

    // Four quadrant quads (clip space).
    float quads[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,  // quad0: clip bottom-left
       0.0f, -1.0f, 1.0f, 0.0f,  // quad1: clip bottom-right
      -1.0f,  0.0f, 0.0f, 1.0f,  // quad2: clip top-left
       0.0f,  0.0f, 1.0f, 1.0f,  // quad3: clip top-right
    };
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":50,"byteLength":64})"), "vbuf3");
    store.setCpuData(50, quads, sizeof(quads));

    // Index buffer selects quad0 + quad3 (the diagonal pair).
    std::uint32_t indices[] = {0, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":51,"byteLength":8})"), "ibuf3");
    store.setCpuData(51, indices, sizeof(indices));

    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":300,"vertexBufferId":50,"vertexCount":4,"format":"pos2_uv4","indexBufferId":51,"indexCount":2})"),
        "geom3");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"texturedQuad@1","geometryId":300})"),
        "bind3");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":42,"r":1,"g":1,"b":1,"a":1})"), "color3");
    // Reuse the 1x1 white texture (id 2) so any covered pixel reads near-white.
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemTexture","drawItemId":42,"textureId":2})"), "tex3");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 42);
    dev.endRenderPass();
    check(calls == 1, "indexed: 1 draw call (2 instances)");

    // After the WGSL y-flip, clip-bottom lands at framebuffer-top:
    //   quad0 (clip bottom-left)  -> framebuffer TOP-left      (selected)
    //   quad3 (clip top-right)    -> framebuffer BOTTOM-right   (selected)
    //   quad1 (clip bottom-right) -> framebuffer TOP-right      (NOT selected)
    //   quad2 (clip top-left)     -> framebuffer BOTTOM-left    (NOT selected)
    std::uint8_t sel0[4], sel3[4], skip1[4], skip2[4];
    px(W / 4, H / 4, sel0);
    px(3 * W / 4, 3 * H / 4, sel3);
    px(3 * W / 4, H / 4, skip1);
    px(W / 4, 3 * H / 4, skip2);
    std::printf("  indexed sel q0(TL)  R=%u G=%u B=%u A=%u\n", sel0[0], sel0[1], sel0[2], sel0[3]);
    std::printf("  indexed sel q3(BR)  R=%u G=%u B=%u A=%u\n", sel3[0], sel3[1], sel3[2], sel3[3]);
    std::printf("  indexed skip q1(TR) R=%u G=%u B=%u A=%u\n", skip1[0], skip1[1], skip1[2], skip1[3]);
    std::printf("  indexed skip q2(BL) R=%u G=%u B=%u A=%u\n", skip2[0], skip2[1], skip2[2], skip2[3]);

    check(sel0[0] > 220 && sel0[1] > 220 && sel0[2] > 220, "indexed: quad0 textured -> white");
    check(sel3[0] > 220 && sel3[1] > 220 && sel3[2] > 220, "indexed: quad3 textured -> white");
    check(skip1[0] < 24 && skip1[1] < 24 && skip1[2] < 24, "indexed: quad1 filtered -> clear");
    check(skip2[0] < 24 && skip2[1] < 24 && skip2[2] < 24, "indexed: quad2 filtered -> clear");
  }

  std::printf("=== Dawn texturedQuad: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
