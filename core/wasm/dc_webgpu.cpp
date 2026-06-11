// dc_webgpu.cpp — Emscripten/emdawnwebgpu harness for the dc_gpu triSolid render
// path. ENC-503 (P6.2): the FIRST browser WebGPU render in the DynaCharting
// WebGPU/Dawn migration.
//
// This compiles the *same* dc_gpu rendering code (DawnDevice's webgpu_cpp render
// path + DawnTriSolidBackend) that runs on native Dawn, but against Emscripten's
// emdawnwebgpu port (navigator.gpu). It proves the Dawn/webgpu_cpp rendering code
// is browser-portable: only DEVICE ACQUISITION (RequestAdapter/RequestDevice) and
// the readback event-pump differ in-browser (both #ifdef __EMSCRIPTEN__ in
// DawnDevice), and both are exercised here.
//
// ASYNC SHAPE: DawnDevice::init() and the readback pump yield to the JS event
// loop via ASYNCIFY (emscripten_sleep) so the synchronous native init/readback
// API works unchanged in-browser. The render entry point therefore SUSPENDS, so
// it is exposed as a plain C function (dc_webgpu_render) the page calls with
// Module.ccall(..., { async: true }) -> Promise<int status>. The result fields
// and the rendered framebuffer are then read back through Embind getters (those
// do not suspend). This split keeps the async surface on the one call that needs
// it and avoids depending on em::async() (absent in this Embind version).
//
// NOTE: this is a HARNESS, not production wiring. It drives the backend directly
// (mirroring core/tests/d2_1_dawn_first_render.cpp), the minimal slice that
// exercises device bring-up + triSolid + readback end-to-end in the browser.

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>

#include <cstdint>
#include <string>
#include <vector>

#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTriSolidBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

namespace {

// Last render result, read back through the Embind getters below. A single
// process-wide instance is fine: the harness page renders once (or sequentially).
struct RenderState {
  int status{0};  // 0 = not run, 1 = ok, negative = failure stage
  std::string message;
  int width{0};
  int height{0};
  int centerR{0};
  int centerG{0};
  int centerB{0};
  int centerA{0};
  bool centerIsFill{false};
  std::vector<std::uint8_t> framebuffer;
  std::uint32_t fbW{0};
  std::uint32_t fbH{0};
};

RenderState& state() {
  static RenderState s;
  return s;
}

// Render one red triSolid@1 triangle (the canonical ENC-484 first-render scene)
// into an offscreen RGBA8 target at (w,h), read back the center pixel + the full
// framebuffer, and stash everything in state(). Returns 1 on success, negative on
// failure. SUSPENDS via ASYNCIFY across device acquisition + buffer-map readback.
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
  const std::uint32_t W = static_cast<std::uint32_t>(w);
  const std::uint32_t H = static_cast<std::uint32_t>(h);

  // 1. Bring up the browser Dawn device (navigator.gpu via emdawnwebgpu).
  dc::DawnDevice dev;
  if (!dev.init()) {
    st.message = "DawnDevice::init failed: " + dev.errorMessage();
    st.status = -2;
    return st.status;
  }

