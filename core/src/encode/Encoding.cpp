// ENC-600 (P1.9) — Encoding/channel-binding model implementation. See header.
#include "dc/encode/Encoding.hpp"

namespace dc {

const char* toString(Channel c) {
  switch (c) {
    case Channel::X: return "x";
    case Channel::Y: return "y";
    case Channel::X2: return "x2";
    case Channel::Y2: return "y2";
    case Channel::Size: return "size";
    case Channel::Open: return "open";
    case Channel::High: return "high";
    case Channel::Low: return "low";
    case Channel::Close: return "close";
    case Channel::Color: return "color";
  }
  return "unknown";
}

Encoding& Encoding::set(Channel ch, ChannelBinding binding) {
  bindings_[ch] = std::move(binding);
  return *this;
}

const ChannelBinding* Encoding::binding(Channel ch) const {
  auto it = bindings_.find(ch);
  return it == bindings_.end() ? nullptr : &it->second;
}

std::optional<double> Encoding::resolve(Channel ch, std::size_t rowIndex,
                                        const TableStore& tables, Id tableId,
                                        const BufferByteSource& src) const {
  const ChannelBinding* b = binding(ch);
  if (!b) return std::nullopt;

  // Constant channel: same value for every row, no column read.
  if (b->isConstant()) return b->value;

  // ENC-606 (Part A) — TIME position channel: a `timestamp` (i64 epoch-ms) column
  // bound to a TimeScale. Reading it via the f32 column view is impossible by
  // design (epoch-ms ~1.7e12 overflows the f32 mantissa ~16.7M — the named
  // correctness trap, TableStore.hpp / Scale.hpp), so viewF32 returns an empty
  // view and the channel would be (wrongly) rejected at build. Instead read the
  // i64 column LOSSLESSLY into f64 epoch-ms, normalize it against the scale's
  // CPU-held base epoch to a small RELATIVE f32 offset (the only f32 surface, per
  // TimeScale::normalizedOffsetF32), and map that offset through the scale. The
  // scale's map() is the exact f64 affine in epoch-ms space; feeding it the
  // base-relative offset (range position depends only on the difference) yields
  // the identical range value with NO epoch-ms→f32 narrowing anywhere. This is the
  // minimal, additive wiring of the timestamp→encode path (the ENC-605 gap).
  if (b->scale && b->scale->type() == ScaleType::Time) {
    const auto* ts = static_cast<const TimeScale*>(b->scale);
    ColumnView<std::int64_t> tcol = tables.viewTimestamp(tableId, b->field, src);
    if (!tcol.valid() || rowIndex >= tcol.count) return std::nullopt;
    const double epochMs = static_cast<double>(tcol[rowIndex]);  // i64 -> f64 lossless
    const float offsetF32 = ts->normalizedOffsetF32(epochMs);    // small relative f32
    // map(base + offset) == map(epochMs): map() subtracts the domain min (== base),
    // so adding the f64 base back in f64 before map() is exact and overflow-free.
    return ts->map(ts->baseEpochMs() + static_cast<double>(offsetF32));
  }

  // Field channel: read the f32 column (Phase 1 numeric channels are f32).
  // Identity if no scale is attached, else scale.map().
  ColumnView<float> col = tables.viewF32(tableId, b->field, src);
  if (!col.valid() || rowIndex >= col.count) return std::nullopt;

  const double raw = static_cast<double>(col[rowIndex]);
  return b->scale ? b->scale->map(raw) : raw;
}

namespace {
// Pack a 0..1 Rgba into a little-endian RGBA8 u32 (byte 0 = R .. byte 3 = A),
// matching the Unorm8x4 attribute layout the instancedRectColor@1 shader reads.
inline std::uint32_t packRgba8(const Rgba& c) {
  auto q = [](float v) -> std::uint32_t {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return static_cast<std::uint32_t>(v * 255.0f + 0.5f);
  };
  return q(c.r) | (q(c.g) << 8) | (q(c.b) << 16) | (q(c.a) << 24);
}
}  // namespace

std::optional<std::uint32_t> Encoding::resolveColorRgba8(
    std::size_t rowIndex, const TableStore& tables, Id tableId,
    const BufferByteSource& src) const {
  // Per-row packed RGBA8 from the bound i32/cat column (the pre-resolved color).
  if (colorField_) {
    ColumnView<std::int32_t> col = tables.viewI32(tableId, *colorField_, src);
    if (!col.valid() || rowIndex >= col.count) return std::nullopt;
    return static_cast<std::uint32_t>(col[rowIndex]);
  }
  // No per-row field: fall back to the constant color (white if unset) so the
  // per-instance buffer is still valid for an all-constant encoding.
  return packRgba8(color_ ? *color_ : Rgba{});
}

std::vector<std::string> Encoding::referencedFields() const {
  std::vector<std::string> out;
  for (const auto& kv : bindings_) {
    if (kv.second.hasField()) out.push_back(kv.second.field);
  }
  if (colorField_) out.push_back(*colorField_);
  return out;
}

}  // namespace dc
