// ENC-981 — lineAA@1 delivered line width must be angle- and aspect-invariant.
//
// WHY THIS IS A CPU TEST AND NOT A DAWN RENDER TEST
// -------------------------------------------------
// The renderer (dc_gpu) only builds with -DDC_FETCH_DAWN=ON, which nobody
// configures by default (Dawn is a 30-60 min from-source build). d28_1_dawn_
// lineaa.cpp — the only lineAA render test — is therefore never compiled in the
// build people actually run, AND it used a 128x128 SQUARE viewport, where this
// bug is *exactly zero*. So the regression guard has to live here, in the
// default `cmake -B build && ctest` build.
//
// This does not test a hand-copied transcription of the shader: the quad
// expansion is defined ONCE as DC_LINEAA_QUAD_EXPAND (dc/render/LineAAQuad.hpp),
// written in the syntactic intersection of WGSL and C++. The Dawn shader
// modules paste the stringified macro; lineAAQuadCorner() below compiles the
// very same tokens. Editing one edits both.
//
// THE BUG
// -------
// The perpendicular used to be normalized in CLIP space and then applied with
// the same magnitude on both axes, with lineWidth converted px -> clip by
// dividing by viewW only. On a non-square viewport that both SHEARS the quad
// and changes its width with the line's angle:
//
//     delivered(theta) = lineWidth * sqrt((H/W)^2 cos^2(theta) + sin^2(theta))
//
// At 1600x900, lineWidth=2: 1.125px horizontal (56.2%), 1.623px at 45deg
// (81.1%), 2.000px vertical (100%). Vertical lines were exactly right and
// horizontal gridlines were the worst — so the symptom was orientation-
// dependent line WEIGHT, not stair-stepping.
//
// THE FIX
// -------
// Build the perpendicular in PIXEL space: scale the clip direction by
// viewport/2, normalize THERE, rotate 90deg, scale by the half width in pixels,
// then divide the offset by viewport/2 to get back to clip. The offset is then
// perpendicular to the line as drawn, with the requested pixel length, for any
// direction and any aspect.
#include "dc/render/LineAAQuad.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int passed = 0;
int failed = 0;

void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

constexpr float kPi = 3.14159265358979323846f;

struct Measured {
  float widthPx;  // delivered width across the line, in pixels
  float shearPx;  // component of the corner separation ALONG the line (must be 0)
};

// Drive the SHARED quad expansion the way the vertex shader does, then measure
// the resulting quad in pixel space.
//
// `angleDeg` is the angle of the segment ON SCREEN (in pixels), which is what a
// human sees: 0 = horizontal, 90 = vertical. A screen direction (cos a, sin a)
// corresponds to the clip direction (cos a / (W/2), sin a / (H/2)).
Measured measure(float W, float H, float lineWidthPx, float angleDeg) {
  using dc::lineaa::Vec2;
  const dc::LineAAParams lp = dc::lineAAParams(lineWidthPx);
  const Vec2 vpHalf(W * 0.5f, H * 0.5f);

  const float a = angleDeg * kPi / 180.0f;
  float cdx = std::cos(a) / vpHalf.x;
  float cdy = std::sin(a) / vpHalf.y;
  const float n = std::sqrt(cdx * cdx + cdy * cdy);
  cdx /= n;
  cdy /= n;

  // A segment through the origin, half a clip unit long, at that screen angle.
  const Vec2 c0(0.0f, 0.0f);
  const Vec2 c1(cdx * 0.5f, cdy * 0.5f);

  // The NOMINAL edges of the line are the corners where v_dist == +/-1, i.e.
  // uv.y == +/- hw/totalHalf. (The quad itself extends further, into the AA
  // fringe.) Sample mid-segment so the endpoints play no part.
  const Vec2 top = dc::lineaa::lineAAQuadCorner(
      c0, c1, Vec2(0.5f, lp.nominalEdgeUv), lp.totalHalfPx, vpHalf);
  const Vec2 bot = dc::lineaa::lineAAQuadCorner(
      c0, c1, Vec2(0.5f, -lp.nominalEdgeUv), lp.totalHalfPx, vpHalf);

  // Clip -> pixels.
  const float ox = (top.x - bot.x) * vpHalf.x;
  const float oy = (top.y - bot.y) * vpHalf.y;

  // Pixel-space line direction and its normal.
  const float dx = cdx * vpHalf.x;
  const float dy = cdy * vpHalf.y;
  const float dl = std::sqrt(dx * dx + dy * dy);

  Measured m;
  m.widthPx = std::fabs(ox * (-dy / dl) + oy * (dx / dl));
  m.shearPx = std::fabs(ox * (dx / dl) + oy * (dy / dl));
  return m;
}

// The pre-ENC-981 behaviour, in closed form. Kept ONLY so the regression table
// can print what the bug used to deliver — nothing in the shipping path uses it.
float legacyWidth(float W, float H, float lineWidthPx, float angleDeg) {
  const float a = angleDeg * kPi / 180.0f;
  const float r = H / W;
  return lineWidthPx *
         std::sqrt(r * r * std::cos(a) * std::cos(a) + std::sin(a) * std::sin(a));
}

struct Viewport {
  const char* name;
  float w;
  float h;
};

}  // namespace

