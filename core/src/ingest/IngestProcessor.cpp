#include "dc/ingest/IngestProcessor.hpp"
#include <algorithm>
#include <cstring>

namespace dc {

static std::uint32_t readU32LE(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0])
       | (static_cast<std::uint32_t>(p[1]) << 8)
       | (static_cast<std::uint32_t>(p[2]) << 16)
       | (static_cast<std::uint32_t>(p[3]) << 24);
}

IngestResult IngestProcessor::processBatch(const std::uint8_t* data, std::uint32_t len) {
  IngestResult result;
  const std::uint8_t* pos = data;
  const std::uint8_t* end = data + len;

  while (pos + HEADER_SIZE <= end) {
    std::uint8_t op = pos[0];
    Id bufferId = static_cast<Id>(readU32LE(pos + 1));
    std::uint32_t offset = readU32LE(pos + 5);
    std::uint32_t payloadLen = readU32LE(pos + 9);
    pos += HEADER_SIZE;

    if (pos + payloadLen > end) {
      result.droppedBytes += static_cast<std::uint32_t>(end - pos);
      break;
    }

    ensureBuffer(bufferId);
    CpuBuffer& buf = buffers_[bufferId];

    if (op == OP_APPEND) {
      buf.data.insert(buf.data.end(), pos, pos + payloadLen);
      enforceCap(buf);
    } else if (op == OP_UPDATE_RANGE) {
      std::uint32_t needed = offset + payloadLen;
      if (needed > static_cast<std::uint32_t>(buf.data.size())) {
        buf.data.resize(needed);
      }
      std::memcpy(buf.data.data() + offset, pos, payloadLen);
    }

    result.payloadBytes += payloadLen;

    // Track touched buffers (avoid duplicates)
    bool found = false;
    for (Id id : result.touchedBufferIds) {
      if (id == bufferId) { found = true; break; }
    }
    if (!found) result.touchedBufferIds.push_back(bufferId);

    pos += payloadLen;
  }

  return result;
}

const std::uint8_t* IngestProcessor::getBufferData(Id id) const {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) return nullptr;
  return it->second.data.data();
}

std::uint32_t IngestProcessor::getBufferSize(Id id) const {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) return 0;
  return static_cast<std::uint32_t>(it->second.data.size());
}

void IngestProcessor::ensureBuffer(Id id) {
  if (buffers_.find(id) == buffers_.end()) {
    CpuBuffer b;
    b.id = id;
    buffers_[id] = std::move(b);
  }
}

void IngestProcessor::syncBufferLengths(Scene& scene) const {
  for (auto& [id, buf] : buffers_) {
    Buffer* b = scene.getBufferMutable(id);
    if (b) {
      b->byteLength = static_cast<std::uint32_t>(buf.data.size());
    }
  }
}

// ---- Cache / Eviction (D2.5) ----

void IngestProcessor::setMaxBytes(Id id, std::uint32_t maxBytes) {
  ensureBuffer(id);
  buffers_[id].maxBytes = maxBytes;
  enforceCap(buffers_[id]);
}

std::uint32_t IngestProcessor::getMaxBytes(Id id) const {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) return DEFAULT_MAX_BYTES;
  return it->second.maxBytes;
}

void IngestProcessor::evictFront(Id id, std::uint32_t bytes) {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) return;
  auto& d = it->second.data;
  std::uint32_t n = std::min(bytes, static_cast<std::uint32_t>(d.size()));
  d.erase(d.begin(), d.begin() + n);
}

void IngestProcessor::keepLast(Id id, std::uint32_t bytes) {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) return;
  auto& d = it->second.data;
  if (bytes >= d.size()) return;
  d.erase(d.begin(), d.end() - bytes);
}

void IngestProcessor::enforceCap(CpuBuffer& buf) {
  if (buf.data.size() > buf.maxBytes) {
    std::uint32_t excess = static_cast<std::uint32_t>(buf.data.size()) - buf.maxBytes;
    buf.data.erase(buf.data.begin(), buf.data.begin() + excess);
  }
}

} // namespace dc
