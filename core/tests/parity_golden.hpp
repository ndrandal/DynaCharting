// ENC-501 (P5 cutover) — Dawn-only golden render harness.
//
// WHAT THIS IS / WHY IT EXISTS
// ----------------------------
// The ENC-510/511/512/500 parity tests originally rendered every scene through
// BOTH the GL backend (Renderer + OsMesaContext + GpuBufferManager) and the Dawn
// backend (DawnSceneRenderer) and compared the two readbacks pixel-by-pixel — the
// GL output was the reference. Dawn is now the proven default renderer and the GL
// backend (dc_gl) is being deleted (ENC-501 Part 2). To preserve the rendering
// regression coverage WITHOUT dc_gl, those tests are converted to render ONLY via
// Dawn and assert the readback against a captured GOLDEN.
//
// The goldens are the SAME Dawn pixels the parity tests validated against GL while
// dc_gl still existed: the parity suite passed (Dawn matched GL within tolerance),
// so the current Dawn output IS the GL-validated reference. We bake a representative
// set of probe pixels (position -> expected RGB) captured from that passing run.
//
// This header is the Dawn-only render core: it builds a Scene from a SceneBuilder
// closure, renders it through DawnSceneRenderer into the headless offscreen target,
// and returns the TOP-LEFT-origin RGBA readback so a test can probe known pixels.
// It deliberately pulls in NO GL headers and links only dc_gpu (no dc_gl / OSMesa).
//
// ORIGIN CONVENTION  (corrected under ENC-1432 — the old text was REFUTED)
// ----------------------------------------------------------------------
// `DawnDevice::readPixel(x, y)` is TOP-LEFT origin: it CopyTextureToBuffer's the
// active target and indexes buffer row `y` with no flip (DawnDevice.cpp,
// readPixel), and WebGPU texture row 0 is NDC y=+1. That part of the old note
// was right. The mapping it drew from it was not:
//
//   WRONG (what this block used to say):  row = (H-1)/2 * (1 - clipY)
//                                         "higher clip y => smaller row index"
//   MEASURED:                             row = (1 + clipY)/2 * H
//                                         higher clip y => LARGER row index
//
// Every Dawn backend negates clip y in its vertex stage (`vec4(p.x, -p.y, 0, 1)`
// — 17 negation sites across 13 files under core/src/gpu, each commented
// "Y-FLIP ... to match the GL bottom-left readback". Count them with
// `grep -rnoE '\-(p|pos|clip|t0|t1)\.y' core/src/gpu/*.cpp`, NOT with the
// `p.x, -p.y` literal: lineAA spells it `-t0.y`/`-t1.y` and instancedCandle
// `-clip.y`, so the literal finds 11 of the 13 files and misses exactly those
// two — one of them lineAA@1, THE default line pipeline). So clip y=+0.7 becomes NDC
// y=-0.7, which WebGPU puts
// near the BOTTOM of the target, and the faithful top-down readback reports it at
// a high row index. The raw readback is therefore vertically MIRRORED relative to
// the authored scene — the general statement is LIMITATIONS.md DC-L05, and the
// browser-side half of it is what EngineHost.blitFramebuffer flips on every frame.
//
// MEASURED, NOT ARGUED (ENC-1432, `dc_parity_origin`, Dawn/Vulkan lavapipe):
//
//   two solid rects, asymmetric in BOTH axes, 240x160
//     RED  clip y +0.50..+0.80 (centroid +0.65), clip x -0.80..-0.20
//          -> row centroid 131.50   (measured-convention prediction 132.0;
//                                    refuted-convention prediction 28.0)
//     BLUE clip y -0.80..-0.50 (centroid -0.65), clip x +0.20..+0.80
//          -> row centroid  27.50   (measured 28.0; refuted 132.0)
//     col centroids 59.50 / 179.50 — clip x is NOT mirrored, so this is a
//     vertical mirror and not a 180-degree rotation.
//
//   ENC-717's own fixture, re-driven here: apex-up triangle, apex clip y=+0.70,
//   base clip y=-0.70, at ENC-717's 600x400
//     -> red spans rows 60..338; the WIDE end (the base, clip -0.70) is row 60.
//        ENC-717 measured apex=338 / base=60 through the BROWSER on the committed
//        wasm (headless Chrome, SwiftShader, status=1 backend=WebGPU drawCalls=2).
//        Byte-for-byte the same rows: the C++ and wasm readbacks share the
//        convention, and both differ in SIGN from the old note.
//
//   NOTE the span 60..338 is very nearly symmetric about H/2 — mirroring it gives
//   61..339. A test that asserted only the vertical SPAN would pass under BOTH
//   conventions. That is the symmetric-fixture trap, and it is why the fixtures
//   above are asymmetric and why the discriminator is WHERE THE WIDE END IS.
//
// The d79_dawn_json_host citation that used to sit here is withdrawn: that file
// contains no orientation claim at all (one label, "top-left RGBA readback"), and
// all four of its probes are convention-independent by the derivation below — it
// is evidence for neither convention.
//
// PROBE RE-DERIVATION (ENC-1432 — why no probe coordinate moved)
// --------------------------------------------------------------
// The goldens were never written against the comment. Every probe here was baked
// from the real output, so the fixtures agreed with the hardware and disagreed
// with the prose — which is exactly why a green run could not detect the error,
// and why the ticket's "re-derive the probes" resolves to "re-derive, then
// confirm unchanged" rather than a re-baseline. Three of the inline comments in
// the suites (parity_conformance's `transforms`, parity_extended's
// `indexed-gather`, parity_multipane's (1) and (4)) already said "clip +y ->
// bottom rows" in so many words.
//
// Each probe was classified two ways — by hand geometry, and by MEASUREMENT with
// the readback rows mirrored (`DC_GOLDEN_FLIP_READBACK`, below). They agreed on
// 62 of 63, and the measurement is what settled the 63rd: the hand derivation
// called `pipelines/line2d-1px` convention-INDEPENDENT (the segment runs through
// the clip origin, so "it passes through the centre either way") and the mirrored
// run shows it FAILING. The hand argument treated the mapping as continuous; the
// raster is not. At an even H there is no pixel row centred on clip y=0 — the
// origin falls on the boundary between rows 47 and 48 — and the probe's own
// centre, (48.5, 48.5) -> clip (0.0104, 0.0104), sits above the line
// (y = x/2 gives 0.0052, i.e. row 48.25). The 1px line therefore rasterises into
// row 48, while the mirror of row 48 is row 47, which is background. The margin
// is a QUARTER of a pixel. Where geometry and measurement disagree, believe the
// measurement — that is the whole subject of this block.
//
// 22 probes in 8 scenes are CONVENTION-DEPENDENT and fail when mirrored:
//
//   parity_conformance  transforms/scale+translate            (65,62)
//                       pipelines/line2d-1px                  (48,48)
//   parity_multipane    multipane/per-pane-clear              (64,32) (64,96)
//                       multipane/content+clear               (64,32) (64,96)
//                                                             (64,8)  (64,120)
//                       multipane/content+clear+border+sep    (64,32) (64,96)
//   parity_extended     texturedQuad/4-corner-texels          4 quadrant probes
//                       indexed-gather/instRect-diagonal      4 quadrant probes
//                       indexed-gather/texQuad-diagonal       4 quadrant probes
//
// NOT ALL 22 ARE EQUALLY SOLID — pick your witness deliberately:
//   * The ROBUST single-shape witness is `transforms/scale+translate` (65,62):
//     the body spans rows 57.6..76.8 measured and 19.2..38.4 under the refuted
//     mapping, so the probe clears the wrong answer by ~24 rows.
//   * `pipelines/line2d-1px` (48,48) is FRAGILE and must not be load-bearing.
//     Its margin is a quarter of a pixel (above), it is a 1px unantialiased
//     primitive, and clip (0,0) maps to continuous row 48.0 under BOTH
//     conventions — row H/2 is the reflection's fixed line. Widen `line2d@1`
//     past 1px, or give it AA, and this probe becomes convention-BLIND without
//     anything failing to announce it.
//   * `texturedQuad/4-corner-texels` pins the COMPOSITION of the row mapping and
//     the texture v axis, not the row mapping alone: mirror BOTH and all four
//     probes pass again. A frame mirror alone does break them, which is what the
//     knob below applies — but do not cite it as a pure origin witness.
//   * `indexed-gather/instRect-diagonal` fails in both directions at once: its
//     two "expect clear" probes go red while its two red probes go clear.
//
// The remaining 41 probes pass under BOTH conventions, i.e. they test nothing
// about origin. That is not a defect in them, but it is worth knowing which
// coverage you do NOT have, and the reasons group into four kinds:
//
//   * vertically symmetric geometry — instRect-sharp, rounded-rect, clip-mask,
//     all four blend scenes, lineAA-solid/dashed (bands centred on clip y=0),
//     volume/candles and recipe/full-chart's centre candle (open +0.3 / close
//     -0.3 straddle 0 symmetrically), multipane/pane-borders and
//     pane-separators (the two pane regions are exact reflections with identical
//     clear colour and style, so the whole frame is mirror-symmetric);
//   * probe on the shape's vertical centre line — triSolid and triAA probe the
//     apex column, where coverage spans the same rows either way;
//     gradient/per-vertex-color is an asymmetric scene probed one row off centre,
//     where the mirror moves it 0.02 in clip y (~3/255 of colour, tol 24);
//   * decided by x alone, or by a uniform frame — every "clear corner" probe,
//     scissor/pane-region, cull/frustum, texturedQuad/solid-red and
//     color-modulate;
//   * permutation-invariant or not frame-shaped — parity_text asserts whole-frame
//     POPULATION COUNTS (strong-stroke / partial-coverage / background totals),
//     every one of which a row mirror leaves identical, so the whole file is
//     convention-independent by construction despite an asymmetric glyph run;
//     and all 10 pick probes go through pickDawn/renderPick, which reads one
//     pixel of the pick target rather than building a frame — each pick scene is
//     either full-frame-covering or y-symmetric with x-decided misses.
//
// So the ENTIRE pick path and the ENTIRE text suite are blind to the convention,
// and in the conformance suite only two probes see it. If you add a scene whose
// correctness depends on orientation, make the fixture asymmetric and check it
// against DC_GOLDEN_FLIP_READBACK before believing it.
//
// SKIP-GRACEFULLY
// ---------------
// If Dawn can't bring up an adapter the helper returns a SKIPPED result and the
// test exits 0 — matching every other Dawn test's graceful-skip behavior.
// `dc_parity_origin` is the deliberate exception: it exits 3 ("CANNOT RUN"),
// because an origin convention that was never measured is not a passing one
// (DC-L01 / ENC-1095 — a skipped target looks exactly like a passing one).
#pragma once

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/gpu/DawnTexturedQuadBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace dc {
namespace golden {

// One logical buffer's CPU bytes (a vertex/index/instance buffer the scene's
// geometry references by id), fed to the Dawn CpuBufferStore.
struct BufferData {
  Id id{0};
  std::vector<std::uint8_t> bytes;

  BufferData() = default;
  BufferData(Id i, const void* data, std::size_t n)
      : id(i),
        bytes(reinterpret_cast<const std::uint8_t*>(data),
              reinterpret_cast<const std::uint8_t*>(data) + n) {}
};

// One logical texture supplied to the Dawn texturedQuad backend (the GL side that
// used to mirror these texels into a GL TextureManager is gone — Dawn-only now).
struct TextureInput {
  std::uint32_t id{1};
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::vector<std::uint8_t> rgba;  // tightly-packed RGBA8 (width*height*4)
  bool valid() const {
    return width > 0 && height > 0 &&
           rgba.size() == static_cast<std::size_t>(width) * height * 4;
  }
};

// In-memory TextureSource backing the Dawn texturedQuad backend.
class MemTextureSource final : public TextureSource {
 public:
  void set(const TextureInput& t) { tex_ = t; }
  bool getTexturePixels(std::uint32_t id, const std::uint8_t** outData,
                        std::uint32_t* outW, std::uint32_t* outH,
                        TextureFormat* outFmt) const override {
    if (!tex_.valid() || id != tex_.id) return false;
    *outData = tex_.rgba.data();
    *outW = tex_.width;
    *outH = tex_.height;
    *outFmt = TextureFormat::RGBA8;
    return true;
  }

 private:
  TextureInput tex_;
};

// Build the Scene (identical CommandProcessor JSON the parity tests used) and
// return the per-buffer CPU bytes.
using SceneBuilder =
    std::function<std::vector<BufferData>(CommandProcessor& cp, Scene& scene)>;

// Optional D78 pane border/separator style applied to the Dawn renderer.
struct GoldenStyle {
  bool enabled{false};
  float paneBorderColor[4] = {0, 0, 0, 0};
  float paneBorderWidth{0.0f};
  float separatorColor[4] = {0, 0, 0, 0};
  float separatorWidth{0.0f};
};

struct GoldenFrame {
  bool skipped{false};
  std::string skipReason;
  int width{0};
  int height{0};
  std::string dawnBackend;
  std::vector<std::uint8_t> rgba;  // top-left origin, row-major RGBA, W*H*4
  // True only when the ENC-1432 falsification knob mirrored the rows (below).
  bool flippedForFalsification{false};

  // RGBA at (x,y) (top-left origin). Out-of-range returns {0,0,0,0}.
  const std::uint8_t* at(int x, int y) const {
    static const std::uint8_t kZero[4] = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= width || y >= height) return kZero;
    return &rgba[(static_cast<std::size_t>(y) * width + x) * 4];
  }
};

// FALSIFICATION KNOB (ENC-1432) — present the readback under the REFUTED
// convention.
//
// `DC_GOLDEN_FLIP_READBACK=1` (or the `--flip-readback` argv flag, via
// flipReadbackRequested()) mirrors the readback rows before any probe sees them.
// That is exactly the frame the old ORIGIN CONVENTION block described: an upright
// raw readback where higher clip y means a smaller row index. It exists so the
// claim "these probes test the origin convention" is falsifiable instead of
// asserted — a probe that passes with the flip on AND off is not testing the
// convention, which is precisely how the wrong comment survived (see the PROBE
// RE-DERIVATION table above).
//
// It is NOT a compatibility switch. Nothing ships with it set; the three ctest
// cases `dc_parity_{conformance,multipane,extended}_flipped` and
// `dc_parity_origin_flipped` set it and are registered `WILL_FAIL TRUE`, so every
// run of the suite re-demonstrates that the convention-dependent probes can fail.
// When it is on, renderDawn prints a loud banner naming the mutation — a mutation
// that did not apply reads exactly like a passing gate.
inline bool& flipReadbackFlag() {
  static bool v = (std::getenv("DC_GOLDEN_FLIP_READBACK") != nullptr &&
                   std::getenv("DC_GOLDEN_FLIP_READBACK")[0] != '\0' &&
                   std::getenv("DC_GOLDEN_FLIP_READBACK")[0] != '0');
  return v;
}

// Opt in from argv too (`--flip-readback`), for a manual run without an env var.
inline bool flipReadbackRequested(int argc, char** argv) {
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--flip-readback") == 0) flipReadbackFlag() = true;
  return flipReadbackFlag();
}

// Render `builder` through Dawn into a top-left-origin RGBA readback.
inline GoldenFrame renderDawn(const char* name, const SceneBuilder& builder, int W,
                              int H, GlyphAtlas* atlas = nullptr,
                              const GoldenStyle& style = {},
                              const TextureInput* texture = nullptr) {
  GoldenFrame f;
  f.width = W;
  f.height = H;

  MemTextureSource texSrc;
  const TextureSource* texSrcPtr = nullptr;
  if (texture && texture->valid()) {
    texSrc.set(*texture);
    texSrcPtr = &texSrc;
  }

  DawnSceneRenderer renderer(atlas, texSrcPtr);
  if (!renderer.init()) {
    f.skipped = true;
    f.skipReason = "Dawn adapter unavailable: " + renderer.errorMessage();
    std::printf("[golden %s] SKIP: %s\n", name, f.skipReason.c_str());
    return f;
  }
  f.dawnBackend = renderer.device().backendName();

  if (style.enabled) {
    DawnRenderStyle rs;
    rs.paneBorderColor[0] = style.paneBorderColor[0];
    rs.paneBorderColor[1] = style.paneBorderColor[1];
    rs.paneBorderColor[2] = style.paneBorderColor[2];
    rs.paneBorderColor[3] = style.paneBorderColor[3];
    rs.paneBorderWidth = style.paneBorderWidth;
    rs.separatorColor[0] = style.separatorColor[0];
    rs.separatorColor[1] = style.separatorColor[1];
    rs.separatorColor[2] = style.separatorColor[2];
    rs.separatorColor[3] = style.separatorColor[3];
    rs.separatorWidth = style.separatorWidth;
    renderer.setRenderStyle(rs);
  }

  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  if (atlas) cp.setGlyphAtlas(atlas);

  std::vector<BufferData> buffers = builder(cp, scene);

  CpuBufferStore store;
  for (const auto& b : buffers) {
    store.setCpuData(b.id, b.bytes.data(),
                     static_cast<std::uint32_t>(b.bytes.size()));
  }

  renderer.render(scene, store, W, H);

  f.rgba.assign(static_cast<std::size_t>(W) * H * 4, 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      std::uint8_t px[4] = {0, 0, 0, 0};
      renderer.device().readPixel(x, y, px);
      std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4;
      f.rgba[idx + 0] = px[0];
      f.rgba[idx + 1] = px[1];
      f.rgba[idx + 2] = px[2];
      f.rgba[idx + 3] = px[3];
    }
  }

  // ENC-1432 falsification: mirror the rows so every probe sees the frame the
  // REFUTED convention predicts. Banner on stdout so a run that claims the
  // mutation cannot be confused with one where it silently did not apply.
  if (flipReadbackFlag()) {
    std::printf(
        "[golden %s] FALSIFICATION ACTIVE: DC_GOLDEN_FLIP_READBACK -- readback "
        "rows mirrored (%dx%d); probes now see the REFUTED convention\n",
        name, W, H);
    const std::size_t rowBytes = static_cast<std::size_t>(W) * 4;
    std::vector<std::uint8_t> tmp(rowBytes);
    for (int y = 0; y < H / 2; ++y) {
      std::uint8_t* a = &f.rgba[static_cast<std::size_t>(y) * rowBytes];
      std::uint8_t* b = &f.rgba[static_cast<std::size_t>(H - 1 - y) * rowBytes];
      std::memcpy(tmp.data(), a, rowBytes);
      std::memcpy(a, b, rowBytes);
      std::memcpy(b, tmp.data(), rowBytes);
    }
    f.flippedForFalsification = true;
  }
  return f;
}

