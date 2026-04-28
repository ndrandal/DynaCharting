# Trial 197: Proportional Symbol Map

**Date:** 2026-03-22
**Goal:** 15 circles of varying radius on a 5x3 grid, sized by data value.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 15 circles (triGradient@1, 24-segment center-fan each). Radius proportional to random value [0.3, 1.0]. Each circle has a distinct color. Arranged in a 5x3 grid layout.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer (15*24*3*6=6480 floats), 1 geometry (1080 verts), 1 drawItem.

## Data Notes

Radii: value * 0.12. 24 triangle segments per circle. Seed=197.
