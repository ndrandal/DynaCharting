# Trial 201: Wind Rose Diagram

**Date:** 2026-03-22
**Goal:** 16 directions x 3 speed bins = 48 wedge segments showing directional frequency.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 48 wedge segments (triGradient@1) in a wind rose pattern. 16 angular directions, each with 3 concentric speed bins (blue/yellow/red). 4 triangle segments per wedge. Aspect-ratio corrected.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer, 1 geometry, 1 drawItem.

## Data Notes

Frequency per bin: uniform(0.05, 0.25). 16 directions * 3 bins = 48 segments. Seed=201.
