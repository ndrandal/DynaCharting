# Trial 092: Vertical Conversion Funnel

**Date:** 2026-03-22
**Goal:** Vertical funnel with 5 trapezoidal shapes narrowing downward.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

600x800 viewport (portrait) with one pane.

Five trapezoids stacked vertically, each narrower than the one above:
Awareness (100%) → Interest (75%) → Desire (55%) → Action (40%) → Loyalty (28%).
Each stage is two triangles forming a trapezoid. Colors progress from blue through green/yellow to red. 0.04 clip-space gaps between stages.

Total: 18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)

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
- **Trapezoid construction.** Each stage has top-width matching its fraction and bottom-width matching the next stage's fraction, creating smooth narrowing.
- **Vertical spacing.** 0.30 height per stage + 0.04 gap = 0.34 per step, 5 stages fit in 1.7 clip units.
- **Color progression.** Blue→cyan→green→yellow→red maps the customer journey intuitively.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Connect adjacent trapezoid widths.** The bottom of each stage matches the top of the next for visual continuity.
2. **Use portrait aspect ratio for vertical funnels.** 600x800 gives each stage more horizontal room.
