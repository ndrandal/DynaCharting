// ENC-605 (P1.14) — Manifest v0 parser (data / scales / coords / marks /
// encoding). RESEARCH §6.1 (the eight-section schema + the anti-nonsense typing
// contract) and §6.2 (worked manifest A: candlestick + SMA over raw OHLC).
//
// WHAT THIS IS
// ------------
// The control-plane glue that ties the whole data→visual layer together: it
// parses a JSON manifest and DRIVES the existing primitives end to end —
//
//   data    -> TableStore tables + typed columns (the §6.2 `columns` block, the
//              PivotIngest column declarations: from / dtype / role)
//   scales  -> the Scale family (LinearScale / TimeScale / BandScale / PointScale),
//              auto-domained from a declared `domainFrom` column or seeded from a
//              literal `domain`
//   coords  -> cartesian (the only v0 coordinate system)
//   marks   -> the four Phase-1 marks (point/line/rect/candle) bound to a pipeline,
//              each with an `encoding` mapping channel -> scale(field) | value
//
// and then runs the ENCODE PASS (ENC-601) per mark to produce the rendered scene:
// the DrawItems + Geometry whose vertex/instance bytes are byte-exact to each
// pipeline's required format. The byte plane (column data) is filled OUT-OF-BAND
// by the existing 13-byte ingest feed; the manifest owns only the schema + the
// per-frame compile. `build(src)` is the per-frame step: it re-folds every scale's
// auto-domain over the live columns (O(Δ)) and re-compiles every mark.
//
// THE §6.1 ANTI-NONSENSE TYPING CONTRACT (checked at load / build, FAIL FAST)
// --------------------------------------------------------------------------
//   1. Reference resolution — every mark `from` resolves to a declared table,
//      every channel `scale` resolves to a declared scale, every `field`/
//      `domainFrom.field` resolves to a declared column. A dangling ref is a hard
//      load error.
//   2. Column-set / dtype — a scale's domain source column must carry a dtype the
//      scale type accepts (linear/log <- f32/i32; time <- timestamp; band/point
//      <- cat). A field bound through a scale must exist with a compatible dtype.
//   3. Channel <-> scale <-> dtype <-> pipeline matrix — the resolved channel set
//      must cover the mark's pipeline's REQUIRED vertex/instance fields. This is
//      enforced at build() by the EncodePass / validateDrawItem gate (a mark
//      missing a required channel is rejected with a clear EncodeError); the
//      parser pre-checks it structurally so the error is localized to the channel.
//
// SCOPE (ENC-605 only)
// --------------------
// ONLY data/scales/coords/marks/encoding parsing driving the EXISTING encode
// compiler. NO general transforms (filter/bin/aggregate/window — Phase 3; an SMA
// column referenced by a manifest is treated as a PRE-EXISTING column, supplied by
// a trivial CPU helper in the ENC-606 proof, never computed here), NO color
// scales / polar (Phase 2), NO facets / interaction / multi-layer, NO pixel render
// proof (that is ENC-606). A single implicit layer holds every mark.
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ids/Id.hpp"
#include "dc/scale/Scale.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// ManifestStatus — the outcome of a parse / build. Ok carries an empty message;
// every failure mode carries a clear, localized message (which ref dangled, which
// dtype a scale rejected, which channel the pipeline was missing).
// ---------------------------------------------------------------------------
enum class ManifestStatus : std::uint8_t {
  Ok,
  BadJson,             // not a JSON object / parse error
  MissingSection,      // a required top-level section is absent / wrong shape
  UnknownCoords,       // coords.type is not "cartesian" (v0 only supports it)
  UnknownDType,        // a column dtype string is not f32/i32/cat/timestamp
  UnknownScaleType,    // a scale type string is not linear/time/band/point
  UnknownMarkType,     // a mark type string is not point/line/rect/candle
  UnknownPipeline,     // a mark pipeline key is not in the catalog
  DuplicateId,         // two tables / scales / marks share an id string
  DanglingRef,         // a from/scale/field/domainFrom ref does not resolve (§6.1 #1)
  DTypeMismatch,       // a scale's domain dtype is rejected by the scale type (§6.1 #2)
  EncodeRejected,      // the encode pass rejected a mark (channel-set / format gate, §6.1 #3)
};

const char* toString(ManifestStatus s);

struct ManifestResult {
  ManifestStatus status{ManifestStatus::Ok};
  std::string message;  // a clear, localized human-readable explanation
  bool ok() const { return status == ManifestStatus::Ok; }
};

// ---------------------------------------------------------------------------
// CompiledMark — one mark's compile output: the EncodeResult (DrawItem +
// Geometry + the packed bytes) plus the mark's manifest id and the pipeline it
// targets. The caller wires `result.geometry` + `result.drawItem` into a Scene.
// ---------------------------------------------------------------------------
struct CompiledMark {
  std::string id;        // the manifest mark id (e.g. "candles")
  Mark mark{Mark::Point};
  LineStyle lineStyle{LineStyle::Line2d};
  std::string pipeline;  // resolved pipeline key (e.g. "instancedCandle@1")
  EncodeResult result;   // the encode pass output (geometry + drawItem + bytes)
};

