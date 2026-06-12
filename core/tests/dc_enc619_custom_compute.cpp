// ENC-619 (Epic ENC-619, the FINAL epic) — the WGSL ESCAPE HATCH fast logic test
// (GPU-FREE). Three pillars, all unit-testable WITHOUT a GPU:
//   1. the SANDBOX CONTRACT (§5.3): a CustomComputeSpec that exceeds ANY static
//      limit (workgroup, shared, storage count, dispatch, binding size, dtype,
//      duplicate binding) is REJECTED at validate() with the right status; a
//      legal spec passes.
//   2. the two SHIPPED kernels' WGSL STRUCTURE (the right storage bindings,
//      atomics, keep-alive on every binding so Dawn's auto layout cannot prune
//      one — the ENC-617a all-zeros lesson).
//   3. the CPU REFERENCE correctness: referenceStft on a pure tone peaks at the
//      tone's bin; referenceMarchingSquares on a known field yields the expected
//      iso-line + variable cardinality.
// The byte-comparable GPU==CPU proofs are the Dawn tests (dc_enc619_dawn_*).
#include "dc/transform/ComputeWgsl.hpp"
#include "dc/transform/CustomCompute.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace dc;

static int passed = 0, failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}
static bool has(const std::string& h, const char* n) {
  return h.find(n) != std::string::npos;
}

// A minimal legal spec: 1 input, 1 output, @workgroup_size(64), under every cap.
static CustomComputeSpec legalSpec() {
  CustomComputeSpec s;
  s.id = "k";
  s.wgsl =
      "@group(0) @binding(0) var<storage, read> a : array<f32>;\n"
      "@group(0) @binding(1) var<storage, read_write> o : array<f32>;\n"
      "@compute @workgroup_size(64) fn main() { o[0] = a[0]; }\n";
  s.entryPoint = "main";
  s.workgroupX = 64; s.workgroupY = 1; s.workgroupZ = 1;
  s.dispatchX = 16; s.dispatchY = 1; s.dispatchZ = 1;
  CcBinding in; in.binding = 0; in.name = "a"; in.column = "a";
  in.dtype = CcDType::F32; in.access = CcAccess::Read;
  CcBinding out; out.binding = 1; out.name = "o"; out.dtype = CcDType::F32;
  out.access = CcAccess::Write; out.capElements = 1024;
  s.inputs.push_back(in);
  s.outputs.push_back(out);
  return s;
}

