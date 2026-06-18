// ENC-637 (E3) — per-instance transition lanes: apply E2's per-row progress to a
// freshly-encoded per-instance buffer so entering rows fade/scale IN and exiting
// rows fade/scale OUT, keyed by durable row id (object constancy).
//
// The render side of data-bound transitions. The InteractionRuntime (B5b) re-encodes
// each mark every refresh into full-strength instance bytes; this post-pass then
// modulates each instance by AnimationController::progressOf(rowId) BEFORE upload.
// Because it runs on the fresh full-strength encode each frame, scaling never
// compounds. Untracked rows (no transition) are left untouched (progress treated as
// 1). Pure `dc`, operates in place on the instance byte buffer.
//
// Lane offsets follow the ENC-608/609 formats:
//   Rect4Color  (24B): rect4(16) + RGBA8(@16) + scalar/rowid(@20)
//   Point4Color (16B): pos2(8)   + RGBA8(@8)  + size(@12)
// RGBA8 byte order is R,G,B,A in the low..high bytes, so alpha is at rgbaOffset+3.
#pragma once

#include "dc/anim/AnimationController.hpp"

#include <cstdint>
#include <vector>

namespace dc {

// Scale each instance's RGBA8 ALPHA by its row's transition progress (opacity
// fade). `bytes` is the instance buffer; `count` instances of `stride` bytes;
// `rgbaOffset` the RGBA8 lane offset within an instance; `rowIds` the per-instance
// durable ids (EncodeResult::instanceRowIds, same order). A row the controller is
// not tracking keeps its alpha (progress = 1).
void applyOpacityTransition(std::uint8_t* bytes, std::size_t count,
                            std::size_t stride, std::size_t rgbaOffset,
                            const std::vector<std::int32_t>& rowIds,
                            const AnimationController& ctrl);

// Scale each instance's f32 SIZE/scalar lane by its row's progress (scale-in/out).
// `sizeOffset` is the f32 lane offset within an instance (e.g. 12 for Point4Color).
void applyScaleTransition(std::uint8_t* bytes, std::size_t count,
                          std::size_t stride, std::size_t sizeOffset,
                          const std::vector<std::int32_t>& rowIds,
                          const AnimationController& ctrl);

}  // namespace dc
