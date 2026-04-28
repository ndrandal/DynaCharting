# Trial 178: Dense Scatter Plot with 500 Points

**Date:** 2026-03-22
**Goal:** 500 scatter points in 3 Gaussian-distributed color clusters.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 500 points distributed across 3 Gaussian clusters (blue at ~(2,5), red at ~(7,3), green at ~(5.5,8)). Each cluster rendered as a separate DrawItem with pointSize=3. Transform maps data range ~[0,10] to clip space.

## Entity Counts

1 pane, 1 layer, 1 transform, 3 buffers, 3 geometries, 3 drawItems (180+160+160 points).

## Data Notes

Gaussian random with seed=178. Cluster sizes: 180, 160, 160 = 500 total.
