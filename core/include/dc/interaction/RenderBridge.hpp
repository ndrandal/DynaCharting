// ENC-654 (B5a) — RenderBridge: get TransformDag output onto the screen.
//
// THE GAP THIS CLOSES
// -------------------
// The encode pass (Encoding::resolve, ENC-601) reads a TableStore + BufferByteSource,
// but the transform DAG (ENC-616) — and the interaction layer's selectionFilter
// (ENC-626) / conditional-color (ENC-625) — write their results into a ColumnStore
// (read API viewF32(node,field)). There was no path from a transform/selection
// RESULT into the rendered scene, so selection state was computed but never drawn.
//
// materializeNodeToTable() bridges it: it copies a TransformDag node's output
// columns into an IngestProcessor (one buffer per column) and defines a TableStore
// over them, so the EXISTING encode pass renders transform output unchanged
// (compile(mark, enc, tables, tableId, makeBufferByteSource(ingest), ...)). Pure
// `dc`, no GPU. setBufferData overwrites in place, so it is safe to call every
// refresh with the same ids (the B5b reactive runtime does exactly that).
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/transform/ColumnStore.hpp"
#include "dc/transform/Transform.hpp"  // ColumnSchema

#include <string>

namespace dc {

// Materialize node `node`'s output (columns named/typed by `schema`, values read
// from `cols`) into `ingest` (one buffer per column, ids starting at
// `firstBufferId`) and (re)define table `tableId` over them. Encode then reads it
// via makeBufferByteSource(ingest). Returns the row count materialized. List
// (ragged) columns are skipped (not a scalar render source). Overwrites existing
// buffers, so repeated calls with the same ids refresh in place.
std::size_t materializeNodeToTable(const ColumnSchema& schema,
                                   const ColumnStore& cols, Id node,
                                   IngestProcessor& ingest, TableStore& tables,
                                   Id tableId, Id firstBufferId,
                                   const std::string& tableName = "xform");

}  // namespace dc
