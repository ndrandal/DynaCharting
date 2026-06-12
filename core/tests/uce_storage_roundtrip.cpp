// ENC-590 (P0.2) — Storage buffer-usage path on DawnDevice (round-trip).
//
// The GPU/Dawn prerequisite that unblocks the WebGPU-compute half of the
// "GPU-Native Streaming Grammar of Graphics" project (ENC-591 spike, Phases 4 &
// 6). RESEARCH.md §5.1: today every DawnDevice buffer is created
// Vertex|Index|CopyDst|CopySrc (kStreamBufferUsage); a compute transform needs a
// buffer with BufferUsage::Storage so a compute pipeline can bind it as a
// read/write storage binding. This test exercises the new ADDITIVE
// DawnDevice::createStorageBuffer path end to end:
//   * create a Storage buffer (usage Storage|CopyDst|CopySrc),
//   * write known bytes from the CPU (queue.WriteBuffer, via the existing
//     updateBuffer / writeBufferRange streaming entry points — CopyDst),
//   * read them back via the existing async map-pump readback
//     (readBuffer: CopyBufferToBuffer -> MapRead, blocked on waitUntil() —
//     CopySrc), and
//   * assert an EXACT byte round-trip.
//
// SCOPE: only the storage usage path + round-trip. No compute pipeline / no
// transform (that is ENC-591). The vertex/index path is untouched and is still
// covered by d85_dawn_range_upload.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

// Read [0, size) of `buf` back through the map-pump and assert it byte-matches
// `expected`.
static void verifyRoundTrip(dc::DawnDevice& dev, dc::BufferHandle buf,
                            const std::vector<std::uint8_t>& expected,
                            const char* ctx) {
  std::vector<std::uint8_t> gpu(expected.size(), 0xCD);  // poison the dst first
  requireTrue(dev.readBuffer(buf, 0, expected.size(), gpu.data()),
              "readBuffer (map-pump readback) succeeded");
  if (std::memcmp(expected.data(), gpu.data(), expected.size()) != 0) {
    std::fprintf(stderr, "ASSERT FAIL [%s]: storage buffer bytes != written\n",
                 ctx);
    // Dump the first mismatch to aid debugging.
    for (std::size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != gpu[i]) {
        std::fprintf(stderr, "  first mismatch at byte %zu: want=0x%02X got=0x%02X\n",
                     i, expected[i], gpu[i]);
        break;
      }
    }
    std::exit(1);
  }
}

int main() {
  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: set "
                 "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "to force lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // --- 1. Create a storage buffer with CPU init data. ---------------------
  // 1024 bytes is comfortably above any alignment rounding and well under the
  // 128 MiB storage-binding limit. Fill with a deterministic pattern so a wrong
  // byte is easy to spot.
  const std::size_t kSize = 1024;
  std::vector<std::uint8_t> init(kSize);
  for (std::size_t i = 0; i < kSize; ++i) {
    init[i] = static_cast<std::uint8_t>((i * 31u + 7u) & 0xFFu);
  }
  dc::BufferHandle storage =
      dev.createStorageBuffer(kSize, init.data(), init.size());
  requireTrue(storage.valid(), "createStorageBuffer returned a valid handle");
  verifyRoundTrip(dev, storage, init, "after create-with-init");

  // --- 2. Full CPU rewrite (updateBuffer / queue.WriteBuffer). ------------
  // updateBuffer reuses the existing buffer when the size fits the capacity, so
  // the Storage usage is preserved (no realloc). Use a DIFFERENT pattern so this
  // proves a real write, not a stale read of step 1.
  std::vector<std::uint8_t> full(kSize);
  for (std::size_t i = 0; i < kSize; ++i) {
    full[i] = static_cast<std::uint8_t>((0xA5u ^ (i * 13u)) & 0xFFu);
  }
  dev.updateBuffer(storage, full.data(), full.size());
  verifyRoundTrip(dev, storage, full, "after full CPU rewrite");

  // --- 3. Partial CPU write (writeBufferRange) at a 4-aligned offset. -----
  // Overwrite a middle slice; the expected image is `full` with that slice
  // replaced. Offset + size are 4-aligned (WebGPU's WriteBuffer constraint).
  const std::size_t kOff = 256;
  std::uint8_t patch[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
                            0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  dev.writeBufferRange(storage, kOff, patch, sizeof(patch));
  std::vector<std::uint8_t> expected = full;
  std::memcpy(expected.data() + kOff, patch, sizeof(patch));
  verifyRoundTrip(dev, storage, expected, "after partial range write");

  // --- 4. A second, independent storage buffer (no cross-talk). -----------
  // A tiny buffer with its own pattern, to confirm storage buffers are distinct
  // slot entries and the readback addresses the right one.
  const std::size_t kSize2 = 64;
  std::vector<std::uint8_t> init2(kSize2);
  for (std::size_t i = 0; i < kSize2; ++i) {
    init2[i] = static_cast<std::uint8_t>(0xF0u - i);
  }
  dc::BufferHandle storage2 =
      dev.createStorageBuffer(kSize2, init2.data(), init2.size());
  requireTrue(storage2.valid(), "second createStorageBuffer valid");
  verifyRoundTrip(dev, storage2, init2, "second storage buffer");
  // The first buffer is still intact (independent of the second).
  verifyRoundTrip(dev, storage, expected, "first storage buffer unchanged");

  std::printf(
      "\nENC-590 storage round-trip: OK "
      "(create + CPU write + range write + readback all byte-exact)\n");
  return 0;
}
