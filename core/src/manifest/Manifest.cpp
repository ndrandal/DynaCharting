// ENC-605 (P1.14) — Manifest v0 parser implementation. See Manifest.hpp.
//
// RapidJSON is the JSON lib already used by CommandProcessor; this mirrors its
// parsing style (getMember / typed accessors, fail-fast on the first malformed
// node with a localized message).
#include "dc/manifest/Manifest.hpp"

#include <rapidjson/document.h>

#include <cmath>
#include <cstring>

namespace dc {

const char* toString(ManifestStatus s) {
  switch (s) {
    case ManifestStatus::Ok: return "Ok";
    case ManifestStatus::BadJson: return "BadJson";
    case ManifestStatus::MissingSection: return "MissingSection";
    case ManifestStatus::UnknownCoords: return "UnknownCoords";
    case ManifestStatus::UnknownDType: return "UnknownDType";
    case ManifestStatus::UnknownScaleType: return "UnknownScaleType";
    case ManifestStatus::UnknownMarkType: return "UnknownMarkType";
    case ManifestStatus::UnknownPipeline: return "UnknownPipeline";
    case ManifestStatus::DuplicateId: return "DuplicateId";
    case ManifestStatus::DanglingRef: return "DanglingRef";
    case ManifestStatus::DTypeMismatch: return "DTypeMismatch";
    case ManifestStatus::EncodeRejected: return "EncodeRejected";
  }
  return "Unknown";
}

namespace {

const rapidjson::Value* member(const rapidjson::Value& obj, const char* key) {
  if (!obj.IsObject()) return nullptr;
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd()) return nullptr;
  return &it->value;
}

std::string strOr(const rapidjson::Value& obj, const char* key,
                  const std::string& dflt = {}) {
  const auto* v = member(obj, key);
  if (v && v->IsString()) return v->GetString();
  return dflt;
}

// A scale type accepts a column dtype iff the §6.1 #2 compatibility holds:
//   linear/log  <- numeric (f32 / i32)
//   time        <- timestamp
//   band/point  <- category (cat)
bool scaleAcceptsDType(ScaleType st, DType dt) {
  switch (st) {
    case ScaleType::Linear:
    case ScaleType::Log:
      return dt == DType::F32 || dt == DType::I32;
    case ScaleType::Time:
      return dt == DType::Timestamp;
    case ScaleType::Band:
    case ScaleType::Point:
      return dt == DType::Cat;
    case ScaleType::Color:
      return false;  // Phase 2
  }
  return false;
}

const char* scaleTypeName(ScaleType st) {
  switch (st) {
    case ScaleType::Linear: return "linear";
    case ScaleType::Time: return "time";
    case ScaleType::Band: return "band";
    case ScaleType::Point: return "point";
    case ScaleType::Log: return "log";
    case ScaleType::Color: return "color";
  }
  return "unknown";
}

std::optional<ScaleType> parseScaleType(const std::string& s) {
  if (s == "linear") return ScaleType::Linear;
  if (s == "time") return ScaleType::Time;
  if (s == "band") return ScaleType::Band;
  if (s == "point") return ScaleType::Point;
  return std::nullopt;
}

// Parse a "#rrggbb" (or "#rrggbbaa") hex color into 0..1 RGBA. Returns false on a
// malformed string (leaves `out` untouched).
bool parseHexColor(const std::string& s, Rgba& out) {
  if (s.size() != 7 && s.size() != 9) return false;
  if (s[0] != '#') return false;
  auto hx = [](char c, int& v) -> bool {
    if (c >= '0' && c <= '9') { v = c - '0'; return true; }
    if (c >= 'a' && c <= 'f') { v = c - 'a' + 10; return true; }
    if (c >= 'A' && c <= 'F') { v = c - 'A' + 10; return true; }
    return false;
  };
  auto byte = [&](std::size_t i, float& dst) -> bool {
    int hi = 0, lo = 0;
    if (!hx(s[i], hi) || !hx(s[i + 1], lo)) return false;
    dst = static_cast<float>(hi * 16 + lo) / 255.0f;
    return true;
  };
  Rgba c;
  c.a = 1.0f;
  if (!byte(1, c.r) || !byte(3, c.g) || !byte(5, c.b)) return false;
  if (s.size() == 9 && !byte(7, c.a)) return false;
  out = c;
  return true;
}

}  // namespace