int main() {
  std::printf("=== ENC-619 escape hatch: sandbox + kernels + CPU refs ===\n");

  // ---------- 1. SANDBOX CONTRACT ----------------------------------------
  {
    auto ok = validateCustomCompute(legalSpec(), {256});
    check(ok.ok(), "sandbox: a legal spec passes");

    // dtype parse rejects f64 (the f32-only contract).
    CcDType dt;
    check(!parseCcDType("f64", dt), "sandbox: f64 dtype rejected at parse");
    check(parseCcDType("f16", dt) && dt == CcDType::F16, "sandbox: f16 accepted");

    // workgroup too large (per-dim).
    { auto s = legalSpec(); s.workgroupX = 512;
      auto v = validateCustomCompute(s, {256});
      check(v.status == CcStatus::WorkgroupTooLarge, "sandbox: workgroup dim > 256 rejected"); }
    // workgroup product too large (16*16*2 = 512 > 256).
    { auto s = legalSpec(); s.workgroupX = 16; s.workgroupY = 16; s.workgroupZ = 2;
      auto v = validateCustomCompute(s, {256});
      check(v.status == CcStatus::WorkgroupTooLarge, "sandbox: workgroup product > 256 rejected"); }
    // shared > 16 KiB.
    { auto s = legalSpec(); s.sharedBytes = 16385;
      auto v = validateCustomCompute(s, {256});
      check(v.status == CcStatus::SharedTooLarge, "sandbox: shared > 16KiB rejected"); }
    // > 8 storage buffers.
    { auto s = legalSpec();
      for (std::uint32_t i = 0; i < 8; ++i) {
        CcBinding b; b.binding = 10 + i; b.name = "x"; b.dtype = CcDType::F32;
        b.access = CcAccess::Read; s.inputs.push_back(b);
      }
      auto v = validateCustomCompute(s, std::vector<std::uint32_t>(s.inputs.size(), 4));
      check(v.status == CcStatus::TooManyBuffers, "sandbox: > 8 storage buffers rejected"); }
    // dispatch > 65535/dim.
    { auto s = legalSpec(); s.dispatchX = 70000;
      auto v = validateCustomCompute(s, {256});
      check(v.status == CcStatus::DispatchTooLarge, "sandbox: dispatch dim > 65535 rejected"); }
    // binding > 128 MiB (cap 40M f32 = 160 MiB).
    { auto s = legalSpec(); s.outputs[0].capElements = 40u * 1024u * 1024u;
      auto v = validateCustomCompute(s, {256});
      check(v.status == CcStatus::BindingTooLarge, "sandbox: binding > 128MiB rejected"); }
    // duplicate @binding.
    { auto s = legalSpec(); s.outputs[0].binding = 0;  // collides with input 0
      auto v = validateCustomCompute(s, {256});
      check(v.status == CcStatus::DuplicateBinding, "sandbox: duplicate @binding rejected"); }
    // empty WGSL / no output.
    { auto s = legalSpec(); s.wgsl.clear();
      check(validateCustomCompute(s, {256}).status == CcStatus::BadSpec, "sandbox: empty WGSL rejected"); }
    { auto s = legalSpec(); s.outputs.clear();
      check(validateCustomCompute(s, {}).status == CcStatus::BadSpec, "sandbox: no output rejected"); }
  }

  // ---------- parseWorkgroupSize ----------------------------------------
  {
    std::uint32_t x, y, z;
    check(parseWorkgroupSize("@compute @workgroup_size(8, 8, 1) fn m(){}", x, y, z)
              && x == 8 && y == 8 && z == 1,
          "parseWorkgroupSize: (8,8,1)");
    check(parseWorkgroupSize("@workgroup_size(256)\nfn m(){}", x, y, z) && x == 256
              && y == 1 && z == 1,
          "parseWorkgroupSize: (256) defaults y=z=1");
    check(!parseWorkgroupSize("fn m(){}", x, y, z),
          "parseWorkgroupSize: none -> false");
  }

  // ---------- 2. SHIPPED KERNEL WGSL STRUCTURE ---------------------------
  {
    std::string fft = buildStftKernelWgsl();
    check(has(fft, "@binding(0)") && has(fft, "sample : array<f32>"),
          "stft: sample input binding 0 (f32)");
    check(has(fft, "mag : array<f32>"), "stft: mag output (f32)");
    check(has(fft, "arrayLength(&sample)"), "stft: sample binding kept alive");
    check(has(fft, "cos(ang)") && has(fft, "sin(ang)"), "stft: DFT re/im accumulate");
    check(has(fft, "0.5f * (1.0f - cos"), "stft: Hann window in-kernel");
    std::uint32_t x, y, z;
    check(parseWorkgroupSize(fft, x, y, z) && x <= 256 && y <= 256,
          "stft: @workgroup_size within the sandbox cap");

    std::string ms = buildMarchingSquaresKernelWgsl();
    check(has(ms, "field : array<f32>"), "marching: field input (f32)");
    check(has(ms, "segs : array<f32>"), "marching: segment output (f32)");
    check(has(ms, "segCount : atomic<u32>"), "marching: atomic segment counter");
    check(has(ms, "atomicAdd(&segCount, 1u)"), "marching: claim slot via atomicAdd");
    check(has(ms, "if (slot >= cap)"), "marching: bounded buffer overflow guard");
    check(has(ms, "arrayLength(&segs)"), "marching: segs binding kept alive");
    check(has(ms, "switch (c)"), "marching: the 16-case table");
  }

  // ---------- 3. CPU REFERENCE CORRECTNESS -------------------------------
  {
    // A pure cosine at bin k0 over fftSize samples: the STFT magnitude must PEAK
    // at bin k0 (windowing spreads a little; the peak is unambiguous).
    const std::uint32_t fftSize = 64, hop = 64, k0 = 8;
    std::vector<float> sig(fftSize);
    const float twoPi = 6.2831853071795864769f;
    for (std::uint32_t n = 0; n < fftSize; ++n)
      sig[n] = std::cos(twoPi * static_cast<float>(k0) * static_cast<float>(n) /
                        static_cast<float>(fftSize));
    std::uint32_t frames = 0, bins = 0;
    std::vector<float> mag = referenceStft(sig, fftSize, hop, frames, bins);
    check(frames == 1 && bins == fftSize / 2 + 1, "stft ref: 1 frame, fftSize/2+1 bins");
    std::uint32_t peak = 0; float pv = -1.0f;
    for (std::uint32_t k = 0; k < bins; ++k)
      if (mag[k] > pv) { pv = mag[k]; peak = k; }
    check(peak == k0, "stft ref: pure tone peaks at its bin");

    // Marching squares on a 3x3 field with a single interior cell crossing.
    // Field (row-major, row 0 = bottom): a plateau low everywhere, one high corner
    // so exactly the cells around it cross iso=0.5.
    // 2x2 simplest: field = {0,0, 0,1} -> top-right corner inside -> 1 segment.
    {
      std::vector<float> f2 = {0.f, 0.f, 0.f, 1.f};  // gridW=2,gridH=2
      auto segs = referenceMarchingSquares(f2, 2, 2, 0.5f);
      check(segs.size() == 1, "marching ref: single corner -> 1 segment");
      // tr inside (case 4): segment right-edge -> top-edge. right x=1, top y=1.
      check(segs.size() == 1 && std::fabs(segs[0].x0 - 1.0f) < 1e-6f &&
                std::fabs(segs[0].y1 - 1.0f) < 1e-6f,
            "marching ref: segment endpoints on the crossing edges");
    }
    // Variable cardinality: a saddle (case 5) yields TWO segments in one cell.
    {
      // bl,tr inside; br,tl outside; center average decides connectivity.
      std::vector<float> f2 = {1.f, 0.f, 0.f, 1.f};  // bl=1 br=0 tl=0 tr=1
      auto segs = referenceMarchingSquares(f2, 2, 2, 0.5f);
      check(segs.size() == 2, "marching ref: saddle cell -> 2 segments (variable cardinality)");
    }
    // A larger field crosses many cells -> the count is data-dependent (the very
    // property the bounded-buffer + atomic-count path exists for).
    {
      const std::uint32_t W = 8, H = 8;
      std::vector<float> f(W * H);
      for (std::uint32_t r = 0; r < H; ++r)
        for (std::uint32_t c = 0; c < W; ++c)
          f[r * W + c] = std::sin(0.6f * static_cast<float>(c)) +
                         std::cos(0.6f * static_cast<float>(r));
      auto segs = referenceMarchingSquares(f, W, H, 0.0f);
      check(!segs.empty(), "marching ref: non-trivial field emits a contour");
      std::printf("  (field 8x8 iso=0 -> %zu segments)\n", segs.size());
    }
  }

  std::printf("\nENC-619 fast: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
