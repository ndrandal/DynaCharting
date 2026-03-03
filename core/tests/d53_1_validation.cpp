// D53.1 — ValidationConfig: strict mode rejects out-of-range values
#include "dc/commands/ValidationConfig.hpp"

#include <cstdio>
#include <cmath>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D53.1 ValidationConfig Strict Mode ===\n");

  dc::ValidationConfig cfg;
  cfg.strictMode = true;

  // Test 1: Strict mode rejects pointSize > maxPointSize (256)
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(300.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(!ok, "strict rejects pointSize=300 (>256 max)");
    check(warnings.empty(), "strict produces no warnings on rejection");
  }

  // Test 2: Strict mode rejects lineWidth > maxLineWidth (64)
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(100.0f, 0.0f, cfg.maxLineWidth, "lineWidth",
                                cfg.strictMode, warnings, clamped);
    check(!ok, "strict rejects lineWidth=100 (>64 max)");
  }

  // Test 3: Strict mode rejects cornerRadius > maxCornerRadius (512)
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(600.0f, 0.0f, cfg.maxCornerRadius, "cornerRadius",
                                cfg.strictMode, warnings, clamped);
    check(!ok, "strict rejects cornerRadius=600 (>512 max)");
  }

  // Test 4: Strict mode rejects negative values
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(-5.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(!ok, "strict rejects negative pointSize");
  }

  // Test 5: Strict mode accepts valid values
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(128.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(ok, "strict accepts pointSize=128 (within [0,256])");
    check(std::fabs(clamped - 128.0f) < 1e-6f, "clamped value equals input when valid");
    check(warnings.empty(), "no warnings for valid value");
  }

  // Test 6: Strict mode accepts boundary values
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(256.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(ok, "strict accepts pointSize=256 (boundary)");
    check(std::fabs(clamped - 256.0f) < 1e-6f, "clamped value equals max boundary");
  }

  // Test 7: Strict color validation rejects out-of-range channel
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(1.5f, 0.5f, 0.5f, 1.0f, true, warnings);
    check(!ok, "strict rejects color r=1.5");
  }

  // Test 8: Strict color validation accepts valid color
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(0.0f, 0.5f, 1.0f, 1.0f, true, warnings);
    check(ok, "strict accepts valid color");
    check(warnings.empty(), "no warnings for valid color");
  }

  // Test 9: Strict color validation rejects negative channel
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(0.5f, -0.1f, 0.5f, 1.0f, true, warnings);
    check(!ok, "strict rejects color g=-0.1");
  }

  // Test 10: Strict color validation rejects alpha > 1
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(0.5f, 0.5f, 0.5f, 1.5f, true, warnings);
    check(!ok, "strict rejects color a=1.5");
  }

  // Test 11: Default config values
  {
    dc::ValidationConfig def;
    check(!def.strictMode, "default strictMode is false");
    check(std::fabs(def.maxPointSize - 256.0f) < 1e-6f, "default maxPointSize is 256");
    check(std::fabs(def.maxLineWidth - 64.0f) < 1e-6f, "default maxLineWidth is 64");
    check(std::fabs(def.maxCornerRadius - 512.0f) < 1e-6f, "default maxCornerRadius is 512");
    check(def.maxVertexCount == 16u * 1024u * 1024u, "default maxVertexCount is 16M");
    check(def.maxByteLength == 256u * 1024u * 1024u, "default maxByteLength is 256M");
  }

  std::printf("=== D53.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