bool Manifest::channelOf(const std::string& key, Channel& out) {
  // The §6.2 channel vocabulary, mapped onto the Phase-1 Channel enum. The candle
  // uses yOpen/yClose/yHigh/yLow for OHLC; `width`/`size` are the candle/bar half
  // extent (Channel::Size). Plain open/high/low/close are accepted as aliases.
  if (key == "x") { out = Channel::X; return true; }
  if (key == "y") { out = Channel::Y; return true; }
  if (key == "x2") { out = Channel::X2; return true; }
  if (key == "y2") { out = Channel::Y2; return true; }
  if (key == "yOpen" || key == "open") { out = Channel::Open; return true; }
  if (key == "yHigh" || key == "high") { out = Channel::High; return true; }
  if (key == "yLow" || key == "low") { out = Channel::Low; return true; }
  if (key == "yClose" || key == "close") { out = Channel::Close; return true; }
  if (key == "width" || key == "size") { out = Channel::Size; return true; }
  return false;
}

// ===========================================================================
// load() — parse + validate the schema (§6.1 #1 + #2, structural pre-check of #3)
// ===========================================================================
ManifestResult Manifest::load(const std::string& jsonText) {
  // Reset prior state (load is idempotent).
  *this = Manifest{};

  rapidjson::Document d;
  d.Parse(jsonText.c_str());
  if (d.HasParseError() || !d.IsObject()) {
    return fail(ManifestStatus::BadJson, "manifest: invalid JSON object");
  }

  manifestId_ = strOr(d, "id");

  // ----- data (required) -----
  const auto* data = member(d, "data");
  if (!data || !data->IsObject()) {
    return fail(ManifestStatus::MissingSection, "manifest: missing 'data' object");
  }
  if (auto r = parseData(data); !r.ok()) return r;

  // ----- coords (optional; defaults cartesian) -----
  if (const auto* coords = member(d, "coords")) {
    if (auto r = parseCoords(coords); !r.ok()) return r;
  }

  // ----- scales (required) -----
  const auto* scales = member(d, "scales");
  if (!scales || !scales->IsArray()) {
    return fail(ManifestStatus::MissingSection, "manifest: missing 'scales' array");
  }
  if (auto r = parseScales(scales); !r.ok()) return r;

  // ----- marks (required) -----
  const auto* marks = member(d, "marks");
  if (!marks || !marks->IsArray()) {
    return fail(ManifestStatus::MissingSection, "manifest: missing 'marks' array");
  }
  if (auto r = parseMarks(marks); !r.ok()) return r;

  return ManifestResult{};
}

