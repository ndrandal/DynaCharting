# Trial 185: Strip Plot with Jitter (5x30)

**Date:** 2026-03-22
**Goal:** 5 categories x 30 points each = 150 points, jittered vertically within category bands.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 150 points in 5 jittered strip categories. Y jitter is Gaussian (sigma=0.3) within each band. pointSize=3.

## Entity Counts

1 pane, 1 layer, 1 transform, 5 buffers, 5 geometries, 5 drawItems.

## Data Notes

X: gauss(cat+2, 0.8). Y: cat*2 + gauss(0, 0.3). Seed=185.
