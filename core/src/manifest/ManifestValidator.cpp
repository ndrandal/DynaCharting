// ENC-620a (Epic ENC-620) — Manifest load-time validator implementation.
// See ManifestValidator.hpp for the four §6.1 checks + the structured report.
//
// RapidJSON is the JSON lib already used by the Manifest parser / CommandProcessor;
// this mirrors that parsing style (member() / typed accessors). The validator owns
// NO byte buffers: it builds a static schema model, instantiates the REAL transform
// nodes (ENC-616*) only to call their data-free inferSchema, and reasons over names
// + dtypes. It never runs a transform and never reads a column byte.
#include "dc/manifest/ManifestValidator.hpp"

#include "dc/encode/EncodePass.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"
#include "dc/scale/Scale.hpp"
#include "dc/transform/Transform.hpp"
#include "dc/transform/transforms/Aggregate.hpp"
#include "dc/transform/transforms/Bin.hpp"
#include "dc/transform/transforms/Filter.hpp"
#include "dc/transform/transforms/Formula.hpp"
#include "dc/transform/transforms/Join.hpp"
#include "dc/transform/transforms/Sample.hpp"
#include "dc/transform/transforms/Sort.hpp"
#include "dc/transform/transforms/Stack.hpp"
#include "dc/transform/transforms/Window.hpp"

#include <rapidjson/document.h>

#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace dc {

const char* toString(ValidationCheck c) {
  switch (c) {
    case ValidationCheck::Structure: return "Structure";
    case ValidationCheck::RefResolution: return "RefResolution";
    case ValidationCheck::ColumnSet: return "ColumnSet";
    case ValidationCheck::ChannelMatrix: return "ChannelMatrix";
    case ValidationCheck::DagCoherence: return "DagCoherence";
  }
  return "Unknown";
}

const char* toString(Severity s) {
  switch (s) {
    case Severity::Error: return "Error";
    case Severity::Warning: return "Warning";
  }
  return "Unknown";
}

std::string ValidationReport::toString() const {
  std::ostringstream os;
  for (const auto& i : issues) {
    os << "[" << dc::toString(i.severity) << " " << dc::toString(i.check) << "]";
    if (!i.nodeId.empty()) os << " node '" << i.nodeId << "'";
    if (!i.path.empty()) os << " " << i.path;
    os << ": " << i.message << "\n";
  }
  return os.str();
}

namespace {

// ----- JSON helpers (mirror Manifest.cpp's style) --------------------------

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

// ----- the §6.1 #2/#3 scale↔dtype matrix (mirror of Manifest.cpp) ----------

// A scale type accepts a column dtype iff the §6.1 #2 compatibility holds:
//   linear/log/sqrt <- numeric (f32 / i32); time <- timestamp;
//   band/point      <- category (cat); color/diverging <- numeric (the field a
//                      color scale interpolates is the scalar t).
bool scaleAcceptsDType(const std::string& scaleType, DType dt) {
  if (scaleType == "linear" || scaleType == "log" || scaleType == "sqrt" ||
      scaleType == "pow" || scaleType == "color" || scaleType == "diverging" ||
      scaleType == "sequential") {
    return dt == DType::F32 || dt == DType::I32;
  }
  if (scaleType == "time") return dt == DType::Timestamp;
  if (scaleType == "band" || scaleType == "point" || scaleType == "ordinal") {
    return dt == DType::Cat;
  }
  return false;
}

// The finite scale-type vocabulary the validator recognizes (RESEARCH §6.1's
// ~11 scales; v0 implements a subset but the TYPE CHECK is over the whole vocab so
// a future-but-declared scale still validates its dtype contract).
bool isKnownScaleType(const std::string& t) {
  return t == "linear" || t == "log" || t == "sqrt" || t == "pow" ||
         t == "time" || t == "band" || t == "point" || t == "ordinal" ||
         t == "color" || t == "diverging" || t == "sequential";
}

// Does a scale type require a strictly-positive domain (so a field that can be ≤0
// is a typing error)? Log is the canonical case (RESEARCH §6.5's worked example).
bool scaleRequiresPositive(const std::string& t) { return t == "log"; }

// The §6.5 positivity check, run WITHOUT data: a `log` scale needs a strictly-
// positive domain, but a plain f32/i32 column carries NO static positivity
// guarantee (it CAN be ≤0), so binding it to a log scale is a typing error — the
// exact RESEARCH §6.5 worked example ("scale `yp` type `log` rejects field `close`
// which can be ≤0"). A column the manifest could prove positive (a future `>0`
// dtype refinement) would pass; v0 has none, so any numeric column is flagged.
bool logRejectsField(const std::string& scaleType, DType dt) {
  return scaleRequiresPositive(scaleType) &&
         (dt == DType::F32 || dt == DType::I32);
}

// ----- the channel vocabulary (mirror of Manifest::channelOf, extended) ----

// Map a manifest channel KEY to the Phase-1/2 Channel enum. Returns false for a
// key that is not a vertex/instance CHANNEL (color/text/strokeWidth/opacity/size
// style hints are handled separately).
bool channelOf(const std::string& key, Channel& out) {
  if (key == "x") { out = Channel::X; return true; }
  if (key == "y") { out = Channel::Y; return true; }
  if (key == "x2") { out = Channel::X2; return true; }
  if (key == "y2") { out = Channel::Y2; return true; }
  if (key == "yOpen" || key == "open") { out = Channel::Open; return true; }
  if (key == "yHigh" || key == "high") { out = Channel::High; return true; }
  if (key == "yLow" || key == "low") { out = Channel::Low; return true; }
  if (key == "yClose" || key == "close") { out = Channel::Close; return true; }
  if (key == "size" || key == "width") { out = Channel::Size; return true; }
  return false;
}

// ----- the mark vocabulary + its legal pipeline -----------------------------

struct MarkInfo {
  bool known{false};
  Mark mark{Mark::Point};
  LineStyle lineStyle{LineStyle::Line2d};
};

// Resolve a manifest mark `type` string to a Mark + lineStyle, given the requested
// pipeline (a line draws line2d@1 by default, lineAA@1 when so requested).
MarkInfo markOf(const std::string& type, const std::string& pipeline) {
  MarkInfo mi;
  if (type == "point") { mi.known = true; mi.mark = Mark::Point; }
  else if (type == "line" || type == "rule") {
    mi.known = true; mi.mark = Mark::Line;
    if (pipeline.rfind("lineAA", 0) == 0) mi.lineStyle = LineStyle::LineAA;
  }
  else if (type == "rect") {
    mi.known = true;
    mi.mark = (pipeline.rfind("instancedRectColor", 0) == 0) ? Mark::RectColor
                                                             : Mark::Rect;
  }
  else if (type == "candle") { mi.known = true; mi.mark = Mark::Candle; }
  else if (type == "arc") { mi.known = true; mi.mark = Mark::Arc; }
  // `text` is a non-vertex mark (textSDF@1) — handled out-of-band below.
  return mi;
}

// ----- the static manifest model the validator builds -----------------------

struct ColumnM {
  std::string name;
  DType dtype{DType::F32};
};

// A dataset NODE in the §6.1 one-namespace: a source (its columns are declared) or
// a transform (its columns are INFERRED from its input). Both expose a column set.
struct DatasetM {
  std::string id;
  bool isSource{false};
  // source: declared columns (filled at parse); transform: inferred (filled in
  // topo order during column-set inference).
  std::vector<ColumnM> columns;
  bool schemaResolved{false};  // transform: has its inferred schema been computed?

