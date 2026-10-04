// ENC-1432 — the origin-convention MEASUREMENT for the Dawn golden harness.
//
// WHY THIS EXISTS
// ---------------
// `parity_golden.hpp` carried an `ORIGIN CONVENTION` block asserting that a
// clip-space point (x, y) maps to readback row `(H-1)/2*(1-y)` — "higher clip y
// => smaller row index (toward the top)", i.e. an upright raw readback. That is
// false, and nothing in the suite could see it: the four golden suites bake their
// probe coordinates from the real output, so fixture and comment disagreed with
// each other for three months while every run stayed green. ENC-717 refuted the
// comment on the wasm path; this test refutes it on THIS path — the C++
// DawnSceneRenderer + `DawnDevice::readPixel` readback the goldens actually use —
// and keeps the measurement registered so the convention cannot silently flip
// again.
//
// Two scenes, in this order, because the order is the lesson:
//
//   PART A (the discriminator) — two solid rects, asymmetric in BOTH axes:
//     a RED rect high-and-left (clip y 0.50..0.80, x -0.80..-0.20) and a BLUE
//     rect low-and-right (clip y -0.80..-0.50, x 0.20..0.80). Asymmetric in y so
//     a vertical mirror is visible at all; asymmetric in x so a 180-degree
//     rotation is distinguishable from a mirror. Measures the row centroid of
//     each colour and therefore the SIGN of d(row)/d(clip y).
//
//   PART B (the ENC-717 replica, and the trap) — an apex-up triangle at 600x400
//     with apex clip y=+0.70 and base clip y=-0.70, the exact fixture and size
//     ENC-717 drove through the browser, so the row numbers are directly
//     comparable across the two paths. Its VERTICAL SPAN is deliberately
//     reported and deliberately NOT used as the assertion: the span is ~60..339
//     under both conventions, so a test that checked only the span would pass
//     either way. What discriminates is WHERE THE WIDE END IS — the base.
//
// NEGATIVE CONTROL
// ----------------
// `--flip-readback` (or `DC_GOLDEN_FLIP_READBACK=1`) mirrors the readback rows in
// `parity_golden.hpp`, presenting exactly the frame the refuted convention
// predicts. Registered as `dc_parity_origin_flipped` with `WILL_FAIL TRUE`, so
// every run of the suite re-demonstrates that these assertions CAN fail. A check
// never seen to fail is not a check (the ENC-1249 pattern).
//
// NO GRACEFUL SKIP
// ----------------
// Unlike the four golden suites, a missing Dawn adapter here is exit **3**
// ("CANNOT RUN"), never 0. The whole defect this test addresses is a green run
// that measured nothing (DC-L01 / ENC-1095); a skip that looks like a pass would
// reproduce it.
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_golden.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using dc::golden::BufferData;
using dc::golden::GoldenFrame;
using dc::golden::renderDawn;

static int g_passed = 0;
static int g_failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++g_passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++g_failed;
  }
}

static void report(const char* name, double value) {
  std::printf("  measured: %-46s = %8.2f\n", name, value);
}

// ---------------------------------------------------------------------------
// PART A scene: RED rect high+left, BLUE rect low+right. instancedRect@1 /
// rect4 = {x0,y0,x1,y1} in clip space, cornerRadius 0 so the fills are flat.
// ---------------------------------------------------------------------------
static std::vector<BufferData> sceneTwoRects(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  // RED: clip y in [+0.50, +0.80] (HIGH), x in [-0.80, -0.20] (LEFT).
  float hi[] = {-0.80f, 0.50f, -0.20f, 0.80f};
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})");

  // BLUE: clip y in [-0.80, -0.50] (LOW), x in [+0.20, +0.80] (RIGHT).
  float lo[] = {0.20f, -0.80f, 0.80f, -0.50f};
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"instancedRect@1","geometryId":101})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":0,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":4,"cornerRadius":0})");

  return {BufferData(10, hi, sizeof(hi)), BufferData(11, lo, sizeof(lo))};
}

