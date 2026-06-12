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

  // Field channel: read the f32 column (Phase 1 numeric channels are f32; a
  // timestamp column has no f32 view by design and would fail here — time scales
  // arrive in ENC-597). Identity if no scale is attached, else scale.map().
  ColumnView<float> col = tables.viewF32(tableId, b->field, src);
  if (!col.valid() || rowIndex >= col.count) return std::nullopt;

  const double raw = static_cast<double>(col[rowIndex]);
  return b->scale ? b->scale->map(raw) : raw;
}

std::vector<std::string> Encoding::referencedFields() const {
  std::vector<std::string> out;
  for (const auto& kv : bindings_) {
    if (kv.second.hasField()) out.push_back(kv.second.field);
  }
  return out;
}

}  // namespace dc
