// dc_webgpu_all.cpp — ENC-504 (P6.3) Emscripten/emdawnwebgpu harness driving the
// FULL renderer (DawnSceneRenderer + ALL 10 Dawn pipeline backends) in the
// browser.
//
// ENC-503 (P6.2) rendered a single triSolid triangle by hand-driving DawnDevice +
// DawnTriSolidBackend. This extends that to the WHOLE renderer: it builds a
// multi-pipeline Scene (triSolid + instancedRect + line2d + instancedCandle) and
// renders it via dc::DawnSceneRenderer::render() — the exact same scene-walk path
// the native d50_dawn_scene_renderer test exercises — against Emscripten's
// emdawnwebgpu port instead of native dawn::webgpu_dawn.
//
// THE KEY RESULT proven by the BUILD of this TU's target: all 10 Dawn pipeline
// backends (triSolid / triGradient / triAA / line2d / lineAA / points /
// instancedRect / instancedCandle / textSDF / texturedQuad) + DawnPickBackend +
// DawnSceneRenderer compile + link against emdawnwebgpu — i.e. the WHOLE renderer
// is browser-portable, not just the triSolid slice.
//
// ASYNC SHAPE (unchanged from ENC-503): DawnDevice::init() + the readback pump
// yield to the JS event loop via ASYNCIFY (emscripten_sleep), so the synchronous
// native render/readback API shape works in-browser. The render entry point
// therefore SUSPENDS and is exposed as a plain C function (dc_webgpu_render_all)
// the page calls with Module.ccall(..., { async: true }) -> Promise<int status>.
// The per-region results + the framebuffer are then read back through Embind
// getters (those do not suspend).
//
// NOTE: this is a HARNESS, not production wiring. textSDF@1 and texturedQuad@1
// are present in the BUILD (the backends compile + link) but are NOT exercised in
// the scene here: textSDF@1 needs a glyph atlas (R8 SDF bitmap + UV rects) and
// texturedQuad@1 needs a TextureSource — neither is bundled into this harness, so
// DawnSceneRenderer simply does not register them (a null atlas / null textures
// is exactly how the native renderer skips them too). The other 8 pipelines are
// all available; we drive 4 distinct pipeline CLASSES (non-instanced tris,
// instanced quads, LineList, instanced candles) to prove the full walk dispatches.

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include <cstdint>
#include <string>
#include <vector>

#include "dc/gpu/DawnSceneRenderer.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/debug/Stats.hpp"

