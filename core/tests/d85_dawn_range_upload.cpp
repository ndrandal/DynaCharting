// ENC-485 (P2.2) — Dawn streaming buffer upload (coalesced dirty ranges).
//
// The Dawn counterpart of d81_3_range_upload.cpp (the GL baseline). It drives the
// SHARED CpuBufferStore coalescing path (per-id CPU bytes + adjacent/overlapping
// writeRange merging + UploadStats) but routes the actual upload through the
// headless DawnDevice (queue.WriteBuffer per coalesced range, realloc-on-grow for
// full uploads), via a DeviceBufferResolver. It asserts:
//   * the SAME coalescing stats d81_3 checks for GL (10 contiguous tail writes ->
//     1 sub-upload; disjoint writes -> separate sub-uploads; grow-past-capacity ->
//     a full reupload), proving the device-agnostic coalescing is intact, and
//   * CORRECTNESS of the bytes that actually landed on the GPU, by reading the
//     Dawn buffer back (CopyBufferToBuffer -> MapRead) and comparing to the CPU
//     bytes — the Dawn analogue of d81_3 but with a real GPU readback.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/render/CpuBufferStore.hpp"

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

static void requireEq(std::uint64_t got, std::uint64_t want, const char* ctx) {
  if (got != want) {
    std::fprintf(stderr, "ASSERT FAIL [%s]: got=%llu want=%llu\n", ctx,
                 (unsigned long long)got, (unsigned long long)want);
    std::exit(1);
  }
}

// Read the whole Dawn buffer for `bufId` back and assert it byte-matches the
// CpuBufferStore's CPU copy over [0, size).
static void verifyBytesMatch(dc::DawnDevice& dev,
                             dc::DeviceBufferResolver& resolver,
                             dc::CpuBufferStore& store, dc::Id bufId,
                             const char* ctx) {
  const std::uint8_t* cpu = store.getCpuData(bufId);
  const std::uint32_t size = store.getCpuDataSize(bufId);
  requireTrue(cpu != nullptr && size > 0, "cpu data present for readback");
  dc::BufferHandle h = resolver.handleFor(bufId);
  requireTrue(h.valid(), "device buffer exists for readback");

  std::vector<std::uint8_t> gpu(size, 0xCD);
  requireTrue(dev.readBuffer(h, 0, size, gpu.data()), "readBuffer succeeded");
  if (std::memcmp(cpu, gpu.data(), size) != 0) {
    std::fprintf(stderr, "ASSERT FAIL [%s]: GPU bytes != CPU bytes\n", ctx);
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

  dc::CpuBufferStore store;
  dc::DeviceBufferResolver resolver(dev);
  const dc::Id bufId = 42;

  // --- Initial full upload (full reupload path). --------------------------
  const std::uint32_t initialSize = 1024;
  std::vector<std::uint8_t> init(initialSize);
  for (std::uint32_t i = 0; i < initialSize; ++i)
    init[i] = static_cast<std::uint8_t>(i & 0xFF);
  store.setCpuData(bufId, init.data(), initialSize);
  store.uploadDirty(dev, resolver);
  auto s = store.lastUploadStats();
  requireEq(s.fullUploads, 1, "initial upload is a full reupload");
  requireEq(s.subUploads, 0, "no subUploads on initial full upload");
  requireEq(s.bytesUploaded, initialSize, "full upload = entire buffer");
  verifyBytesMatch(dev, resolver, store, bufId, "after initial full upload");

  // --- 10 contiguous tail writes coalesce into 1 sub-upload. --------------
  std::uint8_t payload[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
  for (int i = 0; i < 10; ++i) {
    store.writeRange(bufId, 100 + i * 8, payload, 8);  // contiguous 100..180
  }
  store.uploadDirty(dev, resolver);
  s = store.lastUploadStats();
  requireEq(s.fullUploads, 0, "no full reupload when size unchanged");
  requireEq(s.subUploads, 1, "10 contiguous writes coalesce into 1 subUpload");
  requireEq(s.bytesUploaded, 80, "exactly 80 bytes uploaded, not full buffer");
  requireEq(s.rangesCoalesced, 1, "the single coalesced range counted");
  verifyBytesMatch(dev, resolver, store, bufId, "after coalesced tail writes");

  // --- Two disjoint writes -> two sub-uploads. ----------------------------
  store.writeRange(bufId, 0, payload, 8);
  store.writeRange(bufId, 512, payload, 8);
  store.uploadDirty(dev, resolver);
  s = store.lastUploadStats();
  requireEq(s.subUploads, 2, "two disjoint writes = two subUploads");
  requireEq(s.bytesUploaded, 16, "two writes of 8 bytes each");
  verifyBytesMatch(dev, resolver, store, bufId, "after two disjoint writes");

  // --- Grow past capacity forces a full reupload (realloc on Dawn). -------
  store.writeRange(bufId, initialSize, payload, 8);  // extends to 1032
  store.uploadDirty(dev, resolver);
  s = store.lastUploadStats();
  requireEq(s.fullUploads, 1, "grow past capacity forces full reupload");
  requireEq(s.bytesUploaded, initialSize + 8, "full reupload = entire new size");
  verifyBytesMatch(dev, resolver, store, bufId, "after grow + full reupload");

  // --- Subsequent within-capacity write goes back to sub-upload. ----------
  store.writeRange(bufId, 1020, payload, 8);
  store.uploadDirty(dev, resolver);
  s = store.lastUploadStats();
  requireEq(s.fullUploads, 0, "within-capacity write stays in subUpload path");
  requireEq(s.subUploads, 1, "single subUpload for single range");
  verifyBytesMatch(dev, resolver, store, bufId, "after within-capacity tail write");

  std::printf("\nD85 Dawn range upload: OK (coalescing + GPU readback match)\n");
  return 0;
}
