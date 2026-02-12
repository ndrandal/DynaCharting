#include "dc/data/ResolutionPolicy.hpp"

namespace dc {

void ResolutionController::setConfig(const ResolutionPolicyConfig& cfg) {
    config_ = cfg;
}

bool ResolutionController::evaluate(double ppdu) {
    ResolutionTier prev = tier_;

    // Determine target tier: scan thresholds from coarsest to finest.
    // Thresholds are ordered [Agg2x, Agg4x, Agg8x] (finest-to-coarsest enterBelow).
    // We need to find the coarsest tier whose enterBelow threshold is exceeded.

    // First: check if we should go coarser (ppdu dropped below enterBelow for a coarser tier)
    ResolutionTier coarsest = ResolutionTier::Raw;
    for (const auto& th : config_.thresholds) {
        if (ppdu < th.enterBelow) {
            // This tier's enter threshold is met; take the coarsest one
            if (static_cast<std::uint8_t>(th.tier) > static_cast<std::uint8_t>(coarsest)) {
                coarsest = th.tier;
            }
        }
    }

    if (static_cast<std::uint8_t>(coarsest) > static_cast<std::uint8_t>(tier_)) {
        // Going coarser: immediate
        tier_ = coarsest;
    } else if (static_cast<std::uint8_t>(coarsest) < static_cast<std::uint8_t>(tier_)) {
        // Going finer: check exitAbove threshold for current tier (hysteresis)
        // Find the threshold for the current tier
        for (const auto& th : config_.thresholds) {
            if (th.tier == tier_) {
                if (ppdu > th.exitAbove) {
                    // Exit this tier, but don't jump all the way to Raw —
                    // fall back to the coarsest tier that still applies
                    tier_ = coarsest;
                }
                break;
            }
        }
    }

    return tier_ != prev;
}

} // namespace dc
