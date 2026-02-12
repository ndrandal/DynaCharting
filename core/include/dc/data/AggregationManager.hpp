#pragma once
#include "dc/ids/Id.hpp"
#include "dc/data/ResolutionPolicy.hpp"
#include "dc/data/CandleAggregator.hpp"

#include <vector>

namespace dc {

class IngestProcessor;
class CommandProcessor;

struct AggregationBinding {
    Id rawBufferId;       // original candle buffer from recipe
    Id aggBufferId;       // shadow buffer for aggregated data (raw + offset)
    Id geometryId;        // geometry to rebind
};

struct AggregationManagerConfig {
    ResolutionPolicyConfig resolutionPolicy;
    Id aggBufferIdOffset{50000};  // aggBufId = rawBufId + offset
};

class AggregationManager {
public:
    void setConfig(const AggregationManagerConfig& cfg);
    void addBinding(const AggregationBinding& binding);
    void clearBindings();

    // Returns modified agg buffer IDs. Issues setGeometryBuffer + setGeometryVertexCount.
    std::vector<Id> onViewportChanged(double ppdu, IngestProcessor& ingest,
                                       CommandProcessor& cp);

    // If not at Raw tier, recomputes agg buffers for touched raw buffers.
    std::vector<Id> onRawDataChanged(const std::vector<Id>& touchedRaw,
                                      IngestProcessor& ingest);

    ResolutionTier currentTier() const { return ctrl_.currentTier(); }

private:
    void recomputeAggBuffer(const AggregationBinding& b, IngestProcessor& ingest);
    void rebindGeometry(const AggregationBinding& b, Id targetBufferId,
                        IngestProcessor& ingest, CommandProcessor& cp);

    AggregationManagerConfig config_;
    ResolutionController ctrl_;
    std::vector<AggregationBinding> bindings_;
};

} // namespace dc
