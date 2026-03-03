#include "dc/data/MessageCodec.hpp"

#include <algorithm>
#include <cstring>

namespace dc {

// Magic bytes: 0xDC 0x43 0x4D 0x50 ("DC" + "CMP")
static constexpr std::uint8_t kMagic[4] = {0xDC, 0x43, 0x4D, 0x50};
static constexpr std::size_t kHeaderSize = 8; // 4 magic + 4 original size

// -------------------- compress --------------------

std::vector<std::uint8_t> MessageCodec::compress(const std::uint8_t* data, std::size_t size) {
  std::vector<std::uint8_t> out;
  out.reserve(kHeaderSize + size);

  // Write magic
  out.insert(out.end(), kMagic, kMagic + 4);

  // Write original size as 4-byte little-endian
  std::uint32_t origSize = static_cast<std::uint32_t>(size);
  out.push_back(static_cast<std::uint8_t>(origSize & 0xFF));
  out.push_back(static_cast<std::uint8_t>((origSize >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((origSize >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((origSize >> 24) & 0xFF));

  // Append raw data
  if (data && size > 0) {
    out.insert(out.end(), data, data + size);
  }

  return out;
}

// -------------------- decompress --------------------

std::vector<std::uint8_t> MessageCodec::decompress(const std::uint8_t* data, std::size_t size) {
  if (!isCompressed(data, size)) {
    return {};
  }

  // Read original size (LE)
  std::uint32_t origSize = static_cast<std::uint32_t>(data[4])
                         | (static_cast<std::uint32_t>(data[5]) << 8)
                         | (static_cast<std::uint32_t>(data[6]) << 16)
                         | (static_cast<std::uint32_t>(data[7]) << 24);

  // Validate: remaining bytes should match origSize
  std::size_t payloadSize = size - kHeaderSize;
  if (payloadSize != origSize) {
    return {};
  }

  return std::vector<std::uint8_t>(data + kHeaderSize, data + size);
}

// -------------------- isCompressed --------------------

bool MessageCodec::isCompressed(const std::uint8_t* data, std::size_t size) {
  if (!data || size < kHeaderSize) {
    return false;
  }
  return data[0] == kMagic[0]
      && data[1] == kMagic[1]
      && data[2] == kMagic[2]
      && data[3] == kMagic[3];
}

// -------------------- BackoffCalculator --------------------

BackoffCalculator::BackoffCalculator(double initialMs, double maxMs, double multiplier)
    : initialMs_(initialMs), maxMs_(maxMs), multiplier_(multiplier), currentMs_(initialMs) {}

double BackoffCalculator::nextDelay() {
  double delay = currentMs_;
  ++attempt_;
  currentMs_ = std::min(currentMs_ * multiplier_, maxMs_);
  return delay;
}

void BackoffCalculator::reset() {
  currentMs_ = initialMs_;
  attempt_ = 0;
}

int BackoffCalculator::attempt() const {
  return attempt_;
}

} // namespace dc
