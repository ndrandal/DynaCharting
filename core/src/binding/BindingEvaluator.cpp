// D80: BindingEvaluator implementation
#include "dc/binding/BindingEvaluator.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dc {

BindingEvaluator::BindingEvaluator(CommandProcessor& cp,
                                   IngestProcessor& ingest,
                                   DerivedBufferManager& derivedBufs)
    : cp_(cp), ingest_(ingest), derivedBufs_(derivedBufs) {}

void BindingEvaluator::loadBindings(const std::map<Id, DocBinding>& bindings) {
  bindings_.clear();

  for (const auto& [id, spec] : bindings) {
    LiveBinding lb;
    lb.id = id;
    lb.spec = spec;

    // Register DerivedBuffer entries for buffer-effect bindings
    const auto& eff = spec.effect;
    if (eff.type == "filterBuffer" && eff.sourceBufferId != 0 &&
        eff.outputBufferId != 0 && eff.recordStride > 0) {
      DerivedBufferConfig cfg;
      cfg.sourceBufferId = eff.sourceBufferId;
      cfg.outputBufferId = eff.outputBufferId;
      cfg.recordStride = eff.recordStride;
      cfg.mode = DeriveMode::IndexedFilter;
      derivedBufs_.add(id, cfg);
    } else if (eff.type == "rangeBuffer" && eff.sourceBufferId != 0 &&
               eff.outputBufferId != 0 && eff.recordStride > 0) {
      DerivedBufferConfig cfg;
      cfg.sourceBufferId = eff.sourceBufferId;
      cfg.outputBufferId = eff.outputBufferId;
      cfg.recordStride = eff.recordStride;
      cfg.mode = DeriveMode::Range;
      derivedBufs_.add(id, cfg);
    }

    bindings_.push_back(std::move(lb));
  }
}

void BindingEvaluator::attach(EventBus& bus, SelectionState& selection) {
  // Subscribe to SelectionChanged
  auto selSub = bus.subscribe(EventType::SelectionChanged,
    [this, &selection](const EventData&) {
      onSelectionChanged(selection);
    });
  subscriptions_.push_back(selSub);

  // Subscribe to HoverChanged
  auto hovSub = bus.subscribe(EventType::HoverChanged,
    [this](const EventData& ev) {
      onHoverChanged(ev.targetId, static_cast<std::uint32_t>(ev.payload[0]));
    });
  subscriptions_.push_back(hovSub);

  // Subscribe to DataChanged for threshold triggers
  auto dataSub = bus.subscribe(EventType::DataChanged,
    [this](const EventData& ev) {
      std::vector<Id> ids;
      if (ev.targetId != 0) ids.push_back(ev.targetId);
      onDataChanged(ids);
    });
  subscriptions_.push_back(dataSub);
}

void BindingEvaluator::detach(EventBus& bus) {
  for (auto subId : subscriptions_) {
    bus.unsubscribe(subId);
  }
  subscriptions_.clear();
}

std::vector<Id> BindingEvaluator::onSelectionChanged(
    const SelectionState& selection) {
  std::vector<Id> touched;

  for (auto& lb : bindings_) {
    if (lb.spec.trigger.type != "selection") continue;

    // Collect record indices for the trigger's drawItemId
    std::vector<std::uint32_t> indices;
    for (const auto& key : selection.selectedKeys()) {
      if (key.drawItemId == lb.spec.trigger.drawItemId) {
        indices.push_back(key.recordIndex);
      }
    }

    const auto& eff = lb.spec.effect;

    if (eff.type == "filterBuffer") {
      applyFilterEffect(lb, indices);
      if (eff.outputBufferId != 0)
        touched.push_back(eff.outputBufferId);
    } else if (eff.type == "setVisible") {
      applyVisibilityEffect(eff, !indices.empty());
    } else if (eff.type == "setColor") {
      applyColorEffect(eff, !indices.empty());
    }
  }

  return touched;
}

