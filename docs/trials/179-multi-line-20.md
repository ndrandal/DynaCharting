# Trial 179: 20 Overlaid Line Series

**Date:** 2026-03-22
**Goal:** 20 line series with 25 data points each, overlaid in semi-random walk patterns.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 20 lineAA lines, each with 24 segments (25 points). Each line has a distinct hue from the HSL color wheel. Data-space transform maps x=[0,24], y~[-8,8] into the pane.

## Entity Counts

1 pane, 1 layer, 1 transform, 20 buffers, 20 geometries, 20 drawItems.

## Data Notes

Random walk with Gaussian steps (sigma=0.5). Seed=179. 20 distinct colors via HSL.
