// ENC-981 — single source of truth for the lineAA@1 quad-expansion math.
//
// WebGPU has no native line width, so every lineAA segment is expanded in the
// vertex shader into a screen-aligned quad. The perpendicular offset MUST be
// built in PIXEL space: a perpendicular normalized in clip space and then
// offset by an equal amount on both axes is only perpendicular (and only the
// requested width) when the viewport is square. On a non-square viewport the
// quad both shears and loses width, worst for horizontal lines:
//
//     delivered_width(theta) = lineWidth * sqrt((H/W)^2 * cos^2(theta) + sin^2(theta))
//
// i.e. 2px at 1600x900 delivered 1.125px horizontally and 2.000px vertically.
// Computing the perpendicular in pixel space makes the delivered width exactly
// `lineWidth` at every angle and every aspect ratio (see dc_enc981_lineaa_width).
//
// This header lives in `dc` (the default, Dawn-free build) on purpose: the
// renderer only builds with -DDC_FETCH_DAWN=ON, so a Dawn-gated test would
// never run in the build people actually configure. The math therefore lives
// here where a plain CTest can measure it.
//
// ---------------------------------------------------------------------------
// KEEPING THE SHADER AND THE C++ IN SYNC
// ---------------------------------------------------------------------------
// `DC_LINEAA_QUAD_EXPAND` below is the expansion body written in the syntactic
// intersection of WGSL and C++. It is used TWICE, from the one definition:
//
//   * stringified into `kLineAAQuadExpandWgsl` and pasted into the lineAA and
//     pickLineAA WGSL modules (DawnLineAABackend / DawnPickBackend);
//   * compiled as C++ by `lineAAQuadCorner()` below, against the tiny
//     WGSL-alike shim in this file (`vec2<f32>`, `length`, `mix`, `select`,
//     and `let` -> `auto`).
//
// So the CPU test does not test a *copy* of the shader math — it tests the
// exact same tokens the shader is built from. Drift is impossible: editing the
// macro edits both. (A true single source is only possible for this statement
// block; the surrounding shader plumbing — attribute/uniform declarations,
// the y-flip, the varyings — is still hand-written per module.)
//
// The block consumes, and must have in scope:
//   c0, c1      : vec2<f32>  segment endpoints in clip space
//   uv          : vec2<f32>  unit-quad corner, x in {0,1} along, y in {-1,+1} across
//   halfWidthPx : f32        half-extent of the quad across the line, in PIXELS
//   vpHalf      : vec2<f32>  viewport * 0.5, in pixels; both components > 0
// and produces:
//   pos2        : vec2<f32>  the corner position in clip space
#ifndef DC_RENDER_LINEAAQUAD_HPP
#define DC_RENDER_LINEAAQUAD_HPP

#include <cmath>

// clang-format off
#define DC_LINEAA_QUAD_EXPAND                                                  \
  let dirClip = c1 - c0;                                                       \
  let dirPx = dirClip * vpHalf;                                                \
  let lenPx = length(dirPx);                                                   \
  let unitPx = select(vec2<f32>(1.0, 0.0), dirPx / lenPx, lenPx > 0.0001);     \
  let perpPx = vec2<f32>(-unitPx.y, unitPx.x);                                 \
  let offsetPx = perpPx * (uv.y * halfWidthPx);                                \
  let pos2 = mix(c0, c1, uv.x) + offsetPx / vpHalf;
// clang-format on

#define DC_LINEAA_STRINGIFY_(x) #x
#define DC_LINEAA_STRINGIFY(x) DC_LINEAA_STRINGIFY_(x)

// The same block as a STRING LITERAL, so a WGSL module can splice it in with
// plain adjacent-literal concatenation:
//     const char* kWgsl = R"W(...)W" DC_LINEAA_QUAD_EXPAND_WGSL R"W(...)W";
#define DC_LINEAA_QUAD_EXPAND_WGSL DC_LINEAA_STRINGIFY(DC_LINEAA_QUAD_EXPAND)

namespace dc {

// The WGSL text of the shared block. Stringified HERE, before the `let` shim
// exists, so the preprocessor cannot rewrite the WGSL keywords.
inline constexpr const char* kLineAAQuadExpandWgsl = DC_LINEAA_QUAD_EXPAND_WGSL;

namespace lineaa {

// --- WGSL-alike shim: just enough to compile the shared block as C++. -------
using f32 = float;

template <typename T>
struct vec2 {
  T x, y;
  vec2() : x(0), y(0) {}
  vec2(T ax, T ay) : x(ax), y(ay) {}
};

using Vec2 = vec2<f32>;

inline Vec2 operator+(Vec2 a, Vec2 b) { return Vec2(a.x + b.x, a.y + b.y); }
inline Vec2 operator-(Vec2 a, Vec2 b) { return Vec2(a.x - b.x, a.y - b.y); }
inline Vec2 operator*(Vec2 a, Vec2 b) { return Vec2(a.x * b.x, a.y * b.y); }
inline Vec2 operator/(Vec2 a, Vec2 b) { return Vec2(a.x / b.x, a.y / b.y); }
inline Vec2 operator*(Vec2 a, f32 s) { return Vec2(a.x * s, a.y * s); }
inline Vec2 operator/(Vec2 a, f32 s) { return Vec2(a.x / s, a.y / s); }
inline f32 length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline Vec2 mix(Vec2 a, Vec2 b, f32 t) {
  return Vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}
inline Vec2 select(Vec2 f, Vec2 t, bool cond) { return cond ? t : f; }

// C++ evaluation of the SAME tokens the WGSL is built from. See the note above.
#define let auto
inline Vec2 lineAAQuadCorner(Vec2 c0, Vec2 c1, Vec2 uv, f32 halfWidthPx,
                             Vec2 vpHalf) {
  DC_LINEAA_QUAD_EXPAND
  return pos2;
}
#undef let

}  // namespace lineaa

// --- Per-draw lineAA scalars, in PIXELS (shared by the render + pick paths).
//
// The AA fringe extends `kLineAAFringePx` beyond the nominal edge. `fringeEdge`
// is expressed in v_dist space (v_dist = 0 at the centre, 1 at the nominal
// edge) and is a pure ratio, so it is unchanged by the pixel-vs-clip units.
inline constexpr float kLineAAFringePx = 1.5f;

struct LineAAParams {
  float lineWidthPx;    // nominal width, pixels
  float aaWidthPx;      // AA fringe beyond the nominal edge, pixels
  float totalHalfPx;    // lineWidthPx/2 + aaWidthPx — the quad half-extent, px
  float fringeEdge;     // v_dist at which coverage reaches 0
  float nominalEdgeUv;  // |uv.y| of the corner where v_dist == 1
};

inline LineAAParams lineAAParams(float lineWidthPx) {
  LineAAParams p{};
  p.lineWidthPx = lineWidthPx;
  p.aaWidthPx = kLineAAFringePx;
  const float hw = lineWidthPx * 0.5f;
  p.totalHalfPx = hw + p.aaWidthPx;
  p.fringeEdge = (hw > 0.0001f) ? ((hw + p.aaWidthPx) / hw) : 2.0f;
  p.nominalEdgeUv = (p.totalHalfPx > 0.0001f) ? (hw / p.totalHalfPx) : 1.0f;
  return p;
}

}  // namespace dc

#endif  // DC_RENDER_LINEAAQUAD_HPP