std::vector<Id> BindingEvaluator::onHoverChanged(
    Id drawItemId, std::uint32_t recordIndex) {
  std::vector<Id> touched;

  for (auto& lb : bindings_) {
    if (lb.spec.trigger.type != "hover") continue;
    if (lb.spec.trigger.drawItemId != drawItemId) continue;

    const auto& eff = lb.spec.effect;
    bool active = (recordIndex != static_cast<std::uint32_t>(-1));

    if (eff.type == "filterBuffer") {
      std::vector<std::uint32_t> indices;
      if (active) indices.push_back(recordIndex);
      applyFilterEffect(lb, indices);
      if (eff.outputBufferId != 0)
        touched.push_back(eff.outputBufferId);
    } else if (eff.type == "setVisible") {
      applyVisibilityEffect(eff, active);
    } else if (eff.type == "setColor") {
      applyColorEffect(eff, active);
    }
  }

  return touched;
}

std::vector<Id> BindingEvaluator::onViewportChanged(
    const std::string& viewportName, double xMin, double xMax) {
  std::vector<Id> touched;

  for (auto& lb : bindings_) {
    if (lb.spec.trigger.type != "viewport") continue;
    if (lb.spec.trigger.viewportName != viewportName) continue;

    const auto& eff = lb.spec.effect;
    if (eff.type == "rangeBuffer") {
      applyRangeEffect(lb, xMin, xMax);
      if (eff.outputBufferId != 0)
        touched.push_back(eff.outputBufferId);
    }
  }

  return touched;
}

std::vector<Id> BindingEvaluator::onDataChanged(
    const std::vector<Id>& touchedBufferIds) {
  std::vector<Id> touched;

  for (auto& lb : bindings_) {
    if (lb.spec.trigger.type != "threshold") continue;

    // Check if the trigger's source buffer was touched
    bool relevant = false;
    for (Id bid : touchedBufferIds) {
      if (bid == lb.spec.trigger.sourceBufferId) {
        relevant = true;
        break;
      }
    }
    if (!relevant) continue;

    // Read the latest record's field value
    Id srcBuf = lb.spec.trigger.sourceBufferId;
    const std::uint8_t* data = ingest_.getBufferData(srcBuf);
    std::uint32_t size = ingest_.getBufferSize(srcBuf);
    const auto& eff = lb.spec.effect;

    if (!data || size == 0 || eff.recordStride == 0) continue;

    std::uint32_t totalRecords = size / eff.recordStride;
    if (totalRecords == 0) continue;

    // Read float at fieldOffset in the last record
    std::uint32_t lastRecordOffset =
        (totalRecords - 1) * eff.recordStride + lb.spec.trigger.fieldOffset;
    if (lastRecordOffset + sizeof(float) > size) continue;

    float val = 0;
    std::memcpy(&val, data + lastRecordOffset, sizeof(float));

    bool conditionMet = false;
    double threshold = lb.spec.trigger.value;
    const auto& cond = lb.spec.trigger.condition;

    if (cond == "greaterThan") {
      conditionMet = val > threshold;
    } else if (cond == "lessThan") {
      conditionMet = val < threshold;
    } else if (cond == "crossingUp") {
      conditionMet = val > threshold && !lb.lastThresholdState;
    } else if (cond == "crossingDown") {
      conditionMet = val < threshold && lb.lastThresholdState;
    }

    // Update edge detection state
    lb.lastThresholdState = (val > threshold);

    if (eff.type == "setVisible") {
      applyVisibilityEffect(eff, conditionMet);
    } else if (eff.type == "setColor") {
      applyColorEffect(eff, conditionMet);
    }
  }

  return touched;
}

// --- Effect implementations ---

void BindingEvaluator::applyFilterEffect(
    const LiveBinding& b, const std::vector<std::uint32_t>& indices) {
  derivedBufs_.setIndices(b.id, indices);

  // Rebuild from the source buffer
  std::vector<Id> src = {b.spec.effect.sourceBufferId};
  derivedBufs_.rebuild(src, ingest_);

  // Update geometry vertex count
  if (b.spec.effect.geometryId != 0 && b.spec.effect.recordStride > 0) {
    updateGeometryVertexCount(b.spec.effect.geometryId,
                               b.spec.effect.outputBufferId,
                               b.spec.effect.recordStride);
  }
}