  // transform-only:
  std::string op;          // window/bin/aggregate/filter/formula/sort/stack/...
  std::string input;       // `in` — the left input dataset id
  std::string rightInput;  // join: the right lookup dataset id (from lookup.from)
  std::string streamClass; // stream.class — incremental/windowed/globalRecompute/...
  std::string cadence;     // stream.cadence string (perFrame/onHop/...) if a string
  const rapidjson::Value* spec{nullptr};  // the transform's op-specific JSON sub-obj

  const ColumnM* find(const std::string& n) const {
    for (const auto& c : columns)
      if (c.name == n) return &c;
    return nullptr;
  }
};

struct ScaleM {
  std::string id;
  std::string type;
  bool hasDomainFrom{false};
  std::string domainData;            // domainFrom.data
  std::vector<std::string> domainFields;  // domainFrom.field|fields
};

struct ChannelM {
  std::string key;       // the raw manifest channel key (for the path/message)
  Channel channel{Channel::X};
  bool isConstant{false};
  std::string field;
  std::string scaleId;   // "" => identity
};

struct MarkM {
  std::string id;
  std::string type;
  std::string from;
  std::string pipeline;    // requested pipeline (or the mark default)
  MarkInfo info;
  bool isText{false};      // text mark (textSDF@1) — non-vertex
  std::vector<ChannelM> channels;
};

// ----- the validator's working state ----------------------------------------

class Validator {
 public:
  ValidationReport run(const std::string& jsonText);

 private:
  void err(ValidationCheck c, std::string node, std::string path,
           std::string msg) {
    report_.issues.push_back(
        {Severity::Error, c, std::move(node), std::move(path), std::move(msg)});
  }
  void warn(ValidationCheck c, std::string node, std::string path,
            std::string msg) {
    report_.issues.push_back(
        {Severity::Warning, c, std::move(node), std::move(path), std::move(msg)});
  }

  // parse phases (structural; collect the static model)
  bool parseData(const rapidjson::Value& data);
  void parseTransforms(const rapidjson::Value& transforms);
  void parseScales(const rapidjson::Value& scales);
  void parseMarks(const rapidjson::Value& marks);

  // the four checks
  void checkRefResolution();
  bool checkDagAndOrder();   // §6.1 #4 acyclicity (returns false on a cycle)
  void checkColumnSets();    // §6.1 #2 (depends on the topo order)
  void checkChannelMatrix(); // §6.1 #3
  void checkStreamingCoherence();  // §6.1 #4 class-3 → perFrame warning

  // column-set inference: compute a transform's inferred schema from its input's.
  // Records issues on a typing failure; returns false if the schema cannot be
  // produced (so downstream nodes are skipped, not cascaded with noise).
  bool inferTransformSchema(DatasetM& t);

  // build a TransformNode from a transform's JSON (the REAL ENC-616 node), or
  // nullptr + an error if the op/spec is malformed. arity 2 (join) is reported via
  // `isJoin`.
  std::unique_ptr<TransformNode> buildNode(const DatasetM& t, bool& isJoin,
                                           std::string& buildErr) const;

  DatasetM* dataset(const std::string& id) {
    auto it = datasetById_.find(id);
    return it == datasetById_.end() ? nullptr : &datasets_[it->second];
  }
  ScaleM* scale(const std::string& id) {
    auto it = scaleById_.find(id);
    return it == scaleById_.end() ? nullptr : &scales_[it->second];
  }

  ValidationReport report_;
  rapidjson::Document doc_;

