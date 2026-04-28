# Trial 196: Alluvial Flow Diagram

**Date:** 2026-03-22
**Goal:** Flowing bands between 3 columns showing how 4 categories redistribute.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with flowing bands (triGradient@1) connecting 3 columns. 4 categories flow between columns with smooth cubic interpolation (10 segments per flow). Solid column blocks overlay the flow bands.

## Entity Counts

1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

Category sizes vary per column. Smoothstep interpolation for band curvature. Seed=196.
