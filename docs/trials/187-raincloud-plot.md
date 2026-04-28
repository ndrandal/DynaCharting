# Trial 187: Raincloud Plot

**Date:** 2026-03-22
**Goal:** Combined half-violin, box plot, and jittered raw data points.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 3 layers: (1) half-violin density histogram (triSolid@1, 20 bins), (2) IQR box (instancedRect@1) + median line (lineAA@1), (3) 40 jittered points below. All in clip space.

## Entity Counts

1 pane, 3 layers, 0 transforms, 4 buffers, 4 geometries, 4 drawItems.

## Data Notes

40 Gaussian values (mu=0, sigma=0.25). Q1/median/Q3 at indices 10/20/30. Seed=187.
