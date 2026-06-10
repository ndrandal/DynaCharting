// ENC-492 (P2.9) — textSDF@1 + glyph atlas upload on Dawn.
//
// The Dawn counterpart of the GL textSDF path:
//   * d3_3_text_sdf.cpp — the GL textSDF baseline (Renderer::drawTextSdf,
//     kTextSdfVert/kTextSdfFrag + uploadAtlasIfDirty): instanced glyph quads
//     sampling a single-channel R8 SDF glyph atlas; the fragment reconstructs the
//     SDF coverage alpha (smoothstep around the 0.5 isoline) for crisp,
//     anti-aliased text.
//
// Renders, through the backend registry with DeviceKind::Dawn into the headless
// DawnDevice offscreen RGBA8 target, two glyphs ("Hi", a bright text color on a
// cleared black background), and asserts:
//
//   1. ATLAS UPLOAD + DRAW: the GlyphAtlas R8 SDF bitmap is uploaded to a Dawn R8
//      texture (ENC-491 createTexture(R8) + queue.WriteTexture) and sampled in the
//      SDF shader; exactly one instanced draw call is issued (2 glyph instances).
//   2. GLYPH STROKE PIXELS == TEXT COLOR: inside the glyph strokes the SDF alpha
//      is ~1, so (with the Normal alpha blend over black) the readback equals the
//      text color. We scan the readback and require a healthy count of strong
//      text-colored pixels.
//   3. BACKGROUND CLEAR: pixels well outside the glyph quads stay clear (the
//      cleared black background — no spurious coverage).
//   4. ANTI-ALIASED SDF EDGE: the SDF reconstruction produces partial-coverage
//      edge pixels (text color present but not full intensity) — proving the
//      smoothstep edge, not a hard 1-bit cutout.
//
// This also exercises the DawnDevice R8 texture/sampler support (ENC-491):
// createTexture(R8Unorm, TextureBinding|CopyDst) + queue.WriteTexture + a sampler
// + the texture/sampler bind-group entries — now feeding the glyph atlas.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTextSdfBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
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

