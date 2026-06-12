// ENC-619 — the WGSL escape hatch: sandbox contract + spec parse helpers.
// See dc/transform/CustomCompute.hpp for the §5.3 contract.
#include "dc/transform/CustomCompute.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace dc {

const char* ccDTypeWgsl(CcDType dt) {
  switch (dt) {
    case CcDType::F32: return "f32";
    case CcDType::F16: return "f16";
    case CcDType::I32: return "i32";
    case CcDType::U32: return "u32";
  }
  return "f32";
}

bool parseCcDType(const std::string& s, CcDType& out) {
  if (s == "f32") { out = CcDType::F32; return true; }
  if (s == "f16") { out = CcDType::F16; return true; }
  if (s == "i32") { out = CcDType::I32; return true; }
  if (s == "u32") { out = CcDType::U32; return true; }
  return false;  // "f64" and anything else: rejected by the f32-only contract.
}

const char* toString(CcStatus s) {
  switch (s) {
    case CcStatus::Ok: return "Ok";
    case CcStatus::BadSpec: return "BadSpec";
    case CcStatus::WorkgroupTooLarge: return "WorkgroupTooLarge";
    case CcStatus::SharedTooLarge: return "SharedTooLarge";
    case CcStatus::TooManyBuffers: return "TooManyBuffers";
    case CcStatus::DispatchTooLarge: return "DispatchTooLarge";
    case CcStatus::BindingTooLarge: return "BindingTooLarge";
    case CcStatus::BadDType: return "BadDType";
    case CcStatus::DuplicateBinding: return "DuplicateBinding";
  }
  return "?";
}

namespace {

// Skip whitespace + WGSL line comments starting at `i`.
void skipWs(const std::string& s, std::size_t& i) {
  for (;;) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
      while (i < s.size() && s[i] != '\n') ++i;
      continue;
    }
    break;
  }
}

// Parse a non-negative decimal integer at `i` (skipping a trailing `u`/`i` suffix
// and whitespace). Returns false if no digit is present.
bool parseUint(const std::string& s, std::size_t& i, std::uint32_t& out) {
  skipWs(s, i);
  if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i])))
    return false;
  std::uint64_t v = 0;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
    v = v * 10 + static_cast<std::uint64_t>(s[i] - '0');
    ++i;
  }
  // Optional WGSL integer suffix.
  if (i < s.size() && (s[i] == 'u' || s[i] == 'i')) ++i;
  out = static_cast<std::uint32_t>(v);
  return true;
}

}  // namespace

bool parseWorkgroupSize(const std::string& wgsl, std::uint32_t& x,
                        std::uint32_t& y, std::uint32_t& z) {
  x = y = z = 1;
  const std::string key = "@workgroup_size";
  const std::size_t at = wgsl.find(key);
  if (at == std::string::npos) return false;
  std::size_t i = at + key.size();
  skipWs(wgsl, i);
  if (i >= wgsl.size() || wgsl[i] != '(') return false;
  ++i;  // past '('
  if (!parseUint(wgsl, i, x)) return false;
  skipWs(wgsl, i);
  if (i < wgsl.size() && wgsl[i] == ',') {
    ++i;
    if (!parseUint(wgsl, i, y)) return false;
    skipWs(wgsl, i);
    if (i < wgsl.size() && wgsl[i] == ',') {
      ++i;
      if (!parseUint(wgsl, i, z)) return false;
    }
  }
  return true;
}