int main() {
  std::printf("=== ENC-981 lineAA delivered width (angle/aspect invariance) ===\n");

  // The stringified WGSL must actually carry the pixel-space normalization; if
  // stringification silently produced something else the shader would be broken
  // while the C++ side still passed.
  const char* wgsl = dc::kLineAAQuadExpandWgsl;
  check(wgsl != nullptr && std::strstr(wgsl, "vpHalf") != nullptr &&
            std::strstr(wgsl, "dirPx / lenPx") != nullptr &&
            std::strstr(wgsl, "offsetPx / vpHalf") != nullptr,
        "shared WGSL block carries the pixel-space perpendicular");

  // ---- The ticket's reference case, printed before/after. -----------------
  std::printf("\n  1600x900, lineWidth=2.0 (ticket's reference case)\n");
  std::printf("  %-8s %-14s %-14s\n", "angle", "before(px)", "after(px)");
  for (float ang : {0.0f, 45.0f, 90.0f}) {
    const float before = legacyWidth(1600.0f, 900.0f, 2.0f, ang);
    const Measured after = measure(1600.0f, 900.0f, 2.0f, ang);
    std::printf("  %-8.0f %-6.3f (%4.1f%%) %-6.3f (%5.1f%%)\n", ang, before,
                100.0f * before / 2.0f, after.widthPx,
                100.0f * after.widthPx / 2.0f);
  }
  std::printf("\n");

  // Guard the characterization itself: if this ever stops matching, the "before"
  // column above is lying about what the bug did.
  check(std::fabs(legacyWidth(1600.0f, 900.0f, 2.0f, 0.0f) - 1.125f) < 1e-4f &&
            std::fabs(legacyWidth(1600.0f, 900.0f, 2.0f, 45.0f) - 1.6226f) <
                1e-3f &&
            std::fabs(legacyWidth(1600.0f, 900.0f, 2.0f, 90.0f) - 2.0f) < 1e-4f,
        "legacy clip-space closed form reproduces 1.125 / 1.623 / 2.000");

  // ---- The actual regression assertions. ----------------------------------
  // NON-SQUARE viewports first — this is the axis the old d28_1 test could not
  // see, because at aspect 1.0 the bug is identically zero.
  const Viewport viewports[] = {
      {"1600x900 (16:9)", 1600.0f, 900.0f},
      {"900x1600 (9:16)", 900.0f, 1600.0f},
      {"1920x1080", 1920.0f, 1080.0f},
      {"640x1400 (tall)", 640.0f, 1400.0f},
      {"128x128 (square)", 128.0f, 128.0f},
  };
  const float lineWidths[] = {1.0f, 2.0f, 4.0f, 12.0f};

  for (const Viewport& vp : viewports) {
    for (float lw : lineWidths) {
      float worstErr = 0.0f;
      float worstShear = 0.0f;
      float worstAngle = 0.0f;
      // Sweep a full half-turn (a line and its reverse are the same line).
      for (int i = 0; i <= 72; ++i) {
        const float ang = static_cast<float>(i) * 2.5f;
        const Measured m = measure(vp.w, vp.h, lw, ang);
        const float err = std::fabs(m.widthPx - lw);
        if (err > worstErr) {
          worstErr = err;
          worstAngle = ang;
        }
        if (m.shearPx > worstShear) worstShear = m.shearPx;
      }
      char name[192];
      std::snprintf(name, sizeof(name),
                    "%s lineWidth=%.0f: width==%.0f at every angle "
                    "(worst err %.5fpx @ %.1fdeg)",
                    vp.name, lw, lw, worstErr, worstAngle);
      // Tolerance is float round-off, not a fudge factor: the 1600x900
      // horizontal case was off by 0.875px (44%) before the fix.
      check(worstErr < 1e-3f, name);

      std::snprintf(name, sizeof(name),
                    "%s lineWidth=%.0f: offset is perpendicular (no shear, "
                    "worst %.5fpx)",
                    vp.name, lw, worstShear);
      check(worstShear < 1e-3f, name);
    }
  }

  // ---- The full quad (AA fringe included) is also pixel-exact. ------------
  {
    const dc::LineAAParams lp = dc::lineAAParams(2.0f);
    const float expectedTotal = 2.0f * lp.totalHalfPx;  // lineWidth + 2*aaWidth
    float worst = 0.0f;
    for (int i = 0; i <= 72; ++i) {
      const float ang = static_cast<float>(i) * 2.5f;
      // Re-measure at |uv.y| == 1 by asking for a "line" whose nominal edge IS
      // the quad edge: the expansion is linear in uv.y, so the full quad width
      // is the nominal width scaled by 1 / nominalEdgeUv.
      const Measured m = measure(1600.0f, 900.0f, 2.0f, ang);
      const float total = m.widthPx / lp.nominalEdgeUv;
      worst = std::max(worst, std::fabs(total - expectedTotal));
    }
    char name[160];
    std::snprintf(name, sizeof(name),
                  "1600x900: full AA quad is %.1fpx wide at every angle "
                  "(worst err %.5fpx)",
                  expectedTotal, worst);
    check(worst < 1e-3f, name);
  }

  // ---- fringeEdge is a pure ratio, so it is unit-independent. -------------
  check(std::fabs(dc::lineAAParams(2.0f).fringeEdge - (1.0f + 1.5f) / 1.0f) <
            1e-5f,
        "fringeEdge == (hw + aa) / hw");

  std::printf("=== ENC-981 lineAA width: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
