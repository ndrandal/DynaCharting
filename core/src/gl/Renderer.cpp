#include "dc/gl/Renderer.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/gl/TextureManager.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

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

// ---- instancedRect@1 shader (D28.2: rounded corners) ----

static const char* kInstRectVert = R"GLSL(
#version 330 core
in vec4 a_rect;
uniform mat3 u_transform;
uniform vec2 u_viewportSize;
out vec2 v_uv;
flat out vec2 v_halfSizePx;
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
    v_uv = uv;
    // Compute rect half-size in pixels for SDF rounded corners
    vec3 c0 = u_transform * vec3(a_rect.x, a_rect.y, 1.0);
    vec3 c1 = u_transform * vec3(a_rect.z, a_rect.w, 1.0);
    v_halfSizePx = abs(c1.xy - c0.xy) * 0.5 * u_viewportSize * 0.5;
}
)GLSL";

static const char* kInstRectFrag = R"GLSL(
#version 330 core
out vec4 outColor;
uniform vec4 u_color;
uniform float u_cornerRadius;
in vec2 v_uv;
flat in vec2 v_halfSizePx;
void main() {
    if (u_cornerRadius > 0.0) {
        vec2 p = (v_uv - 0.5) * 2.0 * v_halfSizePx;
        float r = min(u_cornerRadius, min(v_halfSizePx.x, v_halfSizePx.y));
        float d = length(max(abs(p) - v_halfSizePx + r, 0.0)) - r;
        float a = 1.0 - smoothstep(-0.5, 0.5, d);
        outColor = vec4(u_color.rgb, u_color.a * a);
    } else {
        outColor = u_color;
    }
}
)GLSL";

// ---- instancedCandle@1 shader ----

static const char* kInstCandleVert = R"GLSL(
#version 330 core
in vec4 a_c0;
in vec2 a_c1;
uniform mat3 u_transform;
uniform vec2 u_viewportSize;
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

    if (isWick) {
        // Fixed-pixel wick: transform center, then offset in clip space
        float y = mix(low, high, uv.y);
        vec3 center = u_transform * vec3(cx, y, 1.0);
        float wickClipHW = 1.0 / u_viewportSize.x; // 1 pixel half-width
        gl_Position = vec4(center.x + mix(-wickClipHW, wickClipHW, uv.x),
                           center.y, 0.0, 1.0);
    } else {
        float x0 = cx - hw;
        float x1 = cx + hw;
        vec3 p = u_transform * vec3(mix(x0, x1, uv.x), mix(body0, body1, uv.y), 1.0);
        gl_Position = vec4(p.xy, 0.0, 1.0);
    }
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
    float val = texture(u_atlas, v_uv).r;
    // u_pxRange < 0 signals raw-alpha mode (no SDF reconstruction)
    float a = (u_pxRange < 0.0) ? val : smoothstep(0.45, 0.55, val);
    outColor = vec4(u_color.rgb, u_color.a * a);
}
)GLSL";

// ---- lineAA@1 shader (D28.1: dashed lines) ----

static const char* kLineAAVert = R"GLSL(
#version 330 core
in vec4 a_rect;
uniform mat3 u_transform;
uniform float u_lineWidth;
uniform float u_aaWidth;
uniform vec2 u_viewportSize;
out float v_dist;
out float v_along;
void main() {
    vec2 p0 = a_rect.xy;
    vec2 p1 = a_rect.zw;

    vec3 c0 = u_transform * vec3(p0, 1.0);
    vec3 c1 = u_transform * vec3(p1, 1.0);

    vec2 dir = c1.xy - c0.xy;
    float len = length(dir);
    vec2 d = (len > 0.0001) ? dir / len : vec2(1.0, 0.0);
    vec2 perp = vec2(-d.y, d.x);

    float hw = u_lineWidth * 0.5;
    float totalHW = hw + u_aaWidth;

    int vid = gl_VertexID % 6;
    vec2 uv;
    if (vid == 0)      uv = vec2(0.0, -1.0);
    else if (vid == 1) uv = vec2(1.0, -1.0);
    else if (vid == 2) uv = vec2(0.0,  1.0);
    else if (vid == 3) uv = vec2(0.0,  1.0);
    else if (vid == 4) uv = vec2(1.0, -1.0);
    else               uv = vec2(1.0,  1.0);

    vec2 pos = mix(c0.xy, c1.xy, uv.x) + perp * (uv.y * totalHW);
    gl_Position = vec4(pos, 0.0, 1.0);
    // v_dist: 0 at center, 1.0 at nominal edge, >1.0 in AA fringe
    v_dist = uv.y * totalHW / max(hw, 0.0001);
    // D28.1: distance along line in pixel space for dash pattern
    vec2 dirPx = dir * u_viewportSize * 0.5;
    v_along = uv.x * length(dirPx);
}
)GLSL";

static const char* kLineAAFrag = R"GLSL(
#version 330 core
uniform vec4 u_color;
uniform float u_fringeEdge;
uniform float u_dashLen;
uniform float u_gapLen;
in float v_dist;
in float v_along;
out vec4 outColor;
void main() {
    // D28.1: dash pattern (discard gap fragments)
    if (u_dashLen > 0.0) {
        float cycle = u_dashLen + u_gapLen;
        float pos = mod(v_along, cycle);
        if (pos > u_dashLen) discard;
    }
    float d = abs(v_dist);
    // d <= 1.0: inside nominal line (full alpha)
    // d 1.0 → u_fringeEdge: AA fringe (smooth fade to 0)
    float a = 1.0 - smoothstep(1.0, u_fringeEdge, d);
    outColor = vec4(u_color.rgb, u_color.a * a);
}
)GLSL";

// ---- triAA@1 shader (per-vertex alpha for edge-fringe AA) ----

static const char* kTriAAVert = R"GLSL(
#version 330 core
in vec3 a_pos_alpha;
uniform mat3 u_transform;
out float v_alpha;
void main() {
    vec3 p = u_transform * vec3(a_pos_alpha.xy, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    v_alpha = a_pos_alpha.z;
}
)GLSL";

