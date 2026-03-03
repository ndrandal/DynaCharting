#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace dc {

struct ValidationWarning {
  std::string field;
  std::string message;
};

struct ValidationConfig {
  bool strictMode{false};
  float maxPointSize{256.0f};
  float maxLineWidth{64.0f};
  float maxCornerRadius{512.0f};
  std::uint32_t maxVertexCount{16 * 1024 * 1024};
  std::uint32_t maxByteLength{256 * 1024 * 1024};
};

// Free helpers
bool validateRange(float val, float minV, float maxV, const std::string& field,
                   bool strict, std::vector<ValidationWarning>& warnings, float& clamped);
bool validateColor(float r, float g, float b, float a,
                   bool strict, std::vector<ValidationWarning>& warnings);

} // namespace dc
