#include "dc/gl/Renderer.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"
#include <cstdio>

namespace dc {

static const float kIdentityMat3[9] = {1,0,0, 0,1,0, 0,0,1};

static const float* resolveTransform(const DrawItem& di, const Scene& scene) {
  if (!di.transformId) return kIdentityMat3;
  const Transform* t = scene.getTransform(di.transformId);
  return t ? t->mat3 : kIdentityMat3;
}

// ---- Pos2 shader (triSolid + line2d + points) ----

static const char* kPos2Vert = R"GLSL(
#version 330 core
in vec2 a_pos;
uniform mat3 u_transform;
uniform float u_pointSize;
void main() {
    vec3 p = u_transform * vec3(a_pos, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    gl_PointSize = u_pointSize;
}
)GLSL";

static const char* kPos2Frag = R"GLSL(
#version 330 core
out vec4 outColor;
uniform vec4 u_color;
void main() {
    outColor = u_color;
}
)GLSL";

// ---- instancedRect@1 shader ----

static const char* kInstRectVert = R"GLSL(
#version 330 core
in vec4 a_rect;
uniform mat3 u_transform;
void main() {
    int v = gl_VertexID % 6;
    vec2 uv;
    if (v == 0)      uv = vec2(0.0, 0.0);
    else if (v == 1) uv = vec2(1.0, 0.0);
    else if (v == 2) uv = vec2(0.0, 1.0);
    else if (v == 3) uv = vec2(0.0, 1.0);
    else if (v == 4) uv = vec2(1.0, 0.0);
    else             uv = vec2(1.0, 1.0);
    float x = mix(a_rect.x, a_rect.z, uv.x);
    float y = mix(a_rect.y, a_rect.w, uv.y);
    vec3 p = u_transform * vec3(x, y, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
}
)GLSL";

static const char* kInstRectFrag = R"GLSL(
#version 330 core
out vec4 outColor;
uniform vec4 u_color;
void main() {
    outColor = u_color;
}
)GLSL";

// ---- instancedCandle@1 shader ----

static const char* kInstCandleVert = R"GLSL(
#version 330 core
in vec4 a_c0;
in vec2 a_c1;
uniform mat3 u_transform;
flat out float v_isUp;
void main() {
    float cx    = a_c0.x;
    float open  = a_c0.y;
    float high  = a_c0.z;
    float low   = a_c0.w;
    float close = a_c1.x;
    float hw    = a_c1.y;

    float body0 = min(open, close);
    float body1 = max(open, close);
    float wickW = hw * 0.25;

    int vid = gl_VertexID % 12;
    bool isWick = (vid >= 6);
    int lid = isWick ? (vid - 6) : vid;

    vec2 uv;
    if (lid == 0)      uv = vec2(0.0, 0.0);
    else if (lid == 1) uv = vec2(1.0, 0.0);
    else if (lid == 2) uv = vec2(0.0, 1.0);
    else if (lid == 3) uv = vec2(0.0, 1.0);
    else if (lid == 4) uv = vec2(1.0, 0.0);
    else               uv = vec2(1.0, 1.0);

    float x0 = isWick ? (cx - wickW) : (cx - hw);
    float x1 = isWick ? (cx + wickW) : (cx + hw);
    float y0 = isWick ? low  : body0;
    float y1 = isWick ? high : body1;

    vec3 p = u_transform * vec3(mix(x0, x1, uv.x), mix(y0, y1, uv.y), 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    v_isUp = (close >= open) ? 1.0 : 0.0;
}
)GLSL";

static const char* kInstCandleFrag = R"GLSL(
#version 330 core
out vec4 outColor;
uniform vec4 u_colorUp;
uniform vec4 u_colorDown;
flat in float v_isUp;
void main() {
    outColor = (v_isUp > 0.5) ? u_colorUp : u_colorDown;
}
)GLSL";

// ---- textSDF@1 shader ----

static const char* kTextSdfVert = R"GLSL(
#version 330 core
in vec4 a_g0;
in vec4 a_g1;
uniform mat3 u_transform;
out vec2 v_uv;
void main() {
    int vid = gl_VertexID % 6;
    vec2 uv;
    if (vid == 0)      uv = vec2(0.0, 0.0);
    else if (vid == 1) uv = vec2(1.0, 0.0);
    else if (vid == 2) uv = vec2(0.0, 1.0);
    else if (vid == 3) uv = vec2(0.0, 1.0);
    else if (vid == 4) uv = vec2(1.0, 0.0);
    else               uv = vec2(1.0, 1.0);
    float x = mix(a_g0.x, a_g0.z, uv.x);
    float y = mix(a_g0.y, a_g0.w, uv.y);
    v_uv = vec2(mix(a_g1.x, a_g1.z, uv.x), mix(a_g1.y, a_g1.w, uv.y));
    vec3 p = u_transform * vec3(x, y, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
}
)GLSL";

