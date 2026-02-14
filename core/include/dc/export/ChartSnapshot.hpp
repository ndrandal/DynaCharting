#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// D18: Chart screenshot/export utilities.

// Write raw RGBA pixel data to a PPM file (RGB, no alpha).
bool writePPM(const std::string& path,
              const std::uint8_t* pixels,
              int width, int height);

// Write raw RGBA pixel data to a PPM file, flipping Y axis
// (OpenGL readPixels is bottom-up; PPM is top-down).
bool writePPMFlipped(const std::string& path,
                      const std::uint8_t* pixels,
                      int width, int height);

// D18.2: Write raw RGBA pixel data to a PNG file (RGB, no alpha).
// Self-contained encoder using stored deflate (no zlib/libpng dependency).
bool writePNG(const std::string& path,
              const std::uint8_t* pixels,
              int width, int height);

// Write raw RGBA pixel data to a PNG file, flipping Y axis
// (OpenGL readPixels is bottom-up; PNG is top-down).
bool writePNGFlipped(const std::string& path,
                      const std::uint8_t* pixels,
                      int width, int height);

} // namespace dc
