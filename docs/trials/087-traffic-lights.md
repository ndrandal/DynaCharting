# Trial 087: Traffic Lights

**Date:** 2026-03-22
**Goal:** Three colored circles (red/yellow/green) on dark housing background.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

300x700 viewport (portrait) with a single pane.

A traffic light housing (dark gray rounded rectangle) with three circles: red (top), yellow (middle), green (bottom). Each circle uses 32-segment center-fan tessellation for smooth rendering. Circle radius 0.18, spaced 0.5 apart.

Total: 15 unique IDs (1 pane, 2 layers, 3×(buf+geo+di) for circles + 1 housing group = 12)

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
- **Vertical spacing.** Circles at y=0.5, 0.0, −0.5 with r=0.18 leave 0.14 gap between circles.
- **Housing proportions.** Rectangle [-0.35,−0.78] to [0.35,0.78] encloses all three circles with padding.
- **Layer ordering.** Housing on layer 10 renders behind circles on layer 11.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use sufficient tessellation for circles.** 32 segments is enough for smooth appearance at this size.
2. **Portrait aspect ratio for vertical layouts.** 300x700 viewport naturally suits the traffic light form.