// ----- data: sources -> tables + typed columns (the §6.2 `columns` block) -----
ManifestResult Manifest::parseData(const void* dataValue) {
  const auto& data = *static_cast<const rapidjson::Value*>(dataValue);
  const auto* sources = member(data, "sources");
  if (!sources || !sources->IsArray()) {
    return fail(ManifestStatus::MissingSection,
                "data: missing 'sources' array");
  }

  for (const auto& srcV : sources->GetArray()) {
    if (!srcV.IsObject()) {
      return fail(ManifestStatus::MissingSection, "data.sources[]: not an object");
    }
    SourceDecl decl;
    decl.id = strOr(srcV, "id");
    if (decl.id.empty()) {
      return fail(ManifestStatus::MissingSection,
                  "data.sources[]: missing string 'id'");
    }
    if (sourceById_.count(decl.id)) {
      return fail(ManifestStatus::DuplicateId,
                  "data.sources: duplicate id '" + decl.id + "'");
    }

    // The §6.2 `columns` block lives under stream.columns (a stream source) or
    // directly under `columns`. Accept either.
    const rapidjson::Value* columns = nullptr;
    if (const auto* stream = member(srcV, "stream")) {
      columns = member(*stream, "columns");
      decl.rowKeyColumn = strOr(*stream, "rowKey");
    }
    if (!columns) columns = member(srcV, "columns");
    if (!columns || !columns->IsObject()) {
      return fail(ManifestStatus::MissingSection,
                  "data.sources['" + decl.id + "']: missing 'columns' object");
    }

    decl.tableId = nextTableId_++;
    tables_.defineTable(decl.tableId, decl.id);

    // Each column: { from, dtype, role? }. The buffer id is allocated densely off
    // the table id so the caller (the ingest feed) appends to a known buffer.
    Id colBuf = static_cast<Id>(decl.tableId) * 1000 + 1;
    for (auto it = columns->MemberBegin(); it != columns->MemberEnd(); ++it) {
      ColumnDecl col;
      col.name = it->name.GetString();
      const auto& colObj = it->value;
      if (!colObj.IsObject()) {
        return fail(ManifestStatus::MissingSection,
                    "column '" + col.name + "': not an object");
      }
      const std::string dtStr = strOr(colObj, "dtype", "f32");
      auto dt = parseDType(dtStr);
      if (!dt) {
        return fail(ManifestStatus::UnknownDType,
                    "column '" + col.name + "': unknown dtype '" + dtStr + "'");
      }
      col.dtype = *dt;
      col.bufferId = colBuf++;
      tables_.addColumn(decl.tableId, col.name, col.dtype, col.bufferId);
      decl.columnByName[col.name] = decl.columns.size();
      decl.columns.push_back(col);
    }

    if (decl.columns.empty()) {
      return fail(ManifestStatus::MissingSection,
                  "data.sources['" + decl.id + "']: declares no columns");
    }
    if (!decl.rowKeyColumn.empty() &&
        !decl.columnByName.count(decl.rowKeyColumn)) {
      return fail(ManifestStatus::DanglingRef,
                  "data.sources['" + decl.id + "']: rowKey '" +
                      decl.rowKeyColumn + "' is not a declared column");
    }

    sourceById_[decl.id] = sources_.size();
    sources_.push_back(std::move(decl));
  }

  if (sources_.empty()) {
    return fail(ManifestStatus::MissingSection, "data.sources: empty");
  }
  return ManifestResult{};
}

// ----- coords: cartesian only (v0) -----
ManifestResult Manifest::parseCoords(const void* coordsValue) {
  const auto& coords = *static_cast<const rapidjson::Value*>(coordsValue);
  const std::string type = strOr(coords, "type", "cartesian");
  if (type != "cartesian") {
    return fail(ManifestStatus::UnknownCoords,
                "coords: v0 supports only 'cartesian', got '" + type + "'");
  }
  return ManifestResult{};
}

