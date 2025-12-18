#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>

namespace dc {

using Id = std::uint64_t;

inline constexpr Id kInvalidId = 0;

// Very small helper: accept either JSON numeric IDs (preferred) or string decimal.
inline Id parseIdString(const std::string& s) {
  if (s.empty()) return kInvalidId;
  std::uint64_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') throw std::runtime_error("Id must be decimal digits");
    v = v * 10 + static_cast<std::uint64_t>(c - '0');
  }
  return static_cast<Id>(v);
}

} // namespace dc