static const char* kTriAAFrag = R"GLSL(
#version 330 core
uniform vec4 u_color;
in float v_alpha;
out vec4 outColor;
void main() {
    outColor = vec4(u_color.rgb, u_color.a * v_alpha);
}
)GLSL";

// ---- triGradient@1 shader (D28.3: per-vertex color) ----

static const char* kTriGradientVert = R"GLSL(
#version 330 core
in vec2 a_pos;
in vec4 a_color;
uniform mat3 u_transform;
out vec4 v_color;
void main() {
    vec3 p = u_transform * vec3(a_pos, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    v_color = a_color;
}
)GLSL";

static const char* kTriGradientFrag = R"GLSL(
#version 330 core
in vec4 v_color;
out vec4 outColor;
void main() {
    outColor = v_color;
}
)GLSL";

// ---- D41: texturedQuad@1 shader ----

static const char* kTexQuadVert = R"GLSL(
#version 330 core
in vec4 a_pos_uv;
uniform mat3 u_transform;
out vec2 v_uv;
void main() {
    int v = gl_VertexID % 6;
    vec2 uv;
    if (v == 0)      uv = vec2(0.0, 0.0);
    else if (v == 1) uv = vec2(1.0, 0.0);
    else if (v == 2) uv = vec2(0.0, 1.0);
    else if (v == 3) uv = vec2(0.0, 1.0);
    else if (v == 4) uv = vec2(1.0, 0.0);
    else             uv = vec2(1.0, 1.0);
    float x = mix(a_pos_uv.x, a_pos_uv.z, uv.x);
    float y = mix(a_pos_uv.y, a_pos_uv.w, uv.y);
    vec3 p = u_transform * vec3(x, y, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    v_uv = uv;
}
)GLSL";

static const char* kTexQuadFrag = R"GLSL(
#version 330 core
uniform sampler2D u_texture;
uniform vec4 u_color;
in vec2 v_uv;
out vec4 outColor;
void main() {
    vec4 texel = texture(u_texture, v_uv);
    outColor = texel * u_color;
}
)GLSL";

// ---- D29.3: Pick shaders ----

static const char* kPickFlatVert = R"GLSL(
#version 330 core
in vec2 a_pos;
uniform mat3 u_transform;
void main() {
    vec3 p = u_transform * vec3(a_pos, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
    gl_PointSize = 8.0;
}
)GLSL";

static const char* kPickFrag = R"GLSL(
#version 330 core
uniform vec4 u_pickColor;
out vec4 outColor;
void main() {
    outColor = u_pickColor;
}
)GLSL";

static const char* kPickInstRectVert = R"GLSL(
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

static const char* kPickInstCandleVert = R"GLSL(
#version 330 core
in vec4 a_c0;
in vec2 a_c1;
uniform mat3 u_transform;
uniform vec2 u_viewportSize;
void main() {
    float cx = a_c0.x, open = a_c0.y, high = a_c0.z, low = a_c0.w;
    float close = a_c1.x, hw = a_c1.y;
    float body0 = min(open, close), body1 = max(open, close);
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
    if (isWick) {
        float y = mix(low, high, uv.y);
        vec3 center = u_transform * vec3(cx, y, 1.0);
        float wickClipHW = 1.0 / u_viewportSize.x;
        gl_Position = vec4(center.x + mix(-wickClipHW, wickClipHW, uv.x),
                           center.y, 0.0, 1.0);
    } else {
        vec3 p = u_transform * vec3(mix(cx - hw, cx + hw, uv.x),
                                    mix(body0, body1, uv.y), 1.0);
        gl_Position = vec4(p.xy, 0.0, 1.0);
    }
}
)GLSL";

static const char* kPickLineAAVert = R"GLSL(
#version 330 core
in vec4 a_rect;
uniform mat3 u_transform;
uniform float u_lineWidth;
uniform float u_aaWidth;
void main() {
    vec2 p0 = a_rect.xy, p1 = a_rect.zw;
    vec3 c0 = u_transform * vec3(p0, 1.0);
    vec3 c1 = u_transform * vec3(p1, 1.0);
    vec2 dir = c1.xy - c0.xy;
    float len = length(dir);
    vec2 d = (len > 0.0001) ? dir / len : vec2(1.0, 0.0);
    vec2 perp = vec2(-d.y, d.x);
    float totalHW = u_lineWidth * 0.5 + u_aaWidth;
    int vid = gl_VertexID % 6;
    vec2 uv;
    if (vid == 0)      uv = vec2(0.0, -1.0);
    else if (vid == 1) uv = vec2(1.0, -1.0);
    else if (vid == 2) uv = vec2(0.0,  1.0);
    else if (vid == 3) uv = vec2(0.0,  1.0);
    else if (vid == 4) uv = vec2(1.0, -1.0);
    else               uv = vec2(1.0,  1.0);
    vec2 pos = mix(c0.xy, c1.xy, uv.x) + perp * (uv.y * totalHW);
    gl_Position = vec4(pos, 0.0, 1.0);
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
  if (scratchVbo_) {
    glDeleteBuffers(1, &scratchVbo_);
  }
  if (pickRbo_) {
    glDeleteRenderbuffers(1, &pickRbo_);
  }
  if (pickFbo_) {
    glDeleteFramebuffers(1, &pickFbo_);
  }
}

void Renderer::setGlyphAtlas(GlyphAtlas* atlas) {
  atlas_ = atlas;
}

void Renderer::setTextureManager(TextureManager* mgr) {
  texMgr_ = mgr;
}

void Renderer::setEventBus(EventBus* bus) {
  eventBus_ = bus;
}