// ---------------------------------------------------------------------------
// Manifest — the parsed + driven manifest. load() parses + validates the schema
// (§6.1 checks 1+2, plus a structural pre-check of 3); build(src) re-folds every
// scale's auto-domain over the live byte source and runs the encode pass per mark
// (§6.1 check 3, the validateDrawItem gate). The two are split so a streaming host
// loads once and builds each frame.
//
// The Manifest OWNS its TableStore + scales (control plane). The column BYTES live
// in the caller's ingest/buffer store, read through a BufferByteSource — exactly
// the seam every primitive already uses, so the demo (ENC-606) feeds OHLC through
// the unchanged ingest feed and the manifest sees the growth.
// ---------------------------------------------------------------------------
class Manifest {
 public:
  Manifest() = default;

  // Parse + validate `jsonText`. Builds the TableStore tables/columns, the scales
  // (with their auto-domain bindings + ranges), and the marks/encodings. Applies
  // §6.1 checks 1 (reference resolution) and 2 (dtype compatibility) plus a
  // structural pre-check of 3 (channel set covers the pipeline). Returns a
  // ManifestResult; on failure the manifest is left in a partially-built state and
  // must NOT be built.
  ManifestResult load(const std::string& jsonText);

  // Re-fold every auto-domained scale over the live columns (O(Δ)) and run the
  // encode pass for every mark, producing the rendered scene (compiledMarks()).
  // Applies §6.1 check 3 (the EncodePass validateDrawItem gate): a mark whose
  // resolved channel set does not cover its pipeline's required format is rejected
  // with EncodeRejected + a clear message. Ids for geometry/draw-item/vertex-buffer
  // are allocated densely from `firstId`.
  ManifestResult build(const BufferByteSource& src, Id firstId = 100000);

  // ----- accessors (post-load / post-build) ---------------------------------

  const std::string& id() const { return manifestId_; }

  // The schema (tables + columns) the data section declared. The caller drives the
  // ingest feed against these column buffer ids.
  const TableStore& tables() const { return tables_; }
  TableStore& tables() { return tables_; }

  // The logical table id for a manifest data-source id (e.g. "ohlc" -> Id). 0/none
  // if unknown.
  std::optional<Id> tableId(const std::string& sourceId) const;

  // The column buffer id a manifest (sourceId, column) declares — what the caller
  // appends bytes to via the ingest feed.
  std::optional<Id> columnBufferId(const std::string& sourceId,
                                   const std::string& column) const;

  // The scale named `scaleId`, or nullptr.
  const Scale* scale(const std::string& scaleId) const;
  Scale* scale(const std::string& scaleId);

  // The marks compiled by the last build() — the rendered scene (DrawItems +
  // Geometry). Empty before build().
  const std::vector<CompiledMark>& compiledMarks() const { return compiled_; }

  // Look up one compiled mark by its manifest id (or nullptr).
  const CompiledMark* compiledMark(const std::string& markId) const;

 private:
  // ----- parsed schema (control plane) --------------------------------------

  struct ColumnDecl {
    std::string name;
    DType dtype{DType::F32};
    Id bufferId{kInvalidId};
  };

  struct SourceDecl {
    std::string id;
    Id tableId{kInvalidId};
    std::string rowKeyColumn;                 // the `rowKey` column name (if any)
    std::vector<ColumnDecl> columns;
    std::unordered_map<std::string, std::size_t> columnByName;
  };

  struct ScaleDecl {
    std::string id;
    ScaleType type{ScaleType::Linear};
    std::unique_ptr<Scale> scale;
    // auto-domain binding (empty domainTable => a literal domain was supplied).
    Id domainTable{kInvalidId};
    std::string domainColumn;
    bool autodomain{false};
    bool nice{false};
    int niceTarget{5};
  };

  struct ChannelDecl {
    Channel channel{Channel::X};
    bool isConstant{false};
    double value{0.0};        // constant value (isConstant)
    std::string field;        // column name (field binding)
    std::string scaleId;      // scale ref (empty => identity)
  };

  struct MarkDecl {
    std::string id;
    Mark mark{Mark::Point};
    LineStyle lineStyle{LineStyle::Line2d};
    std::string pipeline;     // resolved + validated pipeline key
    std::string from;         // data-source id the mark draws from
    Id tableId{kInvalidId};
    std::vector<ChannelDecl> channels;
    std::optional<Rgba> color;
    std::optional<Rgba> colorUp;
    std::optional<Rgba> colorDown;
  };

  // helpers (return a localized failure or Ok)
  ManifestResult parseData(const void* dataValue);
  ManifestResult parseScales(const void* scalesValue);
  ManifestResult parseCoords(const void* coordsValue);
  ManifestResult parseMarks(const void* marksValue);

  // Resolve a manifest channel key ("x","yOpen","width",...) -> Channel. Returns
  // false on an unknown channel key.
  static bool channelOf(const std::string& key, Channel& out);

  ManifestResult fail(ManifestStatus s, std::string msg) const {
    return ManifestResult{s, std::move(msg)};
  }

  std::string manifestId_;
  TableStore tables_;
  Id nextTableId_{1};

  std::vector<SourceDecl> sources_;
  std::unordered_map<std::string, std::size_t> sourceById_;

  std::vector<ScaleDecl> scales_;
  std::unordered_map<std::string, std::size_t> scaleById_;

  std::vector<MarkDecl> marks_;
  std::unordered_map<std::string, std::size_t> markById_;

  EncodePass encodePass_;
  std::vector<CompiledMark> compiled_;
};

}  // namespace dc