// ----- scales: type + domain + range + autodomain binding -----
ManifestResult Manifest::parseScales(const void* scalesValue) {
  const auto& scales = *static_cast<const rapidjson::Value*>(scalesValue);

  for (const auto& scV : scales.GetArray()) {
    if (!scV.IsObject()) {
      return fail(ManifestStatus::MissingSection, "scales[]: not an object");
    }
    ScaleDecl decl;
    decl.id = strOr(scV, "id");
    if (decl.id.empty()) {
      return fail(ManifestStatus::MissingSection, "scales[]: missing string 'id'");
    }
    if (scaleById_.count(decl.id)) {
      return fail(ManifestStatus::DuplicateId,
                  "scales: duplicate id '" + decl.id + "'");
    }

    const std::string typeStr = strOr(scV, "type");
    auto st = parseScaleType(typeStr);
    if (!st) {
      return fail(ManifestStatus::UnknownScaleType,
                  "scale '" + decl.id + "': unknown type '" + typeStr + "'");
    }
    decl.type = *st;

    // Instantiate the concrete scale + its range.
    switch (decl.type) {
      case ScaleType::Linear: decl.scale = std::make_unique<LinearScale>(); break;
      case ScaleType::Time:   decl.scale = std::make_unique<TimeScale>(); break;
      case ScaleType::Band:   decl.scale = std::make_unique<BandScale>(); break;
      case ScaleType::Point:  decl.scale = std::make_unique<PointScale>(); break;
      default:
        return fail(ManifestStatus::UnknownScaleType,
                    "scale '" + decl.id + "': type '" + typeStr +
                        "' not supported in v0");
    }

    // range: "width"/"height" => [0,1] (the cartesian normalized extent; screen-y
    // grows down, so "height" flips); or an explicit [r0,r1] pair.
    if (const auto* rangeV = member(scV, "range")) {
      if (rangeV->IsString()) {
        const std::string r = rangeV->GetString();
        if (r == "width") decl.scale->setRange(0.0, 1.0);
        else if (r == "height") decl.scale->setRange(1.0, 0.0);
        else decl.scale->setRange(0.0, 1.0);
      } else if (rangeV->IsArray() && rangeV->Size() == 2 &&
                 (*rangeV)[0].IsNumber() && (*rangeV)[1].IsNumber()) {
        decl.scale->setRange((*rangeV)[0].GetDouble(), (*rangeV)[1].GetDouble());
      }
    }

    // nice (numeric scales only).
    if (const auto* niceV = member(scV, "nice")) {
      if (niceV->IsBool()) decl.nice = niceV->GetBool();
    }

    // domain source: either a literal `domain` [lo,hi] or an auto-domain binding
    // `domainFrom` { data, field|fields }.
    if (const auto* domV = member(scV, "domain")) {
      if (domV->IsArray() && domV->Size() == 2 && (*domV)[0].IsNumber() &&
          (*domV)[1].IsNumber()) {
        decl.scale->setDomain((*domV)[0].GetDouble(), (*domV)[1].GetDouble());
      }
    } else if (const auto* fromV = member(scV, "domainFrom")) {
      // §6.1 #1: the data ref must resolve to a declared source.
      const std::string dataId = strOr(*fromV, "data");
      auto sit = sourceById_.find(dataId);
      if (sit == sourceById_.end()) {
        return fail(ManifestStatus::DanglingRef,
                    "scale '" + decl.id + "': domainFrom.data '" + dataId +
                        "' does not resolve to a data source");
      }
      const SourceDecl& srcDecl = sources_[sit->second];

      // Accept either `field` (single) or `fields` (a [low,high] pair, e.g. the
      // §6.2 yp bound to [low,high]) — both must resolve + carry an accepted dtype.
      // The auto-domain reducer folds the FIRST field's column (the running
      // min/max over the others is folded at build time too if present).
      std::vector<std::string> fields;
      if (const auto* fV = member(*fromV, "field")) {
        if (fV->IsString()) fields.push_back(fV->GetString());
      }
      if (const auto* fsV = member(*fromV, "fields")) {
        if (fsV->IsArray()) {
          for (const auto& f : fsV->GetArray())
            if (f.IsString()) fields.push_back(f.GetString());
        }
      }
      if (fields.empty()) {
        return fail(ManifestStatus::DanglingRef,
                    "scale '" + decl.id +
                        "': domainFrom has neither 'field' nor 'fields'");
      }
      for (const auto& f : fields) {
        auto cit = srcDecl.columnByName.find(f);
        if (cit == srcDecl.columnByName.end()) {
          return fail(ManifestStatus::DanglingRef,
                      "scale '" + decl.id + "': domainFrom field '" + f +
                          "' is not a column of source '" + dataId + "'");
        }
        // §6.1 #2: the column dtype must be accepted by the scale type.
        const DType dt = srcDecl.columns[cit->second].dtype;
        if (!scaleAcceptsDType(decl.type, dt)) {
          return fail(ManifestStatus::DTypeMismatch,
                      "scale '" + decl.id + "' type '" + scaleTypeName(decl.type) +
                          "' rejects field '" + f + "' of dtype '" +
                          toString(dt) + "'");
        }
      }
      decl.domainTable = srcDecl.tableId;
      decl.domainColumn = fields.front();
      decl.autodomain = true;

      // Wire the concrete scale's streaming auto-domain binding.
      if (auto* ls = dynamic_cast<LinearScale*>(decl.scale.get())) {
        ls->bindColumn(decl.domainTable, decl.domainColumn);
      } else if (auto* ts = dynamic_cast<TimeScale*>(decl.scale.get())) {
        ts->bindColumn(decl.domainTable, decl.domainColumn);
      } else if (auto* os = dynamic_cast<OrdinalScale*>(decl.scale.get())) {
        os->bindColumn(decl.domainTable, decl.domainColumn);
      }
    }

    scaleById_[decl.id] = scales_.size();
    scales_.push_back(std::move(decl));
  }

  if (scales_.empty()) {
    return fail(ManifestStatus::MissingSection, "scales: empty");
  }
  return ManifestResult{};
}

