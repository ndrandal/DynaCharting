#include "dc/data/AggregationManager.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Geometry.hpp"

#include <string>

namespace dc {

void AggregationManager::setConfig(const AggregationManagerConfig& cfg) {
    config_ = cfg;
    ctrl_.setConfig(cfg.resolutionPolicy);
}

void AggregationManager::addBinding(const AggregationBinding& binding) {
    bindings_.push_back(binding);
}

void AggregationManager::clearBindings() {
    bindings_.clear();
}

std::vector<Id> AggregationManager::onViewportChanged(double ppdu,
                                                        IngestProcessor& ingest,
                                                        CommandProcessor& cp) {
    std::vector<Id> modified;

    bool changed = ctrl_.evaluate(ppdu);
    if (!changed) return modified;

    auto tier = ctrl_.currentTier();

    for (const auto& b : bindings_) {
        if (tier == ResolutionTier::Raw) {
            // Rebind geometry to raw buffer
            rebindGeometry(b, b.rawBufferId, ingest, cp);
            modified.push_back(b.rawBufferId);
        } else {
            // Recompute aggregated data
            recomputeAggBuffer(b, ingest);
            // Rebind geometry to agg buffer
            rebindGeometry(b, b.aggBufferId, ingest, cp);
            modified.push_back(b.aggBufferId);
        }
    }

    return modified;
}

std::vector<Id> AggregationManager::onRawDataChanged(const std::vector<Id>& touchedRaw,
                                                       IngestProcessor& ingest) {
    std::vector<Id> modified;

    if (ctrl_.currentTier() == ResolutionTier::Raw) return modified;

    for (const auto& b : bindings_) {
        for (Id rawId : touchedRaw) {
            if (rawId == b.rawBufferId) {
                recomputeAggBuffer(b, ingest);
                modified.push_back(b.aggBufferId);
                break;
            }
        }
    }

    return modified;
}

void AggregationManager::recomputeAggBuffer(const AggregationBinding& b,
                                              IngestProcessor& ingest) {
    std::uint32_t factor = ctrl_.currentFactor();
    const std::uint8_t* rawData = ingest.getBufferData(b.rawBufferId);
    std::uint32_t rawSize = ingest.getBufferSize(b.rawBufferId);

    auto agg = aggregateCandles(rawData, rawSize, factor);

    ingest.ensureBuffer(b.aggBufferId);
    if (!agg.data.empty()) {
        ingest.setBufferData(b.aggBufferId, agg.data.data(),
                              static_cast<std::uint32_t>(agg.data.size()));
    } else {
        // No aggregated data: write empty
        ingest.setBufferData(b.aggBufferId, nullptr, 0);
    }
}

void AggregationManager::rebindGeometry(const AggregationBinding& b,
                                          Id targetBufferId,
                                          IngestProcessor& ingest,
                                          CommandProcessor& cp) {
    // Issue setGeometryBuffer command
    cp.applyJsonText(
        R"({"cmd":"setGeometryBuffer","geometryId":)" + std::to_string(b.geometryId) +
        R"(,"vertexBufferId":)" + std::to_string(targetBufferId) + "}");

    // Update vertex count based on buffer contents
    std::uint32_t bufSize = ingest.getBufferSize(targetBufferId);
    std::uint32_t stride = 24; // candle6
    std::uint32_t vertexCount = (stride > 0) ? bufSize / stride : 0;
    if (vertexCount < 1) vertexCount = 1; // min for geometry validation

    cp.applyJsonText(
        R"({"cmd":"setGeometryVertexCount","geometryId":)" + std::to_string(b.geometryId) +
        R"(,"vertexCount":)" + std::to_string(vertexCount) + "}");
}

} // namespace dc