int main() {
  std::printf("=== D3.3 Dawn textSDF + glyph atlas upload ===\n");

  constexpr std::uint32_t W = 128;
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

  // --- Glyph atlas: load the test font + rasterize ASCII (SDF). -----------
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  if (!atlas.loadFontFile(FONT_PATH)) {
    std::fprintf(stderr, "FAIL: font load (%s)\n", FONT_PATH);
    return 1;
  }
  atlas.ensureAscii();
  check(atlas.useSdf(), "atlas in SDF mode (smoothstep reconstruction)");
  check(atlas.isDirty(), "atlas dirty before first upload");

  // --- Backend + registry (registered ADDITIVELY under DeviceKind::Dawn). --
  dc::DawnTextSdfBackend textSdf(&atlas);
  if (!textSdf.init(dev)) {
    std::fprintf(stderr, "DawnTextSdfBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &textSdf);
  check(backends.find(dc::DeviceKind::Dawn, "textSDF@1") == &textSdf,
        "registry: textSDF@1 -> DawnTextSdfBackend");

  // --- Scene: a single textSDF draw item with a Glyph8 geometry. ----------
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
  // The vertex buffer must exist before the geometry references it.
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "buf");
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"glyph8"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"textSDF@1","geometryId":100})"),
      "bind");
  // Bright cyan text color (R=0,G=1,B=1,A=1): each channel is distinctive vs the
  // black background, so stroke pixels read back ~ (0, 255, 255).
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":1,"a":1})"), "color");

  // --- Build "Hi" glyph instances (Glyph8: x0 y0 x1 y1 u0 v0 u1 v1). ------
  // Same layout math as the GL d3_3_text_sdf baseline.
  const char* text = "Hi";
  const float fontSize = 0.9f;  // clip-space units (large so glyphs cover pixels)
  float cursorX = -0.55f;
  const float baselineY = -0.3f;

  std::vector<float> instances;
  int glyphCount = 0;
  for (const char* p = text; *p; ++p) {
    const dc::GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (!g) continue;
    const float scale = fontSize / 48.0f;
    if (g->w <= 0 || g->h <= 0) {
      cursorX += g->advance * scale;
      continue;
    }
    const float x0 = cursorX + g->bearingX * scale;
    const float y1 = baselineY + g->bearingY * scale;  // top of glyph
    const float y0 = y1 - g->h * scale;                // bottom of glyph
    const float x1 = x0 + g->w * scale;

    instances.push_back(x0);
    instances.push_back(y0);
    instances.push_back(x1);
    instances.push_back(y1);
    instances.push_back(g->u0);
    instances.push_back(g->v0);
    instances.push_back(g->u1);
    instances.push_back(g->v1);
    ++glyphCount;
    cursorX += g->advance * scale;
  }
  std::printf("Built %d glyph instances for \"Hi\"\n", glyphCount);
  check(glyphCount == 2, "2 visible glyphs for 'Hi'");

  store.setCpuData(10, instances.data(),
                   static_cast<std::uint32_t>(instances.size() * sizeof(float)));
  requireOk(cp.applyJsonText(
      std::string(R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":)") +
      std::to_string(glyphCount) + "}"), "setVC");

  // --- Render. ------------------------------------------------------------
  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  dev.beginRenderPass(rp);
  const dc::DrawItem* di = scene.getDrawItem(3);
  dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
  dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                           static_cast<int>(W),
                                           static_cast<int>(H));
  dev.endRenderPass();
  std::printf("Draw calls: %u\n", bs.drawCalls);
  check(bs.drawCalls == 1, "1 instanced draw call for text");
  check(!atlas.isDirty(), "atlas clean after upload (uploadAtlasIfDirty ran)");

  // --- Read back the whole frame and classify pixels. ---------------------
  std::vector<std::uint8_t> px(static_cast<std::size_t>(W) * H * 4, 0);
  for (std::uint32_t y = 0; y < H; ++y) {
    for (std::uint32_t x = 0; x < W; ++x) {
      std::uint8_t rgba[4];
      dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), rgba);
      const std::size_t i = (static_cast<std::size_t>(y) * W + x) * 4;
      px[i + 0] = rgba[0];
      px[i + 1] = rgba[1];
      px[i + 2] = rgba[2];
      px[i + 3] = rgba[3];
    }
  }

  // Text color is (0, 255, 255). A pixel is "stroke" if green+blue are strong and
  // red stays near zero (no text-color contamination of the black background).
  int strongStroke = 0;   // SDF alpha ~1 (interior)
  int edgePartial = 0;    // 0 < SDF alpha < 1 (anti-aliased edge)
  int background = 0;     // clear/near-black
  int contaminated = 0;   // red leaked (would indicate a wrong sample)
  int maxGB = 0, sampleX = -1, sampleY = -1;
  for (std::uint32_t y = 0; y < H; ++y) {
    for (std::uint32_t x = 0; x < W; ++x) {
      const std::size_t i = (static_cast<std::size_t>(y) * W + x) * 4;
      const int r = px[i + 0], g = px[i + 1], b = px[i + 2];
      const int gb = (g < b ? g : b);  // min of the two text channels
      if (r > 40) ++contaminated;
      if (gb >= 200) {
        ++strongStroke;
        if (gb > maxGB) { maxGB = gb; sampleX = static_cast<int>(x); sampleY = static_cast<int>(y); }
      } else if (gb >= 24) {
        ++edgePartial;
      } else {
        ++background;
      }
    }
  }

  std::printf("Pixel classes: strongStroke=%d edgePartial=%d background=%d contaminated=%d\n",
              strongStroke, edgePartial, background, contaminated);
  if (sampleX >= 0) {
    const std::size_t i = (static_cast<std::size_t>(sampleY) * W + sampleX) * 4;
    std::printf("  brightest stroke px (%d,%d) = R=%u G=%u B=%u A=%u (expect ~0,255,255)\n",
                sampleX, sampleY, px[i + 0], px[i + 1], px[i + 2], px[i + 3]);
  }

  // Assertions (cross-checked vs the GL d3_3_text_sdf semantics: visible text
  // pixels in the text color, clear background, SDF-smooth edges).
  check(strongStroke > 20, "glyph stroke pixels are the text color (SDF alpha ~1 inside)");
  if (sampleX >= 0) {
    const std::size_t i = (static_cast<std::size_t>(sampleY) * W + sampleX) * 4;
    check(px[i + 0] < 40 && px[i + 1] > 200 && px[i + 2] > 200,
          "brightest stroke pixel == text color (0,255,255)");
  }
  check(background > strongStroke, "background/outside-glyph pixels are clear");
  check(edgePartial > 5, "SDF edge produces partial-coverage (anti-aliased) pixels");
  check(contaminated == 0, "no red contamination (atlas sampled correctly)");

  std::printf("=== Dawn textSDF: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