// Render the Dawn pick pass and return decoded DrawItem ids at probe pixels.
struct PickProbe {
  int x{0};
  int y{0};
  std::uint32_t expectId{0};
};

struct PickResultRow {
  int x{0}, y{0};
  int queriedY{0};  // y actually handed to renderPick (mirrored under the knob)
  std::uint32_t expectId{0};
  std::uint32_t gotId{0};
  bool match{false};
};

struct PickFrame {
  bool skipped{false};
  std::string skipReason;
  std::string dawnBackend;
  std::vector<PickResultRow> rows;
  // True only when the ENC-1432 knob mirrored the probe y (see pickDawn).
  bool flippedForFalsification{false};
};

inline PickFrame pickDawn(const char* name, const SceneBuilder& builder, int W,
                          int H, const std::vector<PickProbe>& probes) {
  PickFrame f;
  DawnSceneRenderer renderer(nullptr, nullptr);
  if (!renderer.init()) {
    f.skipped = true;
    f.skipReason = "Dawn adapter unavailable: " + renderer.errorMessage();
    std::printf("[golden-pick %s] SKIP: %s\n", name, f.skipReason.c_str());
    return f;
  }
  f.dawnBackend = renderer.device().backendName();

  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  std::vector<BufferData> buffers = builder(cp, scene);
  CpuBufferStore store;
  for (const auto& b : buffers)
    store.setCpuData(b.id, b.bytes.data(),
                     static_cast<std::uint32_t>(b.bytes.size()));

  // ENC-1432 falsification, pick edition. `renderPick` takes a SCREEN (x, y) and
  // reads that one pixel of the pick target — there is no frame to mirror, so the
  // readback-row knob above cannot reach this path. The analogue of presenting the
  // refuted convention to a point query is to mirror the query itself: under the
  // other convention the same scene point sits at row H-1-y. Without this, "the
  // pick probes pass with the knob on" would be a no-op masquerading as evidence.
  const bool flip = flipReadbackFlag();
  if (flip)
    std::printf(
        "[golden-pick %s] FALSIFICATION ACTIVE: DC_GOLDEN_FLIP_READBACK -- probe "
        "y mirrored to H-1-y (H=%d); probes now query the REFUTED convention\n",
        name, H);
  f.flippedForFalsification = flip;

  for (const auto& p : probes) {
    const int qy = flip ? (H - 1 - p.y) : p.y;
    DawnPickResult pr = renderer.renderPick(scene, store, W, H, p.x, qy);
    PickResultRow row;
    row.x = p.x;
    row.y = p.y;
    row.queriedY = qy;
    row.expectId = p.expectId;
    row.gotId = pr.drawItemId;
    row.match = (row.gotId == row.expectId);
    f.rows.push_back(row);
  }
  return f;
}

}  // namespace golden
}  // namespace dc