// ----- marks: type + pipeline + from + encoding(channel -> scale(field)|value) --
ManifestResult Manifest::parseMarks(const void* marksValue) {
  const auto& marks = *static_cast<const rapidjson::Value*>(marksValue);

  // For the channel-set pre-check (§6.1 #3) we need each pipeline's required
  // channel set; markSpecOf gives exactly that.
  for (const auto& mkV : marks.GetArray()) {
    if (!mkV.IsObject()) {
      return fail(ManifestStatus::MissingSection, "marks[]: not an object");
    }
    MarkDecl decl;
    decl.id = strOr(mkV, "id");
    if (decl.id.empty()) {
      return fail(ManifestStatus::MissingSection, "marks[]: missing string 'id'");
    }
    if (markById_.count(decl.id)) {
      return fail(ManifestStatus::DuplicateId,
                  "marks: duplicate id '" + decl.id + "'");
    }

    const std::string typeStr = strOr(mkV, "type");
    if (typeStr == "point") decl.mark = Mark::Point;
    else if (typeStr == "line") decl.mark = Mark::Line;
    else if (typeStr == "rect") decl.mark = Mark::Rect;
    else if (typeStr == "candle") decl.mark = Mark::Candle;
    else
      return fail(ManifestStatus::UnknownMarkType,
                  "mark '" + decl.id + "': unknown type '" + typeStr + "'");

    // §6.1 #1: the `from` data ref must resolve.
    decl.from = strOr(mkV, "from");
    auto sit = sourceById_.find(decl.from);
    if (sit == sourceById_.end()) {
      return fail(ManifestStatus::DanglingRef,
                  "mark '" + decl.id + "': from '" + decl.from +
                      "' does not resolve to a data source");
    }
    const SourceDecl& srcDecl = sources_[sit->second];
    decl.tableId = srcDecl.tableId;

    // pipeline: explicit key, else the mark's default. The line style is inferred
    // from the resolved pipeline (lineAA@1 => LineAA), then markSpecOf is the
    // authority on the pipeline key + required channels.
    std::string pipelineReq = strOr(mkV, "pipeline");
    if (decl.mark == Mark::Line && pipelineReq.rfind("lineAA", 0) == 0)
      decl.lineStyle = LineStyle::LineAA;
    MarkSpec spec = markSpecOf(decl.mark, decl.lineStyle);
    decl.pipeline = spec.pipeline;
    if (!pipelineReq.empty() && pipelineReq != spec.pipeline) {
      // The manifest named a pipeline that disagrees with the mark's. For v0 the
      // mark<->pipeline mapping is fixed (RESEARCH §4.3), so a mismatch is a hard
      // error (e.g. a candle pinned to lineAA@1).
      return fail(ManifestStatus::UnknownPipeline,
                  "mark '" + decl.id + "' type '" + typeStr +
                      "' uses pipeline '" + spec.pipeline +
                      "', manifest requested '" + pipelineReq + "'");
    }

    // encoding (required): channel -> { scale, field } | { value } | { color }.
    const auto* enc = member(mkV, "encoding");
    if (!enc || !enc->IsObject()) {
      return fail(ManifestStatus::MissingSection,
                  "mark '" + decl.id + "': missing 'encoding' object");
    }
    for (auto it = enc->MemberBegin(); it != enc->MemberEnd(); ++it) {
      const std::string key = it->name.GetString();
      const auto& bind = it->value;
      if (!bind.IsObject()) {
        return fail(ManifestStatus::MissingSection,
                    "mark '" + decl.id + "' channel '" + key +
                        "': binding is not an object");
      }

      // color is special — Phase 1 routes it onto the DrawItem (single uniform, or
      // up/down for a candle via a condition). We accept:
      //   { "value": "#rrggbb" }                            -> setColor
      //   { "condition": { "value": "#up" }, "value": "#down" } -> up/down
      if (key == "color") {
        // candle up/down: condition.value = up color, value = down color.
        if (const auto* cond = member(bind, "condition")) {
          Rgba up, down;
          const std::string upS = strOr(*cond, "value");
          const std::string downS = strOr(bind, "value");
          if (!parseHexColor(upS, up) || !parseHexColor(downS, down)) {
            return fail(ManifestStatus::MissingSection,
                        "mark '" + decl.id +
                            "': color condition needs hex up/down values");
          }
          decl.colorUp = up;
          decl.colorDown = down;
        } else {
          Rgba c;
          const std::string s = strOr(bind, "value");
          if (!parseHexColor(s, c)) {
            return fail(ManifestStatus::MissingSection,
                        "mark '" + decl.id + "': color value '" + s +
                            "' is not a hex color");
          }
          decl.color = c;
        }
        continue;
      }

      // strokeWidth / opacity etc. are draw-item style hints, not vertex channels;
      // ignore unknown non-vertex keys gracefully in v0 (they don't break typing).
      Channel ch;
      if (!channelOf(key, ch)) {
        // A non-vertex style key (strokeWidth, ...) — skip without error.
        continue;
      }

      ChannelDecl cd;
      cd.channel = ch;
      if (const auto* valV = member(bind, "value")) {
        if (valV->IsNumber()) {
          cd.isConstant = true;
          cd.value = valV->GetDouble();
          decl.channels.push_back(cd);
          continue;
        }
      }
      // field binding: field (required) + scale (optional ref).
      cd.field = strOr(bind, "field");
      if (cd.field.empty()) {
        return fail(ManifestStatus::MissingSection,
                    "mark '" + decl.id + "' channel '" + key +
                        "': needs a numeric 'value' or a 'field'");
      }
      // §6.1 #1: field must resolve to a column of the mark's source.
      auto cit = srcDecl.columnByName.find(cd.field);
      if (cit == srcDecl.columnByName.end()) {
        return fail(ManifestStatus::DanglingRef,
                    "mark '" + decl.id + "' channel '" + key + "': field '" +
                        cd.field + "' is not a column of source '" + decl.from +
                        "'");
      }
      cd.scaleId = strOr(bind, "scale");
      if (!cd.scaleId.empty()) {
        // §6.1 #1: the scale ref must resolve.
        auto scit = scaleById_.find(cd.scaleId);
        if (scit == scaleById_.end()) {
          return fail(ManifestStatus::DanglingRef,
                      "mark '" + decl.id + "' channel '" + key + "': scale '" +
                          cd.scaleId + "' does not resolve");
        }
        // §6.1 #2: the column dtype must be accepted by the bound scale's type.
        const DType dt = srcDecl.columns[cit->second].dtype;
        const ScaleType st = scales_[scit->second].type;
        if (!scaleAcceptsDType(st, dt)) {
          return fail(ManifestStatus::DTypeMismatch,
                      "mark '" + decl.id + "' channel '" + key + "': scale '" +
                          cd.scaleId + "' type '" + scaleTypeName(st) +
                          "' rejects field '" + cd.field + "' of dtype '" +
                          toString(dt) + "'");
        }
      }
      decl.channels.push_back(cd);
    }

    // §6.1 #3 (structural pre-check): the resolved channel set must cover the
    // pipeline's required channels. The EncodePass enforces this again at build
    // (the authoritative validateDrawItem gate), but pre-checking here gives a
    // localized "missing channel X" error before any byte streams.
    for (Channel req : spec.required) {
      bool bound = false;
      for (const auto& cd : decl.channels)
        if (cd.channel == req) { bound = true; break; }
      if (!bound) {
        return fail(ManifestStatus::EncodeRejected,
                    "mark '" + decl.id + "' (" + spec.pipeline +
                        "): encoding does not cover required channel '" +
                        toString(req) + "'");
      }
    }

    markById_[decl.id] = marks_.size();
    marks_.push_back(std::move(decl));
  }

  if (marks_.empty()) {
    return fail(ManifestStatus::MissingSection, "marks: empty");
  }
  return ManifestResult{};
}

