// ENC-91: minimal "first interactive C++ demo" — opens a GLFW window,
// boots the dc rendering core, and renders a single static triangle.
// Smallest possible reference for embedding the engine in a real
// window. Use this as the template; d2_7_candle_demo and friends
// have richer feature coverage but more boilerplate.
//
// Built only when GLFW is available (-DDC_HAS_GLFW=1, auto-detected
// via find_package(glfw3)).

#ifndef DC_HAS_GLFW
#  include <cstdio>
int main() {
  std::fprintf(stderr,
               "dc_hello_glfw: not built — install GLFW3 and reconfigure.\n");
  return 1;
}
#else

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GlfwContext.hpp"

#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>

namespace {

void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void appendAppendRecord(std::vector<std::uint8_t>& batch,
                        std::uint32_t bufferId,
                        const void* payload,
                        std::uint32_t len) {
  batch.push_back(1);                       // OP_APPEND
  writeU32LE(batch, bufferId);
  writeU32LE(batch, 0);                     // offset
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.code.c_str(), r.message.c_str());
    std::exit(1);
  }
}

}  // namespace

int main() {
  // 1. Window + GL context.
  auto ctx = std::make_unique<dc::GlfwContext>();
  if (!ctx->init(1024, 768)) {
    std::fprintf(stderr, "dc_hello_glfw: GlfwContext init failed\n");
    return 1;
  }

  // 2. Engine plumbing.
  dc::Scene scene;
  dc::ResourceRegistry resources;
  dc::CommandProcessor cmds(scene, resources);
  dc::IngestProcessor ingest;
  dc::GpuBufferManager gpu;
  dc::Renderer renderer;

  // 3. Build a trivial scene: pane → layer → buffer → geometry → draw item.
  requireOk(cmds.applyJsonText(R"({"cmd":"createPane","name":"hello"})"), "pane");
  requireOk(cmds.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"tri"})"), "layer");
  requireOk(cmds.applyJsonText(R"({"cmd":"createBuffer","byteLength":0})"), "buffer");
  requireOk(cmds.applyJsonText(
      R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":3})"),
      "geometry");
  requireOk(cmds.applyJsonText(
      R"({"cmd":"createDrawItem","layerId":2,"pipeline":"triSolid@1",)"
      R"("geometryId":4,"color":[0.4,0.7,1.0,1.0]})"),
      "drawItem");

  // 4. Ingest the triangle vertices through the binary record path.
  const float verts[] = {
    -0.6f, -0.5f,
     0.6f, -0.5f,
     0.0f,  0.7f,
  };
  std::vector<std::uint8_t> batch;
  appendAppendRecord(batch, /*bufferId=*/3, verts, sizeof(verts));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  ingest.syncBufferLengths(scene);

  // 5. Frame loop.
  std::printf("dc_hello_glfw: window open. Close to exit.\n");
  while (!ctx->shouldClose()) {
    (void)ctx->pollInput();
    dc::Stats stats = renderer.render(scene, gpu, ctx->width(), ctx->height());
    (void)stats;
    ctx->swapBuffers();
  }
  return 0;
}

#endif  // DC_HAS_GLFW
