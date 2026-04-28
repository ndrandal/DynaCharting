# Trial 198: Dot Density Grid (4x4)

**Date:** 2026-03-22
**Goal:** 4x4 grid regions with random point densities, ~200 points total.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 242 points (points@1) scattered across 16 grid cells. Density varies by region (5-25 points per cell). pointSize=3. All in clip space.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer (484 floats), 1 geometry (242 verts), 1 drawItem.

## Data Notes

Per-cell count: randint(5,25). Points uniform within cell bounds. Seed=198.
