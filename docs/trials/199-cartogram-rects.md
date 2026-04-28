# Trial 199: Cartogram Rectangles

**Date:** 2026-03-22
**Goal:** 12 rectangles sized proportional to values, packed in approximate geographic layout.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 12 rectangles (triGradient@1) in a 4x3 grid. Widths proportional to value within each row. Heights scaled by row average. Each rectangle has a distinct color.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer (12*6*6=432 floats), 1 geometry (72 verts), 1 drawItem.

## Data Notes

Values: uniform(0.3, 1.0). Width proportional within row. Seed=199.