CcValidation validateCustomCompute(
    const CustomComputeSpec& spec,
    const std::vector<std::uint32_t>& inputElements, const CcLimits& limits) {
  auto fail = [](CcStatus s, std::string m) -> CcValidation {
    return CcValidation{s, std::move(m)};
  };

  // ----- structural -------------------------------------------------------
  if (spec.wgsl.empty())
    return fail(CcStatus::BadSpec,
                "customCompute '" + spec.id + "': empty WGSL kernel");
  if (spec.entryPoint.empty())
    return fail(CcStatus::BadSpec,
                "customCompute '" + spec.id + "': missing entry point");
  if (spec.outputs.empty())
    return fail(CcStatus::BadSpec, "customCompute '" + spec.id +
                                       "': declares no output binding");

  // ----- workgroup size ---------------------------------------------------
  const std::uint32_t wx = spec.workgroupX, wy = spec.workgroupY,
                      wz = spec.workgroupZ;
  if (wx == 0 || wy == 0 || wz == 0)
    return fail(CcStatus::BadSpec, "customCompute '" + spec.id +
                                       "': zero workgroup dimension");
  if (wx > limits.maxWorkgroupPerDim || wy > limits.maxWorkgroupPerDim ||
      wz > limits.maxWorkgroupPerDim) {
    return fail(CcStatus::WorkgroupTooLarge,
                "customCompute '" + spec.id + "': workgroup dim exceeds " +
                    std::to_string(limits.maxWorkgroupPerDim));
  }
  const std::uint64_t product = static_cast<std::uint64_t>(wx) * wy * wz;
  if (product > limits.maxWorkgroupProduct) {
    return fail(CcStatus::WorkgroupTooLarge,
                "customCompute '" + spec.id + "': workgroup invocations " +
                    std::to_string(product) + " exceed " +
                    std::to_string(limits.maxWorkgroupProduct));
  }

  // ----- shared (workgroup) memory ---------------------------------------
  if (spec.sharedBytes > limits.maxSharedBytes) {
    return fail(CcStatus::SharedTooLarge,
                "customCompute '" + spec.id + "': workgroup memory " +
                    std::to_string(spec.sharedBytes) + " bytes exceeds " +
                    std::to_string(limits.maxSharedBytes));
  }

  // ----- storage-buffer count --------------------------------------------
  if (spec.bindingCount() > limits.maxStorageBuffers) {
    return fail(CcStatus::TooManyBuffers,
                "customCompute '" + spec.id + "': " +
                    std::to_string(spec.bindingCount()) +
                    " storage bindings exceed " +
                    std::to_string(limits.maxStorageBuffers));
  }

  // ----- dispatch grid ----------------------------------------------------
  if (spec.dispatchX > limits.maxDispatchPerDim ||
      spec.dispatchY > limits.maxDispatchPerDim ||
      spec.dispatchZ > limits.maxDispatchPerDim) {
    return fail(CcStatus::DispatchTooLarge,
                "customCompute '" + spec.id + "': dispatch dim exceeds " +
                    std::to_string(limits.maxDispatchPerDim));
  }

  // ----- per-binding dtype + byte size + unique @binding index ------------
  std::vector<bool> seen(256, false);
  auto checkBinding = [&](const CcBinding& b, std::uint32_t elems,
                          CcValidation& v) -> bool {
    if (b.binding < seen.size()) {
      if (seen[b.binding]) {
        v = fail(CcStatus::DuplicateBinding,
                 "customCompute '" + spec.id + "': duplicate @binding(" +
                     std::to_string(b.binding) + ")");
        return false;
      }
      seen[b.binding] = true;
    }
    // dtype already constrained at parse (parseCcDType), but re-assert so a spec
    // built in code (the shipped kernels) cannot smuggle an out-of-contract dtype.
    switch (b.dtype) {
      case CcDType::F32:
      case CcDType::F16:
      case CcDType::I32:
      case CcDType::U32:
        break;
      default:
        v = fail(CcStatus::BadDType,
                 "customCompute '" + spec.id + "': binding '" + b.name +
                     "' has an out-of-contract dtype");
        return false;
    }
    const std::uint64_t bytes =
        static_cast<std::uint64_t>(elems) * ccDTypeBytes(b.dtype);
    if (bytes > limits.maxBindingBytes) {
      v = fail(CcStatus::BindingTooLarge,
               "customCompute '" + spec.id + "': binding '" + b.name + "' is " +
                   std::to_string(bytes) + " bytes (cap " +
                   std::to_string(limits.maxBindingBytes) + ")");
      return false;
    }
    return true;
  };

  CcValidation v;
  for (std::size_t k = 0; k < spec.inputs.size(); ++k) {
    const std::uint32_t elems =
        k < inputElements.size() ? inputElements[k] : 0u;
    if (!checkBinding(spec.inputs[k], elems, v)) return v;
  }
  for (const auto& o : spec.outputs) {
    // An output's byte size is its declared cap (the engine-owned bounded buffer).
    if (!checkBinding(o, o.capElements, v)) return v;
  }

  return CcValidation{CcStatus::Ok, {}};
}

// ===========================================================================
// ENC-619 — CPU references (mirror the WGSL kernels bit-for-bit in f32 math).
// ===========================================================================