  std::vector<DatasetM> datasets_;  // sources + transforms (the one namespace half)
  std::unordered_map<std::string, std::size_t> datasetById_;
  std::vector<ScaleM> scales_;
  std::unordered_map<std::string, std::size_t> scaleById_;
  std::vector<MarkM> marks_;

  std::vector<std::string> topoOrder_;  // transform ids in eval order (post-check 4)
  bool dagOk_{false};
};

// ===========================================================================
// run — orchestrate the four checks
// ===========================================================================
ValidationReport Validator::run(const std::string& jsonText) {
  doc_.Parse(jsonText.c_str());
  if (doc_.HasParseError() || !doc_.IsObject()) {
    err(ValidationCheck::Structure, "", "", "manifest: invalid JSON object");
    return std::move(report_);
  }

  // ----- data (required) -----
  const auto* data = member(doc_, "data");
  if (!data || !data->IsObject()) {
    err(ValidationCheck::Structure, "", "data", "manifest: missing 'data' object");
    return std::move(report_);
  }
  if (!parseData(*data)) return std::move(report_);

  // ----- transforms (optional) -----
  if (const auto* tf = member(doc_, "transforms")) {
    if (tf->IsArray()) parseTransforms(*tf);
    else err(ValidationCheck::Structure, "", "transforms",
             "'transforms' must be an array");
  }

  // ----- scales (required) -----
  const auto* scales = member(doc_, "scales");
  if (!scales || !scales->IsArray()) {
    err(ValidationCheck::Structure, "", "scales",
        "manifest: missing 'scales' array");
    return std::move(report_);
  }
  parseScales(*scales);

  // ----- coords (optional; cartesian only in v0) -----
  if (const auto* coords = member(doc_, "coords")) {
    const std::string t = strOr(*coords, "type", "cartesian");
    if (t != "cartesian")
      err(ValidationCheck::Structure, "", "coords.type",
          "coords: v0 supports only 'cartesian', got '" + t + "'");
  }

  // ----- marks (required) -----
  const auto* marks = member(doc_, "marks");
  if (!marks || !marks->IsArray()) {
    err(ValidationCheck::Structure, "", "marks",
        "manifest: missing 'marks' array");
    return std::move(report_);
  }
  parseMarks(*marks);

  // The four §6.1 checks, in order (1 → 4 → 2 → 3): ref resolution first (the
  // namespace must be sound), then the DAG topo-order (column-set inference walks
  // it), then column-set inference, then the channel↔scale↔pipeline matrix.
  checkRefResolution();
  dagOk_ = checkDagAndOrder();
  if (dagOk_) {
    checkColumnSets();
    checkChannelMatrix();
  }
  checkStreamingCoherence();

  return std::move(report_);
}

// ===========================================================================
// parse phases — build the static model (structural issues only here)
// ===========================================================================
bool Validator::parseData(const rapidjson::Value& data) {
  const auto* sources = member(data, "sources");
  if (!sources || !sources->IsArray()) {
    err(ValidationCheck::Structure, "", "data.sources",
        "data: missing 'sources' array");
    return false;
  }
  for (const auto& srcV : sources->GetArray()) {
    if (!srcV.IsObject()) {
      err(ValidationCheck::Structure, "", "data.sources[]",
          "data.sources[]: not an object");
      continue;
    }
    DatasetM ds;
    ds.isSource = true;
    ds.id = strOr(srcV, "id");
    if (ds.id.empty()) {
      err(ValidationCheck::Structure, "", "data.sources[]",
          "data.sources[]: missing string 'id'");
      continue;
    }
    if (datasetById_.count(ds.id)) {
      err(ValidationCheck::Structure, ds.id, "data.sources",
          "duplicate dataset id '" + ds.id + "'");
      continue;
    }

    const rapidjson::Value* columns = nullptr;
    if (const auto* stream = member(srcV, "stream"))
      columns = member(*stream, "columns");
    if (!columns) columns = member(srcV, "columns");
    if (!columns || !columns->IsObject()) {
      err(ValidationCheck::Structure, ds.id, "data.sources['" + ds.id + "']",
          "source '" + ds.id + "': missing 'columns' object");
      continue;
    }
    for (auto it = columns->MemberBegin(); it != columns->MemberEnd(); ++it) {
      ColumnM col;
      col.name = it->name.GetString();
      const std::string dtStr = strOr(it->value, "dtype", "f32");
      auto dt = parseDType(dtStr);
      if (!dt) {
        err(ValidationCheck::Structure, ds.id,
            "data.sources['" + ds.id + "'].columns." + col.name,
            "column '" + col.name + "': unknown dtype '" + dtStr + "'");
        continue;
      }
      col.dtype = *dt;
      ds.columns.push_back(col);
    }
    ds.schemaResolved = true;  // a source's schema is its declared columns
    datasetById_[ds.id] = datasets_.size();
    datasets_.push_back(std::move(ds));
  }
  if (datasets_.empty()) {
    err(ValidationCheck::Structure, "", "data.sources", "data.sources: empty");
    return false;
  }
  return true;
}

void Validator::parseTransforms(const rapidjson::Value& transforms) {
  for (const auto& tV : transforms.GetArray()) {
    if (!tV.IsObject()) {
      err(ValidationCheck::Structure, "", "transforms[]",
          "transforms[]: not an object");
      continue;
    }
    DatasetM ds;
    ds.isSource = false;
    ds.id = strOr(tV, "id");
    if (ds.id.empty()) {
      err(ValidationCheck::Structure, "", "transforms[]",
          "transforms[]: missing string 'id'");
      continue;
    }
    if (datasetById_.count(ds.id)) {
      err(ValidationCheck::Structure, ds.id, "transforms",
          "duplicate dataset id '" + ds.id + "'");
      continue;
    }
    ds.op = strOr(tV, "op");
    ds.input = strOr(tV, "in");
    ds.spec = member(tV, ds.op.c_str());  // e.g. the "window" sub-object
    // The join/lookup right input is named under the op's `from` (§6.4 lookup.from).
    if (ds.spec && ds.spec->IsObject())
      ds.rightInput = strOr(*ds.spec, "from");

    if (const auto* stream = member(tV, "stream")) {
      ds.streamClass = strOr(*stream, "class");
      if (const auto* cad = member(*stream, "cadence"))
        if (cad->IsString()) ds.cadence = cad->GetString();
    }
    datasetById_[ds.id] = datasets_.size();
    datasets_.push_back(std::move(ds));
  }
}

void Validator::parseScales(const rapidjson::Value& scales) {
  for (const auto& scV : scales.GetArray()) {
    if (!scV.IsObject()) {
      err(ValidationCheck::Structure, "", "scales[]", "scales[]: not an object");
      continue;
    }
    ScaleM sc;
    sc.id = strOr(scV, "id");
    if (sc.id.empty()) {
      err(ValidationCheck::Structure, "", "scales[]",
          "scales[]: missing string 'id'");
      continue;
    }
    if (scaleById_.count(sc.id)) {
      err(ValidationCheck::Structure, sc.id, "scales",
          "duplicate scale id '" + sc.id + "'");
      continue;
    }
    sc.type = strOr(scV, "type");
    if (!isKnownScaleType(sc.type)) {
      err(ValidationCheck::Structure, sc.id, "scales['" + sc.id + "'].type",
          "scale '" + sc.id + "': unknown type '" + sc.type + "'");
    }
    if (const auto* fromV = member(scV, "domainFrom")) {
      sc.hasDomainFrom = true;
      sc.domainData = strOr(*fromV, "data");
      if (const auto* fV = member(*fromV, "field"))
        if (fV->IsString()) sc.domainFields.push_back(fV->GetString());
      if (const auto* fsV = member(*fromV, "fields"))
        if (fsV->IsArray())
          for (const auto& f : fsV->GetArray())
            if (f.IsString()) sc.domainFields.push_back(f.GetString());
    }
    scaleById_[sc.id] = scales_.size();
    scales_.push_back(std::move(sc));
  }
}

void Validator::parseMarks(const rapidjson::Value& marks) {
  for (const auto& mkV : marks.GetArray()) {
    if (!mkV.IsObject()) {
      err(ValidationCheck::Structure, "", "marks[]", "marks[]: not an object");
      continue;
    }
    MarkM mk;
    mk.id = strOr(mkV, "id");
    if (mk.id.empty()) {
      err(ValidationCheck::Structure, "", "marks[]",
          "marks[]: missing string 'id'");
      continue;
    }
    mk.type = strOr(mkV, "type");
    mk.from = strOr(mkV, "from");
    mk.pipeline = strOr(mkV, "pipeline");
    mk.isText = (mk.type == "text");
    if (!mk.isText) {
      mk.info = markOf(mk.type, mk.pipeline);
      if (!mk.info.known)
        err(ValidationCheck::Structure, mk.id, "marks['" + mk.id + "'].type",
            "mark '" + mk.id + "': unknown type '" + mk.type + "'");
    }

    const auto* enc = member(mkV, "encoding");
    if (!enc || !enc->IsObject()) {
      err(ValidationCheck::Structure, mk.id, "marks['" + mk.id + "'].encoding",
          "mark '" + mk.id + "': missing 'encoding' object");
      marks_.push_back(std::move(mk));
      continue;
    }
    for (auto it = enc->MemberBegin(); it != enc->MemberEnd(); ++it) {
      const std::string key = it->name.GetString();
      const auto& bind = it->value;
      Channel ch;
      if (!channelOf(key, ch)) continue;  // color/text/strokeWidth handled elsewhere
      if (!bind.IsObject()) {
        err(ValidationCheck::Structure, mk.id,
            "marks['" + mk.id + "'].encoding." + key,
            "channel '" + key + "': binding is not an object");
        continue;
      }
      ChannelM cm;
      cm.key = key;
      cm.channel = ch;
      if (const auto* valV = member(bind, "value")) {
        if (valV->IsNumber()) {
          cm.isConstant = true;
          mk.channels.push_back(cm);
          continue;
        }
      }
      cm.field = strOr(bind, "field");
      cm.scaleId = strOr(bind, "scale");
      if (cm.field.empty() && cm.scaleId.empty()) {
        // a non-constant channel that names neither a field nor a scale is a
        // structural hole the matrix check would otherwise mis-attribute.
        err(ValidationCheck::Structure, mk.id,
            "marks['" + mk.id + "'].encoding." + key,
            "channel '" + key + "': needs a 'value', a 'field', or a 'scale'");
        continue;
      }
      mk.channels.push_back(cm);
    }
    marks_.push_back(std::move(mk));
  }
}

// ===========================================================================
// CHECK 1 — reference resolution (§6.1 #1)
// ===========================================================================
void Validator::checkRefResolution() {
  // transform `in` + join `from` must resolve to a dataset.
  for (const auto& t : datasets_) {
    if (t.isSource) continue;
    if (t.input.empty()) {
      err(ValidationCheck::RefResolution, t.id, "transforms['" + t.id + "'].in",
          "transform '" + t.id + "': missing 'in' input");
    } else if (!datasetById_.count(t.input)) {
      err(ValidationCheck::RefResolution, t.id, "transforms['" + t.id + "'].in",
          "transform '" + t.id + "': in '" + t.input +
              "' does not resolve to a data source or transform");
    }
    if (!t.rightInput.empty() && !datasetById_.count(t.rightInput)) {
      err(ValidationCheck::RefResolution, t.id,
          "transforms['" + t.id + "']." + t.op + ".from",
          "transform '" + t.id + "': lookup.from '" + t.rightInput +
              "' does not resolve to a data source or transform");
    }
  }
  // scale domainFrom.data must resolve.
  for (const auto& s : scales_) {
    if (s.hasDomainFrom && !datasetById_.count(s.domainData)) {
      err(ValidationCheck::RefResolution, s.id,
          "scales['" + s.id + "'].domainFrom.data",
          "scale '" + s.id + "': domainFrom.data '" + s.domainData +
              "' does not resolve to a data source or transform");
    }
  }
  // mark `from` + each channel's `scale` must resolve.
  for (const auto& m : marks_) {
    if (m.from.empty() || !datasetById_.count(m.from)) {
      err(ValidationCheck::RefResolution, m.id, "marks['" + m.id + "'].from",
          "mark '" + m.id + "': from '" + m.from +
              "' does not resolve to a data source or transform");
    }
    for (const auto& c : m.channels) {
      if (!c.scaleId.empty() && !scaleById_.count(c.scaleId)) {
        err(ValidationCheck::RefResolution, m.id,
            "marks['" + m.id + "'].encoding." + c.key + ".scale",
            "mark '" + m.id + "' channel '" + c.key + "': scale '" + c.scaleId +
                "' does not resolve");
      }
    }
  }
}

// ===========================================================================
// CHECK 4 (part A) — DAG acyclicity (§6.1 #4). Topo-sorts the data→transform DAG
// over `in` (+ join `from`) edges. A cycle is a HARD error. Returns the eval order
// in topoOrder_ on success.
// ===========================================================================
bool Validator::checkDagAndOrder() {
  // Kahn's algorithm over transform nodes. Edges: input(s) -> transform. Sources
  // have no incoming edges (always ready). An edge to an UNRESOLVED ref is dropped
  // here (already reported by check 1) so we don't double-count.
  std::unordered_map<std::string, int> indeg;
  std::unordered_map<std::string, std::vector<std::string>> out;
  for (const auto& d : datasets_) indeg[d.id] = 0;

  auto addEdge = [&](const std::string& from, const std::string& to) {
    if (!datasetById_.count(from)) return;
    out[from].push_back(to);
    indeg[to] += 1;
  };
  for (const auto& t : datasets_) {
    if (t.isSource) continue;
    addEdge(t.input, t.id);
    if (!t.rightInput.empty()) addEdge(t.rightInput, t.id);
  }

  std::vector<std::string> ready;
  for (const auto& d : datasets_)
    if (indeg[d.id] == 0) ready.push_back(d.id);

  std::vector<std::string> order;
  while (!ready.empty()) {
    std::string n = ready.back();
    ready.pop_back();
    order.push_back(n);
    for (const auto& m : out[n])
      if (--indeg[m] == 0) ready.push_back(m);
  }

  if (order.size() != datasets_.size()) {
    // The nodes NOT in `order` are exactly the cycle members (+ their downstream).
    std::unordered_set<std::string> placed(order.begin(), order.end());
    std::string cyc;
    for (const auto& d : datasets_)
      if (!placed.count(d.id)) cyc += (cyc.empty() ? "" : " -> ") + d.id;
    err(ValidationCheck::DagCoherence, "", "transforms",
        "transform DAG has a cycle among: " + cyc + " (topo-sort failed)");
    return false;
  }

  // Keep only transform ids in eval order (sources are leaves, already resolved).
  for (const auto& id : order)
    if (auto* d = dataset(id); d && !d->isSource) topoOrder_.push_back(id);
  return true;
}

// ===========================================================================
// CHECK 2 — column-set + dtype inference (§6.1 #2)
//   Walk transforms in topo order, infer each output schema from its input's via
//   the REAL transform node's inferSchema. Then verify every scale domain field and
//   every mark channel field resolves to a column with a scale-accepted dtype.
// ===========================================================================
std::unique_ptr<TransformNode> Validator::buildNode(const DatasetM& t,
                                                    bool& isJoin,
                                                    std::string& buildErr) const {
  isJoin = false;
  const std::string& op = t.op;
  const rapidjson::Value* s = t.spec;  // op-specific sub-object (may be null)

  auto needObj = [&]() -> bool {
    if (s && s->IsObject()) return true;
    buildErr = "transform '" + t.id + "': op '" + op + "' missing its '" + op +
               "' spec object";
    return false;
  };

  if (op == "filter") {
    // predicate may sit under filter.expr / filter.predicate, or a bare string.
    std::string pred;
    if (s && s->IsObject()) {
      pred = strOr(*s, "expr");
      if (pred.empty()) pred = strOr(*s, "predicate");
    } else if (s && s->IsString()) {
      pred = s->GetString();
    }
    if (pred.empty()) { buildErr = "filter '" + t.id + "': missing predicate expr"; return nullptr; }
    return std::make_unique<FilterTransform>(pred);
  }
  if (op == "formula") {
    if (!needObj()) return nullptr;
    const std::string expr = strOr(*s, "expr");
    const std::string as = strOr(*s, "as");
    if (expr.empty() || as.empty()) { buildErr = "formula '" + t.id + "': needs expr + as"; return nullptr; }
    return std::make_unique<FormulaTransform>(expr, as);
  }
  if (op == "window") {
    if (!needObj()) return nullptr;
    const std::string field = strOr(*s, "field");
    const std::string as = strOr(*s, "as");
    const std::string aggS = strOr(*s, "agg", "mean");
    WindowAgg agg = WindowAgg::Mean;
    if (aggS == "mean") agg = WindowAgg::Mean;
    else if (aggS == "sum") agg = WindowAgg::Sum;
    else if (aggS == "min") agg = WindowAgg::Min;
    else if (aggS == "max") agg = WindowAgg::Max;
    else if (aggS == "ema") agg = WindowAgg::Ema;
    else { buildErr = "window '" + t.id + "': unknown agg '" + aggS + "'"; return nullptr; }
    // window length from `frame:[-N,0]` (length N+1) or an explicit `window`/`period`.
    std::uint32_t win = 1;
    if (const auto* fr = member(*s, "frame");
        fr && fr->IsArray() && fr->Size() == 2 && (*fr)[0].IsNumber()) {
      const long lo = static_cast<long>((*fr)[0].GetDouble());
      win = static_cast<std::uint32_t>(lo < 0 ? (-lo + 1) : 1);
    } else if (const auto* w = member(*s, "window"); w && w->IsNumber()) {
      win = static_cast<std::uint32_t>(w->GetDouble());
    } else if (const auto* p = member(*s, "period"); p && p->IsNumber()) {
      win = static_cast<std::uint32_t>(p->GetDouble());
    }
    if (field.empty() || as.empty()) { buildErr = "window '" + t.id + "': needs field + as"; return nullptr; }
    return std::make_unique<WindowTransform>(field, agg, win, as);
  }
  if (op == "bin") {
    if (!needObj()) return nullptr;
    const std::string field = strOr(*s, "field");
    const std::string as = strOr(*s, "as");
    if (field.empty() || as.empty()) { buildErr = "bin '" + t.id + "': needs field + as"; return nullptr; }
    if (const auto* st = member(*s, "step"); st && st->IsNumber())
      return std::make_unique<BinTransform>(BinTransform::byStep(field, st->GetDouble(), as));
    int maxbins = 10;
    if (const auto* mb = member(*s, "maxbins"); mb && mb->IsNumber())
      maxbins = static_cast<int>(mb->GetDouble());
    return std::make_unique<BinTransform>(BinTransform::byMaxBins(field, maxbins, as));
  }
  if (op == "aggregate") {
    if (!needObj()) return nullptr;
    std::vector<std::string> groupBy;
    if (const auto* gb = member(*s, "groupBy"); gb && gb->IsArray())
      for (const auto& g : gb->GetArray())
        if (g.IsString()) groupBy.push_back(g.GetString());
    std::vector<AggMeasure> measures;
    if (const auto* ops = member(*s, "ops"); ops && ops->IsArray()) {
      for (const auto& o : ops->GetArray()) {
        if (!o.IsObject()) continue;
        AggMeasure m;
        const std::string a = strOr(o, "agg", "count");
        if (a == "count") m.op = AggOp::Count;
        else if (a == "sum") m.op = AggOp::Sum;
        else if (a == "mean") m.op = AggOp::Mean;
        else if (a == "min") m.op = AggOp::Min;
        else if (a == "max") m.op = AggOp::Max;
        else if (a == "median" || a == "p50") m.op = AggOp::Median;
        else if (a == "first") m.op = AggOp::Min;   // first/last approximated as a
        else if (a == "last") m.op = AggOp::Max;    // numeric reducer for typing
        else { buildErr = "aggregate '" + t.id + "': unknown agg '" + a + "'"; return nullptr; }
        m.field = strOr(o, "field");
        m.as = strOr(o, "as");
        measures.push_back(std::move(m));
      }
    }
    return std::make_unique<AggregateTransform>(std::move(groupBy), std::move(measures));
  }
  if (op == "sort") {
    if (!needObj()) return nullptr;
    const std::string key = strOr(*s, "field");
    const std::string keyAlt = key.empty() ? strOr(*s, "key") : key;
    const bool asc = strOr(*s, "order", "ascending") != "descending";
    const std::string as = strOr(*s, "as");
    if (keyAlt.empty()) { buildErr = "sort '" + t.id + "': needs a key field"; return nullptr; }
    if (!as.empty())
      return std::make_unique<SortTransform>(SortTransform::rank(keyAlt, as, asc));
    return std::make_unique<SortTransform>(SortTransform::reorder(keyAlt, asc));
  }
  if (op == "stack") {
    if (!needObj()) return nullptr;
    const std::string value = strOr(*s, "value");
    const std::string groupBy = strOr(*s, "groupBy");
    const std::string offS = strOr(*s, "offset", "zero");
    StackOffset off = StackOffset::Zero;
    if (offS == "zero") off = StackOffset::Zero;
    else if (offS == "normalize") off = StackOffset::Normalize;
    else if (offS == "wiggle") off = StackOffset::Wiggle;
    if (value.empty()) { buildErr = "stack '" + t.id + "': needs a value column"; return nullptr; }
    return std::make_unique<StackTransform>(value, groupBy, off);
  }
  if (op == "sample") {
    if (!needObj()) return nullptr;
    const std::string x = strOr(*s, "x");
    const std::string y = strOr(*s, "y");
    std::uint32_t budget = 1000;
    if (const auto* b = member(*s, "budget"); b && b->IsNumber())
      budget = static_cast<std::uint32_t>(b->GetDouble());
    if (x.empty() || y.empty()) { buildErr = "sample '" + t.id + "': needs x + y"; return nullptr; }
    return std::make_unique<SampleTransform>(x, y, budget);
  }
  if (op == "join" || op == "lookup") {
    if (!needObj()) return nullptr;
    isJoin = true;
    const std::string rightKey = strOr(*s, "key");
    std::vector<JoinLookup> lookups;
    if (const auto* on = member(*s, "on"); on && on->IsString()) {
      JoinLookup jl;
      jl.leftKey = on->GetString();
      jl.prefix = strOr(*s, "as", jl.leftKey);
      if (const auto* fs = member(*s, "fields"); fs && fs->IsArray())
        for (const auto& f : fs->GetArray())
          if (f.IsString()) jl.fields.push_back(f.GetString());
      lookups.push_back(std::move(jl));
    }
    if (rightKey.empty()) { buildErr = "join '" + t.id + "': needs a right 'key'"; return nullptr; }
    return std::make_unique<JoinTransform>(rightKey, std::move(lookups));
  }

  buildErr = "transform '" + t.id + "': unknown / unsupported op '" + op + "'";
  return nullptr;
}

bool Validator::inferTransformSchema(DatasetM& t) {
  if (t.schemaResolved) return true;
  // Its input must be resolved (topo order guarantees this for a valid DAG).
  DatasetM* in = dataset(t.input);
  if (!in || !in->schemaResolved) {
    // missing/unresolved input — already reported by check 1; skip quietly.
    return false;
  }

  bool isJoin = false;
  std::string buildErr;
  std::unique_ptr<TransformNode> node = buildNode(t, isJoin, buildErr);
  if (!node) {
    err(ValidationCheck::ColumnSet, t.id, "transforms['" + t.id + "']", buildErr);
    return false;
  }

  // Build the input ColumnSchema(s) from the resolved column model.
  auto toSchema = [](const DatasetM& d) {
    ColumnSchema s;
    for (const auto& c : d.columns) s.columns.push_back({c.name, c.dtype});
    return s;
  };
  ColumnSchema inSchema = toSchema(*in);

  SchemaResult res;
  if (isJoin) {
    DatasetM* right = dataset(t.rightInput);
    if (!right || !right->schemaResolved) {
      err(ValidationCheck::ColumnSet, t.id,
          "transforms['" + t.id + "']." + t.op + ".from",
          "transform '" + t.id + "': join right input unresolved");
      return false;
    }
    res = node->inferSchemaBinary(inSchema, toSchema(*right));
  } else {
    res = node->inferSchema(inSchema);
  }

  if (!res.ok) {
    err(ValidationCheck::ColumnSet, t.id, "transforms['" + t.id + "']",
        "transform '" + t.id + "' (" + t.op + "): " + res.error);
    return false;
  }
  for (const auto& c : res.schema.columns) t.columns.push_back({c.name, c.dtype});
  t.schemaResolved = true;
  return true;
}

void Validator::checkColumnSets() {
  // 1) infer every transform's output schema in topo order.
  for (const auto& id : topoOrder_) {
    if (auto* t = dataset(id)) inferTransformSchema(*t);
  }

  // 2) scale domainFrom fields must resolve to a column of the right dtype.
  for (const auto& s : scales_) {
    if (!s.hasDomainFrom) continue;
    DatasetM* d = dataset(s.domainData);
    if (!d || !d->schemaResolved) continue;  // unresolved data ref / typing error upstream
    if (s.domainFields.empty()) {
      err(ValidationCheck::ColumnSet, s.id, "scales['" + s.id + "'].domainFrom",
          "scale '" + s.id + "': domainFrom has neither 'field' nor 'fields'");
      continue;
    }
    for (const auto& f : s.domainFields) {
      const ColumnM* col = d->find(f);
      if (!col) {
        err(ValidationCheck::ColumnSet, s.id,
            "scales['" + s.id + "'].domainFrom.field",
            "scale '" + s.id + "': domainFrom field '" + f +
                "' is not a column of '" + s.domainData + "'");
        continue;
      }
      if (logRejectsField(s.type, col->dtype)) {
        err(ValidationCheck::ColumnSet, s.id,
            "scales['" + s.id + "'].domainFrom.field",
            "scale '" + s.id + "' type 'log' rejects field '" + f +
                "' which can be ≤0 (a 'log' scale needs a strictly-positive "
                "domain; an unconstrained '" +
                std::string(dc::toString(col->dtype)) +
                "' column carries no positivity guarantee)");
      } else if (!scaleAcceptsDType(s.type, col->dtype)) {
        err(ValidationCheck::ColumnSet, s.id,
            "scales['" + s.id + "'].domainFrom.field",
            "scale '" + s.id + "' type '" + s.type + "' rejects field '" + f +
                "' of dtype '" + std::string(dc::toString(col->dtype)) + "'");
      }
    }
  }

  // 3) mark channel fields must resolve to a column of the mark's source, and the
  //    bound scale must accept that column's dtype.
  for (const auto& m : marks_) {
    DatasetM* d = dataset(m.from);
    if (!d || !d->schemaResolved) continue;
    for (const auto& c : m.channels) {
      if (c.isConstant || c.field.empty()) continue;
      const ColumnM* col = d->find(c.field);
      if (!col) {
        err(ValidationCheck::ColumnSet, m.id,
            "marks['" + m.id + "'].encoding." + c.key + ".field",
            "mark '" + m.id + "' channel '" + c.key + "': field '" + c.field +
                "' is not a column of '" + m.from + "'");
        continue;
      }
      if (!c.scaleId.empty()) {
        ScaleM* sc = scale(c.scaleId);
        if (sc && logRejectsField(sc->type, col->dtype)) {
          err(ValidationCheck::ColumnSet, m.id,
              "marks['" + m.id + "'].encoding." + c.key,
              "mark '" + m.id + "' channel '" + c.key + "': scale '" + c.scaleId +
                  "' type 'log' rejects field '" + c.field +
                  "' which can be ≤0 (a 'log' scale needs a strictly-positive "
                  "domain)");
        } else if (sc && !scaleAcceptsDType(sc->type, col->dtype)) {
          err(ValidationCheck::ColumnSet, m.id,
              "marks['" + m.id + "'].encoding." + c.key,
              "mark '" + m.id + "' channel '" + c.key + "': scale '" + c.scaleId +
                  "' type '" + sc->type + "' rejects field '" + c.field +
                  "' of dtype '" + std::string(dc::toString(col->dtype)) + "'");
        }
      }
    }
  }
}

// ===========================================================================
// CHECK 3 — channel↔scale↔dtype↔pipeline matrix (§6.1 #3)
//   The mark↔pipeline legality + the resolved-channel-set-covers-required-format
//   clause (the validateDrawItem mirror). The dtype↔scale half rode check 2.
// ===========================================================================
void Validator::checkChannelMatrix() {
  PipelineCatalog catalog;
  for (const auto& m : marks_) {
    if (m.isText) {
      // text -> textSDF@1 (a non-vertex mark). Only require the pipeline resolve.
      const std::string key = m.pipeline.empty() ? "textSDF@1" : m.pipeline;
      if (!catalog.find(key))
        err(ValidationCheck::ChannelMatrix, m.id, "marks['" + m.id + "'].pipeline",
            "mark '" + m.id + "': pipeline '" + key + "' is not in the catalog");
      continue;
    }
    if (!m.info.known) continue;  // unknown mark type already reported

    // The mark's canonical spec (pipeline + required channels + format).
    MarkSpec spec = markSpecOf(m.info.mark, m.info.lineStyle);

    // mark↔pipeline legality: a manifest pipeline that disagrees with the mark's
    // canonical one is illegal (v0's mark↔pipeline map is fixed, RESEARCH §4.3).
    if (!m.pipeline.empty() && m.pipeline != spec.pipeline) {
      err(ValidationCheck::ChannelMatrix, m.id, "marks['" + m.id + "'].pipeline",
          "mark '" + m.id + "' type '" + m.type + "' uses pipeline '" +
              spec.pipeline + "', manifest requested '" + m.pipeline + "'");
      continue;
    }

    // pipeline↔catalog: the resolved pipeline must exist + agree on the format.
    const PipelineSpec* ps = catalog.find(spec.pipeline);
    if (!ps) {
      err(ValidationCheck::ChannelMatrix, m.id, "marks['" + m.id + "']",
          "mark '" + m.id + "': pipeline '" + spec.pipeline +
              "' is not in the catalog");
      continue;
    }
    if (ps->requiredVertexFormat != spec.format) {
      err(ValidationCheck::ChannelMatrix, m.id, "marks['" + m.id + "']",
          "mark '" + m.id + "': format mismatch — mark packs '" +
              std::string(dc::toString(spec.format)) + "' but pipeline '" +
              spec.pipeline + "' requires '" +
              std::string(dc::toString(ps->requiredVertexFormat)) + "'");
      continue;
    }

    // the validateDrawItem clause: the resolved channel SET must cover the
    // pipeline's REQUIRED vertex/instance channels.
    for (Channel req : spec.required) {
      bool bound = false;
      for (const auto& c : m.channels)
        if (c.channel == req) { bound = true; break; }
      if (!bound) {
        err(ValidationCheck::ChannelMatrix, m.id, "marks['" + m.id + "'].encoding",
            "mark '" + m.id + "' (" + spec.pipeline +
                "): encoding does not cover required channel '" +
                std::string(dc::toString(req)) + "' for its vertex/instance format");
      }
    }
  }
}

// ===========================================================================
// CHECK 4 (part B) — streaming-class coherence (§6.1 #4)
//   A globalRecompute (class-3) node feeding a perFrame mark is downgraded + WARNED
//   (not a hard error): the mark inherits the global's throttled cadence. We detect
//   it structurally — a mark whose `from` is (transitively, but flagged at the
//   direct producer) a class-3 transform with a non-perFrame cadence.
// ===========================================================================
void Validator::checkStreamingCoherence() {
  auto isClass3 = [](const DatasetM& t) {
    return t.streamClass == "globalRecompute" || t.streamClass == "global" ||
           t.streamClass == "baseline";
  };
  for (const auto& m : marks_) {
    DatasetM* d = dataset(m.from);
    if (!d || d->isSource) continue;
    if (isClass3(*d) && d->cadence != "perFrame") {
      warn(ValidationCheck::DagCoherence, m.id, "marks['" + m.id + "'].from",
           "mark '" + m.id + "' draws from class-3 (globalRecompute) transform '" +
               d->id + "' (cadence '" +
               (d->cadence.empty() ? "throttled" : d->cadence) +
               "'): the mark cannot render perFrame off it — DOWNGRADED to the "
               "global's cadence");
    }
  }
}

}  // namespace

// ===========================================================================
// public entry
// ===========================================================================
ValidationReport ManifestValidator::validate(const std::string& jsonText) const {
  Validator v;
  return v.run(jsonText);
}

}  // namespace dc
