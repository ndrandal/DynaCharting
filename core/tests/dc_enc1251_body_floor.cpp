// ENC-1251 — the HOST half of the doji floor, in the DEFAULT build.
//
// The geometry decision itself is WGSL (dc::kCandleBodyFloorWgsl) and is proved
// on pixels by dc_enc1249_tier0_truthful case C, which needs Dawn. LIMITATIONS.md
// DC-L01 is the reason this file exists anyway: a rule checked only inside
// dc_gpu is a rule checked by a build almost nobody configures. The arithmetic
// that converts "two device pixels" into the clip number the shader compares
// against is plain C++, so it is checked here, where `ctest` actually runs it.
//
// What would break silently without this: the factor of two. Clip y spans
// [-1, 1] over `viewH` pixels, so ONE pixel is 2/viewH — drop that and the floor
// becomes one pixel tall, which is exactly the sub-pixel span the rule exists to
// avoid (no MSAA, pixel-centre sampling).

#include "dc/render/CandleBodyFloor.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failed = 0;

void check(bool ok, const char* what, const std::string& detail) {
  std::printf("  %s  %-58s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
  if (!ok) ++g_failed;
}

// The inverse of candleBodyMinHeightClip: clip units -> device pixels.
double pxOfClip(double clip, int viewH) { return clip * viewH * 0.5; }

}  // namespace

int main() {
  std::printf("=== ENC-1251: the candle body floor (host arithmetic) ===\n");

  check(dc::kMinBodyHeightPx == 2.0f, "the floor is two device pixels",
        std::string("kMinBodyHeightPx = ") +
            std::to_string(dc::kMinBodyHeightPx));

  // [1] The conversion round-trips to exactly kMinBodyHeightPx at every
  //     viewport height a chart is plausibly drawn at.
  const int heights[] = {64, 200, 256, 600, 800, 1200, 1440, 2160};
  bool allExact = true;
  for (int h : heights) {
    const double px = pxOfClip(dc::candleBodyMinHeightClip(h), h);
    if (std::fabs(px - dc::kMinBodyHeightPx) > 1e-4) {
      allExact = false;
      std::printf("        viewH=%4d -> %.6f px (want %.1f)\n", h, px,
                  dc::kMinBodyHeightPx);
    }
  }
  check(allExact, "[1] the clip floor is kMinBodyHeightPx px at every height",
        "8 viewport heights, 64..2160");

  // [2] The floor SHRINKS in clip units as the viewport grows — it is a pixel
  //     quantity, not a data-space or clip-space constant. A constant here
  //     would mean a different number of pixels on every chart.
  const float c256 = dc::candleBodyMinHeightClip(256);
  const float c1024 = dc::candleBodyMinHeightClip(1024);
  check(c256 > c1024 && std::fabs(c256 / c1024 - 4.0f) < 1e-5f,
        "[2] the clip floor scales inversely with the viewport",
        std::string("256 -> ") + std::to_string(c256) + ", 1024 -> " +
            std::to_string(c1024) + " (ratio 4)");

  // [3] No viewport means the rule is OFF, and off must be 0 — the shader
  //     compares `minH > 0.0`, so any other sentinel would silently apply a
  //     floor during an offscreen/degenerate draw.
  check(dc::candleBodyMinHeightClip(0) == 0.0f &&
            dc::candleBodyMinHeightClip(-1) == 0.0f,
        "[3] no viewport disables the rule", "viewH 0 and -1 both give 0");

  // [4] The shared WGSL is actually shared: one definition, and it carries the
  //     two properties the C case asserts on pixels — the floor itself and the
  //     slide back inside low..high.
  const std::string wgsl = dc::kCandleBodyFloorWgsl;
  check(wgsl.find("fn dcCandleBodyFloor(") != std::string::npos,
        "[4a] the snippet defines dcCandleBodyFloor", "");
  check(wgsl.find("minH > 0.0 && abs(by1 - by0) < minH") != std::string::npos,
        "[4b] it floors only a body thinner than minH", "it is a floor, not a resize");
  check(wgsl.find("shift") != std::string::npos &&
            wgsl.find("(whi - wlo) >= minH") != std::string::npos,
        "[4c] it slides the floored body back inside low..high",
        "so a doji on its own high cannot overhang the wick");

  std::printf("=== %s ===\n", g_failed == 0 ? "all passed" : "FAILURES");
  return g_failed == 0 ? 0 : 1;
}