void Renderer::setRenderStyle(const RenderStyle& style) {
  renderStyle_ = style;
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
  if (!lineAAProg_.build(kLineAAVert, kLineAAFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build lineAA shader\n");
    return false;
  }
  if (!triAAProg_.build(kTriAAVert, kTriAAFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build triAA shader\n");
    return false;
  }
  if (!triGradientProg_.build(kTriGradientVert, kTriGradientFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build triGradient shader\n");
    return false;
  }
  // D41: texturedQuad shader
  if (!texQuadProg_.build(kTexQuadVert, kTexQuadFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build texQuad shader\n");
    return false;
  }
  // D29.3: pick shaders
  if (!pickFlatProg_.build(kPickFlatVert, kPickFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build pickFlat shader\n");
    return false;
  }
  if (!pickInstRectProg_.build(kPickInstRectVert, kPickFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build pickInstRect shader\n");
    return false;
  }
  if (!pickInstCandleProg_.build(kPickInstCandleVert, kPickFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build pickInstCandle shader\n");
    return false;
  }
  if (!pickLineAAProg_.build(kPickLineAAVert, kPickFrag)) {
    std::fprintf(stderr, "Renderer::init: failed to build pickLineAA shader\n");
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
  pos2Prog_.setUniformFloat(pos2Prog_.uniformLocation("u_pointSize"), di.pointSize);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLint aPos = pos2Prog_.attribLocation("a_pos");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  if (mode == GL_LINES) glLineWidth(di.lineWidth);

  // D26: indexed draw path
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    GLuint ibo = gpuBufs.getGlBuffer(geo->indexBufferId);
    if (ibo) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
      glDrawElements(mode, static_cast<GLsizei>(geo->indexCount),
                     GL_UNSIGNED_INT, nullptr);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      stats.drawCalls++;
    }
  } else {
    glDrawArrays(mode, 0, static_cast<GLsizei>(geo->vertexCount));
    stats.drawCalls++;
  }

  glDisableVertexAttribArray(static_cast<GLuint>(aPos));
}

void Renderer::drawInstancedRect(const DrawItem& di, const Scene& scene,
                                 GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  instRectProg_.use();

  const float* xform = resolveTransform(di, scene);
  instRectProg_.setUniformMat3(instRectProg_.uniformLocation("u_transform"), xform);
  instRectProg_.setUniformVec4(instRectProg_.uniformLocation("u_color"),
                               di.color[0], di.color[1], di.color[2], di.color[3]);
  // D28.2: rounded corners
  glUniform2f(instRectProg_.uniformLocation("u_viewportSize"),
              static_cast<float>(viewW), static_cast<float>(viewH));
  instRectProg_.setUniformFloat(instRectProg_.uniformLocation("u_cornerRadius"), di.cornerRadius);

  // D26: indexed gather for instanced pipelines
  GLuint bindVbo = vbo;
  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
    const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
    std::uint32_t stride = strideOf(VertexFormat::Rect4);
    std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
    if (idxData && vtxData) {
      instanceCount = static_cast<GLsizei>(geo->indexCount);
      scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
      for (GLsizei i = 0; i < instanceCount; i++) {
        std::uint32_t idx = indices[i];
        std::uint32_t off = idx * stride;
        if (off + stride <= vtxSize) {
          std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
      }
      if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
      glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(scratchData_.size()),
                   scratchData_.data(), GL_STREAM_DRAW);
      bindVbo = scratchVbo_;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
  GLint aRect = instRectProg_.attribLocation("a_rect");
  glEnableVertexAttribArray(static_cast<GLuint>(aRect));
  glVertexAttribPointer(static_cast<GLuint>(aRect), 4, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(strideOf(VertexFormat::Rect4)), nullptr);
  glVertexAttribDivisor(static_cast<GLuint>(aRect), 1);

  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aRect), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aRect));
}

void Renderer::drawInstancedCandle(const DrawItem& di, const Scene& scene,
                                   GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  instCandleProg_.use();

  const float* xform = resolveTransform(di, scene);
  instCandleProg_.setUniformMat3(instCandleProg_.uniformLocation("u_transform"), xform);
  glUniform2f(instCandleProg_.uniformLocation("u_viewportSize"),
              static_cast<float>(viewW), static_cast<float>(viewH));
  instCandleProg_.setUniformVec4(instCandleProg_.uniformLocation("u_colorUp"),
                                  di.colorUp[0], di.colorUp[1], di.colorUp[2], di.colorUp[3]);
  instCandleProg_.setUniformVec4(instCandleProg_.uniformLocation("u_colorDown"),
                                  di.colorDown[0], di.colorDown[1], di.colorDown[2], di.colorDown[3]);

  // D26: indexed gather for instanced candle
  GLuint bindVbo = vbo;
  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  GLsizei stride = static_cast<GLsizei>(strideOf(VertexFormat::Candle6));
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
    const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
    std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
    if (idxData && vtxData) {
      instanceCount = static_cast<GLsizei>(geo->indexCount);
      scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
      for (GLsizei i = 0; i < instanceCount; i++) {
        std::uint32_t idx = indices[i];
        std::uint32_t off = idx * static_cast<std::uint32_t>(stride);
        if (off + static_cast<std::uint32_t>(stride) <= vtxSize) {
          std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
      }
      if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
      glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(scratchData_.size()),
                   scratchData_.data(), GL_STREAM_DRAW);
      bindVbo = scratchVbo_;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, bindVbo);

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
  float pxRange = (atlas_ && !atlas_->useSdf()) ? -1.0f : 12.0f;
  textSdfProg_.setUniformFloat(textSdfProg_.uniformLocation("u_pxRange"), pxRange);

  // Bind atlas texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlasTexture_);
  glUniform1i(textSdfProg_.uniformLocation("u_atlas"), 0);

  // D26: indexed gather for instanced text
  GLuint bindVbo = vbo;
  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  GLsizei stride = static_cast<GLsizei>(strideOf(VertexFormat::Glyph8));
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
    const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
    std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
    if (idxData && vtxData) {
      instanceCount = static_cast<GLsizei>(geo->indexCount);
      scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
      for (GLsizei i = 0; i < instanceCount; i++) {
        std::uint32_t idx = indices[i];
        std::uint32_t off = idx * static_cast<std::uint32_t>(stride);
        if (off + static_cast<std::uint32_t>(stride) <= vtxSize) {
          std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
      }
      if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
      glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(scratchData_.size()),
                   scratchData_.data(), GL_STREAM_DRAW);
      bindVbo = scratchVbo_;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, bindVbo);

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

  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aG0), 0);
  glVertexAttribDivisor(static_cast<GLuint>(aG1), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aG0));
  glDisableVertexAttribArray(static_cast<GLuint>(aG1));
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::drawLineAA(const DrawItem& di, const Scene& scene,
                          GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  lineAAProg_.use();

  const float* xform = resolveTransform(di, scene);
  lineAAProg_.setUniformMat3(lineAAProg_.uniformLocation("u_transform"), xform);
  lineAAProg_.setUniformVec4(lineAAProg_.uniformLocation("u_color"),
                              di.color[0], di.color[1], di.color[2], di.color[3]);

  // Convert lineWidth from pixels to clip units
  float lineWidthClip = (viewW > 0) ? (di.lineWidth / static_cast<float>(viewW) * 2.0f) : 0.01f;
  lineAAProg_.setUniformFloat(lineAAProg_.uniformLocation("u_lineWidth"), lineWidthClip);
  // AA fringe: 1.5 pixels beyond nominal line edge
  float aaWidthClip = (viewW > 0) ? (1.5f / static_cast<float>(viewW) * 2.0f) : 0.005f;
  lineAAProg_.setUniformFloat(lineAAProg_.uniformLocation("u_aaWidth"), aaWidthClip);
  // Fringe edge in v_dist space: (hw + aaWidth) / hw
  float hw = lineWidthClip * 0.5f;
  float fringeEdge = (hw > 0.0001f) ? ((hw + aaWidthClip) / hw) : 2.0f;
  lineAAProg_.setUniformFloat(lineAAProg_.uniformLocation("u_fringeEdge"), fringeEdge);
  // D28.1: viewport size for pixel-space dash calculation, dash/gap uniforms
  glUniform2f(lineAAProg_.uniformLocation("u_viewportSize"),
              static_cast<float>(viewW), static_cast<float>(viewH));
  lineAAProg_.setUniformFloat(lineAAProg_.uniformLocation("u_dashLen"), di.dashLength);
  lineAAProg_.setUniformFloat(lineAAProg_.uniformLocation("u_gapLen"), di.gapLength);

  // D26: indexed gather for instanced lineAA
  GLuint bindVbo = vbo;
  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  std::uint32_t stride = strideOf(VertexFormat::Rect4);
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
    const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
    std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
    if (idxData && vtxData) {
      instanceCount = static_cast<GLsizei>(geo->indexCount);
      scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
      for (GLsizei i = 0; i < instanceCount; i++) {
        std::uint32_t idx = indices[i];
        std::uint32_t off = idx * stride;
        if (off + stride <= vtxSize) {
          std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
      }
      if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
      glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(scratchData_.size()),
                   scratchData_.data(), GL_STREAM_DRAW);
      bindVbo = scratchVbo_;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
  GLint aRect = lineAAProg_.attribLocation("a_rect");
  glEnableVertexAttribArray(static_cast<GLuint>(aRect));
  glVertexAttribPointer(static_cast<GLuint>(aRect), 4, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(stride), nullptr);
  glVertexAttribDivisor(static_cast<GLuint>(aRect), 1);

  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aRect), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aRect));
}