// ===========================================================================
// build() — re-fold auto-domains + run the encode pass per mark (§6.1 #3 gate)
// ===========================================================================
ManifestResult Manifest::build(const BufferByteSource& src, Id firstId) {
  // 1) Re-fold every auto-domained scale over its live column (O(Δ)).
  for (auto& sd : scales_) {
    if (!sd.autodomain) continue;
    if (auto* ls = dynamic_cast<LinearScale*>(sd.scale.get())) {
      ls->updateDomain(tables_, src);
      if (sd.nice) ls->nice(sd.niceTarget);
    } else if (auto* ts = dynamic_cast<TimeScale*>(sd.scale.get())) {
      ts->updateDomain(tables_, src);
    } else if (auto* os = dynamic_cast<OrdinalScale*>(sd.scale.get())) {
      os->updateDomain(tables_);
    }
  }

  // 2) Run the encode pass per mark, allocating ids densely off firstId.
  compiled_.clear();
  Id nextId = firstId;
  for (auto& md : marks_) {
    Encoding enc;
    for (const auto& cd : md.channels) {
      if (cd.isConstant) {
        enc.constant(cd.channel, cd.value);
      } else {
        const Scale* sc = nullptr;
        if (!cd.scaleId.empty()) sc = scales_[scaleById_[cd.scaleId]].scale.get();
        enc.set(cd.channel, ChannelBinding::fieldScaled(cd.field, sc));
      }
    }
    if (md.color) enc.setColor(*md.color);
    if (md.colorUp) enc.setColorUp(*md.colorUp);
    if (md.colorDown) enc.setColorDown(*md.colorDown);

    const Id geoId = nextId++;
    const Id diId = nextId++;
    const Id vbId = nextId++;

    // §6.1 #3 — the authoritative validateDrawItem-at-compile gate.
    EncodeResult res = encodePass_.compile(md.mark, enc, tables_, md.tableId, src,
                                           geoId, diId, vbId, nullptr,
                                           md.lineStyle);
    if (!res.ok) {
      return fail(ManifestStatus::EncodeRejected,
                  "mark '" + md.id + "' (" + md.pipeline + "): encode rejected: " +
                      std::string(toString(res.error)) + " — " + res.message);
    }
    res.drawItem.pipeline = md.pipeline;

    CompiledMark cm;
    cm.id = md.id;
    cm.mark = md.mark;
    cm.lineStyle = md.lineStyle;
    cm.pipeline = md.pipeline;
    cm.result = std::move(res);
    compiled_.push_back(std::move(cm));
  }

  return ManifestResult{};
}

