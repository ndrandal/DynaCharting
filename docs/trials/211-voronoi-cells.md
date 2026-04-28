# Trial 211: Voronoi Cell Diagram

**Date:** 2026-03-22
**Goal:** 8 approximate Voronoi regions via grid rasterization with seed points overlaid.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with approximate Voronoi regions (triGradient@1, 40x40 grid = 1600 cells). Each grid cell colored by nearest seed. 8 white seed points overlaid. Rasterized approximation — not exact Voronoi edges.

## Entity Counts

1 pane, 2 layers, 0 transforms, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

8 random seeds in [-0.7, 0.7]. 40x40 raster grid. Nearest-neighbor assignment. Seed=211.