void BindingEvaluator::applyRangeEffect(
    const LiveBinding& b, double xMin, double xMax) {
  const auto& eff = b.spec.effect;
  const std::uint8_t* data = ingest_.getBufferData(eff.sourceBufferId);
  std::uint32_t size = ingest_.getBufferSize(eff.sourceBufferId);

  if (!data || size == 0 || eff.recordStride == 0) {
    derivedBufs_.setRange(b.id, 0, 0);
    std::vector<Id> src = {eff.sourceBufferId};
    derivedBufs_.rebuild(src, ingest_);
    if (eff.geometryId != 0)
      updateGeometryVertexCount(eff.geometryId, eff.outputBufferId,
                                 eff.recordStride);
    return;
  }

  std::uint32_t totalRecords = size / eff.recordStride;

  // Scan for records where x-field falls within [xMin, xMax]
  std::uint32_t startIdx = totalRecords;
  std::uint32_t endIdx = 0;

  for (std::uint32_t i = 0; i < totalRecords; i++) {
    std::uint32_t offset = i * eff.recordStride + eff.xFieldOffset;
    if (offset + sizeof(float) > size) break;

    float xVal = 0;
    std::memcpy(&xVal, data + offset, sizeof(float));

    if (xVal >= xMin && xVal <= xMax) {
      if (i < startIdx) startIdx = i;
      endIdx = i + 1;
    }
  }

  if (startIdx >= endIdx) {
    derivedBufs_.setRange(b.id, 0, 0);
  } else {
    derivedBufs_.setRange(b.id, startIdx, endIdx);
  }

  std::vector<Id> src = {eff.sourceBufferId};
  derivedBufs_.rebuild(src, ingest_);

  if (eff.geometryId != 0)
    updateGeometryVertexCount(eff.geometryId, eff.outputBufferId,
                               eff.recordStride);
}

void BindingEvaluator::applyVisibilityEffect(
    const DocBindingEffect& eff, bool active) {
  if (eff.drawItemId == 0) return;

  bool vis = active ? eff.visible : eff.defaultVisible;
  std::string json = "{\"cmd\":\"setDrawItemVisible\",\"drawItemId\":";
  json += std::to_string(eff.drawItemId);
  json += ",\"visible\":";
  json += (vis ? "true" : "false");
  json += "}";
  cp_.applyJsonText(json);
}

void BindingEvaluator::applyColorEffect(
    const DocBindingEffect& eff, bool active) {
  if (eff.drawItemId == 0) return;

  const float* c = active ? eff.color : eff.defaultColor;

  char buf[256];
  std::snprintf(buf, sizeof(buf),
    "{\"cmd\":\"setDrawItemColor\",\"drawItemId\":%u,"
    "\"r\":%.6f,\"g\":%.6f,\"b\":%.6f,\"a\":%.6f}",
    static_cast<unsigned>(eff.drawItemId),
    static_cast<double>(c[0]), static_cast<double>(c[1]),
    static_cast<double>(c[2]), static_cast<double>(c[3]));
  cp_.applyJsonText(std::string(buf));
}

void BindingEvaluator::updateGeometryVertexCount(
    Id geometryId, Id outputBufferId, std::uint32_t recordStride) {
  std::uint32_t outSize = ingest_.getBufferSize(outputBufferId);
  std::uint32_t vertexCount = (recordStride > 0) ? outSize / recordStride : 0;

  char buf[128];
  std::snprintf(buf, sizeof(buf),
    "{\"cmd\":\"setGeometryVertexCount\",\"geometryId\":%u,\"vertexCount\":%u}",
    static_cast<unsigned>(geometryId),
    static_cast<unsigned>(vertexCount));
  cp_.applyJsonText(std::string(buf));
}

} // namespace dc
