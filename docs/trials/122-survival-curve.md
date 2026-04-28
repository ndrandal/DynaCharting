# Trial 122: Kaplan-Meier Survival Curve

**Date:** 2026-03-22
**Goal:** Kaplan-Meier step function with 12 event steps (lineAA@1) and semi-transparent confidence band (triSolid@1).
**Outcome:** Kaplan-Meier survival curve with 13 time points (12 steps) rendered as a step function (24 lineAA@1 segments). Zero defects.

---

## What Was Built
Viewport 800x500. Kaplan-Meier survival curve with 13 time points (12 steps) rendered as a step function (24 lineAA@1 segments). Semi-transparent confidence band (triSolid@1, 144 vertices, alpha=0.2) shows +-6% uncertainty. Survival decreases from 1.0 to 0.10 over 65 time units.

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 10 | Conf band | triSolid@1 | 144 vtx | cyan 20% |
| 105 | 11 | KM curve | lineAA@1 | 24 seg | cyan |

Total: 10 unique IDs (1 pane, 2 layers, 1 transform, 2 buffers, 2 geometrys, 2 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Step function correctly renders as horizontal segments at each survival probability followed by vertical drops at event times.** 
- **Survival starts at 1.0 (all alive) and monotonically decreases, matching KM estimator behavior.** 
- **Confidence band is rendered behind the curve (layer 10 < 11) with 20% alpha transparency.** 
- **All 12 step drops are visible at the specified event times.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **KM step functions need horizontal-then-vertical point pairs: (t_i, S_i), (t_{i+1}, S_i) for each step.** 
2. **Confidence bands as triSolid area fill need paired upper/lower bounds at each x coordinate.** 
