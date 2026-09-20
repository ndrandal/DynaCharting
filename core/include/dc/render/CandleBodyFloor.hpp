#pragma once

// ENC-1251 — the doji floor: single source of truth for the MINIMUM HEIGHT a
// candle body may be drawn at.
//
// WHY IT EXISTS
// -------------
// `instancedCandle@1` builds the body quad from min(open,close)..max(open,close).
// When open == close those are the same number, both triangles are DEGENERATE —
// zero area, zero fragments — and the open/close level is not drawn at all. The
// wick still spans low..high, so the bar renders as a bare vertical line.
//
// That is not a corner case. ENC-1251 wiretapped the live dataplane for 100 s
// (customer-layer -> embassy `candles-v1` -> GMA_V3 -> feed-simulator, NEXO /
// lastPrice, 3 s tumbling windows) and decoded the candle6 records that drew the
// chart: 17 of 146 — 11.6% — carried open == close EXACTLY. A three-second
// window whose first tick and last tick agree is an ordinary doji. Every other
// body in that capture measured >= 27 device px, so the distribution is bimodal:
// a body is either plainly visible or literally absent, never "thin". That is
// what settled SPEC section 1.0's open question — "several wicks carry no
// visible body … real volatility or a geometry defect?" — as BOTH: the varying
// body-to-wick proportion is real, and the bodyless bars are this defect.
//
// It is a TIER 0 failure (SPEC D1, "the mark depicts the data"): the record
// carries an open and a close, and the render depicted neither.
//
// THE RULE
// --------
// Floor the body's height in CLIP space — a true pixel quantity under any
// transform and any zoom, rather than a data-space epsilon that would mean
// something different on every chart. This is the vertical counterpart of
// ENC-1257's horizontal rule (`dc/render/BarSizing.hpp`) and rests on the same
// observation: the reader's eye works in pixels, and the rasteriser samples
// pixel CENTRES with no MSAA.
//
// `kMinBodyHeightPx` is 2, matching the wick's width and for the same reason —
// a one-pixel span that straddles a row boundary can rasterise into neither row
// (see the u_wickHalf comment in DawnInstancedCandleBackend.cpp).
//
// It is a FLOOR, not a resize: a body that already clears it is untouched, bit
// for bit, which is why every pre-existing render in this repo is unchanged.
// And the floored body is slid back INSIDE the wick whenever the wick has the
// room, so a doji sitting on its own high cannot overhang low..high and claim a
// price the bar never traded at.
//
// Both the visible backend and the PICK backend include this header and call
// the same WGSL function, so the pick footprint stays the drawn footprint —
// the invariant ENC-1257 established and the reason this is a shared snippet
// rather than the same forty lines pasted twice.

namespace dc {

// The minimum candle-body height, in DEVICE PIXELS.
constexpr float kMinBodyHeightPx = 2.0f;

// `kMinBodyHeightPx` expressed in clip units for a viewport `viewH` pixels
// tall. Clip y spans [-1, 1] over `viewH` pixels, so one pixel is 2/viewH.
// Returns 0 — "rule off, geometry exactly as it was" — without a viewport.
inline float candleBodyMinHeightClip(int viewH) {
  return viewH > 0 ? 2.0f * kMinBodyHeightPx / static_cast<float>(viewH) : 0.0f;
}

// The WGSL half. Returns the body's two clip-space y end-points, floored to
// `minH` and slid back inside the wick when it has room.
//
// Orientation is PRESERVED: a transform with a negative y scale hands the end
// points back in the other order, and re-sorting them here would flip the
// quad's winding for no reason. When the floor does not engage the result is
// exactly `(m*(cx,body0)).y, (m*(cx,body1)).y` — the pre-ENC-1251 geometry.
constexpr const char* kCandleBodyFloorWgsl = R"WGSL(
fn dcCandleBodyFloor(m : mat3x3<f32>, cx : f32, body0 : f32, body1 : f32,
                     low : f32, high : f32, minH : f32) -> vec2<f32> {
  let pb0 = m * vec3<f32>(cx, body0, 1.0);
  let pb1 = m * vec3<f32>(cx, body1, 1.0);
  var by0 = pb0.y;
  var by1 = pb1.y;
  if (minH > 0.0 && abs(by1 - by0) < minH) {
    let mid   = (by0 + by1) * 0.5;
    let halfH = select(-minH, minH, by1 >= by0) * 0.5;
    by0 = mid - halfH;
    by1 = mid + halfH;
    let pw0 = m * vec3<f32>(cx, low, 1.0);
    let pw1 = m * vec3<f32>(cx, high, 1.0);
    let wlo = min(pw0.y, pw1.y);
    let whi = max(pw0.y, pw1.y);
    if ((whi - wlo) >= minH) {
      let blo = min(by0, by1);
      let bhi = max(by0, by1);
      let shift = max(0.0, wlo - blo) - max(0.0, bhi - whi);
      by0 = by0 + shift;
      by1 = by1 + shift;
    }
  }
  return vec2<f32>(by0, by1);
}
)WGSL";

}  // namespace dc