static const char* kTextSdfFrag = R"GLSL(
#version 330 core
uniform sampler2D u_atlas;
uniform vec4 u_color;
uniform float u_pxRange;
in vec2 v_uv;
out vec4 outColor;
void main() {
    float sdf = texture(u_atlas, v_uv).r;
    float dist = sdf - 0.5;
    float w = fwidth(dist) * u_pxRange;
    float a = smoothstep(-w, w, dist);
    outColor = vec4(u_color.rgb, u_color.a * a);
}
)GLSL";

// ---- Renderer implementation ----

Renderer::~Renderer() {
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
  }
  if (atlasTexture_) {
    glDeleteTextures(1, &atlasTexture_);
  }
}

void Renderer::setGlyphAtlas(GlyphAtlas* atlas) {
  atlas_ = atlas;
}

bool Renderer::init() {
  if (!pos2Prog_.build(kPos2Vert, kPos2Frag)) {
    std::fprintf(stderr, "Renderer::init: failed to build pos2 shader\n");
    return false;
  }
  if (!instRectProg_.build(kInstRectVert, kInstRectFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build instRect shader\n");
    return false;
  }
  if (!instCandleProg_.build(kInstCandleVert, kInstCandleFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build instCandle shader\n");
    return false;
  }
  if (!textSdfProg_.build(kTextSdfVert, kTextSdfFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build textSdf shader\n");
    return false;
  }

  glGenVertexArrays(1, &vao_);
  glGenTextures(1, &atlasTexture_);
  glEnable(GL_PROGRAM_POINT_SIZE);
  inited_ = true;
  return true;
}

void Renderer::drawPos2(const DrawItem& di, const Scene& scene,
                        GpuBufferManager& gpuBufs, GLenum mode, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  pos2Prog_.use();

  const float* xform = resolveTransform(di, scene);
  pos2Prog_.setUniformMat3(pos2Prog_.uniformLocation("u_transform"), xform);
  pos2Prog_.setUniformVec4(pos2Prog_.uniformLocation("u_color"),
                           di.color[0], di.color[1], di.color[2], di.color[3]);
  pos2Prog_.setUniformFloat(pos2Prog_.uniformLocation("u_pointSize"), 4.0f);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLint aPos = pos2Prog_.attribLocation("a_pos");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  glDrawArrays(mode, 0, static_cast<GLsizei>(geo->vertexCount));
  stats.drawCalls++;

  glDisableVertexAttribArray(static_cast<GLuint>(aPos));
}

void Renderer::drawInstancedRect(const DrawItem& di, const Scene& scene,
                                 GpuBufferManager& gpuBufs, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  instRectProg_.use();

  const float* xform = resolveTransform(di, scene);
  instRectProg_.setUniformMat3(instRectProg_.uniformLocation("u_transform"), xform);
  instRectProg_.setUniformVec4(instRectProg_.uniformLocation("u_color"),
                               di.color[0], di.color[1], di.color[2], di.color[3]);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLint aRect = instRectProg_.attribLocation("a_rect");
  glEnableVertexAttribArray(static_cast<GLuint>(aRect));
  glVertexAttribPointer(static_cast<GLuint>(aRect), 4, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(strideOf(VertexFormat::Rect4)), nullptr);
  glVertexAttribDivisor(static_cast<GLuint>(aRect), 1);

  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aRect), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aRect));
}

void Renderer::drawInstancedCandle(const DrawItem& di, const Scene& scene,
                                   GpuBufferManager& gpuBufs, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  instCandleProg_.use();

  const float* xform = resolveTransform(di, scene);
  instCandleProg_.setUniformMat3(instCandleProg_.uniformLocation("u_transform"), xform);
  instCandleProg_.setUniformVec4(instCandleProg_.uniformLocation("u_colorUp"),
                                  0.0f, 0.8f, 0.0f, 1.0f);
  instCandleProg_.setUniformVec4(instCandleProg_.uniformLocation("u_colorDown"),
                                  0.8f, 0.0f, 0.0f, 1.0f);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  GLsizei stride = static_cast<GLsizei>(strideOf(VertexFormat::Candle6));

  GLint aC0 = instCandleProg_.attribLocation("a_c0");
  glEnableVertexAttribArray(static_cast<GLuint>(aC0));
  glVertexAttribPointer(static_cast<GLuint>(aC0), 4, GL_FLOAT, GL_FALSE,
                        stride, nullptr);
  glVertexAttribDivisor(static_cast<GLuint>(aC0), 1);

  GLint aC1 = instCandleProg_.attribLocation("a_c1");
  glEnableVertexAttribArray(static_cast<GLuint>(aC1));
  glVertexAttribPointer(static_cast<GLuint>(aC1), 2, GL_FLOAT, GL_FALSE,
                        stride, reinterpret_cast<const void*>(16));
  glVertexAttribDivisor(static_cast<GLuint>(aC1), 1);

  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 12, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aC0), 0);
  glVertexAttribDivisor(static_cast<GLuint>(aC1), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aC0));
  glDisableVertexAttribArray(static_cast<GLuint>(aC1));
}

