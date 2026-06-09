// ENC-483 (P1.3) — GlTriSolidBackend implementation. See header.
//
// The body is the triSolid path of the old Renderer::drawPos2 helper, verbatim
// in behaviour (program use -> u_transform/u_color/u_pointSize -> a_pos pointer
// -> indexed-or-arrays GL_TRIANGLES draw). Only the host moved: from a Renderer
// member into this registered backend.
#include "dc/gl/GlTriSolidBackend.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"
#include <glad/gl.h>

namespace dc {

namespace {

const float kIdentityMat3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

const float* resolveTransform(const DrawItem& di, const Scene& scene) {
  if (!di.transformId) return kIdentityMat3;
  const Transform* t = scene.getTransform(di.transformId);
  return t ? t->mat3 : kIdentityMat3;
}

// The shared Pos2 shader (triSolid + line2d + points). Duplicated from
// Renderer.cpp on purpose: triSolid now owns its own program here, while
// line2d/points keep using Renderer's pos2Prog_ until they are ported
// (ENC-486+). Identical source => identical output.
const char* kPos2Vert = R"GLSL(
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

const char* kPos2Frag = R"GLSL(
#version 330 core
out vec4 outColor;
uniform vec4 u_color;
void main() {
    outColor = u_color;
}
)GLSL";

}  // namespace

bool GlTriSolidBackend::init(GpuDevice& /*device*/) {
  // GpuDevice::createPipeline is still an ENC-484 stub for the GL draw path, so
  // this backend compiles the program directly for now (same as the old
  // Renderer::init did for pos2Prog_). The `device` reference would be retained
  // here once createPipeline/createBindGroup are live (ENC-484).
  if (!prog_.build(kPos2Vert, kPos2Frag)) {
    return false;
  }
  return true;
}

BackendStats GlTriSolidBackend::renderDrawItem(GpuDevice& /*device*/,
                                               const Scene& scene,
                                               GpuBufferManager& gpu,
                                               const DrawItem& di,
                                               int /*viewW*/, int /*viewH*/) {
  BackendStats stats{};

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;
  GLuint vbo = gpu.getGlBuffer(geo->vertexBufferId);
  if (!vbo) return stats;

  prog_.use();

  const float* xform = resolveTransform(di, scene);
  prog_.setUniformMat3(prog_.uniformLocation("u_transform"), xform);
  prog_.setUniformVec4(prog_.uniformLocation("u_color"),
                       di.color[0], di.color[1], di.color[2], di.color[3]);
  prog_.setUniformFloat(prog_.uniformLocation("u_pointSize"), di.pointSize);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLint aPos = prog_.attribLocation("a_pos");
  glEnableVertexAttribArray(static_cast<GLuint>(aPos));
  glVertexAttribPointer(static_cast<GLuint>(aPos), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

  // D26: indexed draw path.
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    GLuint ibo = gpu.getGlBuffer(geo->indexBufferId);
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
  return stats;
}

}  // namespace dc