void Renderer::drawTriAA(const DrawItem& di, const Scene& scene,
                         GpuBufferManager& gpuBufs, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  triAAProg_.use();

  const float* xform = resolveTransform(di, scene);
  triAAProg_.setUniformMat3(triAAProg_.uniformLocation("u_transform"), xform);
  triAAProg_.setUniformVec4(triAAProg_.uniformLocation("u_color"),
                            di.color[0], di.color[1], di.color[2], di.color[3]);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLint aPos = triAAProg_.attribLocation("a_pos_alpha");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 3, GL_FLOAT, GL_FALSE, 12, nullptr);

  // D26: indexed draw path
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    GLuint ibo = gpuBufs.getGlBuffer(geo->indexBufferId);
    if (ibo) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(geo->indexCount),
                     GL_UNSIGNED_INT, nullptr);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      stats.drawCalls++;
    }
  } else {
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(geo->vertexCount));
    stats.drawCalls++;
  }

  glDisableVertexAttribArray(static_cast<GLuint>(aPos));
}

void Renderer::drawTriGradient(const DrawItem& di, const Scene& scene,
                                GpuBufferManager& gpuBufs, Stats& stats) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  triGradientProg_.use();

  const float* xform = resolveTransform(di, scene);
  triGradientProg_.setUniformMat3(triGradientProg_.uniformLocation("u_transform"), xform);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLsizei stride = static_cast<GLsizei>(strideOf(VertexFormat::Pos2Color4)); // 24

  GLint aPos = triGradientProg_.attribLocation("a_pos");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, stride, nullptr);

  GLint aColor = triGradientProg_.attribLocation("a_color");
  glEnableVertexAttribArray(static_cast<GLuint>(aColor));
  glVertexAttribPointer(static_cast<GLuint>(aColor), 4, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<const void*>(8));

  // D26: indexed draw path
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    GLuint ibo = gpuBufs.getGlBuffer(geo->indexBufferId);
    if (ibo) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(geo->indexCount),
                     GL_UNSIGNED_INT, nullptr);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      stats.drawCalls++;
    }
  } else {
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(geo->vertexCount));
    stats.drawCalls++;
  }

  glDisableVertexAttribArray(static_cast<GLuint>(aPos));
  glDisableVertexAttribArray(static_cast<GLuint>(aColor));
}