// ---------------------------------------------------------------------------
// PART B scene: ENC-717's apex-up triangle. apex clip (0, +0.70); base clip
// y = -0.70 spanning x -0.60..+0.60. Solid red through triSolid@1.
// ---------------------------------------------------------------------------
static std::vector<BufferData> sceneApexUpTriangle(dc::CommandProcessor& cp,
                                                   dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float tri[] = {-0.60f, -0.70f, 0.60f, -0.70f, 0.00f, 0.70f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  return {BufferData(10, tri, sizeof(tri))};
}

static bool isRed(const std::uint8_t* p) {
  return p[0] > 200 && p[1] < 60 && p[2] < 60;
}
static bool isBlue(const std::uint8_t* p) {
  return p[2] > 200 && p[0] < 60 && p[1] < 60;
}

int main(int argc, char** argv) {
  const bool flipped = dc::golden::flipReadbackRequested(argc, argv);

  std::printf("=== ENC-1432 Dawn origin-convention measurement ===\n");
  if (flipped)
    std::printf(
        "MODE: NEGATIVE CONTROL — readback rows will be mirrored; every "
        "convention assertion below MUST fail.\n");

  // -----------------------------------------------------------------------
  // PART A — the discriminator.
  // -----------------------------------------------------------------------
  constexpr int AW = 240, AH = 160;
  GoldenFrame a = renderDawn("origin/two-rects", sceneTwoRects, AW, AH);
  if (a.skipped) {
    std::fprintf(stderr,
                 "CANNOT RUN: no Dawn adapter (%s). This test does NOT skip to "
                 "0 — an unmeasured origin convention is not a passing one.\n",
                 a.skipReason.c_str());
    return 3;
  }
  if (flipped && !a.flippedForFalsification) {
    std::fprintf(stderr,
                 "MUTATION DID NOT APPLY: --flip-readback was requested and the "
                 "frame came back unflipped. Refusing to report.\n");
    return 4;
  }
  std::printf("[A] dawn=%s %dx%d  flippedForFalsification=%s\n",
              a.dawnBackend.c_str(), AW, AH,
              a.flippedForFalsification ? "true" : "false");
  // Belt and braces on top of the harness's Null rejection: name the adapter in
  // the output, so a reader of a green run can see WHICH renderer produced it.
  // Dawn's Null backend accepts every command and draws nothing, and a suite that
  // measured nothing must never be mistaken for one that agreed with us.
  if (dc::golden::backendIsNull(a.dawnBackend)) {
    std::fprintf(stderr,
                 "CANNOT RUN: Dawn backend is '%s' — nothing was rendered.\n",
                 a.dawnBackend.c_str());
    return 3;
  }

  double redRowSum = 0, redColSum = 0, blueRowSum = 0, blueColSum = 0;
  long redN = 0, blueN = 0;
  for (int y = 0; y < AH; ++y) {
    for (int x = 0; x < AW; ++x) {
      const std::uint8_t* p = a.at(x, y);
      if (isRed(p)) { redRowSum += y; redColSum += x; ++redN; }
      else if (isBlue(p)) { blueRowSum += y; blueColSum += x; ++blueN; }
    }
  }
  // PRECONDITION, not an origin finding: invariant under a row mirror, so it
  // passes with --flip-readback too. Labelled, per the rule this file enforces.
  check(redN > 1000 && blueN > 1000,
        "[A0-precondition] both rects rendered (solid fills found) — "
        "convention-blind by construction");
  if (redN == 0 || blueN == 0) {
    std::fprintf(stderr, "  (red=%ld blue=%ld px — cannot measure)\n", redN, blueN);
    return 1;
  }
  const double redRow = redRowSum / redN, blueRow = blueRowSum / blueN;
  const double redCol = redColSum / redN, blueCol = blueColSum / blueN;

  // Predictions. MEASURED convention: row = (1 + clipY)/2 * H  (clip +y -> BOTTOM).
  // REFUTED convention:               row = (1 - clipY)/2 * H  (clip +y -> TOP).
  //
  // NOTE those are EDGE coordinates, not pixel-centre indices: the centre of
  // integer row r is at clip y = (r + 0.5)/H*2 - 1, so the centre-index form is
  // (1 + clipY)/2 * H - 0.5. That half-pixel is exactly why the measured
  // centroids below come out at 131.50 / 27.50 against edge predictions of
  // 132.0 / 28.0 — the agreement is exact, not approximate, and the +-4px
  // tolerance is not absorbing an error. (The sign claim does not depend on it;
  // the two hypotheses are ~104 rows apart at H=160.)
  const double redClipYMid = 0.65, blueClipYMid = -0.65;
  const double predMeasuredRed  = (1.0 + redClipYMid)  / 2.0 * AH;   // 132.0
  const double predMeasuredBlue = (1.0 + blueClipYMid) / 2.0 * AH;   //  28.0
  const double predRefutedRed   = (1.0 - redClipYMid)  / 2.0 * AH;   //  28.0
  const double predRefutedBlue  = (1.0 - blueClipYMid) / 2.0 * AH;   // 132.0

  report("row centroid of HIGH rect (clip y +0.65)", redRow);
  report("row centroid of LOW  rect (clip y -0.65)", blueRow);
  report("col centroid of HIGH rect (clip x -0.50)", redCol);
  report("col centroid of LOW  rect (clip x +0.50)", blueCol);
  std::printf(
      "  predictions (H=%d): measured-convention row=(1+y)/2*H -> HIGH %.1f / "
      "LOW %.1f ;  refuted-convention row=(1-y)/2*H -> HIGH %.1f / LOW %.1f\n",
      AH, predMeasuredRed, predMeasuredBlue, predRefutedRed, predRefutedBlue);

  // A1 — THE SIGN. This is the whole claim: d(row)/d(clip y) > 0.
  check(redRow > blueRow,
        "[A1] clip +y lands at a LARGER row index (raw readback is "
        "bottom-up relative to the authored scene)");
  // A2 — the magnitude, so "bottom-up" is not satisfied by any old mess.
  check(std::fabs(redRow - predMeasuredRed) <= 4.0 &&
            std::fabs(blueRow - predMeasuredBlue) <= 4.0,
        "[A2] row == (1 + clipY)/2 * H within 4px (the measured mapping)");
  // A3 — and the refuted mapping is excluded outright, not merely unpreferred.
  check(std::fabs(redRow - predRefutedRed) > 20.0 &&
            std::fabs(blueRow - predRefutedBlue) > 20.0,
        "[A3] row != (1 - clipY)/2 * H — the refuted mapping is off by >20px");
  // A4 — x is NOT mirrored, so this is a vertical mirror and not a 180 rotation.
  // A4 discriminates a DIFFERENT hypothesis — 180-degree rotation vs vertical
  // mirror — and is decided purely by x, so it is invariant under a row mirror
  // and passes with --flip-readback. It is NOT evidence about origin; it is what
  // rules out the rotation reading of A1-A3. Labelled for exactly the reason B3
  // is: an unlabelled assertion that passes under both conventions is how the
  // defect this file documents survived.
  check(redCol < AW / 2.0 && blueCol > AW / 2.0,
        "[A4-convention-blind] clip x is NOT mirrored (HIGH-left stays left) — "
        "rules out a 180-degree rotation; says nothing about origin");

  // -----------------------------------------------------------------------
  // PART B — ENC-717's fixture at ENC-717's size, for cross-path comparison.
  // -----------------------------------------------------------------------
  constexpr int BW = 600, BH = 400;
  GoldenFrame b = renderDawn("origin/apex-up-triangle", sceneApexUpTriangle, BW, BH);
  if (b.skipped) { std::fprintf(stderr, "CANNOT RUN: %s\n", b.skipReason.c_str()); return 3; }
  if (flipped && !b.flippedForFalsification) {
    std::fprintf(stderr, "MUTATION DID NOT APPLY on part B. Refusing to report.\n");
    return 4;
  }

  std::vector<int> widthAt(BH, 0);
  int topRow = -1, botRow = -1;
  for (int y = 0; y < BH; ++y) {
    int w = 0;
    for (int x = 0; x < BW; ++x)
      if (isRed(b.at(x, y))) ++w;
    widthAt[y] = w;
    if (w > 0) {
      if (topRow < 0) topRow = y;
      botRow = y;
    }
  }
  if (topRow < 0) {
    std::fprintf(stderr, "  [B] no red pixels — triangle did not render\n");
    ++g_failed;
  } else {
    int widestRow = 0;
    for (int y = 0; y < BH; ++y)
      if (widthAt[y] > widthAt[widestRow]) widestRow = y;

    std::printf("[B] dawn=%s %dx%d  flippedForFalsification=%s\n",
                b.dawnBackend.c_str(), BW, BH,
                b.flippedForFalsification ? "true" : "false");
    report("first red row (vertical span, lo)", topRow);
    report("last  red row (vertical span, hi)", botRow);
    report("widest red row (the BASE, clip y -0.70)", widestRow);
    report("red width at span-lo + 3", widthAt[topRow + 3]);
    report("red width at span-hi - 3", widthAt[botRow - 3]);
    std::printf(
        "  ENC-717 drove this same fixture at this same size through the "
        "browser/wasm path and read apex=row 338, base=row 60 of the RAW "
        "readback.\n");

    // THE TRAP, stated as a measurement rather than a warning: the span alone
    // cannot tell the conventions apart, because it is very nearly symmetric.
    std::printf(
        "  NOTE the span %d..%d is ~symmetric about H/2=%d — mirroring it gives "
        "%d..%d. A test asserting only the SPAN would pass under BOTH "
        "conventions. The discriminator is where the WIDE end is.\n",
        topRow, botRow, BH / 2, BH - 1 - botRow, BH - 1 - topRow);

    // B1 — the base (wide end) is at the TOP of the raw readback => clip -0.70
    // is at the top => clip +y grows downward. Same sign claim as A1, different
    // pipeline (triSolid@1 vs instancedRect@1) and different primitive.
    check(widestRow < BH / 2,
          "[B1] the BASE (clip y -0.70, the wide end) is in the TOP half of the "
          "raw readback");
    check(widthAt[topRow + 3] > 300 && widthAt[botRow - 3] < 40,
          "[B2] wide near span-lo, narrow near span-hi — the apex (clip y "
          "+0.70) is at the BOTTOM");
    // B3 — the two paths agree on the SPAN, which pins the magnitude of the
    // mapping and nothing about its sign. It is CONVENTION-BLIND on purpose and
    // says so in its own name: it passes with --flip-readback too, and that is
    // the trap demonstrated rather than described. Do not read a green B3 as
    // evidence about origin; B1/B2 are the origin assertions.
    check(std::abs(topRow - 60) <= 4 && std::abs(botRow - 339) <= 4,
          "[B3-convention-blind] span matches ENC-717's browser-path rows (raw "
          "base 60 / apex 338, flipped 61/339) within 4px — pins the MAGNITUDE "
          "across the C++ and wasm paths; passes under both conventions");
  }

  std::printf("\n=== ENC-1432 origin measurement: %d passed, %d failed ===\n",
              g_passed, g_failed);
  if (flipped) {
    std::printf(
        "(negative control: a FAILING run here is the expected outcome and is "
        "what `dc_parity_origin_flipped` asserts via WILL_FAIL.)\n");
  }
  return g_failed > 0 ? 1 : 0;
}
