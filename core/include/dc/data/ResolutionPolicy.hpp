#pragma once
#include <cstdint>
#include <vector>

namespace dc {

enum class ResolutionTier : std::uint8_t {
    Raw   = 0,
    Agg2x = 1,
    Agg4x = 2,
    Agg8x = 3
};

inline std::uint32_t tierFactor(ResolutionTier t) {
    switch (t) {
        case ResolutionTier::Raw:   return 1;
        case ResolutionTier::Agg2x: return 2;
        case ResolutionTier::Agg4x: return 4;
        case ResolutionTier::Agg8x: return 8;
    }
    return 1;
}

struct TierThreshold {
    ResolutionTier tier;
    double enterBelow;  // ppdu drops below this → enter coarser tier
    double exitAbove;   // ppdu rises above this → exit back to finer tier
};

struct ResolutionPolicyConfig {
    std::vector<TierThreshold> thresholds = {
        {ResolutionTier::Agg2x, 6.0,  10.0},
        {ResolutionTier::Agg4x, 3.0,  5.0},
        {ResolutionTier::Agg8x, 1.5,  2.5},
    };
};

class ResolutionController {
public:
    void setConfig(const ResolutionPolicyConfig& cfg);
    bool evaluate(double ppdu);           // returns true if tier changed
    ResolutionTier currentTier() const { return tier_; }
    std::uint32_t currentFactor() const { return tierFactor(tier_); }

private:
    ResolutionPolicyConfig config_;
    ResolutionTier tier_{ResolutionTier::Raw};
};

} // namespace dc