void Renderer::drawTexturedQuad(const DrawItem& di, const Scene& scene,
                                GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats) {
  if (!texMgr_ || di.textureId == 0) return;
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  texQuadProg_.use();

  const float* xform = resolveTransform(di, scene);
  texQuadProg_.setUniformMat3(texQuadProg_.uniformLocation("u_transform"), xform);
  texQuadProg_.setUniformVec4(texQuadProg_.uniformLocation("u_color"),
                               di.color[0], di.color[1], di.color[2], di.color[3]);

  // Bind texture
  glActiveTexture(GL_TEXTURE0);
  texMgr_->bind(di.textureId, 0);
  glUniform1i(texQuadProg_.uniformLocation("u_texture"), 0);

  // D26: indexed gather for instanced texturedQuad
  GLuint bindVbo = vbo;
  GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
  std::uint32_t stride = strideOf(VertexFormat::Pos2Uv4);
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
    const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
    std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
    if (idxData && vtxData) {
      instanceCount = static_cast<GLsizei>(geo->indexCount);
      scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
      for (GLsizei i = 0; i < instanceCount; i++) {
        std::uint32_t off = indices[i] * stride;
        if (off + stride <= vtxSize)
          std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
      }
      if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
      glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(scratchData_.size()),
                   scratchData_.data(), GL_STREAM_DRAW);
      bindVbo = scratchVbo_;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
  GLint aPosUv = texQuadProg_.attribLocation("a_pos_uv");
  glEnableVertexAttribArray(static_cast<GLuint>(aPosUv));
  glVertexAttribPointer(static_cast<GLuint>(aPosUv), 4, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(stride), nullptr);
  glVertexAttribDivisor(static_cast<GLuint>(aPosUv), 1);

  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
  stats.drawCalls++;

  glVertexAttribDivisor(static_cast<GLuint>(aPosUv), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(aPosUv));
  glBindTexture(GL_TEXTURE_2D, 0);
}

// D78: Draw a thin border rectangle around a pane's clip region
void Renderer::drawPaneBorder(const Pane& pane, int viewW, int viewH) {
  if (renderStyle_.paneBorderWidth <= 0.0f) return;

  const auto& r = pane.region;
  const float* c = renderStyle_.paneBorderColor;

  // Build 4-edge line loop as GL_LINES (8 vertices = 4 line segments)
  float verts[] = {
    r.clipXMin, r.clipYMin,  r.clipXMax, r.clipYMin,  // bottom
    r.clipXMax, r.clipYMin,  r.clipXMax, r.clipYMax,  // right
    r.clipXMax, r.clipYMax,  r.clipXMin, r.clipYMax,  // top
    r.clipXMin, r.clipYMax,  r.clipXMin, r.clipYMin   // left
  };

  pos2Prog_.use();
  pos2Prog_.setUniformMat3(pos2Prog_.uniformLocation("u_transform"), kIdentityMat3);
  pos2Prog_.setUniformVec4(pos2Prog_.uniformLocation("u_color"), c[0], c[1], c[2], c[3]);

  GLuint tmpVbo;
  glGenBuffers(1, &tmpVbo);
  glBindBuffer(GL_ARRAY_BUFFER, tmpVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);

  GLint aPos = pos2Prog_.attribLocation("a_pos");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  glLineWidth(renderStyle_.paneBorderWidth);
  glDrawArrays(GL_LINES, 0, 8);

  glDisableVertexAttribArray(static_cast<GLuint>(aPos));
  glDeleteBuffers(1, &tmpVbo);
}

// D78: Draw separator lines between consecutive panes
void Renderer::drawPaneSeparators(const Scene& scene, int viewW, int viewH) {
  if (renderStyle_.separatorWidth <= 0.0f) return;

  auto pIds = scene.paneIds();
  if (pIds.size() < 2) return;

  const float* c = renderStyle_.separatorColor;
  std::vector<float> verts;

  // Draw horizontal lines at boundaries between consecutive panes
  for (std::size_t i = 0; i + 1 < pIds.size(); ++i) {
    const Pane* upper = scene.getPane(pIds[i]);
    const Pane* lower = scene.getPane(pIds[i + 1]);
    if (!upper || !lower) continue;

    // Separator at the boundary between panes
    float sepY = (upper->region.clipYMin + lower->region.clipYMax) * 0.5f;
    float xMin = std::min(upper->region.clipXMin, lower->region.clipXMin);
    float xMax = std::max(upper->region.clipXMax, lower->region.clipXMax);

    verts.push_back(xMin); verts.push_back(sepY);
    verts.push_back(xMax); verts.push_back(sepY);
  }

  if (verts.empty()) return;

  pos2Prog_.use();
  pos2Prog_.setUniformMat3(pos2Prog_.uniformLocation("u_transform"), kIdentityMat3);
  pos2Prog_.setUniformVec4(pos2Prog_.uniformLocation("u_color"), c[0], c[1], c[2], c[3]);

  GLuint tmpVbo;
  glGenBuffers(1, &tmpVbo);
  glBindBuffer(GL_ARRAY_BUFFER, tmpVbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
               verts.data(), GL_STREAM_DRAW);

  GLint aPos = pos2Prog_.attribLocation("a_pos");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  glLineWidth(renderStyle_.separatorWidth);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(verts.size() / 2));

  glDisableVertexAttribArray(static_cast<GLuint>(aPos));
  glDeleteBuffers(1, &tmpVbo);
}

