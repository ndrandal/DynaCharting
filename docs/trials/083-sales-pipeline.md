# Trial 083: Sales Pipeline

**Date:** 2026-03-22
**Goal:** 5-stage horizontal pipeline bars decreasing left-to-right with rounded corners.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x400 viewport with a single pane.

Five bars representing sales pipeline stages: Leads (500), Qualified (320), Proposal (180), Negotiation (100), Closed (60). Bar heights are proportional to values, centered vertically. Arranged left-to-right with blue gradient darkening. Rounded corners (8px) on all bars.

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
- **Bar height proportionality.** Heights scale linearly from 0.7 (Leads) to 0.084 (Closed) in clip space.
- **Horizontal spacing.** 0.36 clip units per stage with 0.30-wide bars gives 0.06 gap between bars.
- **Vertical centering.** Each bar centered at y=0 creates a symmetric funnel effect.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Center bars vertically for pipeline visuals.** Using ±height/2 creates symmetry that reads as a funnel.
2. **Rounded corners add polish.** cornerRadius=8.0 softens the industrial look.