  // 2. Build the triSolid scene — identical to d2_1_dawn_first_render.
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  auto step = [&](const char* json, const char* ctx) -> bool {
    dc::CmdResult r = cp.applyJsonText(json);
    if (!r.ok) {
      st.message = std::string("command failed [") + ctx + "]: " + r.err.code +
                   " " + r.err.message;
      return false;
    }
    return true;
  };
  if (!step(R"({"cmd":"createPane","name":"P1"})", "createPane") ||
      !step(R"({"cmd":"createLayer","paneId":1,"name":"L1"})", "createLayer") ||
      !step(R"({"cmd":"createBuffer","byteLength":24})", "createBuffer") ||
      !step(R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":3})",
            "createGeometry") ||
      !step(R"({"cmd":"createDrawItem","layerId":2,"name":"Tri"})",
            "createDrawItem") ||
      !step(R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":4})",
            "bindDrawItem") ||
      !step(R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1.0,"g":0.0,"b":0.0,"a":1.0})",
            "setDrawItemColor")) {
    st.status = -3;
    return st.status;
  }

  // Triangle (-0.5,-0.5),(0.5,-0.5),(0.0,0.5) in clip space (CPU bytes only).
  float verts[] = {
      -0.5f, -0.5f,
       0.5f, -0.5f,
       0.0f,  0.5f,
  };
  dc::CpuBufferStore gpuBufs;
  gpuBufs.setCpuData(3, verts, sizeof(verts));

  // 3. Register + init the Dawn triSolid backend (creates the WGSL pipeline).
  dc::DawnTriSolidBackend triSolid;
  if (!triSolid.init(dev)) {
    st.message = "DawnTriSolidBackend::init (pipeline create) failed";
    st.status = -4;
    return st.status;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &triSolid);

  // 4. Render into the offscreen target (clear to black, then the triangle).
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

  const dc::DrawItem* di = scene.getDrawItem(5);
  if (!di) {
    st.message = "drawItem 5 missing";
    st.status = -5;
    return st.status;
  }
  if (dc::IRendererBackend* be =
          backends.find(dc::DeviceKind::Dawn, di->pipeline)) {
    be->renderDrawItem(dev, scene, gpuBufs, *di, static_cast<int>(W),
                       static_cast<int>(H));
  } else {
    st.message = "no Dawn backend for triSolid@1";
    st.status = -6;
    return st.status;
  }
  dev.endRenderPass();

  // 5. Read back the center pixel (PASS/FAIL) + the full framebuffer (canvas).
  std::uint8_t center[4] = {0, 0, 0, 0};
  dev.readPixel(static_cast<std::int32_t>(W / 2),
                static_cast<std::int32_t>(H / 2), center);
  st.centerR = center[0];
  st.centerG = center[1];
  st.centerB = center[2];
  st.centerA = center[3];
  // PASS heuristic mirrors the native d2_1_dawn_first_render assertion: the
  // center is the triSolid fill color (red). The page shows PASS/FAIL, but the
  // user's eyes on the canvas are the real confirmation.
  st.centerIsFill = (center[0] > 200 && center[1] < 16 && center[2] < 16);

  st.framebuffer.assign(static_cast<std::size_t>(W) * H * 4u, 0);
  std::uint32_t gotW = 0, gotH = 0;
  bool fbOk = dev.readFramebufferRGBA(st.framebuffer.data(),
                                      st.framebuffer.size(), &gotW, &gotH);
  st.fbW = gotW;
  st.fbH = gotH;

  st.status = fbOk ? 1 : -7;
  st.message = st.centerIsFill
                   ? ("PASS: triSolid rendered; center is fill color (backend=" +
                      dev.backendName() + ")")
                   : ("rendered, but center is not fill color (backend=" +
                      dev.backendName() + "): see canvas");
  return st.status;
}

}  // namespace

// C entry point the page calls via Module.ccall('dc_webgpu_render', 'number',
// ['number','number'], [w,h], { async: true }) -> Promise<int status>. ASYNCIFY
// wraps the suspension (device acquisition + readback) into that Promise.
extern "C" EMSCRIPTEN_KEEPALIVE int dc_webgpu_render(int w, int h) {
  return doRender(w, h);
}

// Embind getters for the (already-computed) result + framebuffer. These do not
// suspend, so they are plain synchronous Embind functions the page reads after
// the render Promise resolves.
EMSCRIPTEN_BINDINGS(dc_webgpu) {
  namespace em = emscripten;
  em::function("dcStatus", +[]() { return state().status; });
  em::function("dcMessage", +[]() { return state().message; });
  em::function("dcWidth", +[]() { return state().width; });
  em::function("dcHeight", +[]() { return state().height; });
  em::function("dcCenterR", +[]() { return state().centerR; });
  em::function("dcCenterG", +[]() { return state().centerG; });
  em::function("dcCenterB", +[]() { return state().centerB; });
  em::function("dcCenterA", +[]() { return state().centerA; });
  em::function("dcCenterIsFill", +[]() { return state().centerIsFill; });
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