Stats Renderer::render(const Scene& scene, GpuBufferManager& gpuBufs,
                       int viewW, int viewH) {
  Stats stats{};
  if (!inited_) return stats;

  uploadAtlasIfDirty();

  glViewport(0, 0, viewW, viewH);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  // Enable blending for SDF text
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindVertexArray(vao_);

  // Walk all draw items (pane → layer → drawItem).
  glEnable(GL_SCISSOR_TEST);

  for (Id paneId : scene.paneIds()) {
    const Pane* pane = scene.getPane(paneId);
    if (!pane) continue;

    // Convert pane clip region to pixel scissor rect
    int sx = static_cast<int>(std::round((pane->region.clipXMin + 1.0f) / 2.0f * viewW));
    int sy = static_cast<int>(std::round((pane->region.clipYMin + 1.0f) / 2.0f * viewH));
    int sx2 = static_cast<int>(std::round((pane->region.clipXMax + 1.0f) / 2.0f * viewW));
    int sy2 = static_cast<int>(std::round((pane->region.clipYMax + 1.0f) / 2.0f * viewH));
    glScissor(sx, sy, sx2 - sx, sy2 - sy);

    // Per-pane clear color (D10.4) + stencil reset per pane (D29.2)
    if (pane->hasClearColor) {
      glClearColor(pane->clearColor[0], pane->clearColor[1],
                   pane->clearColor[2], pane->clearColor[3]);
      glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    for (Id layerId : scene.layerIds()) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;

      for (Id diId : scene.drawItemIds()) {
        const DrawItem* di = scene.getDrawItem(diId);
        if (!di || di->layerId != layerId) continue;
        if (di->pipeline.empty()) continue;
        if (!di->visible) continue;  // D14.2: skip invisible DrawItems

        // Frustum culling (D10.5)
        const Geometry* cullGeo = scene.getGeometry(di->geometryId);
        if (cullGeo && cullGeo->boundsValid && di->transformId != 0) {
          const Transform* xf = scene.getTransform(di->transformId);
          if (xf) {
            float cMinX = xf->mat3[0] * cullGeo->boundsMin[0] + xf->mat3[6];
            float cMinY = xf->mat3[4] * cullGeo->boundsMin[1] + xf->mat3[7];
            float cMaxX = xf->mat3[0] * cullGeo->boundsMax[0] + xf->mat3[6];
            float cMaxY = xf->mat3[4] * cullGeo->boundsMax[1] + xf->mat3[7];
            if (cMaxX < pane->region.clipXMin || cMinX > pane->region.clipXMax ||
                cMaxY < pane->region.clipYMin || cMinY > pane->region.clipYMax) {
              stats.culledDrawCalls++;
              continue;
            }
          }
        }

        // D29.1: per-DrawItem blend mode
        applyBlendMode(di->blendMode);

        // D29.2: stencil-based clipping
        if (di->isClipSource) {
          glEnable(GL_STENCIL_TEST);
          glStencilFunc(GL_ALWAYS, 1, 0xFF);
          glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
          glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        } else if (di->useClipMask) {
          glEnable(GL_STENCIL_TEST);
          glStencilFunc(GL_EQUAL, 1, 0xFF);
          glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
          glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        } else {
          glDisable(GL_STENCIL_TEST);
          glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }

        if (di->pipeline == "triSolid@1") {
          drawPos2(*di, scene, gpuBufs, GL_TRIANGLES, stats);
        } else if (di->pipeline == "line2d@1") {
          drawPos2(*di, scene, gpuBufs, GL_LINES, stats);
        } else if (di->pipeline == "points@1") {
          drawPos2(*di, scene, gpuBufs, GL_POINTS, stats);
        } else if (di->pipeline == "instancedRect@1") {
          drawInstancedRect(*di, scene, gpuBufs, viewW, viewH, stats);
        } else if (di->pipeline == "instancedCandle@1") {
          drawInstancedCandle(*di, scene, gpuBufs, viewW, viewH, stats);
        } else if (di->pipeline == "textSDF@1") {
          drawTextSdf(*di, scene, gpuBufs, stats);
        } else if (di->pipeline == "lineAA@1") {
          drawLineAA(*di, scene, gpuBufs, viewW, viewH, stats);
        } else if (di->pipeline == "triGradient@1") {
          drawTriGradient(*di, scene, gpuBufs, stats);
        } else if (di->pipeline == "triAA@1") {
          drawTriAA(*di, scene, gpuBufs, stats);
        } else if (di->pipeline == "texturedQuad@1") {
          drawTexturedQuad(*di, scene, gpuBufs, viewW, viewH, stats);
        }

        // Restore color mask after clip source draw
        if (di->isClipSource) {
          glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
      }
    }
  }

  // D78: Draw pane borders (with scissor still active per-pane)
  for (Id paneId : scene.paneIds()) {
    const Pane* pane = scene.getPane(paneId);
    if (!pane) continue;

    // Expand scissor slightly to allow border to draw on edges
    int bsx = static_cast<int>(std::round((pane->region.clipXMin + 1.0f) / 2.0f * viewW)) - 1;
    int bsy = static_cast<int>(std::round((pane->region.clipYMin + 1.0f) / 2.0f * viewH)) - 1;
    int bsx2 = static_cast<int>(std::round((pane->region.clipXMax + 1.0f) / 2.0f * viewW)) + 1;
    int bsy2 = static_cast<int>(std::round((pane->region.clipYMax + 1.0f) / 2.0f * viewH)) + 1;
    glScissor(bsx, bsy, bsx2 - bsx, bsy2 - bsy);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawPaneBorder(*pane, viewW, viewH);
  }

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);

  // D78: Draw pane separators (full viewport, no scissor)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  drawPaneSeparators(scene, viewW, viewH);

  // Restore default blend mode
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glFlush();
  return stats;
}

// ---- D29.1: Blend mode helper ----

void Renderer::applyBlendMode(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::Additive:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      break;
    case BlendMode::Multiply:
      glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::Screen:
      glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
  }
}

// ---- D29.3: GPU picking ----

void Renderer::ensurePickFbo(int w, int h) {
  if (pickFbo_ && pickW_ == w && pickH_ == h) return;

  if (pickRbo_) glDeleteRenderbuffers(1, &pickRbo_);
  if (pickFbo_) glDeleteFramebuffers(1, &pickFbo_);

  glGenFramebuffers(1, &pickFbo_);
  glGenRenderbuffers(1, &pickRbo_);

  glBindRenderbuffer(GL_RENDERBUFFER, pickRbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);

  glBindFramebuffer(GL_FRAMEBUFFER, pickFbo_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, pickRbo_);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  pickW_ = w;
  pickH_ = h;
}

