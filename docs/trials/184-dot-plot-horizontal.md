# Trial 184: Horizontal Dot Plot (6x25)

**Date:** 2026-03-22
**Goal:** 6 categories x 25 dots each = 150 points, arranged horizontally per category row.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 150 points across 6 horizontal category rows. Each category has 25 Gaussian-distributed points along x, with minimal y jitter. pointSize=4. Each category is a separate DrawItem with a distinct color.

## Entity Counts

1 pane, 1 layer, 1 transform, 6 buffers, 6 geometries, 6 drawItems.

## Data Notes

X values: gauss(5+cat*1.5, 1.5). Y jitter: uniform(-0.05, 0.05). Seed=184.
