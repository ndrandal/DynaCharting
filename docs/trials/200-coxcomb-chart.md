# Trial 200: Coxcomb (Nightingale Rose) Chart

**Date:** 2026-03-22
**Goal:** 12 angular sectors with varying radii, like a Nightingale rose diagram.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 12 colored wedge sectors (triGradient@1). Each sector's radius encodes a random value [0.2, 0.8]. 8 triangle segments per wedge. Aspect-ratio corrected for non-square viewport.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer (1728 floats), 1 geometry (288 verts), 1 drawItem.

## Data Notes

Radii: uniform(0.2, 0.8). 12 equal angular sectors (30 deg each). Seed=200.
