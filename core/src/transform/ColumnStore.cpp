// ENC-616a — Typed ColumnStore implementation. See ColumnStore.hpp for design.
#include "dc/transform/ColumnStore.hpp"

#include <cstring>

namespace dc {

ColumnStore::Stored* ColumnStore::find(Id node, const std::string& field) {
  auto it = cols_.find(ColumnRef{node, field});
  return it == cols_.end() ? nullptr : &it->second;
}

const ColumnStore::Stored* ColumnStore::find(Id node,
                                             const std::string& field) const {
  auto it = cols_.find(ColumnRef{node, field});
  return it == cols_.end() ? nullptr : &it->second;
}

bool ColumnStore::allocColumn(Id node, const std::string& field, DType dt,
                              std::size_t rows) {
  const std::size_t w = dtypeByteWidth(dt);
  if (w == 0) return false;
  Stored& s = cols_[ColumnRef{node, field}];
  s.dtype = dt;
  s.bytes.assign(rows * w, 0);
  return true;
}

void ColumnStore::dropColumn(Id node, const std::string& field) {
  cols_.erase(ColumnRef{node, field});
}

void ColumnStore::dropNode(Id node) {
  for (auto it = cols_.begin(); it != cols_.end();) {
    if (it->first.node == node) {
      it = cols_.erase(it);
    } else {
      ++it;
    }
  }
}

bool ColumnStore::hasColumn(Id node, const std::string& field) const {
  return find(node, field) != nullptr;
}

DType ColumnStore::dtypeOf(Id node, const std::string& field) const {
  const Stored* s = find(node, field);
  return s ? s->dtype : DType::F32;
}

std::size_t ColumnStore::rowCount(Id node, const std::string& field) const {
  const Stored* s = find(node, field);
  if (!s) return 0;
  const std::size_t w = dtypeByteWidth(s->dtype);
  return w == 0 ? 0 : s->bytes.size() / w;
}

// ---------------------------------------------------------------------------
// typed views — element type must match dtype (the same guard TableStore uses).
// ---------------------------------------------------------------------------
template <typename T>
static ColumnView<T> viewAs(const ColumnStore::Stored* s, DType expect) {
  if (!s || s->dtype != expect || s->bytes.empty()) return {};
  ColumnView<T> v;
  v.data = reinterpret_cast<const T*>(s->bytes.data());
  v.count = s->bytes.size() / sizeof(T);
  return v;
}

ColumnView<float> ColumnStore::viewF32(Id node, const std::string& field) const {
  return viewAs<float>(find(node, field), DType::F32);
}
ColumnView<std::int32_t> ColumnStore::viewI32(Id node,
                                              const std::string& field) const {
  return viewAs<std::int32_t>(find(node, field), DType::I32);
}
ColumnView<std::uint32_t> ColumnStore::viewCat(Id node,
                                               const std::string& field) const {
  return viewAs<std::uint32_t>(find(node, field), DType::Cat);
}
ColumnView<std::int64_t> ColumnStore::viewTimestamp(
    Id node, const std::string& field) const {
  return viewAs<std::int64_t>(find(node, field), DType::Timestamp);
}

// ---------------------------------------------------------------------------
// typed writes — dtype must match; bounds enforced (no-op past the end).
// ---------------------------------------------------------------------------
template <typename T>
static void writeAs(ColumnStore::Stored* s, DType expect, std::size_t i, T v) {
  if (!s || s->dtype != expect) return;
  const std::size_t off = i * sizeof(T);
  if (off + sizeof(T) > s->bytes.size()) return;
  std::memcpy(s->bytes.data() + off, &v, sizeof(T));
}

void ColumnStore::setF32(Id node, const std::string& field, std::size_t i,
                         float v) {
  writeAs<float>(find(node, field), DType::F32, i, v);
}
void ColumnStore::setI32(Id node, const std::string& field, std::size_t i,
                         std::int32_t v) {
  writeAs<std::int32_t>(find(node, field), DType::I32, i, v);
}
void ColumnStore::setCat(Id node, const std::string& field, std::size_t i,
                         std::uint32_t v) {
  writeAs<std::uint32_t>(find(node, field), DType::Cat, i, v);
}
void ColumnStore::setTimestamp(Id node, const std::string& field, std::size_t i,
                               std::int64_t v) {
  writeAs<std::int64_t>(find(node, field), DType::Timestamp, i, v);
}

}  // namespace dc
