# Trial 088: Project Burndown

**Date:** 2026-03-22
**Goal:** Burndown chart with actual progress line and dashed ideal trend line.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x500 viewport with one pane and two layers.

1. **Ideal line** (layer 10) -- Gray dashed lineAA showing linear burndown from 100 to 0 over 10 sprints.
2. **Actual line** (layer 11) -- Cyan solid lineAA showing actual progress: 100→92→85→78→68→60→48→38→25→12→5.

Actual line slightly underperforms ideal in early sprints but catches up.

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
- **Draw order.** Ideal (dashed, gray) on layer 10 behind actual (solid, cyan) on layer 11.
- **Data padding.** Viewport range [−0.5,10.5]×[−5,110] extends beyond data for margin.
- **Dashed vs solid.** Distinct visual treatment clearly separates ideal from actual.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Put reference lines behind data lines.** Ideal trend renders first (lower layer ID), data overlays it.
2. **Use alpha for secondary data.** Ideal line at alpha 0.6 recedes visually while remaining visible.