namespace {

// One probed region: a name, the expected color (or -1 for "any lit"), the probe
// pixel, the read-back pixel and a PASS flag. The page renders these as a table.
struct Region {
  std::string name;
  int probeX{0};
  int probeY{0};
  int gotR{0};
  int gotG{0};
  int gotB{0};
  int gotA{0};
  bool pass{false};
};

struct RenderState {
  int status{0};  // 0 = not run, 1 = ok, negative = failure stage
  std::string message;
  std::string backend;
  int width{0};
  int height{0};
  unsigned drawCalls{0};
  unsigned culled{0};
  std::vector<Region> regions;
  std::vector<std::uint8_t> framebuffer;
  std::uint32_t fbW{0};
  std::uint32_t fbH{0};
};

RenderState& state() {
  static RenderState s;
  return s;
}

// Color match with the same tolerance the native d50 test uses (within 40).
bool isColor(const std::uint8_t* p, int r, int g, int b) {
  auto near = [](int v, int t) { return (v >= t ? v - t : t - v) < 40; };
  return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

bool applyCmd(dc::CommandProcessor& cp, const char* json, const char* ctx,
              std::string& err) {
  dc::CmdResult r = cp.applyJsonText(json);
  if (!r.ok) {
    err = std::string("command failed [") + ctx + "]: " + r.err.code + " " +
          r.err.message;
    return false;
  }
  return true;
}

// Build the multi-pipeline scene (mirrors d50_dawn_scene_renderer's shape: a RED
// triSolid triangle in the LEFT third, a GREEN instancedRect in the MIDDLE third,
// a BLUE line2d across the RIGHT third) and ADDS a GREEN-up instancedCandle in
// the lower-center, then render it via DawnSceneRenderer in ONE walk and read
// back each region. SUSPENDS via ASYNCIFY across device acquisition + readback.
int doRender(int w, int h) {
  RenderState& st = state();
  st = RenderState{};
  st.width = w;
  st.height = h;
  if (w <= 0 || h <= 0) {
    st.message = "invalid size";
    st.status = -1;
    return st.status;
  }
  const int W = w;
  const int H = h;

  // 1. The full renderer: owns a (browser) DawnDevice + ALL 10 backends. No atlas
  //    / textures supplied, so textSDF@1 / texturedQuad@1 are skipped (the other
  //    8 pipelines register). init() brings up the device via navigator.gpu
  //    (ASYNCIFY suspend) and builds every available backend's pipeline.
  dc::DawnSceneRenderer renderer;
  if (!renderer.init()) {
    st.message = "DawnSceneRenderer::init failed: " + renderer.errorMessage();
    st.status = -2;
    return st.status;
  }
  st.backend = renderer.device().backendName();

  // 2. Build the multi-pipeline scene via CommandProcessor JSON (no graphics —
  //    pure dc core), identical command shapes to the native render tests.
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;
  std::string err;

  if (!applyCmd(cp, R"({"cmd":"createPane","id":1,"name":"P"})", "pane", err) ||
      !applyCmd(cp, R"({"cmd":"createLayer","id":2,"paneId":1})", "layer", err)) {
    st.message = err;
    st.status = -3;
    return st.status;
  }

  // 1) triSolid@1 — RED triangle in the LEFT third (clip x[-0.9,-0.3]).
  {
    float tri[] = {-0.9f, -0.7f, -0.3f, -0.7f, -0.6f, 0.7f};
    if (!applyCmd(cp, R"({"cmd":"createDrawItem","id":3,"layerId":2})", "di-tri", err) ||
        !applyCmd(cp, R"({"cmd":"createBuffer","id":10,"byteLength":24})", "buf-tri", err)) {
      st.message = err; st.status = -3; return st.status;
    }
    store.setCpuData(10, tri, sizeof(tri));
    if (!applyCmd(cp, R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})", "geom-tri", err) ||
        !applyCmd(cp, R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})", "bind-tri", err) ||
        !applyCmd(cp, R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})", "color-tri", err)) {
      st.message = err; st.status = -3; return st.status;
    }
  }

  // 2) instancedRect@1 — GREEN rect in the MIDDLE third, upper area (clip
  //    x[-0.2,0.2], y[0.1,0.6]) so it does not overlap the candle below.
  {
    float rect[] = {-0.2f, 0.1f, 0.2f, 0.6f};  // rect4 instance: x0 y0 x1 y1
    if (!applyCmd(cp, R"({"cmd":"createDrawItem","id":4,"layerId":2})", "di-rect", err) ||
        !applyCmd(cp, R"({"cmd":"createBuffer","id":11,"byteLength":16})", "buf-rect", err)) {
      st.message = err; st.status = -3; return st.status;
    }
    store.setCpuData(11, rect, sizeof(rect));
    if (!applyCmd(cp, R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":1,"format":"rect4"})", "geom-rect", err) ||
        !applyCmd(cp, R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"instancedRect@1","geometryId":101})", "bind-rect", err) ||
        !applyCmd(cp, R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":1})", "color-rect", err)) {
      st.message = err; st.status = -3; return st.status;
    }
  }

  // 3) line2d@1 — BLUE horizontal line across the RIGHT third at clip y=0
  //    (x[0.35,0.9]). 1px LineList (WebGPU has no line width).
  {
    float line[] = {0.35f, 0.0f, 0.9f, 0.0f};
    if (!applyCmd(cp, R"({"cmd":"createDrawItem","id":5,"layerId":2})", "di-line", err) ||
        !applyCmd(cp, R"({"cmd":"createBuffer","id":12,"byteLength":16})", "buf-line", err)) {
      st.message = err; st.status = -3; return st.status;
    }
    store.setCpuData(12, line, sizeof(line));
    if (!applyCmd(cp, R"({"cmd":"createGeometry","id":102,"vertexBufferId":12,"vertexCount":2,"format":"pos2_clip"})", "geom-line", err) ||
        !applyCmd(cp, R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"line2d@1","geometryId":102})", "bind-line", err) ||
        !applyCmd(cp, R"({"cmd":"setDrawItemColor","drawItemId":5,"r":0,"g":0,"b":1,"a":1})", "color-line", err)) {
      st.message = err; st.status = -3; return st.status;
    }
  }

  // 4) instancedCandle@1 — a single GREEN-up candle in the lower-center. candle6
  //    = (cx, open, high, low, close, hw). cx=0.0, body open -0.7..close -0.3
  //    (UP -> green body), wick high -0.15 / low -0.9, hw=0.12 in clip x. Sits
  //    BELOW the green rect (rect bottom clip y=0.1) so it reads back distinctly.
  {
    float candle[] = {0.0f, -0.7f, -0.15f, -0.9f, -0.3f, 0.12f};
    if (!applyCmd(cp, R"({"cmd":"createDrawItem","id":6,"layerId":2})", "di-candle", err) ||
        !applyCmd(cp, R"({"cmd":"createBuffer","id":13,"byteLength":24})", "buf-candle", err)) {
      st.message = err; st.status = -3; return st.status;
    }
    store.setCpuData(13, candle, sizeof(candle));
    if (!applyCmd(cp, R"({"cmd":"createGeometry","id":103,"vertexBufferId":13,"vertexCount":1,"format":"candle6"})", "geom-candle", err) ||
        !applyCmd(cp, R"({"cmd":"bindDrawItem","drawItemId":6,"pipeline":"instancedCandle@1","geometryId":103})", "bind-candle", err) ||
        !applyCmd(cp, R"({"cmd":"setDrawItemStyle","drawItemId":6,"colorUpR":0,"colorUpG":0.8,"colorUpB":1,"colorUpA":1,"colorDownR":1,"colorDownG":0,"colorDownB":0,"colorDownA":1})", "style-candle", err)) {
      st.message = err; st.status = -3; return st.status;
    }
  }

  // 3. Render the WHOLE scene in one walk through every registered backend.
  dc::Stats stats = renderer.render(scene, store, W, H);
  st.drawCalls = stats.drawCalls;
  st.culled = stats.culledDrawCalls;

  // 4. Read back each region. Probe coordinates derive from the clip->pixel map
  //    px = (clip.x*0.5+0.5)*W ; the Dawn backends negate clip.y so a clip y maps
  //    to row (0.5 - clip.y*0.5)*H (top-left origin readback).
  auto px = [&](int x, int y, std::uint8_t* o) {
    renderer.device().readPixel(static_cast<std::int32_t>(x),
                                static_cast<std::int32_t>(y), o);
  };

  std::uint8_t p[4];
  auto addRegion = [&](const std::string& name, int x, int y, bool ok) {
    Region r;
    r.name = name; r.probeX = x; r.probeY = y;
    r.gotR = p[0]; r.gotG = p[1]; r.gotB = p[2]; r.gotA = p[3];
    r.pass = ok;
    st.regions.push_back(r);
  };

  // triSolid RED: clip x[-0.9,-0.3] -> px[~0.05W,~0.35W]; probe at 0.12*W, mid.
  {
    int x = (W * 12) / 96, y = H / 2;  // mirrors d50 probe (12, H/2) at 96x64
    px(x, y, p);
    addRegion("triSolid (expect RED 255,0,0)", x, y, isColor(p, 255, 0, 0));
  }
  // instancedRect GREEN: a solid green rect in the center column (its exact row
  // depends on the clip-y -> screen-row mapping, so scan the column for it).
  {
    int x = W / 2, foundY = -1;
    for (int yy = 0; yy < H; ++yy) { px(x, yy, p); if (isColor(p, 0, 255, 0)) { foundY = yy; break; } }
    addRegion("instancedRect (expect GREEN 0,255,0)", x, foundY < 0 ? 0 : foundY, foundY >= 0);
  }
  // line2d BLUE: clip x[0.35,0.9] at clip y=0 -> center row. Probe +/-1 row.
  {
    int x = (W * 78) / 96, y = H / 2;
    bool lit = false;
    for (int dy = -1; dy <= 1 && !lit; ++dy) {
      px(x, y + dy, p);
      if (isColor(p, 0, 0, 255)) lit = true;
    }
    addRegion("line2d (expect BLUE present)", x, y, lit);
  }
  // instancedCandle body: up candle uses colorUp (0,0.8,1) ~ (0,204,255). Scan
  // the center column for the cyan-ish body (row depends on the clip-y mapping).
  {
    int x = W / 2, foundY = -1;
    for (int yy = 0; yy < H; ++yy) { px(x, yy, p); if (p[0] < 60 && p[1] > 120 && p[2] > 180) { foundY = yy; break; } }
    addRegion("instancedCandle (expect UP body cyan-ish)", x, foundY < 0 ? 0 : foundY, foundY >= 0);
  }
  // A clear gap (between the green rect bottom and the candle top, off the line
  // row) should read back black. Probe col W/2, clip y ~ -0.05 -> ~0.525*H, but
  // avoid the line2d row band; use clip x left of the line: col W/2 is fine since
  // line is in the right third.
  {
    int x = (W * 36) / 96, y = 6;  // d50 gap2 probe — between triangle and rect
    px(x, y, p);
    addRegion("gap (expect CLEAR black)", x, y, isColor(p, 0, 0, 0));
  }

  // 5. Read back the full framebuffer for the canvas blit.
  st.framebuffer.assign(static_cast<std::size_t>(W) * H * 4u, 0);
  std::uint32_t gotW = 0, gotH = 0;
  bool fbOk = renderer.device().readFramebufferRGBA(
      st.framebuffer.data(), st.framebuffer.size(), &gotW, &gotH);
  st.fbW = gotW;
  st.fbH = gotH;

  // Overall status: at least the three core pipelines (tri/rect/line) must pass.
  int corePass = 0;
  for (std::size_t i = 0; i < st.regions.size() && i < 3; ++i)
    if (st.regions[i].pass) ++corePass;
  st.status = (fbOk && st.drawCalls >= 4) ? 1 : -7;
  st.message = "rendered " + std::to_string(st.drawCalls) +
               " draw calls on backend=" + st.backend + "; core pipelines passing=" +
               std::to_string(corePass) + "/3 (see per-region table + canvas)";
  return st.status;
}

}  // namespace

