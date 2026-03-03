// D53.2 — ValidationConfig: normal mode clamps values with warnings
#include "dc/commands/ValidationConfig.hpp"

#include <cstdio>
#include <cmath>
#include <string>

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
  std::printf("=== D53.2 ValidationConfig Clamp Mode ===\n");

  dc::ValidationConfig cfg;
  cfg.strictMode = false;

  // Test 1: Normal mode clamps pointSize=300 to 256 with warning
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(300.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(ok, "normal mode does not reject pointSize=300");
    check(std::fabs(clamped - 256.0f) < 1e-6f, "pointSize=300 clamped to 256");
    check(warnings.size() == 1, "one warning generated for clamped pointSize");
    if (!warnings.empty()) {
      check(warnings[0].field == "pointSize", "warning field is 'pointSize'");
      check(warnings[0].message.find("clamped") != std::string::npos, "warning mentions 'clamped'");
    }
  }

  // Test 2: Normal mode clamps lineWidth=100 to 64 with warning
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(100.0f, 0.0f, cfg.maxLineWidth, "lineWidth",
                                cfg.strictMode, warnings, clamped);
    check(ok, "normal mode does not reject lineWidth=100");
    check(std::fabs(clamped - 64.0f) < 1e-6f, "lineWidth=100 clamped to 64");
    check(warnings.size() == 1, "one warning generated for clamped lineWidth");
    if (!warnings.empty()) {
      check(warnings[0].field == "lineWidth", "warning field is 'lineWidth'");
    }
  }

  // Test 3: Normal mode clamps cornerRadius=600 to 512 with warning
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(600.0f, 0.0f, cfg.maxCornerRadius, "cornerRadius",
                                cfg.strictMode, warnings, clamped);
    check(ok, "normal mode does not reject cornerRadius=600");
    check(std::fabs(clamped - 512.0f) < 1e-6f, "cornerRadius=600 clamped to 512");
    check(warnings.size() == 1, "one warning generated for clamped cornerRadius");
  }

  // Test 4: Normal mode clamps negative to minimum (0)
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 999.0f;
    bool ok = dc::validateRange(-10.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(ok, "normal mode does not reject negative");
    check(std::fabs(clamped - 0.0f) < 1e-6f, "negative value clamped to 0");
    check(warnings.size() == 1, "one warning for clamped negative");
  }

  // Test 5: Valid value produces no warning and passes through unchanged
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    bool ok = dc::validateRange(50.0f, 0.0f, cfg.maxPointSize, "pointSize",
                                cfg.strictMode, warnings, clamped);
    check(ok, "valid value accepted in normal mode");
    check(std::fabs(clamped - 50.0f) < 1e-6f, "valid value passes through unchanged");
    check(warnings.empty(), "no warnings for valid value");
  }

  // Test 6: Multiple clamps accumulate warnings
  {
    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;

    dc::validateRange(300.0f, 0.0f, cfg.maxPointSize, "pointSize",
                      cfg.strictMode, warnings, clamped);
    dc::validateRange(100.0f, 0.0f, cfg.maxLineWidth, "lineWidth",
                      cfg.strictMode, warnings, clamped);
    dc::validateRange(600.0f, 0.0f, cfg.maxCornerRadius, "cornerRadius",
                      cfg.strictMode, warnings, clamped);

    check(warnings.size() == 3, "three warnings accumulated across multiple validations");
    check(warnings[0].field == "pointSize", "first warning is pointSize");
    check(warnings[1].field == "lineWidth", "second warning is lineWidth");
    check(warnings[2].field == "cornerRadius", "third warning is cornerRadius");
  }

  // Test 7: Normal color validation produces warnings but succeeds
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(1.5f, -0.2f, 0.5f, 1.0f, false, warnings);
    check(ok, "normal mode color validation succeeds with out-of-range");
    check(warnings.size() == 2, "two warnings for two invalid channels (r=1.5, g=-0.2)");
  }

  // Test 8: Valid color produces no warnings in normal mode
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(0.0f, 0.5f, 1.0f, 0.8f, false, warnings);
    check(ok, "valid color accepted in normal mode");
    check(warnings.empty(), "no warnings for valid color");
  }

  // Test 9: All four color channels out of range
  {
    std::vector<dc::ValidationWarning> warnings;
    bool ok = dc::validateColor(2.0f, -1.0f, 5.0f, -0.5f, false, warnings);
    check(ok, "normal mode accepts with 4 bad channels");
    check(warnings.size() == 4, "four warnings for four invalid channels");
  }

  // Test 10: ValidationWarning fields are correct
  {
    std::vector<dc::ValidationWarning> warnings;
    dc::validateColor(2.0f, 0.5f, 0.5f, 0.5f, false, warnings);
    check(warnings.size() == 1, "one warning for r=2.0");
    if (!warnings.empty()) {
      check(warnings[0].field == "color.r", "warning field is 'color.r'");
      check(warnings[0].message.find("2") != std::string::npos, "warning message contains value");
    }
  }

  // Test 11: Custom ValidationConfig limits
  {
    dc::ValidationConfig custom;
    custom.strictMode = false;
    custom.maxPointSize = 10.0f;

    std::vector<dc::ValidationWarning> warnings;
    float clamped = 0.0f;
    dc::validateRange(15.0f, 0.0f, custom.maxPointSize, "pointSize",
                      custom.strictMode, warnings, clamped);
    check(std::fabs(clamped - 10.0f) < 1e-6f, "custom max: 15 clamped to 10");
  }

  std::printf("=== D53.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
