# Trial 089: A/B Test Distributions

**Date:** 2026-03-22
**Goal:** Two overlapping bell curves with alpha blending showing A/B test results.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x500 viewport with one pane and two layers.

Two normal distributions rendered as filled areas using triAA@1:
- **Group A** (blue, mean=5.0, sigma=1.2) on layer 10
- **Group B** (orange, mean=6.5, sigma=1.0) on layer 11

Both use additive blending for transparency in the overlap region. Each curve has 24 strip segments (48 triangles) with per-vertex alpha for smooth edges at the baseline.

Total: 9 unique IDs (1 pane, 2 layers, 1 transform, 2 buf+geo+DI groups)

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
- **Baseline alpha.** Bottom vertices have alpha=0.0, top vertices have alpha=0.7 for smooth fadeout at base.
- **Additive blending.** Overlap region adds blue+orange, producing a visible combined intensity.
- **Transform range.** X [0,12] covers both curves (means at 5.0 and 6.5 with 3.5σ tails).

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use triAA@1 for smooth filled areas.** Per-vertex alpha enables gradient edges without separate geometry.
2. **Additive blending for overlapping distributions.** It naturally shows where two datasets coincide.
