# Trial 079: E-Commerce Funnel

**Date:** 2026-03-22
**Goal:** Single-pane funnel visualization with 5 horizontal bars narrowing top-to-bottom.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

600x800 viewport with a single pane.

5 horizontal bars representing conversion funnel stages (Visits → Complete). Each bar is centered horizontally with width proportional to its value. Bars are colored in a blue gradient that darkens from top (lightest) to bottom (darkest). Rounded corners (6px) on all bars.

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
- **Bar centering.** Each bar is symmetrically positioned around x=0 with half-width proportional to value/max.
- **Vertical spacing.** 0.34 clip units between bar tops, 0.28 bar height, leaving 0.06 gap between bars.
- **Color gradient.** Five distinct blue shades from [0.3,0.6,1.0] to [0.1,0.2,0.6] convey funnel narrowing.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use separate draw items for per-element colors.** instancedRect@1 applies one color to all rects, so separate buffers per stage allow distinct colors.
2. **Pre-computed clip space works well for static layouts.** No transform needed when data is directly in clip coordinates.
