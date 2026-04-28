# Trial 182: 5-Series Stacked Area Chart

**Date:** 2026-03-22
**Goal:** 5 stacked area series with 20 data points each, rendered as triangle strips.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 5 area series stacked bottom-to-top. Each series is triSolid@1 triangulated from cumulative sums. 19 quads (38 triangles, 114 vertices) per series. Transform maps x=[0,19], y=[0,~5] to clip space.

## Entity Counts

1 pane, 1 layer, 1 transform, 5 buffers, 5 geometries, 5 drawItems.

## Data Notes

Random values [0.3,1.0] per point. Cumulative stacking. Seed=182.