void Renderer::drawPick(const DrawItem& di, const Scene& scene,
                         GpuBufferManager& gpuBufs, int viewW, int viewH,
                         float pickR, float pickG, float pickB) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;
  GLuint vbo = gpuBufs.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return;

  const float* xform = resolveTransform(di, scene);

  // Non-instanced pipelines: triSolid, line2d, points, triAA, triGradient
  if (di.pipeline == "triSolid@1" || di.pipeline == "line2d@1" ||
      di.pipeline == "points@1" || di.pipeline == "triAA@1" ||
      di.pipeline == "triGradient@1") {
    pickFlatProg_.use();
    pickFlatProg_.setUniformMat3(pickFlatProg_.uniformLocation("u_transform"), xform);
    pickFlatProg_.setUniformVec4(pickFlatProg_.uniformLocation("u_pickColor"),
                                  pickR, pickG, pickB, 1.0f);

    // Determine stride based on format
    GLsizei stride = 0;
    if (di.pipeline == "triAA@1") stride = 12;
    else if (di.pipeline == "triGradient@1") stride = 24;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLint aPos = pickFlatProg_.attribLocation("a_pos");
    glEnableVertexAttribArray(static_cast<GLuint>(aPos));
    glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, stride, nullptr);

    GLenum mode = GL_TRIANGLES;
    if (di.pipeline == "line2d@1") { mode = GL_LINES; glLineWidth(di.lineWidth); }
    else if (di.pipeline == "points@1") mode = GL_POINTS;

    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      GLuint ibo = gpuBufs.getGlBuffer(geo->indexBufferId);
      if (ibo) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glDrawElements(mode, static_cast<GLsizei>(geo->indexCount),
                       GL_UNSIGNED_INT, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      }
    } else {
      glDrawArrays(mode, 0, static_cast<GLsizei>(geo->vertexCount));
    }
    glDisableVertexAttribArray(static_cast<GLuint>(aPos));

  } else if (di.pipeline == "instancedRect@1") {
    pickInstRectProg_.use();
    pickInstRectProg_.setUniformMat3(pickInstRectProg_.uniformLocation("u_transform"), xform);
    pickInstRectProg_.setUniformVec4(pickInstRectProg_.uniformLocation("u_pickColor"),
                                      pickR, pickG, pickB, 1.0f);

    GLuint bindVbo = vbo;
    GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
    std::uint32_t stride = strideOf(VertexFormat::Rect4);
    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
      const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
      std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
      if (idxData && vtxData) {
        instanceCount = static_cast<GLsizei>(geo->indexCount);
        scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
        for (GLsizei i = 0; i < instanceCount; i++) {
          std::uint32_t off = indices[i] * stride;
          if (off + stride <= vtxSize)
            std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
        if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scratchData_.size()),
                     scratchData_.data(), GL_STREAM_DRAW);
        bindVbo = scratchVbo_;
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
    GLint aRect = pickInstRectProg_.attribLocation("a_rect");
    glEnableVertexAttribArray(static_cast<GLuint>(aRect));
    glVertexAttribPointer(static_cast<GLuint>(aRect), 4, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(stride), nullptr);
    glVertexAttribDivisor(static_cast<GLuint>(aRect), 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
    glVertexAttribDivisor(static_cast<GLuint>(aRect), 0);
    glDisableVertexAttribArray(static_cast<GLuint>(aRect));

  } else if (di.pipeline == "instancedCandle@1") {
    pickInstCandleProg_.use();
    pickInstCandleProg_.setUniformMat3(pickInstCandleProg_.uniformLocation("u_transform"), xform);
    pickInstCandleProg_.setUniformVec4(pickInstCandleProg_.uniformLocation("u_pickColor"),
                                        pickR, pickG, pickB, 1.0f);
    glUniform2f(pickInstCandleProg_.uniformLocation("u_viewportSize"),
                static_cast<float>(viewW), static_cast<float>(viewH));

    GLuint bindVbo = vbo;
    GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
    GLsizei stride = static_cast<GLsizei>(strideOf(VertexFormat::Candle6));
    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
      const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
      std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
      if (idxData && vtxData) {
        instanceCount = static_cast<GLsizei>(geo->indexCount);
        scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
        for (GLsizei i = 0; i < instanceCount; i++) {
          std::uint32_t off = indices[i] * static_cast<std::uint32_t>(stride);
          if (off + static_cast<std::uint32_t>(stride) <= vtxSize)
            std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
        if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scratchData_.size()),
                     scratchData_.data(), GL_STREAM_DRAW);
        bindVbo = scratchVbo_;
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
    GLint aC0 = pickInstCandleProg_.attribLocation("a_c0");
    glEnableVertexAttribArray(static_cast<GLuint>(aC0));
    glVertexAttribPointer(static_cast<GLuint>(aC0), 4, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribDivisor(static_cast<GLuint>(aC0), 1);
    GLint aC1 = pickInstCandleProg_.attribLocation("a_c1");
    glEnableVertexAttribArray(static_cast<GLuint>(aC1));
    glVertexAttribPointer(static_cast<GLuint>(aC1), 2, GL_FLOAT, GL_FALSE,
                          stride, reinterpret_cast<const void*>(16));
    glVertexAttribDivisor(static_cast<GLuint>(aC1), 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 12, instanceCount);
    glVertexAttribDivisor(static_cast<GLuint>(aC0), 0);
    glVertexAttribDivisor(static_cast<GLuint>(aC1), 0);
    glDisableVertexAttribArray(static_cast<GLuint>(aC0));
    glDisableVertexAttribArray(static_cast<GLuint>(aC1));

  } else if (di.pipeline == "texturedQuad@1") {
    // D41: pick for texturedQuad uses same vertex layout as instRect
    pickInstRectProg_.use();
    pickInstRectProg_.setUniformMat3(pickInstRectProg_.uniformLocation("u_transform"), xform);
    pickInstRectProg_.setUniformVec4(pickInstRectProg_.uniformLocation("u_pickColor"),
                                      pickR, pickG, pickB, 1.0f);

    GLuint bindVbo = vbo;
    GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
    std::uint32_t stride = strideOf(VertexFormat::Pos2Uv4);
    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
      const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
      std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
      if (idxData && vtxData) {
        instanceCount = static_cast<GLsizei>(geo->indexCount);
        scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
        for (GLsizei i = 0; i < instanceCount; i++) {
          std::uint32_t off = indices[i] * stride;
          if (off + stride <= vtxSize)
            std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
        if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scratchData_.size()),
                     scratchData_.data(), GL_STREAM_DRAW);
        bindVbo = scratchVbo_;
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
    GLint aRect = pickInstRectProg_.attribLocation("a_rect");
    glEnableVertexAttribArray(static_cast<GLuint>(aRect));
    glVertexAttribPointer(static_cast<GLuint>(aRect), 4, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(stride), nullptr);
    glVertexAttribDivisor(static_cast<GLuint>(aRect), 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
    glVertexAttribDivisor(static_cast<GLuint>(aRect), 0);
    glDisableVertexAttribArray(static_cast<GLuint>(aRect));

  } else if (di.pipeline == "lineAA@1") {
    pickLineAAProg_.use();
    pickLineAAProg_.setUniformMat3(pickLineAAProg_.uniformLocation("u_transform"), xform);
    pickLineAAProg_.setUniformVec4(pickLineAAProg_.uniformLocation("u_pickColor"),
                                    pickR, pickG, pickB, 1.0f);
    float lineWidthClip = (viewW > 0) ? (di.lineWidth / static_cast<float>(viewW) * 2.0f) : 0.01f;
    pickLineAAProg_.setUniformFloat(pickLineAAProg_.uniformLocation("u_lineWidth"), lineWidthClip);
    float aaWidthClip = (viewW > 0) ? (1.5f / static_cast<float>(viewW) * 2.0f) : 0.005f;
    pickLineAAProg_.setUniformFloat(pickLineAAProg_.uniformLocation("u_aaWidth"), aaWidthClip);

    GLuint bindVbo = vbo;
    GLsizei instanceCount = static_cast<GLsizei>(geo->vertexCount);
    std::uint32_t stride = strideOf(VertexFormat::Rect4);
    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      const auto* idxData = gpuBufs.getCpuData(geo->indexBufferId);
      const auto* vtxData = gpuBufs.getCpuData(geo->vertexBufferId);
      std::uint32_t vtxSize = gpuBufs.getCpuDataSize(geo->vertexBufferId);
      if (idxData && vtxData) {
        instanceCount = static_cast<GLsizei>(geo->indexCount);
        scratchData_.resize(static_cast<std::size_t>(instanceCount) * stride);
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idxData);
        for (GLsizei i = 0; i < instanceCount; i++) {
          std::uint32_t off = indices[i] * stride;
          if (off + stride <= vtxSize)
            std::memcpy(scratchData_.data() + i * stride, vtxData + off, stride);
        }
        if (!scratchVbo_) glGenBuffers(1, &scratchVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, scratchVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scratchData_.size()),
                     scratchData_.data(), GL_STREAM_DRAW);
        bindVbo = scratchVbo_;
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, bindVbo);
    GLint aRect = pickLineAAProg_.attribLocation("a_rect");
    glEnableVertexAttribArray(static_cast<GLuint>(aRect));
    glVertexAttribPointer(static_cast<GLuint>(aRect), 4, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(stride), nullptr);
    glVertexAttribDivisor(static_cast<GLuint>(aRect), 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount);
    glVertexAttribDivisor(static_cast<GLuint>(aRect), 0);
    glDisableVertexAttribArray(static_cast<GLuint>(aRect));
  }
  // textSDF@1 is skipped for picking
}