std::vector<float> referenceStft(const std::vector<float>& samples,
                                  std::uint32_t fftSize, std::uint32_t hop,
                                  std::uint32_t& frames, std::uint32_t& bins) {
  frames = 0;
  bins = fftSize / 2u + 1u;
  if (fftSize == 0 || hop == 0 || samples.size() < fftSize) {
    return {};
  }
  frames = static_cast<std::uint32_t>((samples.size() - fftSize) / hop) + 1u;
  std::vector<float> mag(static_cast<std::size_t>(frames) * bins, 0.0f);
  const float twoPi = 6.28318530717958647692528676655900577f;
  for (std::uint32_t f = 0; f < frames; ++f) {
    const std::uint32_t base = f * hop;
    for (std::uint32_t k = 0; k < bins; ++k) {
      const float wk = -twoPi * static_cast<float>(k) / static_cast<float>(fftSize);
      float re = 0.0f, im = 0.0f;
      for (std::uint32_t n = 0; n < fftSize; ++n) {
        const float hann =
            0.5f * (1.0f - std::cos(twoPi * static_cast<float>(n) /
                                    static_cast<float>(fftSize - 1u)));
        const float sn = samples[base + n] * hann;
        const float ang = wk * static_cast<float>(n);
        re += sn * std::cos(ang);
        im += sn * std::sin(ang);
      }
      mag[static_cast<std::size_t>(f) * bins + k] =
          std::sqrt(re * re + im * im) / static_cast<float>(fftSize);
    }
  }
  return mag;
}

std::vector<IsoSegment> referenceMarchingSquares(const std::vector<float>& field,
                                                 std::uint32_t gridW,
                                                 std::uint32_t gridH, float iso) {
  std::vector<IsoSegment> segs;
  if (gridW < 2 || gridH < 2) return segs;
  auto at = [&](std::uint32_t col, std::uint32_t row) -> float {
    return field[static_cast<std::size_t>(row) * gridW + col];
  };
  auto lerpT = [](float a, float b, float lvl) -> float {
    const float d = b - a;
    if (d == 0.0f) return 0.5f;
    return (lvl - a) / d;
  };
  auto emit = [&](float x0, float y0, float x1, float y1) {
    segs.push_back(IsoSegment{x0, y0, x1, y1});
  };
  for (std::uint32_t gy = 0; gy + 1u < gridH; ++gy) {
    for (std::uint32_t gx = 0; gx + 1u < gridW; ++gx) {
      const float bl = at(gx, gy);
      const float br = at(gx + 1u, gy);
      const float tl = at(gx, gy + 1u);
      const float tr = at(gx + 1u, gy + 1u);
      const float fx = static_cast<float>(gx);
      const float fy = static_cast<float>(gy);
      std::uint32_t c = 0;
      if (bl >= iso) c |= 1u;
      if (br >= iso) c |= 2u;
      if (tr >= iso) c |= 4u;
      if (tl >= iso) c |= 8u;
      const float bX = fx + lerpT(bl, br, iso), bY = fy;
      const float rX = fx + 1.0f, rY = fy + lerpT(br, tr, iso);
      const float tX = fx + lerpT(tl, tr, iso), tY = fy + 1.0f;
      const float lX = fx, lY = fy + lerpT(bl, tl, iso);
      switch (c) {
        case 0u: case 15u: break;
        case 1u: case 14u: emit(lX, lY, bX, bY); break;
        case 2u: case 13u: emit(bX, bY, rX, rY); break;
        case 3u: case 12u: emit(lX, lY, rX, rY); break;
        case 4u: case 11u: emit(rX, rY, tX, tY); break;
        case 6u: case 9u:  emit(bX, bY, tX, tY); break;
        case 7u: case 8u:  emit(lX, lY, tX, tY); break;
        case 5u: {
          const float center = (bl + br + tr + tl) * 0.25f;
          if (center >= iso) { emit(lX, lY, tX, tY); emit(bX, bY, rX, rY); }
          else { emit(lX, lY, bX, bY); emit(tX, tY, rX, rY); }
          break;
        }
        case 10u: {
          const float center = (bl + br + tr + tl) * 0.25f;
          if (center >= iso) { emit(lX, lY, bX, bY); emit(tX, tY, rX, rY); }
          else { emit(lX, lY, tX, tY); emit(bX, bY, rX, rY); }
          break;
        }
        default: break;
      }
    }
  }
  return segs;
}

}  // namespace dc
