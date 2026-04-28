# Trial 192: Diverging Bar Chart

**Date:** 2026-03-22
**Goal:** 20 bars extending left (negative/red) and right (positive/green) from center axis.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 20 horizontal bars diverging from a center axis. Positive values go right (green), negative go left (red). A thin gray center line. triGradient@1 for per-bar colors, lineAA@1 for the axis.

## Entity Counts

1 pane, 1 layer, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

Values: gauss(0, 0.35), clamped to [-0.85, 0.85]. Seed=192.
