#pragma once
#include <cstdint>
#include <vector>

namespace dc {

class MessageCodec {
public:
  // Simple stored-block compression (prefix length + raw data, like deflate stored blocks)
  static std::vector<std::uint8_t> compress(const std::uint8_t* data, std::size_t size);
  static std::vector<std::uint8_t> decompress(const std::uint8_t* data, std::size_t size);
  static bool isCompressed(const std::uint8_t* data, std::size_t size);
};

class BackoffCalculator {
public:
  BackoffCalculator(double initialMs = 1000.0, double maxMs = 30000.0, double multiplier = 2.0);
  double nextDelay();
  void reset();
  int attempt() const;

private:
  double initialMs_, maxMs_, multiplier_;
  double currentMs_;
  int attempt_{0};
};

} // namespace dc
