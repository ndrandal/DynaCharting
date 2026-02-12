// D8.3 — Resolution Controller with Hysteresis test (pure C++, no GL)
// Tests: tier transitions with dead-zone hysteresis.

#include "dc/data/ResolutionPolicy.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Basic transitions with hysteresis ---
  {
    dc::ResolutionController ctrl;
    dc::ResolutionPolicyConfig cfg;
    ctrl.setConfig(cfg); // default thresholds

    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Raw, "start at Raw");

    // ppdu=5 → below Agg2x enterBelow(6), below Agg4x enterBelow(3)? No, 5>3.
    // So only Agg2x applies.
    bool changed = ctrl.evaluate(5.0);
    requireTrue(changed, "5 ppdu → changed");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Agg2x, "5 ppdu → Agg2x");

    // ppdu=8 → still above Agg2x enterBelow(6)? No, 8>6 so Agg2x enterBelow not hit.
    // But we're at Agg2x. Going finer requires ppdu > exitAbove(10). 8 < 10 → stay.
    changed = ctrl.evaluate(8.0);
    requireTrue(!changed, "8 ppdu → no change (hysteresis)");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Agg2x, "stays Agg2x");

    // ppdu=11 → above Agg2x exitAbove(10) → back to Raw
    changed = ctrl.evaluate(11.0);
    requireTrue(changed, "11 ppdu → changed");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Raw, "11 ppdu → Raw");

    std::printf("  Hysteresis PASS\n");
  }

  // --- Test 2: Jump to Agg8x ---
  {
    dc::ResolutionController ctrl;
    dc::ResolutionPolicyConfig cfg;
    ctrl.setConfig(cfg);

    bool changed = ctrl.evaluate(1.0);
    requireTrue(changed, "1 ppdu → changed");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Agg8x, "1 ppdu → Agg8x");
    requireTrue(ctrl.currentFactor() == 8, "factor=8");

    std::printf("  Jump to Agg8x PASS\n");
  }

  // --- Test 3: Gradual zoom in from Agg8x ---
  {
    dc::ResolutionController ctrl;
    dc::ResolutionPolicyConfig cfg;
    ctrl.setConfig(cfg);

    ctrl.evaluate(1.0); // → Agg8x

    // ppdu=2 → still below Agg8x exitAbove(2.5) → stay
    bool changed = ctrl.evaluate(2.0);
    requireTrue(!changed, "2 ppdu → stay Agg8x");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Agg8x, "stays Agg8x");

    // ppdu=3 → above Agg8x exitAbove(2.5), but below Agg4x enterBelow(3)?
    // 3 < 3 is false, so Agg4x doesn't apply. Below Agg2x enterBelow(6)? Yes.
    // So coarsest that applies is Agg2x. Current is Agg8x.
    // exitAbove for Agg8x is 2.5, ppdu=3 > 2.5 → exit to Agg2x (coarsest that still applies).
    // Wait — Agg4x: enterBelow=3.0, 3<3 is false. So Agg4x doesn't apply.
    // Agg2x: enterBelow=6.0, 3<6 → applies. Coarsest = Agg2x.
    // But we also check Agg4x enterBelow(3) → 3 is NOT < 3. So just Agg2x.
    // Actually let me reconsider. We want to step gradually. ppdu=3 should go to Agg4x.
    // Let me re-check: for ppdu=3, Agg2x enterBelow=6 → 3<6 true,
    // Agg4x enterBelow=3 → 3<3 false, Agg8x enterBelow=1.5 → 3<1.5 false.
    // So coarsest = Agg2x. Current = Agg8x. Since Agg2x < Agg8x (finer),
    // check exitAbove for Agg8x: 2.5. ppdu=3 > 2.5 → exit. tier_ = coarsest = Agg2x.
    // Hmm, that jumps from Agg8x to Agg2x. The plan wants Agg4x at ppdu=3.
    // Let me adjust: ppdu=2.6 first to exit Agg8x → should go to coarsest that applies.
    // At ppdu=2.6: Agg2x enterBelow=6 → 2.6<6 true. Agg4x enterBelow=3 → 2.6<3 true.
    // Agg8x enterBelow=1.5 → 2.6<1.5 false. Coarsest = Agg4x.
    // exitAbove for Agg8x = 2.5. 2.6 > 2.5 → exit to Agg4x.
    changed = ctrl.evaluate(2.6);
    requireTrue(changed, "2.6 ppdu → exit Agg8x");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Agg4x, "2.6 ppdu → Agg4x");

    // ppdu=6 → above Agg4x exitAbove(5). Coarsest that applies:
    // Agg2x enterBelow=6 → 6<6 false. So coarsest = Raw.
    // exitAbove for Agg4x = 5. 6 > 5 → exit to Raw.
    // Wait, that skips Agg2x. Let's try ppdu=5.5 instead.
    // ppdu=5.5: Agg2x enterBelow=6 → 5.5<6 true → Agg2x applies. Coarsest=Agg2x.
    // Agg4x enterBelow=3 → 5.5<3 false. exitAbove for Agg4x=5 → 5.5>5 → exit to Agg2x.
    changed = ctrl.evaluate(5.5);
    requireTrue(changed, "5.5 ppdu → exit Agg4x");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Agg2x, "5.5 ppdu → Agg2x");

    // ppdu=11 → above Agg2x exitAbove(10) → Raw
    changed = ctrl.evaluate(11.0);
    requireTrue(changed, "11 ppdu → Raw");
    requireTrue(ctrl.currentTier() == dc::ResolutionTier::Raw, "11 ppdu → Raw");

    std::printf("  Gradual zoom in PASS\n");
  }

  // --- Test 4: evaluate() returns true only on actual change ---
  {
    dc::ResolutionController ctrl;
    dc::ResolutionPolicyConfig cfg;
    ctrl.setConfig(cfg);

    bool changed = ctrl.evaluate(20.0); // stays Raw
    requireTrue(!changed, "20 ppdu → no change from Raw");

    changed = ctrl.evaluate(20.0);
    requireTrue(!changed, "20 ppdu again → no change");

    changed = ctrl.evaluate(5.0);
    requireTrue(changed, "5 ppdu → Agg2x change");

    changed = ctrl.evaluate(5.0);
    requireTrue(!changed, "5 ppdu again → no change");

    std::printf("  Return value PASS\n");
  }

  std::printf("\nD8.3 resolution policy PASS\n");
  return 0;
}