void Renderer::uploadAtlasIfDirty() {
  if (!atlas_ || !atlas_->isDirty()) return;

  glBindTexture(GL_TEXTURE_2D, atlasTexture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  GLsizei sz = static_cast<GLsizei>(atlas_->atlasSize());
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, sz, sz, 0,
               GL_RED, GL_UNSIGNED_BYTE, atlas_->atlasData());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  atlas_->clearDirty();
}

void Renderer::drawTextSdf(const DrawItem& di, const Scene& scene,
                            GpuBufferManager& gpuBufs, Stats& stats) {
  if (!atlas_) return;
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  textSdfProg_.use();

  const float* xform = resolveTransform(di, scene);
  textSdfProg_.setUniformMat3(textSdfProg_.uniformLocation("u_transform"), xform);
  textSdfProg_.setUniformVec4(textSdfProg_.uniformLocation("u_color"),
                               di.color[0], di.color[1], di.color[2], di.color[3]);
  textSdfProg_.setUniformFloat(textSdfProg_.uniformLocation("u_pxRange"), 12.0f);

  // Bind atlas texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlasTexture_);
  glUniform1i(textSdfProg_.uniformLocation("u_atlas"), 0);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  GLsizei stride = static_cast<GLsizei>(strideOf(VertexFormat::Glyph8));

  GLint aG0 = textSdfProg_.attribLocation("a_g0");
  glEnableVertexAttribArray(static_cast<GLuint>(aG0));
  glVertexAttribPointer(static_cast<GLuint>(aG0), 4, GL_FLOAT, GL_FALSE,
                        stride, nullptr);
  glVertexAttribDivisor(static_cast<GLuint>(aG0), 1);

  GLint aG1 = textSdfProg_.attribLocation("a_g1");
  glEnableVertexAttribArray(static_cast<GLuint>(aG1));
  glVertexAttribPointer(static_cast<GLuint>(aG1), 4, GL_FLOAT, GL_FALSE,
                        stride, reinterpret_cast<const void*>(16));
  glVertexAttribDivisor(static_cast<GLuint>(aG1), 1);

  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aG0), 0);
  glVertexAttribDivisor(static_cast<GLuint>(aG1), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aG0));
  glDisableVertexAttribArray(static_cast<GLuint>(aG1));
  glBindTexture(GL_TEXTURE_2D, 0);
}

Stats Renderer::render(const Scene& scene, GpuBufferManager& gpuBufs,
                       int viewW, int viewH) {
  Stats stats{};
  if (!inited_) return stats;

  uploadAtlasIfDirty();

  glViewport(0, 0, viewW, viewH);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Enable blending for SDF text
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindVertexArray(vao_);

  // Walk all draw items (pane → layer → drawItem).
  for (Id paneId : scene.paneIds()) {
    for (Id layerId : scene.layerIds()) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;

      for (Id diId : scene.drawItemIds()) {
        const DrawItem* di = scene.getDrawItem(diId);
        if (!di || di->layerId != layerId) continue;
        if (di->pipeline.empty()) continue;

        if (di->pipeline == "triSolid@1") {
          drawPos2(*di, scene, gpuBufs, GL_TRIANGLES, stats);
        } else if (di->pipeline == "line2d@1") {
          drawPos2(*di, scene, gpuBufs, GL_LINES, stats);
        } else if (di->pipeline == "points@1") {
          drawPos2(*di, scene, gpuBufs, GL_POINTS, stats);
        } else if (di->pipeline == "instancedRect@1") {
          drawInstancedRect(*di, scene, gpuBufs, stats);
        } else if (di->pipeline == "instancedCandle@1") {
          drawInstancedCandle(*di, scene, gpuBufs, stats);
        } else if (di->pipeline == "textSDF@1") {
          drawTextSdf(*di, scene, gpuBufs, stats);
        }
      }
    }
  }

  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glFlush();
  return stats;
}

} // namespace dc