PickResult Renderer::renderPick(const Scene& scene, GpuBufferManager& gpuBufs,
                                 int viewW, int viewH, int pickX, int pickY) {
  PickResult result;
  if (!inited_) return result;

  ensurePickFbo(viewW, viewH);

  glBindFramebuffer(GL_FRAMEBUFFER, pickFbo_);
  glViewport(0, 0, viewW, viewH);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_BLEND);

  glBindVertexArray(vao_);
  glEnable(GL_SCISSOR_TEST);

  for (Id paneId : scene.paneIds()) {
    const Pane* pane = scene.getPane(paneId);
    if (!pane) continue;

    int sx = static_cast<int>(std::round((pane->region.clipXMin + 1.0f) / 2.0f * viewW));
    int sy = static_cast<int>(std::round((pane->region.clipYMin + 1.0f) / 2.0f * viewH));
    int sx2 = static_cast<int>(std::round((pane->region.clipXMax + 1.0f) / 2.0f * viewW));
    int sy2 = static_cast<int>(std::round((pane->region.clipYMax + 1.0f) / 2.0f * viewH));
    glScissor(sx, sy, sx2 - sx, sy2 - sy);

    for (Id layerId : scene.layerIds()) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;

      for (Id diId : scene.drawItemIds()) {
        const DrawItem* di = scene.getDrawItem(diId);
        if (!di || di->layerId != layerId) continue;
        if (di->pipeline.empty() || !di->visible) continue;
        if (di->isClipSource) continue;

        // Encode ID as R8G8B8
        float r = static_cast<float>(di->id & 0xFF) / 255.0f;
        float g = static_cast<float>((di->id >> 8) & 0xFF) / 255.0f;
        float b = static_cast<float>((di->id >> 16) & 0xFF) / 255.0f;

        drawPick(*di, scene, gpuBufs, viewW, viewH, r, g, b);
      }
    }
  }

  glDisable(GL_SCISSOR_TEST);

  // Read pixel at cursor
  if (pickX >= 0 && pickX < viewW && pickY >= 0 && pickY < viewH) {
    unsigned char pixel[4];
    glReadPixels(pickX, pickY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    Id id = static_cast<Id>(pixel[0]) |
            (static_cast<Id>(pixel[1]) << 8) |
            (static_cast<Id>(pixel[2]) << 16);
    result.drawItemId = id;

    // D42: emit GeometryClicked if we hit something
    if (id != 0 && eventBus_) {
      EventData ev;
      ev.type = EventType::GeometryClicked;
      ev.targetId = id;
      ev.payload[0] = static_cast<double>(pickX);
      ev.payload[1] = static_cast<double>(pickY);
      eventBus_->emit(ev);
    }
  }

  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return result;
}

} // namespace dc
