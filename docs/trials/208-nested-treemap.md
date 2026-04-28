# Trial 208: Nested Treemap (2-Level)

**Date:** 2026-03-22
**Goal:** 4 parent rectangles each containing 3-4 child rects, with parent outlines.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with ~15 child rectangles (triGradient@1) nested inside 4 parent regions. Parent outlines drawn with lineAA@1. Children sized proportional to value. 2x2 parent layout, children stacked horizontally.

## Entity Counts

1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

Parent sizes by child sum. Child values: uniform(1,5). Seed=208.
