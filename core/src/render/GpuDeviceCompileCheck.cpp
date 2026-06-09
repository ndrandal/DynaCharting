// ENC-481 (P1.1) — compile-check translation unit.
//
// The `dc` library globs core/src/*.cpp (excluding gl/ and host/). The new
// device-agnostic interfaces in dc/render/ are header-only, so without a .cpp
// that includes them they would never be compiled by the `dc` build and a
// regression (e.g. a leaked GL type, a malformed descriptor) would slip
// through. This TU pulls them in so the pure `dc` target actually type-checks
// the headers. It adds NO runtime behaviour — there is nothing to link beyond
// the (empty) anchor symbol below.

#include "dc/render/GpuDevice.hpp"
#include "dc/render/IRendererBackend.hpp"

#include <type_traits>

namespace dc {
namespace {

// Static assertions documenting + enforcing the POD/value-type contract: the
// handles and descriptors that cross the GpuDevice boundary must stay trivial
// and free of any graphics-API objects. If someone later embeds a GLuint
// wrapper or a WGPU* handle here, these break the `dc` build immediately.
static_assert(std::is_trivially_copyable<BufferHandle>::value,
              "BufferHandle must stay a trivial opaque handle");
static_assert(std::is_trivially_copyable<TextureHandle>::value,
              "TextureHandle must stay a trivial opaque handle");
static_assert(std::is_trivially_copyable<PipelineHandle>::value,
              "PipelineHandle must stay a trivial opaque handle");
static_assert(std::is_trivially_copyable<BindGroupHandle>::value,
              "BindGroupHandle must stay a trivial opaque handle");
static_assert(std::is_trivially_copyable<RenderTargetHandle>::value,
              "RenderTargetHandle must stay a trivial opaque handle");
static_assert(sizeof(BufferHandle) == sizeof(std::uint32_t),
              "opaque handle should wrap exactly one u32 id");

static_assert(std::is_abstract<GpuDevice>::value,
              "GpuDevice is an interface to be implemented by GlDevice/DawnDevice");
static_assert(std::is_abstract<IRendererBackend>::value,
              "IRendererBackend is an interface implemented per pipeline");

// Anchor symbol so the TU is never considered empty by any toolchain.
extern const char kGpuDeviceCompileCheck;
const char kGpuDeviceCompileCheck = 0;

}  // namespace
}  // namespace dc
