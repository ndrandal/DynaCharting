// ENC-592 (P1.1) — TableStore + Column model. See TableStore.hpp for the design.
#include "dc/data/TableStore.hpp"

#include <cstring>

namespace dc {

// ---------------------------------------------------------------------------
// DType parsing
// ---------------------------------------------------------------------------
std::optional<DType> parseDType(const std::string& s) {
  if (s == "f32") return DType::F32;
  if (s == "i32") return DType::I32;
  if (s == "cat") return DType::Cat;
  if (s == "timestamp") return DType::Timestamp;
  if (s == "list") return DType::List;
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// CatDictionary
// ---------------------------------------------------------------------------
std::uint32_t CatDictionary::intern(const std::string& label) {
  auto it = index_.find(label);
  if (it != index_.end()) return it->second;
  const auto code = static_cast<std::uint32_t>(labels_.size());
  labels_.push_back(label);
  index_.emplace(label, code);
  return code;
}

const std::string& CatDictionary::labelOf(std::uint32_t code) const {
  static const std::string kEmpty;
  if (code >= labels_.size()) return kEmpty;
  return labels_[code];
}

std::optional<std::uint32_t> CatDictionary::codeOf(
    const std::string& label) const {
  auto it = index_.find(label);
  if (it == index_.end()) return std::nullopt;
  return it->second;
}

// ---------------------------------------------------------------------------
// TableStore — internal lookups
// ---------------------------------------------------------------------------
TableStore::Table* TableStore::find(Id tableId) {
  auto it = tables_.find(tableId);
  return it == tables_.end() ? nullptr : &it->second;
}

const TableStore::Table* TableStore::find(Id tableId) const {
  auto it = tables_.find(tableId);
  return it == tables_.end() ? nullptr : &it->second;
}

const Column* TableStore::findColumn(const Table& t,
                                     const std::string& name) const {
  auto it = t.byName.find(name);
  if (it == t.byName.end()) return nullptr;
  return &t.columns[it->second];
}

// ---------------------------------------------------------------------------
// Table definition
// ---------------------------------------------------------------------------
bool TableStore::defineTable(Id tableId, std::string name) {
  if (tableId == kInvalidId) return false;
  if (tables_.find(tableId) != tables_.end()) return false;
  Table t;
  t.id = tableId;
  t.name = std::move(name);
  t.version = 1;  // first real state => version 1 (a cached 0 always looks stale)
  tables_.emplace(tableId, std::move(t));
  return true;
}

bool TableStore::addColumn(Id tableId, const std::string& name, DType dtype,
                           Id bufferId) {
  Table* t = find(tableId);
  if (!t) return false;
  if (name.empty()) return false;
  // List/ragged columns need two buffers — use addListColumn(). Scalar dtypes
  // only here (a 0-width dtype is List or an invalid enum).
  if (dtypeByteWidth(dtype) == 0) return false;
  if (t->byName.find(name) != t->byName.end()) return false;  // dup name

  Column c;
  c.name = name;
  c.dtype = dtype;
  c.bufferId = bufferId;

  t->byName.emplace(name, t->columns.size());
  t->columns.push_back(std::move(c));
  ++t->version;
  return true;
}

bool TableStore::addListColumn(Id tableId, const std::string& name,
                               DType innerDType, Id offsetsBufferId,
                               Id valuesBufferId) {
  Table* t = find(tableId);
  if (!t) return false;
  if (name.empty()) return false;
  // The inner dtype must be a fixed-width scalar (no list-of-list). dtypeByteWidth
  // is 0 for List itself and for any invalid enum value.
  if (dtypeByteWidth(innerDType) == 0) return false;
  if (t->byName.find(name) != t->byName.end()) return false;  // dup name

  Column c;
  c.name = name;
  c.dtype = DType::List;
  c.bufferId = offsetsBufferId;      // offsets (i32, length rows+1)
  c.valuesBufferId = valuesBufferId; // flat inner values
  c.innerDType = innerDType;

  t->byName.emplace(name, t->columns.size());
  t->columns.push_back(std::move(c));
  ++t->version;
  return true;
}

bool TableStore::setRowIdColumn(Id tableId, const std::string& columnName) {
  Table* t = find(tableId);
  if (!t) return false;
  if (t->byName.find(columnName) == t->byName.end()) return false;
  t->rowIdColumn = columnName;
  ++t->version;
  return true;
}

bool TableStore::removeTable(Id tableId) {
  return tables_.erase(tableId) > 0;
}

bool TableStore::hasTable(Id tableId) const {
  return tables_.find(tableId) != tables_.end();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
const Column* TableStore::column(Id tableId, const std::string& name) const {
  const Table* t = find(tableId);
  if (!t) return nullptr;
  return findColumn(*t, name);
}

const Column* TableStore::column(Id tableId, const std::string& name,
                                 DType expect) const {
  const Column* c = column(tableId, name);
  if (!c || c->dtype != expect) return nullptr;
  return c;
}

std::vector<std::string> TableStore::columnNames(Id tableId) const {
  std::vector<std::string> out;
  const Table* t = find(tableId);
  if (!t) return out;
  out.reserve(t->columns.size());
  for (const Column& c : t->columns) out.push_back(c.name);
  return out;
}

std::optional<std::string> TableStore::rowIdColumn(Id tableId) const {
  const Table* t = find(tableId);
  if (!t) return std::nullopt;
  return t->rowIdColumn;
}

std::size_t TableStore::columnRowCount(Id tableId, const std::string& name,
                                       const BufferByteSource& src) const {
  const Column* c = column(tableId, name);
  if (!c) return 0;
  const std::size_t width = dtypeByteWidth(c->dtype);
  if (width == 0) return 0;
  const std::uint32_t bytes = src.size ? src.size(c->bufferId) : 0;
  return bytes / width;
}

std::size_t TableStore::rowCount(Id tableId,
                                 const BufferByteSource& src) const {
  const Table* t = find(tableId);
  if (!t || t->columns.empty()) return 0;

  // The columns append in lockstep, so in the steady state they agree. Between
  // ticks a batch may be partially applied (columns transiently ragged); we then
  // report the MINIMUM — the number of fully-populated rows across all columns.
  bool first = true;
  std::size_t minCount = 0;
  for (const Column& c : t->columns) {
    const std::size_t width = dtypeByteWidth(c.dtype);
    const std::uint32_t bytes = src.size ? src.size(c.bufferId) : 0;
    const std::size_t rows = width ? bytes / width : 0;
    if (first) {
      minCount = rows;
      first = false;
    } else if (rows < minCount) {
      minCount = rows;
    }
  }
  return minCount;
}

bool TableStore::rowCountConsistent(Id tableId,
                                    const BufferByteSource& src) const {
  const Table* t = find(tableId);
  if (!t || t->columns.size() < 2) return true;

  bool first = true;
  std::size_t expect = 0;
  for (const Column& c : t->columns) {
    const std::size_t width = dtypeByteWidth(c.dtype);
    const std::uint32_t bytes = src.size ? src.size(c.bufferId) : 0;
    const std::size_t rows = width ? bytes / width : 0;
    if (first) {
      expect = rows;
      first = false;
    } else if (rows != expect) {
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Typed reinterpret
// ---------------------------------------------------------------------------
std::pair<const std::uint8_t*, std::size_t> TableStore::rawColumn(
    Id tableId, const std::string& name, std::size_t expectWidth,
    const BufferByteSource& src) const {
  const Column* c = column(tableId, name);
  if (!c) return {nullptr, 0};
  if (dtypeByteWidth(c->dtype) != expectWidth) return {nullptr, 0};
  const std::uint8_t* bytes = src.data ? src.data(c->bufferId) : nullptr;
  const std::uint32_t size = src.size ? src.size(c->bufferId) : 0;
  if (!bytes || size == 0) return {nullptr, 0};
  // Only expose whole elements; a partial trailing element (mid-write) is hidden.
  return {bytes, size / expectWidth};
}

ColumnView<float> TableStore::viewF32(Id tableId, const std::string& name,
                                      const BufferByteSource& src) const {
  // Guard the dtype explicitly: only an F32 column may be viewed as float. A
  // Timestamp column has width 8 (already excluded), and Cat/I32 share width 4
  // but must NOT alias to float — check the dtype.
  const Column* c = column(tableId, name, DType::F32);
  if (!c) return {};
  auto [bytes, count] = rawColumn(tableId, name, sizeof(float), src);
  if (!bytes) return {};
  return {reinterpret_cast<const float*>(bytes), count};
}

ColumnView<std::int32_t> TableStore::viewI32(Id tableId, const std::string& name,
                                             const BufferByteSource& src) const {
  const Column* c = column(tableId, name, DType::I32);
  if (!c) return {};
  auto [bytes, count] = rawColumn(tableId, name, sizeof(std::int32_t), src);
  if (!bytes) return {};
  return {reinterpret_cast<const std::int32_t*>(bytes), count};
}

ColumnView<std::uint32_t> TableStore::viewCat(Id tableId,
                                              const std::string& name,
                                              const BufferByteSource& src) const {
  const Column* c = column(tableId, name, DType::Cat);
  if (!c) return {};
  auto [bytes, count] = rawColumn(tableId, name, sizeof(std::uint32_t), src);
  if (!bytes) return {};
  return {reinterpret_cast<const std::uint32_t*>(bytes), count};
}

ColumnView<std::int64_t> TableStore::viewTimestamp(
    Id tableId, const std::string& name, const BufferByteSource& src) const {
  const Column* c = column(tableId, name, DType::Timestamp);
  if (!c) return {};
  auto [bytes, count] = rawColumn(tableId, name, sizeof(std::int64_t), src);
  if (!bytes) return {};
  return {reinterpret_cast<const std::int64_t*>(bytes), count};
}

RaggedColumn<float> TableStore::viewRaggedF32(Id tableId, const std::string& name,
                                              const BufferByteSource& src) const {
  return rawRagged<float>(tableId, name, DType::F32, src);
}

RaggedColumn<std::int32_t> TableStore::viewRaggedI32(
    Id tableId, const std::string& name, const BufferByteSource& src) const {
  return rawRagged<std::int32_t>(tableId, name, DType::I32, src);
}

CatDictionary* TableStore::catDict(Id tableId, const std::string& name) {
  Table* t = find(tableId);
  if (!t) return nullptr;
  auto it = t->byName.find(name);
  if (it == t->byName.end()) return nullptr;
  Column& c = t->columns[it->second];
  if (c.dtype != DType::Cat) return nullptr;
  return &c.dict;
}

const CatDictionary* TableStore::catDict(Id tableId,
                                         const std::string& name) const {
  const Column* c = column(tableId, name, DType::Cat);
  if (!c) return nullptr;
  return &c->dict;
}

std::uint64_t TableStore::version(Id tableId) const {
  const Table* t = find(tableId);
  return t ? t->version : 0;
}

}  // namespace dc
