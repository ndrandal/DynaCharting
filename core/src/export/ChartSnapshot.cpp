#include "dc/export/ChartSnapshot.hpp"
#include <cstdio>
#include <algorithm>

namespace dc {

// ---------------------------------------------------------------------------
// PPM export (D18.1)
// ---------------------------------------------------------------------------

bool writePPM(const std::string& path,
              const std::uint8_t* pixels,
              int width, int height) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;

  std::fprintf(f, "P6\n%d %d\n255\n", width, height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                          static_cast<std::size_t>(x)) * 4;
      std::fputc(pixels[idx + 0], f); // R
      std::fputc(pixels[idx + 1], f); // G
      std::fputc(pixels[idx + 2], f); // B
    }
  }

  std::fclose(f);
  return true;
}

bool writePPMFlipped(const std::string& path,
                      const std::uint8_t* pixels,
                      int width, int height) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;

  std::fprintf(f, "P6\n%d %d\n255\n", width, height);
  for (int y = height - 1; y >= 0; y--) {
    for (int x = 0; x < width; x++) {
      std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                          static_cast<std::size_t>(x)) * 4;
      std::fputc(pixels[idx + 0], f); // R
      std::fputc(pixels[idx + 1], f); // G
      std::fputc(pixels[idx + 2], f); // B
    }
  }

  std::fclose(f);
  return true;
}

// ---------------------------------------------------------------------------
// PNG export (D18.2) — self-contained, no zlib/libpng dependency.
// Uses stored deflate blocks (uncompressed) for simplicity.
// ---------------------------------------------------------------------------

