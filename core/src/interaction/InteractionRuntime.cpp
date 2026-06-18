// ENC-655 (B5b) — InteractionRuntime implementation. See header.
#include "dc/interaction/InteractionRuntime.hpp"

#include "dc/data/TableStore.hpp"

namespace dc {

std::size_t InteractionRuntime::addMark(std::string id, NodeId node, Mark mark,
                                        Encoding enc, Id geometryId, Id drawItemId,
                                        Id vertexBufferId, Id tableId,
                                        Id firstBufferId, LineStyle lineStyle) {
  auto spec = std::make_unique<MarkSpec>();
  spec->id = std::move(id);
  spec->node = node;
  spec->mark = mark;
  spec->enc = std::move(enc);
  spec->geometryId = geometryId;
  spec->drawItemId = drawItemId;
  spec->vertexBufferId = vertexBufferId;
  spec->tableId = tableId;
  spec->firstBufferId = firstBufferId;
  spec->lineStyle = lineStyle;
  marks_.push_back(std::move(spec));
  return marks_.size() - 1;
}

const std::vector<RuntimeMark>& InteractionRuntime::refresh() {
  // 1) Recompute the dirty closure (data + signal driven, ENC-624).
  dag_.evaluate();

  // 2) Materialize + re-encode each mark from its current transform output.
  compiled_.clear();
  compiled_.reserve(marks_.size());
  for (auto& specPtr : marks_) {
    MarkSpec& s = *specPtr;
    RuntimeMark out;
    out.id = s.id;

    const ColumnSchema* schema = dag_.schemaOf(s.node);
    if (!schema) {
      // Unknown node: leave an explicitly-not-ok result so the caller can tell.
      compiled_.push_back(std::move(out));
      continue;
    }
    materializeNodeToTable(*schema, dag_.columns(), s.node, s.ingest, s.table,
                           s.tableId, s.firstBufferId, s.id);
    BufferByteSource src = makeBufferByteSource(s.ingest);
    out.result = encode_.compile(s.mark, s.enc, s.table, s.tableId, src,
                                 s.geometryId, s.drawItemId, s.vertexBufferId,
                                 /*rowIds=*/nullptr, s.lineStyle);
    compiled_.push_back(std::move(out));
  }
  return compiled_;
}

const RuntimeMark* InteractionRuntime::compiledMark(const std::string& id) const {
  for (const auto& m : compiled_)
    if (m.id == id) return &m;
  return nullptr;
}

}  // namespace dc