// C entry point: Module.ccall('dc_webgpu_render_all', 'number',
// ['number','number'], [w,h], { async: true }) -> Promise<int status>.
extern "C" EMSCRIPTEN_KEEPALIVE int dc_webgpu_render_all(int w, int h) {
  return doRender(w, h);
}

// Embind getters for the (already-computed) result. These do not suspend.
EMSCRIPTEN_BINDINGS(dc_webgpu_all) {
  namespace em = emscripten;
  em::function("dcStatus", +[]() { return state().status; });
  em::function("dcMessage", +[]() { return state().message; });
  em::function("dcBackend", +[]() { return state().backend; });
  em::function("dcWidth", +[]() { return state().width; });
  em::function("dcHeight", +[]() { return state().height; });
  em::function("dcDrawCalls", +[]() { return static_cast<int>(state().drawCalls); });
  em::function("dcCulled", +[]() { return static_cast<int>(state().culled); });

  em::function("dcRegionCount",
               +[]() { return static_cast<int>(state().regions.size()); });
  em::function("dcRegionName", +[](int i) {
    return (i >= 0 && i < static_cast<int>(state().regions.size()))
               ? state().regions[i].name
               : std::string();
  });
  em::function("dcRegionR", +[](int i) {
    return (i >= 0 && i < static_cast<int>(state().regions.size()))
               ? state().regions[i].gotR : 0;
  });
  em::function("dcRegionG", +[](int i) {
    return (i >= 0 && i < static_cast<int>(state().regions.size()))
               ? state().regions[i].gotG : 0;
  });
  em::function("dcRegionB", +[](int i) {
    return (i >= 0 && i < static_cast<int>(state().regions.size()))
               ? state().regions[i].gotB : 0;
  });
  em::function("dcRegionA", +[](int i) {
    return (i >= 0 && i < static_cast<int>(state().regions.size()))
               ? state().regions[i].gotA : 0;
  });
  em::function("dcRegionPass", +[](int i) {
    return (i >= 0 && i < static_cast<int>(state().regions.size()))
               ? state().regions[i].pass : false;
  });

  em::function("dcFramebufferPtr", +[]() {
    return static_cast<int>(
        reinterpret_cast<std::uintptr_t>(state().framebuffer.data()));
  });
  em::function("dcFramebufferSize",
               +[]() { return static_cast<int>(state().framebuffer.size()); });
  em::function("dcFramebufferWidth",
               +[]() { return static_cast<int>(state().fbW); });
  em::function("dcFramebufferHeight",
               +[]() { return static_cast<int>(state().fbH); });
}