namespace {

// CRC32 lookup table (PNG uses ISO 3309 / ITU-T V.42 polynomial).
static std::uint32_t sCrcTable[256];
static bool sCrcTableReady = false;

void initCrcTable() {
  if (sCrcTableReady) return;
  for (std::uint32_t n = 0; n < 256; n++) {
    std::uint32_t c = n;
    for (int k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xEDB88320u ^ (c >> 1);
      else
        c = c >> 1;
    }
    sCrcTable[n] = c;
  }
  sCrcTableReady = true;
}

std::uint32_t computeCrc32(const std::uint8_t* data, std::size_t len) {
  std::uint32_t c = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; i++) {
    c = sCrcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

std::uint32_t computeAdler32(const std::uint8_t* data, std::size_t len) {
  std::uint32_t a = 1, b = 0;
  // Process in chunks to avoid overflow of the 32-bit accumulators.
  // The Adler-32 modulus is 65521 (largest prime < 2^16).
  constexpr std::uint32_t MOD = 65521u;
  constexpr std::size_t NMAX = 5552; // max bytes before modding (see RFC 1950)
  std::size_t remaining = len;
  std::size_t offset = 0;
  while (remaining > 0) {
    std::size_t chunk = std::min(remaining, NMAX);
    for (std::size_t i = 0; i < chunk; i++) {
      a += data[offset + i];
      b += a;
    }
    a %= MOD;
    b %= MOD;
    offset += chunk;
    remaining -= chunk;
  }
  return (b << 16) | a;
}

void pushBE32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
  buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void writeChunk(std::vector<std::uint8_t>& out,
                const char type[4],
                const std::uint8_t* data, std::size_t dataLen) {
  // Length (4B BE)
  pushBE32(out, static_cast<std::uint32_t>(dataLen));

  // Type (4B)
  std::size_t typeStart = out.size();
  out.push_back(static_cast<std::uint8_t>(type[0]));
  out.push_back(static_cast<std::uint8_t>(type[1]));
  out.push_back(static_cast<std::uint8_t>(type[2]));
  out.push_back(static_cast<std::uint8_t>(type[3]));

  // Data
  if (dataLen > 0 && data) {
    out.insert(out.end(), data, data + dataLen);
  }

  // CRC32 over type + data
  std::uint32_t crc = computeCrc32(&out[typeStart], 4 + dataLen);
  pushBE32(out, crc);
}

// Build the filtered image data (filter byte 0x00 = None + RGB per row).
// rowOrder: +1 for top-to-bottom, -1 for flipped (bottom-to-top input).
std::vector<std::uint8_t> buildRawImageData(
    const std::uint8_t* pixels, int width, int height, bool flip) {
  const std::size_t rowBytes = 1 + static_cast<std::size_t>(width) * 3; // filter + RGB
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(height) * rowBytes);

  for (int row = 0; row < height; row++) {
    int srcY = flip ? (height - 1 - row) : row;
    raw.push_back(0x00); // filter type: None
    for (int x = 0; x < width; x++) {
      std::size_t idx = (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(width) +
                          static_cast<std::size_t>(x)) * 4; // RGBA input
      raw.push_back(pixels[idx + 0]); // R
      raw.push_back(pixels[idx + 1]); // G
      raw.push_back(pixels[idx + 2]); // B
    }
  }
  return raw;
}

// Wrap raw data in zlib stored-block format (RFC 1950 / RFC 1951).
// No actual compression — just framing for PNG compatibility.
std::vector<std::uint8_t> wrapZlibStored(const std::uint8_t* data, std::size_t len) {
  std::vector<std::uint8_t> zlib;
  // Conservative reserve: header(2) + blocks(5-byte header per 65535) + data + adler(4)
  std::size_t numBlocks = (len + 65534) / 65535;
  zlib.reserve(2 + numBlocks * 5 + len + 4);

  // Zlib header: CMF=0x78 (deflate, window=32K), FLG=0x01 (no dict, check bits)
  zlib.push_back(0x78);
  zlib.push_back(0x01);

  // Stored deflate blocks (BTYPE=00), max 65535 bytes each.
  std::size_t remaining = len;
  std::size_t offset = 0;
  while (remaining > 0) {
    std::size_t blockLen = std::min(remaining, static_cast<std::size_t>(65535));
    remaining -= blockLen;

    std::uint8_t bfinal = (remaining == 0) ? 0x01 : 0x00;
    zlib.push_back(bfinal); // BFINAL | BTYPE=00

    auto len16 = static_cast<std::uint16_t>(blockLen);
    std::uint16_t nlen16 = static_cast<std::uint16_t>(~len16);
    zlib.push_back(static_cast<std::uint8_t>(len16 & 0xFF));
    zlib.push_back(static_cast<std::uint8_t>((len16 >> 8) & 0xFF));
    zlib.push_back(static_cast<std::uint8_t>(nlen16 & 0xFF));
    zlib.push_back(static_cast<std::uint8_t>((nlen16 >> 8) & 0xFF));

    zlib.insert(zlib.end(), data + offset, data + offset + blockLen);
    offset += blockLen;
  }

  // Adler-32 checksum (big-endian)
  std::uint32_t a32 = computeAdler32(data, len);
  pushBE32(zlib, a32);

  return zlib;
}

bool writePNGImpl(const std::string& path,
                  const std::uint8_t* pixels,
                  int width, int height, bool flip) {
  if (!pixels || width <= 0 || height <= 0) return false;

  initCrcTable();

  std::vector<std::uint8_t> out;
  // Rough estimate: header + IHDR + uncompressed IDAT + IEND
  out.reserve(128 + static_cast<std::size_t>(height) * (1 + static_cast<std::size_t>(width) * 3) + 256);

  // PNG signature
  const std::uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  out.insert(out.end(), sig, sig + 8);

  // IHDR chunk (13 bytes)
  std::vector<std::uint8_t> ihdr;
  ihdr.reserve(13);
  pushBE32(ihdr, static_cast<std::uint32_t>(width));
  pushBE32(ihdr, static_cast<std::uint32_t>(height));
  ihdr.push_back(8);  // bit depth
  ihdr.push_back(2);  // color type: RGB
  ihdr.push_back(0);  // compression method: deflate
  ihdr.push_back(0);  // filter method: adaptive
  ihdr.push_back(0);  // interlace: none
  writeChunk(out, "IHDR", ihdr.data(), ihdr.size());

  // Build filtered image data
  auto rawImg = buildRawImageData(pixels, width, height, flip);

  // Wrap in zlib stored format
  auto zlibData = wrapZlibStored(rawImg.data(), rawImg.size());

  // IDAT chunk
  writeChunk(out, "IDAT", zlibData.data(), zlibData.size());

  // IEND chunk
  writeChunk(out, "IEND", nullptr, 0);

  // Write to file
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::size_t written = std::fwrite(out.data(), 1, out.size(), f);
  std::fclose(f);
  return written == out.size();
}

} // anonymous namespace

bool writePNG(const std::string& path,
              const std::uint8_t* pixels,
              int width, int height) {
  return writePNGImpl(path, pixels, width, height, false);
}

bool writePNGFlipped(const std::string& path,
                      const std::uint8_t* pixels,
                      int width, int height) {
  return writePNGImpl(path, pixels, width, height, true);
}

} // namespace dc
