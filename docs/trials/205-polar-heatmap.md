# Trial 205: Polar Heatmap (8x5)

**Date:** 2026-03-22
**Goal:** 8 angular x 5 radial cells forming an annular heatmap with viridis coloring.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 40 wedge-shaped cells (triGradient@1). Each cell colored by viridis approximation of a random value. 6 triangle segments per cell for smooth arcs. Aspect-ratio corrected.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.

## Data Notes

40 cells (8 angular * 5 radial). Values: uniform(0,1). Seed=205.