// ===========================================================================
// accessors
// ===========================================================================
std::optional<Id> Manifest::tableId(const std::string& sourceId) const {
  auto it = sourceById_.find(sourceId);
  if (it == sourceById_.end()) return std::nullopt;
  return sources_[it->second].tableId;
}

std::optional<Id> Manifest::columnBufferId(const std::string& sourceId,
                                           const std::string& column) const {
  auto it = sourceById_.find(sourceId);
  if (it == sourceById_.end()) return std::nullopt;
  const SourceDecl& s = sources_[it->second];
  auto cit = s.columnByName.find(column);
  if (cit == s.columnByName.end()) return std::nullopt;
  return s.columns[cit->second].bufferId;
}

const Scale* Manifest::scale(const std::string& scaleId) const {
  auto it = scaleById_.find(scaleId);
  if (it == scaleById_.end()) return nullptr;
  return scales_[it->second].scale.get();
}

Scale* Manifest::scale(const std::string& scaleId) {
  auto it = scaleById_.find(scaleId);
  if (it == scaleById_.end()) return nullptr;
  return scales_[it->second].scale.get();
}

const CompiledMark* Manifest::compiledMark(const std::string& markId) const {
  for (const auto& cm : compiled_)
    if (cm.id == markId) return &cm;
  return nullptr;
}

}  // namespace dc
